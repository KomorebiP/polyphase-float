/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef PPF_TEXT_POWER_LIMIT
#define PPF_TEXT_POWER_LIMIT 2000000L
#endif

static ppf_rnd_t resolve_rounding(ppf_rnd_t rnd, ppf_context_t *ctx) {
    if (rnd == PPF_RND_DEFAULT) return ctx ? ctx->default_rounding : PPF_RNDN;
    return rnd;
}

static void set_mpz_uintmax(mpz_ptr z, uintmax_t value) {
    mpz_import(z, 1, -1, sizeof(value), 0, 0, &value);
}

static void set_mpz_intmax(mpz_ptr z, intmax_t value) {
    uintmax_t magnitude;
    if (value < 0) {
        magnitude = (uintmax_t)(-(value + 1));
        magnitude += 1;
        set_mpz_uintmax(z, magnitude);
        mpz_neg(z, z);
    } else {
        set_mpz_uintmax(z, (uintmax_t)value);
    }
}

static int increment_for_fraction(ppf_rnd_t rnd, int sign,
                                  mpz_srcptr quotient,
                                  mpz_srcptr remainder,
                                  mpz_srcptr denominator) {
    int cmp;
    mpz_t twice;
    if (mpz_sgn(remainder) == 0) return 0;
    switch (rnd) {
        case PPF_RNDZ:
        case PPF_RNDF:
            return 0;
        case PPF_RNDA:
            return 1;
        case PPF_RNDU:
            return sign > 0;
        case PPF_RNDD:
            return sign < 0;
        case PPF_RNDN:
            mpz_init(twice);
            mpz_mul_2exp(twice, remainder, 1);
            cmp = mpz_cmp(twice, denominator);
            mpz_clear(twice);
            if (cmp > 0) return 1;
            if (cmp < 0) return 0;
            return mpz_odd_p(quotient) != 0;
        default:
            return 0;
    }
}

static int set_overflow_local(ppf_t *rop, int sign, ppf_rnd_t rnd,
                              ppf_context_t *ctx) {
    int infinity;
    if (rnd == PPF_RNDN || rnd == PPF_RNDA) infinity = 1;
    else if (rnd == PPF_RNDU) infinity = sign > 0;
    else if (rnd == PPF_RNDD) infinity = sign < 0;
    else infinity = 0;
    if (infinity) ppf_set_inf(rop, sign);
    else if (ppf_set_max_finite(rop, sign, ctx) != 0) return PPF_TERNARY_ERROR;
    ppf_raise(ctx, PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT);
    if (sign > 0) return infinity ? 1 : -1;
    return infinity ? -1 : 1;
}

static int compare_ratio_power2(mpz_srcptr numerator,
                                mpz_srcptr denominator,
                                ppf_exp_t power) {
    size_t nb, db;
    uint64_t shift;
    mpz_t temp;
    int cmp;
    nb = mpz_sizeinbase(numerator, 2);
    db = mpz_sizeinbase(denominator, 2);
    if (power >= 0) {
        uint64_t rhs_bits;
        if ((uint64_t)power > UINT64_MAX - db) return -1;
        rhs_bits = db + (uint64_t)power;
        if ((uint64_t)nb < rhs_bits) return -1;
        if ((uint64_t)nb > rhs_bits) return 1;
        shift = (uint64_t)power;
        if (shift > ULONG_MAX) return -1;
        mpz_init(temp);
        mpz_mul_2exp(temp, denominator, (mp_bitcnt_t)shift);
        cmp = mpz_cmp(numerator, temp);
        mpz_clear(temp);
        return (cmp > 0) - (cmp < 0);
    }
    shift = (uint64_t)(-(power + 1)) + 1;
    if (shift > UINT64_MAX - nb) return 1;
    if ((uint64_t)nb + shift < db) return -1;
    if ((uint64_t)nb + shift > db) return 1;
    if (shift > ULONG_MAX) return 1;
    mpz_init(temp);
    mpz_mul_2exp(temp, numerator, (mp_bitcnt_t)shift);
    cmp = mpz_cmp(temp, denominator);
    mpz_clear(temp);
    return (cmp > 0) - (cmp < 0);
}

static int set_underflow_ratio(ppf_t *rop, mpz_srcptr numerator,
                               mpz_srcptr denominator, int sign,
                               ppf_rnd_t rnd, ppf_context_t *ctx) {
    int minimum = 0;
    int cmp;
    if (rnd == PPF_RNDA) minimum = 1;
    else if (rnd == PPF_RNDU) minimum = sign > 0;
    else if (rnd == PPF_RNDD) minimum = sign < 0;
    else if (rnd == PPF_RNDN) {
        cmp = compare_ratio_power2(numerator, denominator, ctx->emin - 2);
        minimum = cmp > 0; /* exact halfway goes to zero */
    }
    if (minimum) {
        if (ppf_set_min_normal(rop, sign, ctx) != 0) return PPF_TERNARY_ERROR;
    } else {
        ppf_set_zero(rop, sign);
    }
    ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
    if (sign > 0) return minimum ? 1 : -1;
    return minimum ? -1 : 1;
}

/* Called only after proving the magnitude lies well below half of the
   smallest normal value.  Its significand alone is not the original ratio. */
static int set_underflow_tiny_local(ppf_t *rop, int sign, ppf_rnd_t rnd,
                                   ppf_context_t *ctx) {
    int minimum = rnd == PPF_RNDA || (rnd == PPF_RNDU && sign > 0) ||
                  (rnd == PPF_RNDD && sign < 0);
    if (minimum) {
        if (ppf_set_min_normal(rop, sign, ctx) != 0) return PPF_TERNARY_ERROR;
    } else ppf_set_zero(rop, sign);
    ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
    return sign > 0 ? (minimum ? 1 : -1) : (minimum ? -1 : 1);
}

static int set_ratio(ppf_t *rop, mpz_srcptr signed_numerator,
                     mpz_srcptr positive_denominator,
                     ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t n, d, scaled_n, scaled_d, q, r;
    size_t nb, db;
    ppf_exp_t k, exponent, shift;
    int sign, cmp, inc, inexact, ternary = PPF_TERNARY_ERROR;

    ctx = ppf_resolve_context(ctx);
    if (!rop || !signed_numerator || !positive_denominator || !ctx ||
        mpz_sgn(positive_denominator) <= 0) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    rnd = resolve_rounding(rnd, ctx);
    if (!ppf_rounding_valid(rnd)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (mpz_sgn(signed_numerator) == 0) {
        ppf_set_zero(rop, 1);
        return 0;
    }

    sign = mpz_sgn(signed_numerator) < 0 ? -1 : 1;
    mpz_inits(n, d, scaled_n, scaled_d, q, r, NULL);
    mpz_abs(n, signed_numerator);
    mpz_set(d, positive_denominator);
    nb = mpz_sizeinbase(n, 2);
    db = mpz_sizeinbase(d, 2);
    if (nb > (size_t)INT64_MAX || db > (size_t)INT64_MAX ||
        rop->precision > (ppf_prec_t)INT64_MAX) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        goto done;
    }

    k = (ppf_exp_t)nb - (ppf_exp_t)db;
    cmp = compare_ratio_power2(n, d, k);
    if (cmp < 0) k--;
    if (k == INT64_MAX) {
        ternary = set_overflow_local(rop, sign, rnd, ctx);
        goto done;
    }
    exponent = k + 1;

    if (exponent > ctx->emax) {
        ternary = set_overflow_local(rop, sign, rnd, ctx);
        goto done;
    }
    if (exponent < ctx->emin - 1) {
        ternary = set_underflow_ratio(rop, n, d, sign, rnd, ctx);
        goto done;
    }

    shift = (ppf_exp_t)rop->precision - exponent;
    mpz_set(scaled_n, n);
    mpz_set(scaled_d, d);
    if (shift >= 0) {
        if ((uint64_t)shift > ULONG_MAX) {
            ppf_raise(ctx, PPF_FLAG_ERANGE);
            goto done;
        }
        mpz_mul_2exp(scaled_n, scaled_n, (mp_bitcnt_t)shift);
    } else {
        uint64_t right = (uint64_t)(-(shift + 1)) + 1;
        if (right > ULONG_MAX) {
            ppf_raise(ctx, PPF_FLAG_ERANGE);
            goto done;
        }
        mpz_mul_2exp(scaled_d, scaled_d, (mp_bitcnt_t)right);
    }
    mpz_fdiv_qr(q, r, scaled_n, scaled_d);
    inexact = mpz_sgn(r) != 0;
    inc = increment_for_fraction(rnd, sign, q, r, scaled_d);
    if (inc) mpz_add_ui(q, q, 1);
    if ((ppf_prec_t)mpz_sizeinbase(q, 2) > rop->precision) {
        mpz_fdiv_q_2exp(q, q, 1);
        if (exponent == INT64_MAX) {
            ternary = set_overflow_local(rop, sign, rnd, ctx);
            goto done;
        }
        exponent++;
    }

    if (exponent > ctx->emax) {
        ternary = set_overflow_local(rop, sign, rnd, ctx);
        goto done;
    }
    if (exponent < ctx->emin) {
        ternary = set_underflow_ratio(rop, n, d, sign, rnd, ctx);
        goto done;
    }
    if (ppf_store_normalized_mpz(rop, q, exponent, sign) != 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        goto done;
    }
    if (!inexact) {
        ternary = 0;
    } else {
        ppf_raise(ctx, PPF_FLAG_INEXACT);
        ternary = sign > 0 ? (inc ? 1 : -1) : (inc ? -1 : 1);
    }

done:
    mpz_clears(n, d, scaled_n, scaled_d, q, r, NULL);
    return ternary;
}

int ppf_set_default_prec(ppf_prec_t precision) {
    return ppf_context_set_default_precision(NULL, precision);
}

ppf_prec_t ppf_get_default_prec(void) {
    ppf_context_t *ctx = ppf_default_context();
    return ctx ? ctx->default_precision : 0;
}

int ppf_inits2(ppf_prec_t precision, ppf_t *first, ...) {
    va_list ap;
    ppf_t *x = first;
    ppf_t **initialized = NULL;
    size_t count = 0, capacity = 0, i;
    int rc = 0;
    va_start(ap, first);
    while (x) {
        if (count == capacity) {
            size_t next = capacity ? capacity * 2 : 8;
            ppf_t **tmp = (ppf_t **)realloc(initialized, next * sizeof(*tmp));
            if (!tmp) { rc = -1; break; }
            initialized = tmp;
            capacity = next;
        }
        if (ppf_init2(x, precision) != 0) { rc = -1; break; }
        initialized[count++] = x;
        x = va_arg(ap, ppf_t *);
    }
    va_end(ap);
    if (rc != 0) {
        for (i = 0; i < count; ++i) ppf_clear(initialized[i]);
    }
    free(initialized);
    return rc;
}

