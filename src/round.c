/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

static int rounded_increment(ppf_rnd_t rnd, int sign,
                             mpz_srcptr quotient, mpz_srcptr remainder,
                             unsigned shift) {
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
            return mpz_odd_p(quotient) != 0;
        default:
            return 0;
    }
}

static int compare_abs_to_power_of_two(mpz_srcptr magnitude,
                                       ppf_exp_t exponent,
                                       ppf_exp_t power) {
    size_t bits;
    ppf_exp_t top;
    if (mpz_sgn(magnitude) == 0) return -1;
    bits = mpz_sizeinbase(magnitude, 2);
    if (bits - 1 > (size_t)INT64_MAX) return 1;
    if (ppf_exp_add(exponent, (ppf_exp_t)(bits - 1), &top) != 0)
        return exponent > 0 ? 1 : -1;
    if (top < power) return -1;
    if (top > power) return 1;
    return mpz_popcount(magnitude) == 1 ? 0 : 1;
}

static int set_overflow(ppf_t *rop, int sign, ppf_rnd_t rnd,
                        ppf_context_t *ctx) {
    int infinity;
    if (rnd == PPF_RNDN) infinity = 1;
    else if (rnd == PPF_RNDA) infinity = 1;
    else if (rnd == PPF_RNDU) infinity = sign > 0;
    else if (rnd == PPF_RNDD) infinity = sign < 0;
    else infinity = 0; /* toward zero and faithful use max finite */

    if (infinity) ppf_set_inf(rop, sign);
    else if (ppf_set_max_finite(rop, sign, ctx) != 0)
        return PPF_TERNARY_ERROR;
    ppf_raise(ctx, PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT);
    if (sign > 0) return infinity ? 1 : -1;
    return infinity ? -1 : 1;
}

static int set_underflow(ppf_t *rop, mpz_srcptr magnitude,
                         ppf_exp_t exponent, int sign, ppf_rnd_t rnd,
                         ppf_context_t *ctx) {
    int minimum = 0;
    int cmp;
    if (rnd == PPF_RNDA) minimum = 1;
    else if (rnd == PPF_RNDU) minimum = sign > 0;
    else if (rnd == PPF_RNDD) minimum = sign < 0;
    else if (rnd == PPF_RNDN) {
        cmp = compare_abs_to_power_of_two(magnitude, exponent,
                                          ctx->emin - 2);
        minimum = cmp > 0; /* halfway is rounded toward zero */
    }

    if (minimum) {
        if (ppf_set_min_normal(rop, sign, ctx) != 0)
            return PPF_TERNARY_ERROR;
    } else {
        ppf_set_zero(rop, sign);
    }
    ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
    if (sign > 0) return minimum ? 1 : -1;
    return minimum ? -1 : 1;
}

