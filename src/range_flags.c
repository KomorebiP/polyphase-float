/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <stdlib.h>

static int signum_int(int value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

static int compare_abs_object_to_pow2(const ppf_t *x, ppf_exp_t power) {
    ppf_exp_t equality_exponent;
    size_t i, top;
    unsigned bit;
    if (!x || !ppf_regular_p(x)) return 0;
    if (ppf_exp_add(power, 1, &equality_exponent) != 0)
        return power < 0 ? 1 : -1;
    if (x->exponent < equality_exponent) return -1;
    if (x->exponent > equality_exponent) return 1;
    top = (size_t)((x->precision - 1) / PPF_LIMB_BITS);
    bit = (unsigned)((x->precision - 1) % PPF_LIMB_BITS);
    if (x->limbs[top] != ((mp_limb_t)1 << bit)) return 1;
    for (i = 0; i < top; ++i) {
        if (x->limbs[i] != 0) return 1;
    }
    return 0;
}

static int overflow_to_infinity(int sign, ppf_rnd_t rnd) {
    switch (rnd) {
        case PPF_RNDN:
        case PPF_RNDA:
            return 1;
        case PPF_RNDU:
            return sign > 0;
        case PPF_RNDD:
            return sign < 0;
        case PPF_RNDZ:
        case PPF_RNDF:
        default:
            return 0;
    }
}

static int underflow_to_minimum(const ppf_t *x, int ternary,
                                ppf_rnd_t rnd, const ppf_context_t *ctx) {
    int cmp;
    switch (rnd) {
        case PPF_RNDA:
            return 1;
        case PPF_RNDU:
            return x->sign > 0;
        case PPF_RNDD:
            return x->sign < 0;
        case PPF_RNDN:
            cmp = compare_abs_object_to_pow2(x, ctx->emin - 2);
            if (cmp > 0) return 1;
            if (cmp < 0) return 0;
            /* MPFR's underflow tie is toward zero.  If the extended-range
               rounded value is exactly the midpoint, the incoming ternary
               tells which side contains the unknown exact result. */
            return x->sign * signum_int(ternary) < 0;
        case PPF_RNDZ:
        case PPF_RNDF:
        default:
            return 0;
    }
}

ppf_exp_t ppf_get_emin(void) {
    ppf_context_t *ctx = ppf_default_context();
    return ctx ? ctx->emin : PPF_EMIN_DEFAULT;
}

ppf_exp_t ppf_get_emax(void) {
    ppf_context_t *ctx = ppf_default_context();
    return ctx ? ctx->emax : PPF_EMAX_DEFAULT;
}

int ppf_set_emin(ppf_exp_t exponent) {
    ppf_context_t *ctx = ppf_default_context();
    if (!ctx || exponent < PPF_EMIN_MIN || exponent > ctx->emax)
        return -1;
    ctx->emin = exponent;
    return 0;
}

int ppf_set_emax(ppf_exp_t exponent) {
    ppf_context_t *ctx = ppf_default_context();
    if (!ctx || exponent > PPF_EMAX_MAX || exponent < ctx->emin)
        return -1;
    ctx->emax = exponent;
    return 0;
}

ppf_exp_t ppf_get_emin_min(void) { return PPF_EMIN_MIN; }
ppf_exp_t ppf_get_emin_max(void) { return PPF_EMAX_MAX; }
ppf_exp_t ppf_get_emax_min(void) { return PPF_EMIN_MIN; }
ppf_exp_t ppf_get_emax_max(void) { return PPF_EMAX_MAX; }

int ppf_check_range(ppf_t *x, int ternary,
                    ppf_rnd_t rnd, ppf_context_t *ctx) {
    int sign, to_inf, to_min;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!x || !ctx || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    ternary = signum_int(ternary);
    if (ppf_nan_p(x)) {
        ppf_raise(ctx, PPF_FLAG_NAN);
        return ternary;
    }
    if (ppf_inf_p(x)) {
        if (ternary != 0)
            ppf_raise(ctx, PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT);
        return ternary;
    }
    if (ppf_zero_p(x)) {
        if (ternary != 0)
            ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
        return ternary;
    }
    sign = x->sign;
    if (x->exponent > ctx->emax) {
        to_inf = overflow_to_infinity(sign, rnd);
        if (to_inf) ppf_set_inf(x, sign);
        else if (ppf_set_max_finite(x, sign, ctx) != 0) {
            ppf_raise(ctx, PPF_FLAG_ERANGE);
            return PPF_TERNARY_ERROR;
        }
        ppf_raise(ctx, PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT);
        return to_inf ? sign : -sign;
    }
    if (x->exponent < ctx->emin) {
        to_min = underflow_to_minimum(x, ternary, rnd, ctx);
        if (to_min) {
            if (ppf_set_min_normal(x, sign, ctx) != 0) {
                ppf_raise(ctx, PPF_FLAG_ERANGE);
                return PPF_TERNARY_ERROR;
            }
        } else {
            ppf_set_zero(x, sign);
        }
        ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
        return to_min ? sign : -sign;
    }
    if (ternary != 0) ppf_raise(ctx, PPF_FLAG_INEXACT);
    return ternary;
}

static int increment_for_integer_round(ppf_rnd_t rnd, int sign,
                                       mpz_srcptr quotient,
                                       mpz_srcptr remainder,
                                       mp_bitcnt_t shift,
                                       int incoming_ternary) {
    int cmp;
    mpz_t half;
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
            mpz_init_set_ui(half, 1);
            mpz_mul_2exp(half, half, shift - 1);
            cmp = mpz_cmp(remainder, half);
            mpz_clear(half);
            if (cmp > 0) return 1;
            if (cmp < 0) return 0;
            if (incoming_ternary != 0)
                return sign * signum_int(incoming_ternary) < 0;
            return mpz_odd_p(quotient) != 0;
        default:
            return 0;
    }
}