int ppf_inits(ppf_t *first, ...) {
    va_list ap;
    ppf_t *x = first;
    ppf_prec_t precision = ppf_get_default_prec();
    ppf_t **initialized = NULL;
    size_t count = 0, capacity = 0, i;
    int rc = 0;
    va_start(ap, first);
    while (x) {
        if (count == capacity) {
            size_t next = capacity ? capacity * 2 : 8;
            ppf_t **tmp = (ppf_t **)realloc(initialized, next * sizeof(*tmp));
            if (!tmp) { rc = -1; break; }
            initialized = tmp;
            capacity = next;
        }
        if (ppf_init2(x, precision) != 0) { rc = -1; break; }
        initialized[count++] = x;
        x = va_arg(ap, ppf_t *);
    }
    va_end(ap);
    if (rc != 0) {
        for (i = 0; i < count; ++i) ppf_clear(initialized[i]);
    }
    free(initialized);
    return rc;
}

void ppf_clears(ppf_t *first, ...) {
    va_list ap;
    ppf_t *x = first;
    va_start(ap, first);
    while (x) {
        ppf_clear(x);
        x = va_arg(ap, ppf_t *);
    }
    va_end(ap);
}

int ppf_set_uj_2exp(ppf_t *rop, uintmax_t value, ppf_exp_t exponent,
                    ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int t;
    mpz_init(z);
    set_mpz_uintmax(z, value);
    t = ppf_set_z_2exp(rop, z, exponent, rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_set_sj_2exp(ppf_t *rop, intmax_t value, ppf_exp_t exponent,
                    ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int t;
    mpz_init(z);
    set_mpz_intmax(z, value);
    t = ppf_set_z_2exp(rop, z, exponent, rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_set_uj(ppf_t *rop, uintmax_t value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_set_uj_2exp(rop, value, 0, rnd, ctx);
}

int ppf_set_sj(ppf_t *rop, intmax_t value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_set_sj_2exp(rop, value, 0, rnd, ctx);
}

int ppf_set_ui_2exp(ppf_t *rop, unsigned long value, ppf_exp_t exponent,
                    ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int t;
    mpz_init_set_ui(z, value);
    t = ppf_set_z_2exp(rop, z, exponent, rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_set_si_2exp(ppf_t *rop, long value, ppf_exp_t exponent,
                    ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int t;
    mpz_init_set_si(z, value);
    t = ppf_set_z_2exp(rop, z, exponent, rnd, ctx);
    mpz_clear(z);
    return t;
}

static int set_binary_real(ppf_t *rop, long double value, int mantissa_bits,
                           ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    long double fraction;
    int exponent, i, sign, t;
    ctx = ppf_resolve_context(ctx);
    rnd = resolve_rounding(rnd, ctx);
    if (!rop || !ctx || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (isnan(value)) {
        ppf_set_nan(rop, signbit(value) ? -1 : 1);
        ppf_raise(ctx, PPF_FLAG_NAN);
        return 0;
    }
    if (isinf(value)) {
        ppf_set_inf(rop, signbit(value) ? -1 : 1);
        return 0;
    }
    if (value == 0.0L) {
        ppf_set_zero(rop, signbit(value) ? -1 : 1);
        return 0;
    }
    sign = signbit(value) ? -1 : 1;
    fraction = frexpl(fabsl(value), &exponent);
    mpz_init_set_ui(z, 0);
    for (i = 0; i < mantissa_bits; ++i) {
        fraction *= 2.0L;
        if (fraction >= 1.0L) {
            mpz_setbit(z, (mp_bitcnt_t)(mantissa_bits - 1 - i));
            fraction -= 1.0L;
        }
    }
    if (sign < 0) mpz_neg(z, z);
    t = ppf_set_z_2exp(rop, z, (ppf_exp_t)exponent - mantissa_bits,
                       rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_set_flt(ppf_t *rop, float value,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    return set_binary_real(rop, (long double)value, FLT_MANT_DIG, rnd, ctx);
}

int ppf_set_d(ppf_t *rop, double value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return set_binary_real(rop, (long double)value, DBL_MANT_DIG, rnd, ctx);
}

int ppf_set_ld(ppf_t *rop, long double value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return set_binary_real(rop, value, LDBL_MANT_DIG, rnd, ctx);
}

int ppf_set_q(ppf_t *rop, mpq_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    if (!value) return PPF_TERNARY_ERROR;
    return set_ratio(rop, mpq_numref(value), mpq_denref(value), rnd, ctx);
}

int ppf_set_f(ppf_t *rop, mpf_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpq_t q;
    int t;
    if (!value) return PPF_TERNARY_ERROR;
    mpq_init(q);
    mpq_set_f(q, value);
    t = ppf_set_q(rop, q, rnd, ctx);
    mpq_clear(q);
    return t;
}

static int init_common(ppf_t *rop, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    return ctx ? ppf_init2_ctx(rop, ctx->default_precision, ctx) : -1;
}

#define DEFINE_INIT_SET(name, type, setter) \
int name(ppf_t *rop, type value, ppf_rnd_t rnd, ppf_context_t *ctx) { \
    int t; \
    if (init_common(rop, ctx) != 0) return PPF_TERNARY_ERROR; \
    t = setter(rop, value, rnd, ctx); \
    return t; \
}

DEFINE_INIT_SET(ppf_init_set, const ppf_t *, ppf_set)
DEFINE_INIT_SET(ppf_init_set_ui, unsigned long, ppf_set_ui)
DEFINE_INIT_SET(ppf_init_set_si, long, ppf_set_si)
DEFINE_INIT_SET(ppf_init_set_uj, uintmax_t, ppf_set_uj)
DEFINE_INIT_SET(ppf_init_set_sj, intmax_t, ppf_set_sj)
DEFINE_INIT_SET(ppf_init_set_flt, float, ppf_set_flt)
DEFINE_INIT_SET(ppf_init_set_d, double, ppf_set_d)
DEFINE_INIT_SET(ppf_init_set_ld, long double, ppf_set_ld)
DEFINE_INIT_SET(ppf_init_set_z, mpz_srcptr, ppf_set_z)
DEFINE_INIT_SET(ppf_init_set_q, mpq_srcptr, ppf_set_q)
DEFINE_INIT_SET(ppf_init_set_f, mpf_srcptr, ppf_set_f)

#undef DEFINE_INIT_SET

/* ---------- text parsing ---------- */

static int digit_value(int c, int base) {
    if (c >= '0' && c <= '9') return c - '0';
    if (base <= 36) {
        if (c >= 'a' && c <= 'z') return c - 'a' + 10;
        if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    } else {
        if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
        if (c >= 'a' && c <= 'z') return c - 'a' + 36;
    }
    return -1;
}

static int ascii_case_prefix(const char *s, const char *word) {
    while (*word) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*word)) return 0;
        ++s; ++word;
    }
    return 1;
}

static int parse_decimal_exponent(const char *s, const char **end,
                                  ppf_exp_t *value) {
    int sign = 1, any = 0;
    uint64_t magnitude = 0;
    if (*s == '+' || *s == '-') {
        if (*s == '-') sign = -1;
        ++s;
    }
    while (isdigit((unsigned char)*s)) {
        unsigned digit = (unsigned)(*s - '0');
        any = 1;
        if (magnitude > (UINT64_MAX - digit) / 10) magnitude = UINT64_MAX;
        else magnitude = magnitude * 10 + digit;
        ++s;
    }
    if (!any) return 0;
    if (sign > 0) {
        *value = magnitude > (uint64_t)INT64_MAX ? INT64_MAX : (ppf_exp_t)magnitude;
    } else {
        *value = magnitude > (uint64_t)INT64_MAX + 1u ? INT64_MIN
                 : magnitude == (uint64_t)INT64_MAX + 1u ? INT64_MIN
                 : -(ppf_exp_t)magnitude;
    }
    *end = s;
    return 1;
}

/* The parser accepts the MPFR numeric grammar used in the manual: optional
   leading space and sign, base-0 binary/hex prefixes, a radix point, @
   exponents in every base, e/E for bases <= 10, and p/P binary exponents for
   bases 2 and 16. */
static int ppf_strtofr_parse(ppf_t *rop, const char *text, char **endptr,
                             int base, ppf_rnd_t rnd,
                             ppf_context_t *ctx) {
    const char *start = text, *s, *digits_start, *exp_mark, *exp_end;
    mpz_t significand, power;
    mpq_t q;
    size_t fractional = 0;
    int sign = 1, saw_digit = 0, saw_point = 0, d, actual_base = base;
    int binary_exp = 0, t = 0;
    ppf_exp_t parsed_exp = 0;
    long double estimated_shift;

    ctx = ppf_resolve_context(ctx);
    if (!rop || !text || !ctx || (base != 0 && (base < 2 || base > 62))) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        if (endptr) *endptr = (char *)text;
        return PPF_TERNARY_ERROR;
    }
    rnd = resolve_rounding(rnd, ctx);
    if (!ppf_rounding_valid(rnd)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        if (endptr) *endptr = (char *)text;
        return PPF_TERNARY_ERROR;
    }
    s = text;
    while (isspace((unsigned char)*s)) ++s;
    if (*s == '+' || *s == '-') {
        if (*s == '-') sign = -1;
        ++s;
    }

    if (ascii_case_prefix(s, "@inf@")) {
        ppf_set_inf(rop, sign); s += 5; if (endptr) *endptr = (char *)s; return 0;
    }
    if (ascii_case_prefix(s, "@nan@")) {
        ppf_set_nan(rop, sign); ppf_raise(ctx, PPF_FLAG_NAN);
        s += 5;
        if (*s == '(') { ++s; while (isalnum((unsigned char)*s) || *s == '_') ++s; if (*s == ')') ++s; }
        if (endptr) *endptr = (char *)s;
        return 0;
    }
    if ((base == 0 || base <= 16) && ascii_case_prefix(s, "infinity")) {
        ppf_set_inf(rop, sign); s += 8; if (endptr) *endptr = (char *)s; return 0;
    }
    if ((base == 0 || base <= 16) && ascii_case_prefix(s, "inf")) {
        ppf_set_inf(rop, sign); s += 3; if (endptr) *endptr = (char *)s; return 0;
    }
    if ((base == 0 || base <= 16) && ascii_case_prefix(s, "nan")) {
        ppf_set_nan(rop, sign); ppf_raise(ctx, PPF_FLAG_NAN);
        s += 3;
        if (*s == '(') { ++s; while (isalnum((unsigned char)*s) || *s == '_') ++s; if (*s == ')') ++s; }
        if (endptr) *endptr = (char *)s;
        return 0;
    }

    if (actual_base == 0) {
        if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B') &&
            (s[2] == '0' || s[2] == '1')) { actual_base = 2; s += 2; }
        else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') &&
                 digit_value((unsigned char)s[2], 16) >= 0) { actual_base = 16; s += 2; }
        else actual_base = 10;
    }

    digits_start = s;
    mpz_init_set_ui(significand, 0);
    while (*s) {
        if (*s == '.' && !saw_point) { saw_point = 1; ++s; continue; }
        d = digit_value((unsigned char)*s, actual_base);
        if (d < 0 || d >= actual_base) break;
        saw_digit = 1;
        mpz_mul_ui(significand, significand, (unsigned long)actual_base);
        mpz_add_ui(significand, significand, (unsigned long)d);
        if (saw_point) fractional++;
        ++s;
    }
    if (!saw_digit) {
        mpz_clear(significand);
        ppf_set_zero(rop, 1);
        if (endptr) *endptr = (char *)start;
        return 0;
    }

    exp_mark = s;
    if (*s == '@' || ((actual_base <= 10) && (*s == 'e' || *s == 'E')) ||
        ((actual_base == 2 || actual_base == 16) && (*s == 'p' || *s == 'P'))) {
        binary_exp = (*s == 'p' || *s == 'P');
        if (parse_decimal_exponent(s + 1, &exp_end, &parsed_exp)) s = exp_end;
        else { parsed_exp = 0; binary_exp = 0; s = exp_mark; }
    }

    if (endptr) *endptr = (char *)s;
    if (mpz_sgn(significand) == 0) {
        ppf_set_zero(rop, sign);
        mpz_clear(significand);
        return 0;
    }
    if (sign < 0) mpz_neg(significand, significand);

    /* Avoid constructing pathological powers when the requested value is
       already unambiguously outside the configured binary range. */
    estimated_shift = (long double)parsed_exp *
                      (binary_exp ? 1.0L : log2l((long double)actual_base)) -
                      (long double)fractional * log2l((long double)actual_base);
    if (fabsl((long double)parsed_exp) > PPF_TEXT_POWER_LIMIT) {
        long double bits = (long double)mpz_sizeinbase(significand, 2);
        long double guard = 16.0L + 16.0L * LDBL_EPSILON *
            (fabsl((long double)parsed_exp) *
             (binary_exp ? 1.0L : log2l((long double)actual_base)) +
             (long double)fractional * log2l((long double)actual_base) + bits);
        /* The integer significand contributes between bits-1 and bits to
           log2(|value|).  A wide guard leaves cancellation near the resource
           boundary undecided instead of manufacturing an overflow/underflow. */
        if (estimated_shift + bits - 1.0L > (long double)ctx->emax + guard) {
            t = set_overflow_local(rop, sign, rnd, ctx);
        } else if (estimated_shift + bits < (long double)ctx->emin - guard) {
            t = set_underflow_tiny_local(rop, sign, rnd, ctx);
        } else {
            ppf_raise(ctx, PPF_FLAG_ERANGE);
            t = PPF_TERNARY_ERROR;
        }
        mpz_clear(significand);
        return t;
    }

    mpq_init(q);
    mpz_set(mpq_numref(q), significand);
    mpz_set_ui(mpq_denref(q), 1);
    mpz_init(power);
    if (fractional) {
        mpz_ui_pow_ui(power, (unsigned long)actual_base, (unsigned long)fractional);
        mpz_mul(mpq_denref(q), mpq_denref(q), power);
    }
    if (parsed_exp > 0) {
        if (binary_exp) mpz_mul_2exp(mpq_numref(q), mpq_numref(q), (mp_bitcnt_t)parsed_exp);
        else {
            mpz_ui_pow_ui(power, (unsigned long)actual_base, (unsigned long)parsed_exp);
            mpz_mul(mpq_numref(q), mpq_numref(q), power);
        }
    } else if (parsed_exp < 0) {
        uint64_t mag = (uint64_t)(-(parsed_exp + 1)) + 1;
        if (binary_exp) mpz_mul_2exp(mpq_denref(q), mpq_denref(q), (mp_bitcnt_t)mag);
        else {
            mpz_ui_pow_ui(power, (unsigned long)actual_base, (unsigned long)mag);
            mpz_mul(mpq_denref(q), mpq_denref(q), power);
        }
    }
    mpq_canonicalize(q);
    t = ppf_set_q(rop, q, rnd, ctx);
    mpz_clear(power);
    mpq_clear(q);
    mpz_clear(significand);
    (void)digits_start;
    return t;
}

static int commit_parsed_value(ppf_t *rop, const ppf_t *parsed,
                               ppf_context_t *ctx) {
    int rc;
    if (!rop || !parsed || !ctx) return PPF_TERNARY_ERROR;
    rc = ppf_set(rop, parsed, PPF_RNDN, ctx);
    return rc == PPF_TERNARY_ERROR ? PPF_TERNARY_ERROR : 0;
}

int ppf_strtofr(ppf_t *rop, const char *text, char **endptr,
                int base, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t parsed;
    int parsed_ready = 0;
    int ternary, commit_rc;

    ctx = ppf_resolve_context(ctx);
    if (!rop || !text || !ctx || !ppf_validate(rop)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        if (endptr) *endptr = (char *)text;
        return PPF_TERNARY_ERROR;
    }
    if (ppf_init2_ctx(&parsed, rop->precision, ctx) != 0)
        return PPF_TERNARY_ERROR;
    parsed_ready = 1;
    ternary = ppf_strtofr_parse(&parsed, text, endptr, base, rnd, ctx);
    if (ternary == PPF_TERNARY_ERROR) goto done;
    commit_rc = commit_parsed_value(rop, &parsed, ctx);
    if (commit_rc == PPF_TERNARY_ERROR) ternary = PPF_TERNARY_ERROR;

done:
    if (parsed_ready) ppf_clear(&parsed);
    return ternary;
}

int ppf_set_str(ppf_t *rop, const char *text, int base,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t parsed;
    char *end = NULL;
    int parsed_ready = 0;
    int ternary, rc = -1;

    ctx = ppf_resolve_context(ctx);
    if (!rop || !text || !ctx || !ppf_validate(rop)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return -1;
    }
    if (ppf_init2_ctx(&parsed, rop->precision, ctx) != 0) return -1;
    parsed_ready = 1;
    ternary = ppf_strtofr_parse(&parsed, text, &end, base, rnd, ctx);
    if (ternary == PPF_TERNARY_ERROR) goto done;
    /* strtofr reports no conversion by leaving the end pointer at the
       original input.  Empty and whitespace-only strings are not numbers. */
    if (end == text) goto done;
    while (end && isspace((unsigned char)*end)) ++end;
    if (!end || *end != '\0') goto done;
    if (commit_parsed_value(rop, &parsed, ctx) == PPF_TERNARY_ERROR)
        goto done;
    rc = 0;

done:
    if (parsed_ready) ppf_clear(&parsed);
    return rc;
}

int ppf_init_set_str(ppf_t *rop, const char *text, int base,
                     ppf_rnd_t rnd, ppf_context_t *ctx) {
    if (init_common(rop, ctx) != 0) return -1;
    return ppf_set_str(rop, text, base, rnd, ctx);
}

/* ---------- integer and floating conversion ---------- */

static int increment_binary_integer_round(ppf_rnd_t rnd, int sign,
                                          mpz_srcptr quotient,
                                          mpz_srcptr magnitude,
                                          uint64_t shift) {
    if (rnd == PPF_RNDA) return 1;
    if (rnd == PPF_RNDU) return sign > 0;
    if (rnd == PPF_RNDD) return sign < 0;
    if (rnd == PPF_RNDZ || rnd == PPF_RNDF) return 0;
    if (rnd != PPF_RNDN || shift == 0 || shift > (uint64_t)ULONG_MAX)
        return 0;
    {
        mp_bitcnt_t half_bit = (mp_bitcnt_t)(shift - 1u);
        if (!mpz_tstbit(magnitude, half_bit)) return 0;
        if (half_bit != 0 && mpz_scan1(magnitude, 0) < half_bit) return 1;
        return mpz_tstbit(quotient, 0) != 0;
    }
}

static int round_object_to_z(mpz_ptr result, const ppf_t *op,
                             ppf_rnd_t rnd, ppf_context_t *ctx,
                             int raise_flags) {
    mpz_t magnitude, q, rem;
    ppf_exp_t exponent;
    uint64_t shift;
    int sign, inc, inexact, ternary;
    ctx = ppf_resolve_context(ctx);
    if (!result || !op || !ctx) return PPF_TERNARY_ERROR;
    rnd = resolve_rounding(rnd, ctx);
    if (ppf_nan_p(op) || ppf_inf_p(op)) {
        mpz_set_ui(result, 0);
        if (raise_flags) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (ppf_zero_p(op)) { mpz_set_ui(result, 0); return 0; }
    if (!ppf_regular_p(op) ||
        ppf_exp_sub_prec(op->exponent, op->precision, &exponent) != 0) {
        mpz_set_ui(result, 0);
        if (raise_flags) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }

    sign = op->sign;
    mpz_roinit_n(magnitude, op->limbs, (mp_size_t)op->used_limbs);
    mpz_inits(q, rem, NULL);
    if (exponent >= 0) {
        if ((uint64_t)exponent > ULONG_MAX) {
            if (raise_flags) ppf_raise(ctx, PPF_FLAG_ERANGE);
            ternary = PPF_TERNARY_ERROR;
            goto done;
        }
        mpz_set(result, magnitude);
        if (exponent != 0)
            mpz_mul_2exp(result, result, (mp_bitcnt_t)exponent);
        if (sign < 0) mpz_neg(result, result);
        ternary = 0;
        goto done;
    }

    shift = (uint64_t)(-(exponent + 1)) + 1u;
    if (shift > ULONG_MAX) {
        mpz_set_ui(q, 0);
        mpz_set(rem, magnitude);
    } else {
        mpz_fdiv_q_2exp(q, magnitude, (mp_bitcnt_t)shift);
        mpz_fdiv_r_2exp(rem, magnitude, (mp_bitcnt_t)shift);
    }
    inexact = mpz_sgn(rem) != 0;
    inc = inexact
        ? increment_binary_integer_round(rnd, sign, q, magnitude, shift)
        : 0;
    if (inc) mpz_add_ui(q, q, 1);
    mpz_set(result, q);
    if (sign < 0) mpz_neg(result, result);
    if (!inexact) ternary = 0;
    else {
        if (raise_flags) ppf_raise(ctx, PPF_FLAG_INEXACT);
        ternary = sign > 0 ? (inc ? 1 : -1) : (inc ? -1 : 1);
    }

done:
    mpz_clears(q, rem, NULL);
    return ternary;
}

int ppf_get_z(mpz_ptr value, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return round_object_to_z(value, op, rnd, ctx, 1);
}

void ppf_get_q(mpq_ptr value, const ppf_t *op, ppf_context_t *ctx) {
    mpz_t z;
    ppf_exp_t e;
    if (!value || !op) return;
    if (!ppf_regular_p(op) && !ppf_zero_p(op)) {
        mpq_set_ui(value, 0, 1);
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return;
    }
    if (ppf_zero_p(op)) { mpq_set_ui(value, 0, 1); return; }
    mpz_init(z);
    e = ppf_get_z_2exp(z, op);
    mpz_set(mpq_numref(value), z);
    mpz_set_ui(mpq_denref(value), 1);
    if (e >= 0) mpz_mul_2exp(mpq_numref(value), mpq_numref(value), (mp_bitcnt_t)e);
    else mpz_mul_2exp(mpq_denref(value), mpq_denref(value),
                      (mp_bitcnt_t)((uint64_t)(-(e + 1)) + 1));
    mpq_canonicalize(value);
    mpz_clear(z);
}

static int mpz_to_uintmax_checked(mpz_srcptr z, uintmax_t *out) {
    uintmax_t v = 0;
    size_t count = 0;
    if (mpz_sgn(z) < 0 || mpz_sizeinbase(z, 2) > sizeof(v) * CHAR_BIT) return 0;
    mpz_export(&v, &count, -1, sizeof(v), 0, 0, z);
    *out = v;
    return 1;
}

static int mpz_to_intmax_checked(mpz_srcptr z, intmax_t *out) {
    mpz_t mag, limit;
    uintmax_t v;
    int negative;
    mpz_inits(mag, limit, NULL);
    negative = mpz_sgn(z) < 0;
    mpz_abs(mag, z);
    if (!negative) {
        set_mpz_uintmax(limit, (uintmax_t)INTMAX_MAX);
        if (mpz_cmp(mag, limit) > 0 || !mpz_to_uintmax_checked(mag, &v)) {
            mpz_clears(mag, limit, NULL); return 0;
        }
        *out = (intmax_t)v;
    } else {
        set_mpz_uintmax(limit, (uintmax_t)INTMAX_MAX + 1u);
        if (mpz_cmp(mag, limit) > 0 || !mpz_to_uintmax_checked(mag, &v)) {
            mpz_clears(mag, limit, NULL); return 0;
        }
        *out = v == (uintmax_t)INTMAX_MAX + 1u ? INTMAX_MIN : -(intmax_t)v;
    }
    mpz_clears(mag, limit, NULL);
    return 1;
}

uintmax_t ppf_get_uj(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z, max;
    uintmax_t out = 0;
    int t;
    ctx = ppf_resolve_context(ctx);
    mpz_inits(z, max, NULL);
    t = round_object_to_z(z, op, rnd, ctx, 0);
    set_mpz_uintmax(max, UINTMAX_MAX);
    if (t == PPF_TERNARY_ERROR) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        out = 0;
    } else if (mpz_sgn(z) < 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE); out = 0;
    } else if (mpz_cmp(z, max) > 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE); out = UINTMAX_MAX;
    } else {
        (void)mpz_to_uintmax_checked(z, &out);
        if (t != 0) ppf_raise(ctx, PPF_FLAG_INEXACT);
    }
    mpz_clears(z, max, NULL);
    return out;
}

intmax_t ppf_get_sj(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z, lo, hi;
    intmax_t out = 0;
    int t;
    ctx = ppf_resolve_context(ctx);
    mpz_inits(z, lo, hi, NULL);
    t = round_object_to_z(z, op, rnd, ctx, 0);
    set_mpz_intmax(lo, INTMAX_MIN);
    set_mpz_intmax(hi, INTMAX_MAX);
    if (t == PPF_TERNARY_ERROR) {
        ppf_raise(ctx, PPF_FLAG_ERANGE); out = 0;
    } else if (mpz_cmp(z, lo) < 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE); out = INTMAX_MIN;
    } else if (mpz_cmp(z, hi) > 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE); out = INTMAX_MAX;
    } else {
        (void)mpz_to_intmax_checked(z, &out);
        if (t != 0) ppf_raise(ctx, PPF_FLAG_INEXACT);
    }
    mpz_clears(z, lo, hi, NULL);
    return out;
}