int ppf_set_z_2exp(ppf_t *rop, mpz_srcptr value,
                   ppf_exp_t exponent, ppf_rnd_t rnd,
                   ppf_context_t *ctx) {
    mpz_t magnitude, quotient, remainder;
    size_t bits;
    unsigned long shift;
    ppf_exp_t rounded_exp;
    int sign, increment, inexact, ternary;

    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !value || !ctx || !ppf_rounding_valid(rnd) ||
        !ppf_precision_valid(rop->precision)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (mpz_sgn(value) == 0) {
        ppf_set_zero(rop, 1);
        return 0;
    }

    sign = mpz_sgn(value) < 0 ? -1 : 1;
    bits = mpz_sizeinbase(value, 2);
    if ((ppf_prec_t)bits == rop->precision &&
        bits <= (size_t)INT64_MAX &&
        ppf_exp_add(exponent, (ppf_exp_t)bits, &rounded_exp) == 0 &&
        rounded_exp >= ctx->emin && rounded_exp <= ctx->emax) {
        size_t need = ppf_limbs_for_precision(rop->precision);
        mp_size_t limb_count = mpz_size(value);
        mp_srcptr limbs = mpz_limbs_read(value);
        if (need != 0 && limb_count == (mp_size_t)need && limbs &&
            ppf_reserve(rop, rop->precision) == 0) {
            memcpy(rop->limbs, limbs, need * sizeof(*rop->limbs));
            rop->used_limbs = need;
            rop->exponent = rounded_exp;
            rop->sign = sign;
            rop->kind = PPF_KIND_REGULAR;
            return 0;
        }
    }
    if (bits <= (size_t)INT64_MAX &&
        ppf_exp_add(exponent, (ppf_exp_t)bits, &rounded_exp) == 0 &&
        rounded_exp >= ctx->emin && rounded_exp <= ctx->emax &&
        ((ppf_prec_t)bits <= rop->precision || rounded_exp < ctx->emax)) {
        mp_size_t limb_count = mpz_size(value);
        mp_srcptr limbs = mpz_limbs_read(value);
        if (limb_count > 0 && limbs) {
            return ppf_round_limbs_2exp(rop, limbs, (size_t)limb_count,
                                        sign, exponent, 0, rnd, ctx);
        }
    }

    mpz_init(magnitude);
    mpz_init(quotient);
    mpz_init(remainder);
    mpz_abs(magnitude, value);
    bits = mpz_sizeinbase(magnitude, 2);
    if (bits > (size_t)INT64_MAX ||
        ppf_exp_add(exponent, (ppf_exp_t)bits, &rounded_exp) != 0) {
        ternary = exponent > 0 ? set_overflow(rop, sign, rnd, ctx)
                               : set_underflow(rop, magnitude, exponent,
                                               sign, rnd, ctx);
        goto done;
    }

    inexact = 0;
    increment = 0;
    if ((ppf_prec_t)bits <= rop->precision) {
        mpz_set(quotient, magnitude);
        mpz_mul_2exp(quotient, quotient,
                     (mp_bitcnt_t)(rop->precision - bits));
    } else {
        ppf_prec_t wide_shift = (ppf_prec_t)bits - rop->precision;
        if (wide_shift > ULONG_MAX) {
            ppf_raise(ctx, PPF_FLAG_ERANGE);
            ternary = PPF_TERNARY_ERROR;
            goto done;
        }
        shift = (unsigned long)wide_shift;
        mpz_fdiv_q_2exp(quotient, magnitude, shift);
        mpz_fdiv_r_2exp(remainder, magnitude, shift);
        inexact = mpz_sgn(remainder) != 0;
        increment = rounded_increment(rnd, sign, quotient, remainder, shift);
        if (increment) mpz_add_ui(quotient, quotient, 1);
        if ((ppf_prec_t)mpz_sizeinbase(quotient, 2) > rop->precision) {
            mpz_fdiv_q_2exp(quotient, quotient, 1);
            if (rounded_exp == INT64_MAX) {
                ternary = set_overflow(rop, sign, rnd, ctx);
                goto done;
            }
            rounded_exp++;
        }
    }

    if (rounded_exp > ctx->emax) {
        ternary = set_overflow(rop, sign, rnd, ctx);
        goto done;
    }
    if (rounded_exp < ctx->emin) {
        ternary = set_underflow(rop, magnitude, exponent, sign, rnd, ctx);
        goto done;
    }

    if (ppf_store_normalized_mpz(rop, quotient, rounded_exp, sign) != 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        ternary = PPF_TERNARY_ERROR;
        goto done;
    }
    if (!inexact) {
        ternary = 0;
    } else {
        ppf_raise(ctx, PPF_FLAG_INEXACT);
        if (sign > 0) ternary = increment ? 1 : -1;
        else ternary = increment ? -1 : 1;
    }

done:
    mpz_clears(magnitude, quotient, remainder, NULL);
    return ternary;
}

int ppf_set_z(ppf_t *rop, mpz_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_set_z_2exp(rop, value, 0, rnd, ctx);
}