int ppf_subnormalize(ppf_t *x, int ternary,
                     ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z, magnitude, quotient, remainder, rounded;
    ppf_exp_t base, quantum;
    ppf_exp_t full_precision_boundary;
    int sign, increment, result_ternary, rc;
    uint64_t shift64;

    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!x || !ctx || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    ternary = signum_int(ternary);
    if (!ppf_regular_p(x)) return ppf_check_range(x, ternary, rnd, ctx);
    if (x->exponent < ctx->emin || x->exponent > ctx->emax)
        return ppf_check_range(x, ternary, rnd, ctx);
    if (x->precision > (ppf_prec_t)INT64_MAX ||
        ppf_exp_add(ctx->emin, (ppf_exp_t)x->precision - 1,
                    &full_precision_boundary) != 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (x->exponent >= full_precision_boundary) {
        if (ternary != 0) ppf_raise(ctx, PPF_FLAG_INEXACT);
        return ternary;
    }

    sign = x->sign;
    mpz_inits(z, magnitude, quotient, remainder, rounded, NULL);
    base = ppf_get_z_2exp(z, x);
    mpz_abs(magnitude, z);
    if (ctx->emin == INT64_MIN) {
        mpz_clears(z, magnitude, quotient, remainder, rounded, NULL);
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    quantum = ctx->emin - 1;
    if (quantum < base) {
        mpz_clears(z, magnitude, quotient, remainder, rounded, NULL);
        if (ternary != 0) ppf_raise(ctx, PPF_FLAG_INEXACT);
        return ternary;
    }
    shift64 = (uint64_t)(quantum - base);
    if (shift64 == 0 || shift64 > (uint64_t)ULONG_MAX) {
        mpz_clears(z, magnitude, quotient, remainder, rounded, NULL);
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    mpz_fdiv_q_2exp(quotient, magnitude, (mp_bitcnt_t)shift64);
    mpz_fdiv_r_2exp(remainder, magnitude, (mp_bitcnt_t)shift64);
    increment = increment_for_integer_round(rnd, sign, quotient, remainder,
                                            (mp_bitcnt_t)shift64, ternary);
    mpz_set(rounded, quotient);
    if (increment) mpz_add_ui(rounded, rounded, 1);

    if (mpz_sgn(remainder) == 0 && !increment) {
        result_ternary = ternary;
    } else if (sign > 0) {
        result_ternary = increment ? 1 : -1;
    } else {
        result_ternary = increment ? -1 : 1;
    }

    if (mpz_sgn(rounded) == 0) {
        ppf_set_zero(x, sign);
        rc = 0;
    } else {
        if (sign < 0) mpz_neg(rounded, rounded);
        rc = ppf_set_z_2exp(x, rounded, quantum, PPF_RNDN, ctx);
    }
    mpz_clears(z, magnitude, quotient, remainder, rounded, NULL);
    if (rc == PPF_TERNARY_ERROR) return rc;
    /* Like mpfr_subnormalize, entering the reduced-precision rounding stage
       raises underflow even if this particular value is exactly representable. */
    ppf_raise(ctx, PPF_FLAG_UNDERFLOW);
    if (result_ternary != 0) {
        ppf_raise(ctx, PPF_FLAG_INEXACT);
    }
    return result_ternary;
}

#define DEFINE_CLEAR_FLAG(name, flag) \
    void ppf_clear_##name(void) { ppf_context_flags_clear(NULL, flag); }
#define DEFINE_SET_FLAG(name, flag) \
    void ppf_set_##name(void) { ppf_context_flags_set(NULL, flag); }
#define DEFINE_TEST_FLAG(name, flag) \
    int ppf_##name##_p(void) { return (ppf_context_flags(NULL) & flag) != 0; }

DEFINE_CLEAR_FLAG(underflow, PPF_FLAG_UNDERFLOW)
DEFINE_CLEAR_FLAG(overflow, PPF_FLAG_OVERFLOW)
DEFINE_CLEAR_FLAG(divby0, PPF_FLAG_DIVBY0)
DEFINE_CLEAR_FLAG(nanflag, PPF_FLAG_NAN)
DEFINE_CLEAR_FLAG(inexflag, PPF_FLAG_INEXACT)
DEFINE_CLEAR_FLAG(erangeflag, PPF_FLAG_ERANGE)
DEFINE_CLEAR_FLAG(allocflag, PPF_FLAG_ALLOC)

DEFINE_SET_FLAG(underflow, PPF_FLAG_UNDERFLOW)
DEFINE_SET_FLAG(overflow, PPF_FLAG_OVERFLOW)
DEFINE_SET_FLAG(divby0, PPF_FLAG_DIVBY0)
DEFINE_SET_FLAG(nanflag, PPF_FLAG_NAN)
DEFINE_SET_FLAG(inexflag, PPF_FLAG_INEXACT)
DEFINE_SET_FLAG(erangeflag, PPF_FLAG_ERANGE)
DEFINE_SET_FLAG(allocflag, PPF_FLAG_ALLOC)

DEFINE_TEST_FLAG(underflow, PPF_FLAG_UNDERFLOW)
DEFINE_TEST_FLAG(overflow, PPF_FLAG_OVERFLOW)
DEFINE_TEST_FLAG(divby0, PPF_FLAG_DIVBY0)
DEFINE_TEST_FLAG(nanflag, PPF_FLAG_NAN)
DEFINE_TEST_FLAG(inexflag, PPF_FLAG_INEXACT)
DEFINE_TEST_FLAG(erangeflag, PPF_FLAG_ERANGE)
DEFINE_TEST_FLAG(allocflag, PPF_FLAG_ALLOC)

void ppf_clear_flags(void) { ppf_context_flags_clear(NULL, PPF_FLAGS_ALL); }
void ppf_flags_clear(ppf_flags_t mask) { ppf_context_flags_clear(NULL, mask); }
void ppf_flags_set(ppf_flags_t mask) { ppf_context_flags_set(NULL, mask); }
ppf_flags_t ppf_flags_test(ppf_flags_t mask) {
    return ppf_context_flags(NULL) & (mask & PPF_FLAGS_ALL);
}
ppf_flags_t ppf_flags_save(void) { return ppf_context_flags_save(NULL); }
void ppf_flags_restore(ppf_flags_t flags, ppf_flags_t mask) {
    ppf_context_flags_restore(NULL, flags, mask);
}