unsigned long ppf_get_ui(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    uintmax_t v = ppf_get_uj(op, rnd, ctx);
    if (v > ULONG_MAX) { ppf_raise(ctx, PPF_FLAG_ERANGE); return ULONG_MAX; }
    return (unsigned long)v;
}

long ppf_get_si(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    intmax_t v = ppf_get_sj(op, rnd, ctx);
    if (v < LONG_MIN) { ppf_raise(ctx, PPF_FLAG_ERANGE); return LONG_MIN; }
    if (v > LONG_MAX) { ppf_raise(ctx, PPF_FLAG_ERANGE); return LONG_MAX; }
    return (long)v;
}

static int fits_unsigned_bits(const ppf_t *op, ppf_rnd_t rnd, unsigned bits) {
    mpz_t z, max;
    int t, ok;
    ppf_context_t local;
    if (ppf_context_init(&local) != 0) return 0;
    if (rnd == PPF_RNDF) {
        int a = fits_unsigned_bits(op, PPF_RNDD, bits);
        int b = fits_unsigned_bits(op, PPF_RNDU, bits);
        ppf_context_clear(&local);
        return a && b;
    }
    mpz_inits(z, max, NULL);
    t = round_object_to_z(z, op, rnd, &local, 0);
    mpz_set_ui(max, 1); mpz_mul_2exp(max, max, bits); mpz_sub_ui(max, max, 1);
    ok = t != PPF_TERNARY_ERROR && mpz_sgn(z) >= 0 && mpz_cmp(z, max) <= 0;
    mpz_clears(z, max, NULL); ppf_context_clear(&local); return ok;
}