int ppf_set_ui(ppf_t *rop, unsigned long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int t;
    mpz_init_set_ui(z, value);
    t = ppf_set_z(rop, z, rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_set_si(ppf_t *rop, long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int t;
    mpz_init_set_si(z, value);
    t = ppf_set_z(rop, z, rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_set(ppf_t *rop, const ppf_t *op,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    ppf_exp_t e;
    int t;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !op || !ctx || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (rop == op) return 0;
    if (ppf_nan_p(op)) {
        ppf_set_nan(rop, op->sign);
        ppf_raise(ctx, PPF_FLAG_NAN);
        return 0;
    }
    if (ppf_inf_p(op)) {
        ppf_set_inf(rop, op->sign);
        return 0;
    }
    if (ppf_zero_p(op)) {
        ppf_set_zero(rop, op->sign);
        return 0;
    }
    if (rop->precision == op->precision &&
        op->exponent >= ctx->emin && op->exponent <= ctx->emax) {
        size_t bytes = op->used_limbs * sizeof(mp_limb_t);
        if (ppf_reserve(rop, rop->precision) != 0) {
            ppf_raise(ctx, PPF_FLAG_ERANGE);
            return PPF_TERNARY_ERROR;
        }
        memcpy(rop->limbs, op->limbs, bytes);
        rop->used_limbs = op->used_limbs;
        rop->exponent = op->exponent;
        rop->sign = op->sign;
        rop->kind = PPF_KIND_REGULAR;
        return 0;
    }
    mpz_init(z);
    e = ppf_get_z_2exp(z, op);
    t = ppf_set_z_2exp(rop, z, e, rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_prec_round(ppf_t *x, ppf_prec_t precision,
                   ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t staged;
    size_t limbs;
    int t = 0;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!x || !ctx || !ppf_validate(x) ||
        !ppf_precision_valid(precision) || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (precision == x->precision) return 0;
    if (x->storage_flags & PPF_STORAGE_EXTERNAL) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }

    /* Round independently before changing either precision or ownership.
       Only the final reserve can alter x's allocation; the remaining commit
       is fixed-size copying and cannot fail. */
    if (ppf_init2_ctx(&staged, precision, ctx) != 0)
        return PPF_TERNARY_ERROR;
    if (ppf_regular_p(x)) t = ppf_set(&staged, x, rnd, ctx);
    else if (ppf_zero_p(x)) ppf_set_zero(&staged, x->sign);
    else if (ppf_inf_p(x)) ppf_set_inf(&staged, x->sign);
    else ppf_set_nan(&staged, x->sign);
    if (t == PPF_TERNARY_ERROR) {
        ppf_clear(&staged);
        return t;
    }
    if (ppf_reserve(x, precision) != 0) {
        ppf_clear(&staged);
        ppf_raise(ctx, PPF_FLAG_ALLOC);
        return PPF_TERNARY_ERROR;
    }
    limbs = ppf_limbs_for_precision(precision);
    memset(x->limbs, 0, x->allocated_limbs * sizeof(*x->limbs));
    memcpy(x->limbs, staged.limbs, limbs * sizeof(*x->limbs));
    x->precision = precision;
    x->used_limbs = staged.used_limbs;
    x->exponent = staged.exponent;
    x->sign = staged.sign;
    x->kind = staged.kind;
    ppf_clear(&staged);
    return t;
}

int ppf_neg(ppf_t *rop, const ppf_t *op,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    int t;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !op || !ctx || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (rop == op) {
        rop->sign = -rop->sign;
        if (ppf_nan_p(rop)) ppf_raise(ctx, PPF_FLAG_NAN);
        return 0;
    }
    if (rnd == PPF_RNDU) rnd = PPF_RNDD;
    else if (rnd == PPF_RNDD) rnd = PPF_RNDU;
    t = ppf_set(rop, op, rnd, ctx);
    if (t == PPF_TERNARY_ERROR) return t;
    rop->sign = -rop->sign;
    return -t;
}

int ppf_abs(ppf_t *rop, const ppf_t *op,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    int source_sign, t;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !op || !ctx || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    source_sign = op->sign;
    if (source_sign < 0) {
        if (rnd == PPF_RNDU) rnd = PPF_RNDD;
        else if (rnd == PPF_RNDD) rnd = PPF_RNDU;
    }
    t = ppf_set(rop, op, rnd, ctx);
    if (t == PPF_TERNARY_ERROR) return t;
    rop->sign = 1;
    return source_sign < 0 ? -t : t;
}

static int exact_compare(mpz_srcptr a, ppf_exp_t ea,
                         mpz_srcptr b, ppf_exp_t eb) {
    mpz_t ta, tb;
    ppf_exp_t common;
    int cmp;
    if (mpz_sgn(a) == 0 || mpz_sgn(b) == 0) return mpz_cmp(a, b);
    if (ea == eb) return mpz_cmp(a, b);
    common = ea < eb ? ea : eb;
    mpz_inits(ta, tb, NULL);
    mpz_set(ta, a);
    mpz_set(tb, b);
    if (ea > common) mpz_mul_2exp(ta, ta, (mp_bitcnt_t)(ea - common));
    if (eb > common) mpz_mul_2exp(tb, tb, (mp_bitcnt_t)(eb - common));
    cmp = mpz_cmp(ta, tb);
    mpz_clears(ta, tb, NULL);
    return cmp;
}

static mp_limb_t scaled_significand_limb(const ppf_t *x,
                                         ppf_prec_t target_precision,
                                         size_t output_limb) {
    ppf_prec_t shift = target_precision - x->precision;
    size_t word_shift = (size_t)(shift / PPF_LIMB_BITS);
    unsigned bit_shift = (unsigned)(shift % PPF_LIMB_BITS);
    size_t source_index;
    mp_limb_t value = 0;
    if (output_limb < word_shift) return 0;
    source_index = output_limb - word_shift;
    if (source_index < x->used_limbs)
        value = x->limbs[source_index] << bit_shift;
    if (bit_shift && source_index > 0 && source_index - 1 < x->used_limbs)
        value |= x->limbs[source_index - 1] >>
                 (PPF_LIMB_BITS - bit_shift);
    return value;
}

int ppf_compare_magnitudes(const ppf_t *a, const ppf_t *b) {
    ppf_prec_t target_precision;
    size_t limbs, i;
    if (!a || !b) return 0;
    if (ppf_inf_p(a) || ppf_inf_p(b))
        return ppf_inf_p(a) == ppf_inf_p(b) ? 0 : (ppf_inf_p(a) ? 1 : -1);
    if (ppf_zero_p(a) || ppf_zero_p(b))
        return ppf_zero_p(a) == ppf_zero_p(b) ? 0 : (ppf_zero_p(a) ? -1 : 1);
    if (!ppf_regular_p(a) || !ppf_regular_p(b)) return 0;
    if (a->exponent != b->exponent)
        return a->exponent < b->exponent ? -1 : 1;
    target_precision = a->precision > b->precision ? a->precision : b->precision;
    limbs = ppf_limbs_for_precision(target_precision);
    for (i = limbs; i-- > 0;) {
        mp_limb_t av = scaled_significand_limb(a, target_precision, i);
        mp_limb_t bv = scaled_significand_limb(b, target_precision, i);
        if (av != bv) return av < bv ? -1 : 1;
    }
    return 0;
}

int ppf_cmp(const ppf_t *a, const ppf_t *b, ppf_context_t *ctx) {
    int cmp;
    if (!a || !b) return 0;
    if (ppf_nan_p(a) || ppf_nan_p(b)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    if (ppf_inf_p(a) || ppf_inf_p(b)) {
        if (ppf_inf_p(a) && ppf_inf_p(b))
            return (a->sign > b->sign) - (a->sign < b->sign);
        return ppf_inf_p(a) ? a->sign : -b->sign;
    }
    if (ppf_zero_p(a) && ppf_zero_p(b)) return 0;
    if (ppf_zero_p(a)) return b->sign < 0 ? 1 : -1;
    if (ppf_zero_p(b)) return a->sign < 0 ? -1 : 1;
    if (a->sign != b->sign) return a->sign < b->sign ? -1 : 1;
    cmp = ppf_compare_magnitudes(a, b);
    return a->sign < 0 ? -cmp : cmp;
}

int ppf_same_value(const ppf_t *a, const ppf_t *b) {
    if (!a || !b || a->kind != b->kind || a->sign != b->sign) return 0;
    if (!ppf_regular_p(a)) return 1;
    if (a->precision != b->precision || a->exponent != b->exponent ||
        a->used_limbs != b->used_limbs) return 0;
    return memcmp(a->limbs, b->limbs,
                  a->used_limbs * sizeof(mp_limb_t)) == 0;
}

int ppf_equal_p(const ppf_t *a, const ppf_t *b) {
    if (!a || !b || ppf_nan_p(a) || ppf_nan_p(b)) return 0;
    return ppf_cmp(a, b, NULL) == 0;
}

int ppf_compare_exact_to_object(mpz_srcptr value, ppf_exp_t exponent,
                                const ppf_t *object) {
    mpz_t z;
    ppf_exp_t e;
    int cmp;
    if (ppf_inf_p(object)) return object->sign > 0 ? -1 : 1;
    if (ppf_zero_p(object)) return mpz_sgn(value);
    if (!ppf_regular_p(object)) return 0;
    mpz_init(z);
    e = ppf_get_z_2exp(z, object);
    cmp = exact_compare(value, exponent, z, e);
    mpz_clear(z);
    return cmp;
}

ppf_interval_status_t ppf_set_dyadic_interval(
    ppf_t *rop, mpz_srcptr lower, mpz_srcptr upper,
    ppf_exp_t exponent, ppf_rnd_t rnd, int *ternary,
    ppf_context_t *ctx) {
    ppf_t lo, hi;
    ppf_context_t lo_ctx, hi_ctx;
    int tlo, thi, cmp_lo, cmp_hi, selected_ternary = 0;
    int rc = PPF_INTERVAL_ERROR;
    int lo_initialized = 0, hi_initialized = 0;
    ppf_flags_t original;
    memset(&lo, 0, sizeof(lo));
    memset(&hi, 0, sizeof(hi));
    ctx = ppf_resolve_context(ctx);
    if (!ctx) return rc;
    if (!rop || !lower || !upper || !ppf_validate(rop) ||
        mpz_cmp(lower, upper) > 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return (ppf_interval_status_t)rc;
    }
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!ppf_rounding_valid(rnd)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return (ppf_interval_status_t)rc;
    }
    original = ctx->flags;
    if (ppf_context_clone(&lo_ctx, ctx) != 0) return rc;
    if (ppf_context_clone(&hi_ctx, ctx) != 0) {
        ppf_context_clear(&lo_ctx);
        return rc;
    }
    lo_ctx.flags = hi_ctx.flags = 0;
    if (ppf_init2_ctx(&lo, rop->precision, &lo_ctx) != 0) {
        ctx->flags = original | lo_ctx.flags | hi_ctx.flags;
        ppf_context_clear(&lo_ctx);
        ppf_context_clear(&hi_ctx);
        return rc;
    }
    lo_initialized = 1;
    if (ppf_init2_ctx(&hi, rop->precision, &hi_ctx) != 0) {
        ctx->flags = original | lo_ctx.flags | hi_ctx.flags;
        ppf_clear(&lo);
        ppf_context_clear(&lo_ctx);
        ppf_context_clear(&hi_ctx);
        return rc;
    }
    hi_initialized = 1;
    tlo = ppf_set_z_2exp(&lo, lower, exponent, rnd, &lo_ctx);
    thi = ppf_set_z_2exp(&hi, upper, exponent, rnd, &hi_ctx);
    if (tlo == PPF_TERNARY_ERROR || thi == PPF_TERNARY_ERROR) {
        ctx->flags = original | lo_ctx.flags | hi_ctx.flags;
        goto done;
    }
    if (!ppf_same_value(&lo, &hi)) {
        rc = PPF_INTERVAL_UNDECIDED;
        goto done;
    }
    cmp_lo = ppf_compare_exact_to_object(lower, exponent, &lo);
    cmp_hi = ppf_compare_exact_to_object(upper, exponent, &lo);
    if (cmp_hi < 0) {
        selected_ternary = 1;
    } else if (cmp_lo > 0) {
        selected_ternary = -1;
    } else if (mpz_cmp(lower, upper) == 0 && cmp_lo == 0) {
        selected_ternary = 0;
    } else {
        rc = PPF_INTERVAL_UNDECIDED;
        goto done;
    }
    /* Both endpoints and the initialized destination have the same precision.
       Copying the decided value therefore needs no allocation and preserves
       caller-managed significand storage and allocator provenance. */
    if (ppf_set(rop, &lo, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) {
        ctx->flags |= original | lo_ctx.flags | hi_ctx.flags;
        goto done;
    }
    if (ternary) *ternary = selected_ternary;
    ctx->flags = original | lo_ctx.flags | hi_ctx.flags;
    rc = PPF_INTERVAL_DECIDED;

done:
    if (lo_initialized) ppf_clear(&lo);
    if (hi_initialized) ppf_clear(&hi);
    ppf_context_clear(&lo_ctx);
    ppf_context_clear(&hi_ctx);
    return (ppf_interval_status_t)rc;
}