static int fits_signed_bits(const ppf_t *op, ppf_rnd_t rnd, unsigned bits) {
    mpz_t z, lo, hi;
    int t, ok;
    ppf_context_t local;
    if (ppf_context_init(&local) != 0) return 0;
    if (rnd == PPF_RNDF) {
        int a = fits_signed_bits(op, PPF_RNDD, bits);
        int b = fits_signed_bits(op, PPF_RNDU, bits);
        ppf_context_clear(&local);
        return a && b;
    }
    mpz_inits(z, lo, hi, NULL);
    t = round_object_to_z(z, op, rnd, &local, 0);
    mpz_set_ui(hi, 1); mpz_mul_2exp(hi, hi, bits - 1); mpz_sub_ui(hi, hi, 1);
    mpz_set_ui(lo, 1); mpz_mul_2exp(lo, lo, bits - 1); mpz_neg(lo, lo);
    ok = t != PPF_TERNARY_ERROR && mpz_cmp(z, lo) >= 0 && mpz_cmp(z, hi) <= 0;
    mpz_clears(z, lo, hi, NULL); ppf_context_clear(&local); return ok;
}

int ppf_fits_ulong_p(const ppf_t *op, ppf_rnd_t rnd) { return fits_unsigned_bits(op, rnd, sizeof(unsigned long)*CHAR_BIT); }
int ppf_fits_slong_p(const ppf_t *op, ppf_rnd_t rnd) { return fits_signed_bits(op, rnd, sizeof(long)*CHAR_BIT); }
int ppf_fits_uint_p(const ppf_t *op, ppf_rnd_t rnd) { return fits_unsigned_bits(op, rnd, sizeof(unsigned int)*CHAR_BIT); }
int ppf_fits_sint_p(const ppf_t *op, ppf_rnd_t rnd) { return fits_signed_bits(op, rnd, sizeof(int)*CHAR_BIT); }
int ppf_fits_ushort_p(const ppf_t *op, ppf_rnd_t rnd) { return fits_unsigned_bits(op, rnd, sizeof(unsigned short)*CHAR_BIT); }
int ppf_fits_sshort_p(const ppf_t *op, ppf_rnd_t rnd) { return fits_signed_bits(op, rnd, sizeof(short)*CHAR_BIT); }
int ppf_fits_uintmax_p(const ppf_t *op, ppf_rnd_t rnd) { return fits_unsigned_bits(op, rnd, sizeof(uintmax_t)*CHAR_BIT); }
int ppf_fits_intmax_p(const ppf_t *op, ppf_rnd_t rnd) { return fits_signed_bits(op, rnd, sizeof(intmax_t)*CHAR_BIT); }

static int target_increment(ppf_rnd_t rnd, int sign, mpz_srcptr q,
                            mpz_srcptr r, uint64_t shift) {
    if (mpz_sgn(r) == 0) return 0;
    /* r is the discarded part modulo 2^shift. Inspect its half and sticky
       bits directly: a tiny finite input must not allocate 2^shift merely
       to round to a fixed-width C floating type. */
    return increment_binary_integer_round(rnd, sign, q, r, shift);
}

static long double mpz_exact_to_ld(mpz_srcptr z) {
    long double value = 0.0L;
    size_t bits = mpz_sgn(z) == 0 ? 0 : mpz_sizeinbase(z, 2);
    size_t i;
    for (i = bits; i-- > 0;) value = value * 2.0L + (mpz_tstbit(z, i) ? 1.0L : 0.0L);
    return value;
}

static long double get_c_binary(const ppf_t *op, ppf_rnd_t rnd,
                                int precision, int emin, int emax,
                                long double max_value,
                                ppf_context_t *ctx) {
    mpz_t z, mag, q, r;
    ppf_exp_t e2, top, unit;
    size_t bits;
    int sign, inc = 0;
    long double out;
    ctx = ppf_resolve_context(ctx);
    rnd = resolve_rounding(rnd, ctx);
    if (ppf_nan_p(op)) return NAN;
    if (ppf_inf_p(op)) return op->sign < 0 ? -INFINITY : INFINITY;
    if (ppf_zero_p(op)) return copysignl(0.0L, op->sign < 0 ? -1.0L : 1.0L);
    mpz_inits(z, mag, q, r, NULL);
    e2 = ppf_get_z_2exp(z, op); sign = mpz_sgn(z) < 0 ? -1 : 1; mpz_abs(mag, z);
    bits = mpz_sizeinbase(mag, 2);
    if (ppf_exp_add(e2, (ppf_exp_t)bits, &top) != 0) top = e2 > 0 ? INT64_MAX : INT64_MIN;
    if (top > emax) {
        int inf = rnd == PPF_RNDN || rnd == PPF_RNDA ||
                  (rnd == PPF_RNDU && sign > 0) || (rnd == PPF_RNDD && sign < 0);
        out = inf ? INFINITY : max_value;
        if (sign < 0) out = -out;
        mpz_clears(z, mag, q, r, NULL); return out;
    }
    unit = top >= emin ? top - precision : (ppf_exp_t)emin - precision;
    if (e2 >= unit) {
        uint64_t sh = (uint64_t)(e2 - unit);
        mpz_mul_2exp(q, mag, (mp_bitcnt_t)sh);
    } else {
        uint64_t sh = (uint64_t)(unit - e2);
        if (sh > ULONG_MAX) { mpz_set_ui(q,0); mpz_set(r,mag); }
        else { mpz_fdiv_q_2exp(q, mag, (mp_bitcnt_t)sh); mpz_fdiv_r_2exp(r, mag, (mp_bitcnt_t)sh); }
        inc = target_increment(rnd,sign,q,r,sh);
        if (inc) mpz_add_ui(q,q,1);
    }
    if (mpz_sgn(q) == 0) out = 0.0L;
    else {
        if (mpz_sizeinbase(q,2) > (size_t)precision) { mpz_fdiv_q_2exp(q,q,1); unit++; }
        if (unit + (ppf_exp_t)mpz_sizeinbase(q,2) > emax) {
            int inf = rnd == PPF_RNDN || rnd == PPF_RNDA ||
                      (rnd == PPF_RNDU && sign > 0) || (rnd == PPF_RNDD && sign < 0);
            out = inf ? INFINITY : max_value;
        } else out = ldexpl(mpz_exact_to_ld(q), (int)unit);
    }
    if (sign < 0) out = -out;
    mpz_clears(z, mag, q, r, NULL);
    return out;
}

float ppf_get_flt(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return (float)get_c_binary(op,rnd,FLT_MANT_DIG,FLT_MIN_EXP,FLT_MAX_EXP,FLT_MAX,ctx);
}

double ppf_get_d(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return (double)get_c_binary(op,rnd,DBL_MANT_DIG,DBL_MIN_EXP,DBL_MAX_EXP,DBL_MAX,ctx);
}

long double ppf_get_ld(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return get_c_binary(op,rnd,LDBL_MANT_DIG,LDBL_MIN_EXP,LDBL_MAX_EXP,LDBL_MAX,ctx);
}

static long double get_mantissa_2exp(long *exp_out, const ppf_t *op,
                                     ppf_rnd_t rnd, int precision,
                                     ppf_context_t *ctx) {
    mpz_t z, mag, q, r;
    ppf_exp_t e2, exponent;
    size_t bits;
    int sign, inc = 0;
    long double value;
    if (!op || !exp_out) return 0.0L;
    if (!ppf_regular_p(op)) { if (ppf_zero_p(op)) *exp_out=0; return ppf_get_ld(op,rnd,ctx); }
    mpz_inits(z,mag,q,r,NULL); e2=ppf_get_z_2exp(z,op); sign=mpz_sgn(z)<0?-1:1; mpz_abs(mag,z);
    bits=mpz_sizeinbase(mag,2); exponent=e2+(ppf_exp_t)bits;
    if (bits <= (size_t)precision) mpz_mul_2exp(q,mag,(mp_bitcnt_t)(precision-bits));
    else { unsigned long sh=(unsigned long)(bits-precision); mpz_fdiv_q_2exp(q,mag,sh); mpz_fdiv_r_2exp(r,mag,sh); inc=target_increment(resolve_rounding(rnd,ppf_resolve_context(ctx)),sign,q,r,sh); if(inc)mpz_add_ui(q,q,1); }
    if (mpz_sizeinbase(q,2)>(size_t)precision){mpz_fdiv_q_2exp(q,q,1); exponent++;}
    value=ldexpl(mpz_exact_to_ld(q),-precision); if(sign<0)value=-value; *exp_out=(long)exponent;
    mpz_clears(z,mag,q,r,NULL); return value;
}

double ppf_get_d_2exp(long *exponent, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx) {
    return (double)get_mantissa_2exp(exponent,op,rnd,DBL_MANT_DIG,ctx);
}

long double ppf_get_ld_2exp(long *exponent, const ppf_t *op,
                            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return get_mantissa_2exp(exponent,op,rnd,LDBL_MANT_DIG,ctx);
}

int ppf_frexp(ppf_exp_t *exponent, ppf_t *mantissa, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    ppf_exp_t e2;
    int t;
    if (!exponent || !mantissa || !op) return PPF_TERNARY_ERROR;
    if (!ppf_regular_p(op)) { *exponent=0; return ppf_set(mantissa,op,rnd,ctx); }
    mpz_init(z); e2=ppf_get_z_2exp(z,op); *exponent=op->exponent;
    t=ppf_set_z_2exp(mantissa,z,e2-op->exponent,rnd,ctx); mpz_clear(z); return t;
}

int ppf_get_f(mpf_ptr value, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t rounded;
    mpz_t significand, aligned;
    ppf_prec_t target_precision;
    ppf_exp_t exact_exp, top_exp, base_exp, shift;
    size_t count = 0;
    mp_size_t sx;
    int sh, ternary, sign;

    ctx = ppf_resolve_context(ctx);
    if (!value || !op || !ctx) return PPF_TERNARY_ERROR;
    sx = value->_mp_prec;
    if (sx <= 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (ppf_zero_p(op)) {
        mpf_set_ui(value, 0);
        return 0;
    }
    if (ppf_nan_p(op)) {
        mpf_set_ui(value, 0);
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    if (ppf_inf_p(op)) {
        mp_size_t i;
        value->_mp_exp = (mp_exp_t)LONG_MAX;
        value->_mp_size = op->sign < 0 ? -sx : sx;
        for (i = 0; i < sx; ++i) value->_mp_d[i] = GMP_NUMB_MASK;
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return op->sign < 0 ? 1 : -1;
    }

    /* GNU MPF stores sx significant limbs, but the binary exponent alignment
       can make sh low bits of the least-significant limb unavailable.  MPFR's
       conversion therefore rounds to sx*GMP_NUMB_BITS-sh bits. */
    sh = (int)(op->exponent % GMP_NUMB_BITS);
    sh = sh <= 0 ? -sh : GMP_NUMB_BITS - sh;
    target_precision = (ppf_prec_t)sx * GMP_NUMB_BITS - (ppf_prec_t)sh;
    if (target_precision < PPF_PREC_MIN) target_precision = PPF_PREC_MIN;
    if (ppf_init2_ctx(&rounded, target_precision, ctx) != 0)
        return PPF_TERNARY_ERROR;
    ternary = ppf_set(&rounded, op, rnd, ctx);
    if (ternary == PPF_TERNARY_ERROR) {
        ppf_clear(&rounded);
        return ternary;
    }

    mpz_inits(significand, aligned, NULL);
    exact_exp = ppf_get_z_2exp(significand, &rounded);
    sign = mpz_sgn(significand) < 0 ? -1 : 1;
    mpz_abs(significand, significand);
    top_exp = rounded.exponent;
    /* ceil(top_exp / GMP_NUMB_BITS), valid for negative exponents too. */
    base_exp = top_exp / GMP_NUMB_BITS;
    if (top_exp > 0 && top_exp % GMP_NUMB_BITS) base_exp++;
    shift = exact_exp - (base_exp - (ppf_exp_t)sx) * GMP_NUMB_BITS;
    if (shift >= 0) {
        if ((uint64_t)shift > ULONG_MAX) goto conversion_error;
        mpz_mul_2exp(aligned, significand, (mp_bitcnt_t)shift);
    } else {
        uint64_t right = (uint64_t)(-(shift + 1)) + 1;
        if (right > ULONG_MAX) goto conversion_error;
        /* Any bits discarded here are alignment zeros, not value bits. */
        if (mpz_scan1(significand, 0) < (mp_bitcnt_t)right)
            goto conversion_error;
        mpz_fdiv_q_2exp(aligned, significand, (mp_bitcnt_t)right);
    }
    if ((mp_size_t)mpz_size(aligned) > sx) goto conversion_error;
    memset(value->_mp_d, 0, (size_t)(sx + 1) * sizeof(mp_limb_t));
    mpz_export(value->_mp_d, &count, -1, sizeof(mp_limb_t), 0,
               GMP_NAIL_BITS, aligned);
    value->_mp_exp = (mp_exp_t)base_exp;
    value->_mp_size = sign < 0 ? -sx : sx;
    mpz_clears(significand, aligned, NULL);
    ppf_clear(&rounded);
    return ternary;

conversion_error:
    mpz_clears(significand, aligned, NULL);
    ppf_clear(&rounded);
    mpf_set_ui(value, 0);
    ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

/* ---------- exact radix output ---------- */

size_t ppf_get_str_ndigits(int base, ppf_prec_t precision) {
    int b;
    long double effective;
    if (base < -62 || base > 62) return 0;
    b=base<0?-base:base;
    if(b<2||b>62||precision<PPF_PREC_MIN)return 0;
    effective=(long double)(precision-((b&(b-1))==0?1:0));
    if(effective<1)effective=1;
    if(effective/log2l((long double)b)>(long double)SIZE_MAX-2)return 0;
    return 1+(size_t)ceill(effective/log2l((long double)b));
}

static int compare_dyadic_base_power(mpz_srcptr magnitude, ppf_exp_t e2,
                                     unsigned base, long power) {
    mpz_t p,left,right;
    int cmp;
    if(labs(power)>PPF_TEXT_POWER_LIMIT)return 2;
    mpz_inits(p,left,right,NULL); mpz_ui_pow_ui(p,base,(unsigned long)labs(power));
    mpz_set(left,magnitude); mpz_set_ui(right,1);
    if(power>=0)mpz_set(right,p); else mpz_mul(left,left,p);
    if(e2>=0)mpz_mul_2exp(left,left,(mp_bitcnt_t)e2); else mpz_mul_2exp(right,right,(mp_bitcnt_t)((uint64_t)(-(e2+1))+1));
    cmp=mpz_cmp(left,right); mpz_clears(p,left,right,NULL); return (cmp>0)-(cmp<0);
}

static char radix_digit(unsigned digit,int base) {
    int b=base<0?-base:base;
    if(digit<10)return(char)('0'+digit);
    if(b<=36)return(char)((base<0?'A':'a')+(digit-10));
    if(digit<36)return(char)('A'+digit-10);
    return(char)('a'+digit-36);
}

static char *ppf_get_str_exact(char *buffer, ppf_exp_t *exponent, int base,
                               size_t digits, const ppf_t *op, ppf_rnd_t rnd,
                               ppf_context_t *ctx) {
    int b=base<0?-base:base, sign, cmp, inc;
    mpz_t z,mag,num,den,q,r,pow,limit;
    ppf_exp_t e2;
    long k;
    size_t i,len,total;
    char *out;
    ctx=ppf_resolve_context(ctx); rnd=resolve_rounding(rnd,ctx);
    if(!op||!exponent||b<2||b>62)return NULL;
    if(digits==0)digits=ppf_get_str_ndigits(b,op->precision);
    if(digits==0)return NULL;
    if(ppf_nan_p(op)){const char*s="@NaN@";total=strlen(s)+1;out=buffer?buffer:ppf_owned_string_alloc_internal(ctx,total);if(out)memcpy(out,s,total);ppf_raise(ctx,PPF_FLAG_NAN);return out;}
    if(ppf_inf_p(op)){const char*s=op->sign<0?"-@Inf@":"@Inf@";total=strlen(s)+1;out=buffer?buffer:ppf_owned_string_alloc_internal(ctx,total);if(out)memcpy(out,s,total);return out;}
    if(ppf_zero_p(op)){
        size_t sign_chars=op->sign<0?1u:0u;
        if (digits > SIZE_MAX - sign_chars - 1u) {
            ppf_raise(ctx, PPF_FLAG_ERANGE);
            return NULL;
        }
        *exponent=0;
        total=sign_chars+digits+1;
        out=buffer?buffer:ppf_owned_string_alloc_internal(ctx,total);
        if(out){size_t j=0;if(sign_chars)out[j++]='-';memset(out+j,'0',digits);out[j+digits]=0;}
        return out;
    }
    mpz_inits(z,mag,num,den,q,r,pow,limit,NULL);e2=ppf_get_z_2exp(z,op);sign=mpz_sgn(z)<0?-1:1;mpz_abs(mag,z);
    {
        long double approx=((long double)mpz_sizeinbase(mag,2)-1.0L+(long double)e2)/log2l((long double)b);
        if(approx>LONG_MAX||approx<LONG_MIN){ppf_raise(ctx,PPF_FLAG_ERANGE);goto fail;}
        k=(long)floorl(approx)+1;
    }
    while((cmp=compare_dyadic_base_power(mag,e2,(unsigned)b,k))>=0&&cmp!=2)k++;
    while((cmp=compare_dyadic_base_power(mag,e2,(unsigned)b,k-1))<0&&cmp!=2)k--;
    if(cmp==2||labs(k)>PPF_TEXT_POWER_LIMIT||digits>(size_t)PPF_TEXT_POWER_LIMIT){ppf_raise(ctx,PPF_FLAG_ERANGE);goto fail;}
    mpz_set(num,mag);mpz_set_ui(den,1);
    if(e2>=0)mpz_mul_2exp(num,num,(mp_bitcnt_t)e2);else mpz_mul_2exp(den,den,(mp_bitcnt_t)((uint64_t)(-(e2+1))+1));
    {
        long scale=(long)digits-k;
        mpz_ui_pow_ui(pow,(unsigned long)b,(unsigned long)labs(scale));
        if(scale>=0)mpz_mul(num,num,pow);else mpz_mul(den,den,pow);
    }
    mpz_fdiv_qr(q,r,num,den);inc=increment_for_fraction(rnd,sign,q,r,den);if(inc)mpz_add_ui(q,q,1);
    mpz_ui_pow_ui(limit,(unsigned long)b,(unsigned long)digits);
    if(mpz_cmp(q,limit)>=0){mpz_fdiv_q_ui(q,q,(unsigned long)b);k++;}
    *exponent=(ppf_exp_t)k;
    total=digits+(sign<0?1:0)+1;out=buffer?buffer:ppf_owned_string_alloc_internal(ctx,total);if(!out)goto fail;
    len=digits;out[total-1]=0;
    for(i=0;i<len;i++){unsigned long rem=mpz_tdiv_q_ui(q,q,(unsigned long)b);out[(sign<0?1:0)+len-1-i]=radix_digit((unsigned)rem,base);}
    if(sign<0)out[0]='-';
    if(mpz_sgn(r)!=0)ppf_raise(ctx,PPF_FLAG_INEXACT);
    mpz_clears(z,mag,num,den,q,r,pow,limit,NULL);return out;
fail:
    mpz_clears(z,mag,num,den,q,r,pow,limit,NULL);return NULL;
}



#ifndef PPF_TEXT_FAST_GUARD_BITS
#define PPF_TEXT_FAST_GUARD_BITS 192u
#endif
#ifndef PPF_TEXT_FAST_MIN_BITS
#define PPF_TEXT_FAST_MIN_BITS 256u
#endif
#ifndef PPF_TEXT_FAST_MAX_ATTEMPTS
#define PPF_TEXT_FAST_MAX_ATTEMPTS 8u
#endif

static unsigned trailing_zeros_limb(mp_limb_t x) {
#if ULONG_MAX == UINT64_MAX
    return x ? (unsigned)__builtin_ctzl((unsigned long)x) : PPF_LIMB_BITS;
#else
    unsigned n = 0;
    if (!x) return PPF_LIMB_BITS;
    while ((x & 1u) == 0) { x >>= 1; ++n; }
    return n;
#endif
}

static ppf_prec_t object_trailing_zero_bits(const ppf_t *x) {
    size_t i;
    if (!x || !ppf_regular_p(x)) return 0;
    for (i = 0; i < x->used_limbs; ++i) {
        if (x->limbs[i] != 0) {
            ppf_prec_t out = (ppf_prec_t)i * PPF_LIMB_BITS;
            unsigned local = trailing_zeros_limb(x->limbs[i]);
            if (out > UINT64_MAX - local) return UINT64_MAX;
            return out + local;
        }
    }
    return x->precision;
}

/* Test whether the digit string returned by get_str denotes the exact source.
   The usual large-value/short-output case is rejected from exponents and
   factor counts without constructing a full-size base power. */
static unsigned floor_log2_unsigned(unsigned value) {
    unsigned out = 0;
    while (value > 1u) { value >>= 1; ++out; }
    return out;
}

static unsigned ceil_log2_unsigned(unsigned value) {
    if (value <= 1u) return 0;
    return floor_log2_unsigned(value - 1u) + 1u;
}

/* Cheap rigorous rejection before the full exactness test.  Let the emitted
   value be Q*b^(exponent-digits).  Its odd part cannot exceed the digit-bound
   times the odd part of the radix exponent.  Integer log2 bounds are loose but
   one-sided, so returning 1 can never misclassify an exact output. */
static int text_candidate_provably_inexact(const ppf_t *op, int base,
                                           ppf_exp_t exponent,
                                           size_t digits) {
    const unsigned b = (unsigned)(base < 0 ? -base : base);
    unsigned odd = b;
    ppf_prec_t trailing, source_odd_bits;
#if defined(__SIZEOF_INT128__)
    __extension__ typedef __int128 i128;
    i128 t, upper, lower;
#endif
    if (!op || !ppf_regular_p(op) || b < 2u || b > 62u || digits == 0)
        return 0;
    while ((odd & 1u) == 0u) odd >>= 1;
    trailing = object_trailing_zero_bits(op);
    if (trailing > op->precision) return 0;
    source_odd_bits = op->precision - trailing;
#if defined(__SIZEOF_INT128__)
    t = (i128)exponent - (i128)digits;
    if (t >= 0) {
        upper = (i128)digits * ceil_log2_unsigned(b) +
                t * ceil_log2_unsigned(odd) + 1;
        lower = t * floor_log2_unsigned(odd) + 1;
        if ((i128)source_odd_bits > upper ||
            (i128)source_odd_bits < lower)
            return 1;
    } else {
        upper = (i128)digits * ceil_log2_unsigned(b) + 1;
        if ((i128)source_odd_bits > upper) return 1;
    }
#else
    (void)exponent;
#endif
    return 0;
}

static int text_candidate_is_exact(const ppf_t *op, int base,
                                   const char *text, ppf_exp_t exponent,
                                   size_t digits) {
    int b = base < 0 ? -base : base;
    const char *body = text;
    int text_sign = 1;
    unsigned twos = 0;
    unsigned odd;
    mpz_t q, qodd, power, xz;
    ppf_prec_t q_twos, x_twos;
    size_t q_bits, x_odd_bits;
#if defined(__SIZEOF_INT128__)
    __extension__ typedef __int128 i128;
    i128 t, candidate_exp, source_exp;
#else
    long double t, candidate_exp, source_exp;
#endif
    int exact = 0;

    if (!op || !ppf_regular_p(op) || !text || b < 2 || b > 62)
        return 0;
    if (*body == '-') { text_sign = -1; ++body; }
    if (text_sign != op->sign) return 0;

    mpz_inits(q, qodd, power, xz, NULL);
    if (mpz_set_str(q, body, b) != 0 || mpz_sgn(q) == 0) goto done;
    q_bits = mpz_sizeinbase(q, 2);
    q_twos = (ppf_prec_t)mpz_scan1(q, 0);
    mpz_fdiv_q_2exp(qodd, q, (mp_bitcnt_t)q_twos);

    odd = (unsigned)b;
    while ((odd & 1u) == 0u) { ++twos; odd >>= 1; }
#if defined(__SIZEOF_INT128__)
    t = (i128)exponent - (i128)digits;
#else
    t = (long double)exponent - (long double)digits;
#endif
    x_twos = object_trailing_zero_bits(op);
#if defined(__SIZEOF_INT128__)
    source_exp = (i128)op->exponent - (i128)op->precision + (i128)x_twos;
#else
    source_exp = (long double)op->exponent - (long double)op->precision +
                 (long double)x_twos;
#endif
    x_odd_bits = (size_t)(op->precision - x_twos);

#if defined(__SIZEOF_INT128__)
    if (t >= 0) {
        candidate_exp = (i128)q_twos + (i128)twos * t;
        if (candidate_exp != source_exp) goto done;
        if (odd != 1u) {
            long double estimated = (long double)mpz_sizeinbase(qodd, 2) +
                (long double)t * log2l((long double)odd);
            if (estimated < (long double)x_odd_bits - 2.0L ||
                estimated > (long double)x_odd_bits + 2.0L) goto done;
            if (t > (i128)ULONG_MAX) goto done;
            mpz_ui_pow_ui(power, odd, (unsigned long)t);
            mpz_mul(qodd, qodd, power);
        }
    } else {
        i128 u = -t;
        i128 remove_twos = (i128)twos * u;
        if (remove_twos > (i128)q_twos) goto done;
        candidate_exp = (i128)q_twos - remove_twos;
        if (candidate_exp != source_exp) goto done;
        if (odd != 1u) {
            /* odd^u >= 2^u, so this is impossible once u reaches the
               complete candidate bit length. */
            if (u >= (i128)q_bits || u > (i128)ULONG_MAX) goto done;
            mpz_ui_pow_ui(power, odd, (unsigned long)u);
            if (!mpz_divisible_p(qodd, power)) goto done;
            mpz_divexact(qodd, qodd, power);
        }
    }
#else
    if (t >= 0.0L) {
        candidate_exp = (long double)q_twos + (long double)twos * t;
        if (candidate_exp != source_exp || t > (long double)ULONG_MAX)
            goto done;
        if (odd != 1u) {
            long double estimated = (long double)mpz_sizeinbase(qodd, 2) +
                t * log2l((long double)odd);
            if (estimated < (long double)x_odd_bits - 2.0L ||
                estimated > (long double)x_odd_bits + 2.0L) goto done;
            mpz_ui_pow_ui(power, odd, (unsigned long)t);
            mpz_mul(qodd, qodd, power);
        }
    } else {
        long double u = -t;
        long double remove_twos = (long double)twos * u;
        if (remove_twos > (long double)q_twos ||
            u >= (long double)q_bits || u > (long double)ULONG_MAX)
            goto done;
        candidate_exp = (long double)q_twos - remove_twos;
        if (candidate_exp != source_exp) goto done;
        if (odd != 1u) {
            mpz_ui_pow_ui(power, odd, (unsigned long)u);
            if (!mpz_divisible_p(qodd, power)) goto done;
            mpz_divexact(qodd, qodd, power);
        }
    }
#endif
    if (mpz_sizeinbase(qodd, 2) != x_odd_bits) goto done;
    (void)ppf_get_z_2exp(xz, op);
    mpz_abs(xz, xz);
    if (x_twos) mpz_fdiv_q_2exp(xz, xz, (mp_bitcnt_t)x_twos);
    exact = mpz_cmp(xz, qodd) == 0;

done:
    mpz_clears(q, qodd, power, xz, NULL);
    return exact;
}


static int set_mpf_from_ppf_exact(mpf_ptr out, const ppf_t *op) {
    size_t source_limbs, target_limbs;
    unsigned shift;
#if defined(__SIZEOF_INT128__)
    __extension__ typedef __int128 i128;
    i128 limb_exponent, padding, count128, shift128;
#else
    ppf_exp_t limb_exponent;
    long double padding, count128, shift128;
#endif
    mp_limb_t carry = 0;

    if (!out || !op || !ppf_regular_p(op) || GMP_NAIL_BITS != 0)
        return -1;
    source_limbs = op->used_limbs;
    if (!source_limbs || !op->limbs) return -1;

    /* Choose the GMP base-B exponent L=ceil(E/W).  The integer mantissa D is
       M shifted left by s bits, where D*B^(L-n)=M*2^(E-p).  This is an exact
       representation and avoids the ppf->mpz->mpf conversion round trip. */
#if defined(__SIZEOF_INT128__)
    limb_exponent = (i128)op->exponent / (i128)PPF_LIMB_BITS;
    if (op->exponent > 0 && op->exponent % (ppf_exp_t)PPF_LIMB_BITS)
        ++limb_exponent;
    padding = limb_exponent * (i128)PPF_LIMB_BITS - (i128)op->exponent;
    count128 = ((i128)op->precision + padding +
                (i128)PPF_LIMB_BITS - 1) / (i128)PPF_LIMB_BITS;
    shift128 = count128 * (i128)PPF_LIMB_BITS -
               (i128)op->precision - padding;
    if (limb_exponent < (i128)LONG_MIN || limb_exponent > (i128)LONG_MAX ||
        count128 < 1 || count128 > (i128)SIZE_MAX ||
        shift128 < 0 || shift128 >= (i128)PPF_LIMB_BITS)
        return -1;
    target_limbs = (size_t)count128;
    shift = (unsigned)shift128;
#else
    limb_exponent = op->exponent / (ppf_exp_t)PPF_LIMB_BITS;
    if (op->exponent > 0 && op->exponent % (ppf_exp_t)PPF_LIMB_BITS)
        ++limb_exponent;
    padding = (long double)limb_exponent * PPF_LIMB_BITS - op->exponent;
    count128 = ceill(((long double)op->precision + padding) / PPF_LIMB_BITS);
    shift128 = count128 * PPF_LIMB_BITS - op->precision - padding;
    if (count128 < 1.0L || count128 > (long double)SIZE_MAX ||
        shift128 < 0.0L || shift128 >= PPF_LIMB_BITS)
        return -1;
    target_limbs = (size_t)count128;
    shift = (unsigned)shift128;
#endif
    if (target_limbs > (size_t)out->_mp_prec + 1u ||
        target_limbs < source_limbs || target_limbs > source_limbs + 1u)
        return -1;

    memset(out->_mp_d, 0,
           ((size_t)out->_mp_prec + 1u) * sizeof(*out->_mp_d));
    if (shift) {
        carry = mpn_lshift(out->_mp_d, op->limbs,
                           (mp_size_t)source_limbs, shift);
        if (carry) {
            if (source_limbs >= target_limbs) return -1;
            out->_mp_d[source_limbs] = carry;
        }
    } else {
        memcpy(out->_mp_d, op->limbs,
               source_limbs * sizeof(*out->_mp_d));
    }
    while (target_limbs > 1u && out->_mp_d[target_limbs - 1u] == 0)
        --target_limbs;
    out->_mp_exp = (mp_exp_t)limb_exponent;
    out->_mp_size = op->sign < 0 ? -(mp_size_t)target_limbs
                                 :  (mp_size_t)target_limbs;
    return 0;
}

static char *get_str_mpf_nearest(ppf_exp_t *exponent, int base,
                                 size_t digits, const ppf_t *op,
                                 mpf_ptr f, ppf_context_t *ctx) {
    int b = base < 0 ? -base : base;
    mp_exp_t eout;
    size_t sign_chars, body;
    char *out = NULL;

    if (!exponent || !op || !ppf_regular_p(op) || b < 2 || b > 62 ||
        digits == 0 || op->precision > (ppf_prec_t)ULONG_MAX -
                                      2u * PPF_LIMB_BITS)
        return NULL;
    if (!f || set_mpf_from_ppf_exact(f, op) != 0) goto done;
    if (digits > SIZE_MAX - 3u) goto done;
    out = ppf_owned_string_alloc_internal(ctx, digits + 3u);
    if (!out) goto done;
    if (!mpf_get_str(out, &eout, b, digits, f)) {
        ppf_owned_string_free_internal(out); out = NULL; goto done;
    }
    sign_chars = out[0] == '-' ? 1u : 0u;
    body = strlen(out + sign_chars);
    if (body > digits) { ppf_owned_string_free_internal(out); out = NULL; goto done; }
    if (body < digits) {
        memset(out + sign_chars + body, '0', digits - body);
        out[sign_chars + digits] = 0;
    }
    if (base < 0) {
        for (size_t i = sign_chars; i < sign_chars + digits; ++i)
            if (out[i] >= 'a' && out[i] <= 'z')
                out[i] = (char)(out[i] - 'a' + 'A');
    }
    if ((mp_exp_t)(ppf_exp_t)eout != eout) {
        ppf_owned_string_free_internal(out); out = NULL; goto done;
    }
    *exponent = (ppf_exp_t)eout;

done:
    return out;
}


#ifndef PPF_TEXT_ONEPASS_EXTRA_DIGITS
#define PPF_TEXT_ONEPASS_EXTRA_DIGITS 16u
#endif

static int radix_char_value(char ch, int base) {
    int b = base < 0 ? -base : base;
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (b <= 36) {
        if (ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    } else {
        if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
        if (ch >= 'a' && ch <= 'z') return ch - 'a' + 36;
    }
    return -1;
}

/* Round one extended nearest-radix conversion to the requested digit count.
   The caller provides a directed binary endpoint with at least 192 guard bits.
   With 16 extra digits and base <= 62, the complete binary enclosure is less
   than 2^-90 of one extended-radix ulp.  Requiring a doubled-tail distance
   greater than four also covers a conservative one-ulp uncertainty in the
   underlying mpf digit conversion.  Near a radix halfway boundary we return
   NULL and the caller uses the two-endpoint certificate. */
static char *round_extended_radix_nearest(const char *extended,
                                          ppf_exp_t *exponent,
                                          ppf_exp_t extended_exponent,
                                          int base, size_t digits,
                                          size_t extra,
                                          ppf_context_t *ctx) {
#if defined(__SIZEOF_INT128__)
    __extension__ typedef unsigned __int128 u128;
    int b = base < 0 ? -base : base;
    const char *body = extended;
    size_t sign_chars = 0, total_digits;
    u128 scale = 1, tail = 0, doubled, boundary, distance;
    int round_up;
    char *out;

    if (!extended || !exponent || b < 2 || b > 62 || !digits || !extra)
        return NULL;
    if (*body == '-') { sign_chars = 1; ++body; }
    total_digits = strlen(body);
    if (total_digits != digits + extra) return NULL;
    for (size_t i = 0; i < extra; ++i) {
        int v = radix_char_value(body[digits + i], base);
        if (v < 0 || v >= b || scale > (~(u128)0) / (u128)b)
            return NULL;
        scale *= (u128)b;
        tail = tail * (u128)b + (u128)v;
    }
    doubled = tail << 1;
    boundary = scale;
    distance = doubled >= boundary ? doubled - boundary : boundary - doubled;
    if (distance <= (u128)4) return NULL;
    round_up = doubled > boundary;

    out = ppf_owned_string_alloc_internal(
        ctx, sign_chars + digits + 1u);
    if (!out) return NULL;
    if (sign_chars) out[0] = '-';
    memcpy(out + sign_chars, body, digits);
    out[sign_chars + digits] = 0;
    *exponent = extended_exponent;

    if (round_up) {
        size_t i = digits;
        while (i != 0) {
            int v;
            --i;
            v = radix_char_value(out[sign_chars + i], base);
            if (v < 0 || v >= b) {
                ppf_owned_string_free_internal(out);
                return NULL;
            }
            if (v + 1 < b) {
                out[sign_chars + i] = radix_digit((unsigned)(v + 1), base);
                return out;
            }
            out[sign_chars + i] = radix_digit(0, base);
        }
        out[sign_chars] = radix_digit(1, base);
        if (*exponent == INT64_MAX) {
            ppf_owned_string_free_internal(out);
            return NULL;
        }
        ++*exponent;
    }
    return out;
#else
    (void)extended; (void)exponent; (void)extended_exponent;
    (void)base; (void)digits; (void)extra; (void)ctx;
    return NULL;
#endif
}

static int fast_text_work_precision(ppf_prec_t *out, size_t digits, int base,
                                    ppf_prec_t source_precision) {
    long double bits;
    ppf_prec_t needed;
    if (!out || base < 2 || base > 62 || digits == 0) return -1;
    bits = ceill((long double)digits * log2l((long double)base));
    if (bits > (long double)UINT64_MAX - PPF_TEXT_FAST_GUARD_BITS)
        return -1;
    needed = (ppf_prec_t)bits + PPF_TEXT_FAST_GUARD_BITS;
    if (needed < PPF_TEXT_FAST_MIN_BITS) needed = PPF_TEXT_FAST_MIN_BITS;
    if (needed > source_precision) needed = source_precision;
    *out = needed;
    return 0;
}

char *ppf_get_str(char *buffer, ppf_exp_t *exponent, int base,
                  size_t digits, const ppf_t *op, ppf_rnd_t rnd,
                  ppf_context_t *ctx) {
    int b;
    ppf_prec_t work, next;
    ppf_exp_t base_exponent;
    ppf_context_t lo_ctx, hi_ctx;
    ppf_t lo, hi;
    int lo_ready = 0, hi_ready = 0, contexts_ready = 0;
    char *slo = NULL, *shi = NULL, *sext = NULL;
    ppf_exp_t elo = 0, ehi = 0, eext = 0;
    ppf_flags_t original_flags;
    ppf_flags_t allocation_flags = 0;
    char *result = NULL;
    mpf_t radix_scratch;
    int radix_scratch_ready = 0;
    ppf_prec_t radix_scratch_precision = 0;

    ctx = ppf_resolve_context(ctx);
    if (base < -62 || base > 62) return NULL;
    b = base < 0 ? -base : base;
    if (!op || !exponent || !ctx || b < 2 || b > 62)
        return NULL;
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!ppf_rounding_valid(rnd)) return NULL;

    /* Special values, default round-trip output, and long output already near
       the source precision use the exact path directly. */
    if (!ppf_regular_p(op) || digits == 0 ||
        fast_text_work_precision(&work, digits, b, op->precision) != 0 ||
        work + 64u >= op->precision) {
        return ppf_get_str_exact(buffer, exponent, base, digits, op, rnd, ctx);
    }
    if (ppf_exp_sub_prec(op->exponent, op->precision, &base_exponent) != 0)
        return ppf_get_str_exact(buffer, exponent, base, digits, op, rnd, ctx);

    original_flags = ctx->flags;
    if (ppf_context_clone(&lo_ctx, ctx) != 0) goto fallback;
    if (ppf_context_clone(&hi_ctx, ctx) != 0) {
        ppf_context_clear(&lo_ctx);
        goto fallback;
    }
    contexts_ready = 1;
    lo_ctx.emin = hi_ctx.emin = PPF_EMIN_MIN;
    lo_ctx.emax = hi_ctx.emax = PPF_EMAX_MAX;
    lo_ctx.flags = hi_ctx.flags = 0;
    if (ppf_init2_ctx(&lo, work, &lo_ctx) != 0) goto fallback;
    lo_ready = 1;
    if (ppf_init2_ctx(&hi, work, &hi_ctx) != 0) goto fallback;
    hi_ready = 1;
    if (rnd == PPF_RNDN) {
        if (work > (ppf_prec_t)ULONG_MAX - 2u * PPF_LIMB_BITS)
            goto fallback;
        radix_scratch_precision = work + 2u * PPF_LIMB_BITS;
        mpf_init2(radix_scratch, (mp_bitcnt_t)radix_scratch_precision);
        radix_scratch_ready = 1;
    }

    for (unsigned attempt = 0; attempt < PPF_TEXT_FAST_MAX_ATTEMPTS; ++attempt) {
        if (lo.precision != work && ppf_set_prec(&lo, work) != 0) goto fallback;
        if (hi.precision != work && ppf_set_prec(&hi, work) != 0) goto fallback;
        lo_ctx.flags = hi_ctx.flags = 0;
        if (ppf_round_limbs_2exp(&lo, op->limbs, op->used_limbs, op->sign,
                                 base_exponent, 0, PPF_RNDD, &lo_ctx) ==
            PPF_TERNARY_ERROR ||
            ppf_round_limbs_2exp(&hi, op->limbs, op->used_limbs, op->sign,
                                 base_exponent, 0, PPF_RNDU, &hi_ctx) ==
            PPF_TERNARY_ERROR ||
            !ppf_regular_p(&lo) || !ppf_regular_p(&hi))
            goto fallback;

        if (rnd == PPF_RNDN) {
            ppf_prec_t needed;
            size_t extended_digits;
            if (work > (ppf_prec_t)ULONG_MAX - 2u * PPF_LIMB_BITS ||
                digits > SIZE_MAX - PPF_TEXT_ONEPASS_EXTRA_DIGITS)
                goto fallback;
            needed = work + 2u * PPF_LIMB_BITS;
            extended_digits = digits + PPF_TEXT_ONEPASS_EXTRA_DIGITS;
            if (!radix_scratch_ready) {
                mpf_init2(radix_scratch, (mp_bitcnt_t)needed);
                radix_scratch_ready = 1;
                radix_scratch_precision = needed;
            } else if (needed != radix_scratch_precision) {
                mpf_set_prec(radix_scratch, (mp_bitcnt_t)needed);
                radix_scratch_precision = needed;
            }
            sext = get_str_mpf_nearest(&eext, base, extended_digits, &lo,
                                       radix_scratch, ctx);
            if (sext) {
                slo = round_extended_radix_nearest(
                    sext, &elo, eext, base, digits,
                    PPF_TEXT_ONEPASS_EXTRA_DIGITS, ctx);
                ppf_owned_string_free_internal(sext); sext = NULL;
                if (slo) {
                    size_t bytes = strlen(slo) + 1u;
                    if (buffer) {
                        memcpy(buffer, slo, bytes);
                        result = buffer;
                        ppf_owned_string_free_internal(slo);
                    } else {
                        result = slo;
                    }
                    slo = NULL;
                    *exponent = elo;
                    allocation_flags |= ctx->flags & PPF_FLAG_ALLOC;
                    allocation_flags |= lo_ctx.flags & PPF_FLAG_ALLOC;
                    allocation_flags |= hi_ctx.flags & PPF_FLAG_ALLOC;
                    ctx->flags = original_flags | allocation_flags;
                    if (text_candidate_provably_inexact(op, base, elo, digits) ||
                        !text_candidate_is_exact(op, base, result, elo, digits))
                        ppf_raise(ctx, PPF_FLAG_INEXACT);
                    goto done;
                }
            }
            /* Near a radix halfway boundary, retain the older independent
               two-endpoint certificate. */
            slo = get_str_mpf_nearest(&elo, base, digits, &lo,
                                       radix_scratch, ctx);
            shi = get_str_mpf_nearest(&ehi, base, digits, &hi,
                                       radix_scratch, ctx);
        } else {
            slo = ppf_get_str_exact(NULL, &elo, base, digits, &lo, rnd, &lo_ctx);
            shi = ppf_get_str_exact(NULL, &ehi, base, digits, &hi, rnd, &hi_ctx);
        }
        if (slo && shi && elo == ehi && strcmp(slo, shi) == 0) {
            size_t bytes = strlen(slo) + 1u;
            if (buffer) {
                memcpy(buffer, slo, bytes);
                result = buffer;
                ppf_owned_string_free_internal(slo);
            } else {
                result = slo;
            }
            slo = NULL;
            ppf_owned_string_free_internal(shi);
            shi = NULL;
            *exponent = elo;
            allocation_flags |= ctx->flags & PPF_FLAG_ALLOC;
            allocation_flags |= lo_ctx.flags & PPF_FLAG_ALLOC;
            allocation_flags |= hi_ctx.flags & PPF_FLAG_ALLOC;
            ctx->flags = original_flags | allocation_flags;
            if (text_candidate_provably_inexact(op, base, elo, digits) ||
                !text_candidate_is_exact(op, base, result, elo, digits))
                ppf_raise(ctx, PPF_FLAG_INEXACT);
            goto done;
        }
        ppf_owned_string_free_internal(slo); ppf_owned_string_free_internal(shi); ppf_owned_string_free_internal(sext); slo = shi = sext = NULL;
        if (work >= op->precision) break;
        next = work > UINT64_MAX / 2u ? op->precision : work * 2u;
        if (next < work + 128u) next = work + 128u;
        if (next > op->precision) next = op->precision;
        work = next;
    }

fallback:
    ppf_owned_string_free_internal(slo);
    ppf_owned_string_free_internal(shi);
    ppf_owned_string_free_internal(sext);
    slo = shi = sext = NULL;
    allocation_flags |= ctx->flags & PPF_FLAG_ALLOC;
    if (contexts_ready) {
        allocation_flags |= lo_ctx.flags & PPF_FLAG_ALLOC;
        allocation_flags |= hi_ctx.flags & PPF_FLAG_ALLOC;
    }
    if (radix_scratch_ready) {
        mpf_clear(radix_scratch);
        radix_scratch_ready = 0;
    }
    if (lo_ready) ppf_clear(&lo);
    if (hi_ready) ppf_clear(&hi);
    if (contexts_ready) {
        ppf_context_clear(&lo_ctx);
        ppf_context_clear(&hi_ctx);
    }
    ctx->flags = original_flags | allocation_flags;
    return ppf_get_str_exact(buffer, exponent, base, digits, op, rnd, ctx);

done:
    if (radix_scratch_ready) {
        mpf_clear(radix_scratch);
        radix_scratch_ready = 0;
    }
    if (lo_ready) ppf_clear(&lo);
    if (hi_ready) ppf_clear(&hi);
    if (contexts_ready) {
        ppf_context_clear(&lo_ctx);
        ppf_context_clear(&hi_ctx);
    }
    return result;
}

void ppf_free_str(char *text){ppf_owned_string_free_internal(text);}
