/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Independent comparison, integer-rounding, fractional, neighboring-value,
 * min/max, and remainder operations.
 *
 * The production library uses only the independent ppf_t object model and
 * GMP integer/rational arithmetic.  MPFR is used solely by the separate test
 * binaries as a differential oracle.
 */

typedef enum {
    INT_FLOOR = 0,
    INT_CEIL = 1,
    INT_TRUNC = 2,
    INT_NEAREST_EVEN = 3,
    INT_NEAREST_AWAY = 4
} integer_round_kind_t;

static ppf_context_t *resolve_ctx_rnd(ppf_context_t *ctx, ppf_rnd_t *rnd) {
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd && *rnd == PPF_RND_DEFAULT) *rnd = ctx->default_rounding;
    return ctx;
}

static int utility_error(ppf_t *rop, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (rop) ppf_set_nan(rop, 1);
    if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

static int utility_nan(ppf_t *rop, int sign, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (rop) ppf_set_nan(rop, sign);
    if (ctx) ppf_raise(ctx, PPF_FLAG_NAN);
    return 0;
}

static int add_exp_checked(ppf_exp_t a, ppf_exp_t b, ppf_exp_t *out) {
    return ppf_exp_add(a, b, out);
}

static int compare_shifted_abs(mpz_srcptr a, ppf_exp_t shift,
                               mpz_srcptr b) {
    size_t ba, bb;
#if defined(__SIZEOF_INT128__)
    __extension__ typedef __int128 i128;
    i128 ta, tb;
#else
    long double ta, tb;
#endif
    mpz_t temp;
    int cmp;

    if (mpz_sgn(a) == 0 || mpz_sgn(b) == 0) {
        if (mpz_sgn(a) == 0 && mpz_sgn(b) == 0) return 0;
        return mpz_sgn(a) == 0 ? -1 : 1;
    }
    ba = mpz_sizeinbase(a, 2);
    bb = mpz_sizeinbase(b, 2);
#if defined(__SIZEOF_INT128__)
    ta = (i128)ba + (i128)shift;
    tb = (i128)bb;
#else
    ta = (long double)ba + (long double)shift;
    tb = (long double)bb;
#endif
    if (ta != tb) return ta > tb ? 1 : -1;

    mpz_init(temp);
    if (shift >= 0) {
        if ((uint64_t)shift > (uint64_t)ULONG_MAX) {
            mpz_clear(temp);
            return 1;
        }
        mpz_mul_2exp(temp, a, (mp_bitcnt_t)shift);
        cmp = mpz_cmp(temp, b);
    } else {
        uint64_t amount = (uint64_t)(-(shift + 1)) + 1u;
        if (amount > (uint64_t)ULONG_MAX) {
            mpz_clear(temp);
            return -1;
        }
        mpz_mul_2exp(temp, b, (mp_bitcnt_t)amount);
        cmp = mpz_cmp(a, temp);
    }
    mpz_clear(temp);
    return cmp;
}

static int compare_object_to_dyadic(const ppf_t *op, mpz_srcptr z,
                                    ppf_exp_t exponent, int abs_only,
                                    ppf_context_t *ctx) {
    mpz_t a, az, bz;
    ppf_exp_t ae;
    int sa, sb, cmp;

    if (!op || !z) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    if (ppf_nan_p(op)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    if (ppf_inf_p(op)) return abs_only ? 1 : op->sign;
    if (ppf_zero_p(op)) {
        sb = mpz_sgn(z);
        if (abs_only) return sb == 0 ? 0 : -1;
        return sb == 0 ? 0 : (sb > 0 ? -1 : 1);
    }

    mpz_inits(a, az, bz, NULL);
    ae = ppf_get_z_2exp(a, op);
    sa = mpz_sgn(a);
    sb = mpz_sgn(z);
    if (!abs_only && sa != sb) {
        cmp = sa > sb ? 1 : -1;
        goto done;
    }
    if (!abs_only && sa == 0) {
        cmp = 0;
        goto done;
    }
    mpz_abs(az, a);
    mpz_abs(bz, z);
    cmp = compare_shifted_abs(az, ae - exponent, bz);
    if (!abs_only && sa < 0) cmp = -cmp;

done:
    mpz_clears(a, az, bz, NULL);
    return cmp;
}

static int compare_object_to_rational(const ppf_t *op, mpq_srcptr q,
                                      ppf_context_t *ctx) {
    mpz_t z, left, right;
    ppf_exp_t e;
    int so, sq, cmp;

    if (!op || !q) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    if (ppf_nan_p(op)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    if (ppf_inf_p(op)) return op->sign;
    sq = mpq_sgn(q);
    if (ppf_zero_p(op)) return sq == 0 ? 0 : (sq > 0 ? -1 : 1);
    so = op->sign;
    if ((so > 0) != (sq > 0)) return so > 0 ? 1 : -1;
    if (sq == 0) return so;

    mpz_inits(z, left, right, NULL);
    e = ppf_get_z_2exp(z, op);
    mpz_abs(z, z);
    mpz_mul(left, z, mpq_denref(q));
    mpz_abs(right, mpq_numref(q));
    cmp = compare_shifted_abs(left, e, right);
    if (so < 0) cmp = -cmp;
    mpz_clears(z, left, right, NULL);
    return cmp;
}

int ppf_cmp_ui(const ppf_t *op, unsigned long value, ppf_context_t *ctx) {
    mpz_t z;
    int cmp;
    mpz_init_set_ui(z, value);
    cmp = compare_object_to_dyadic(op, z, 0, 0, ctx);
    mpz_clear(z);
    return cmp;
}

int ppf_cmp_si(const ppf_t *op, long value, ppf_context_t *ctx) {
    mpz_t z;
    int cmp;
    mpz_init_set_si(z, value);
    cmp = compare_object_to_dyadic(op, z, 0, 0, ctx);
    mpz_clear(z);
    return cmp;
}

int ppf_cmp_z(const ppf_t *op, mpz_srcptr value, ppf_context_t *ctx) {
    return compare_object_to_dyadic(op, value, 0, 0, ctx);
}

int ppf_cmp_q(const ppf_t *op, mpq_srcptr value, ppf_context_t *ctx) {
    return compare_object_to_rational(op, value, ctx);
}

int ppf_cmp_f(const ppf_t *op, mpf_srcptr value, ppf_context_t *ctx) {
    mpq_t q;
    int cmp;
    if (!value) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    mpq_init(q);
    mpq_set_f(q, value);
    cmp = compare_object_to_rational(op, q, ctx);
    mpq_clear(q);
    return cmp;
}

static int compare_to_c_float(const ppf_t *op, long double value,
                              int is_double, ppf_context_t *ctx) {
    ppf_context_t wide;
    ppf_t temp;
    ppf_prec_t precision = is_double ? (ppf_prec_t)DBL_MANT_DIG
                                     : (ppf_prec_t)LDBL_MANT_DIG;
    int cmp = 0;
    ctx = ppf_resolve_context(ctx);
    if (!ctx) return 0;
    if (ppf_context_clone(&wide, ctx) != 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    wide.emin = PPF_EMIN_MIN;
    wide.emax = PPF_EMAX_MAX;
    wide.flags = 0;
    if (ppf_init2_ctx(&temp, precision, &wide) != 0) {
        ppf_context_clear(&wide);
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    if (is_double) (void)ppf_set_d(&temp, (double)value, PPF_RNDN, &wide);
    else (void)ppf_set_ld(&temp, value, PPF_RNDN, &wide);
    cmp = ppf_cmp(op, &temp, ctx);
    ppf_clear(&temp);
    ppf_context_clear(&wide);
    return cmp;
}

int ppf_cmp_d(const ppf_t *op, double value, ppf_context_t *ctx) {
    return compare_to_c_float(op, (long double)value, 1, ctx);
}

int ppf_cmp_ld(const ppf_t *op, long double value, ppf_context_t *ctx) {
    return compare_to_c_float(op, value, 0, ctx);
}

int ppf_cmp_ui_2exp(const ppf_t *op, unsigned long value,
                    ppf_exp_t exponent, ppf_context_t *ctx) {
    mpz_t z;
    int cmp;
    mpz_init_set_ui(z, value);
    cmp = compare_object_to_dyadic(op, z, exponent, 0, ctx);
    mpz_clear(z);
    return cmp;
}

int ppf_cmp_si_2exp(const ppf_t *op, long value,
                    ppf_exp_t exponent, ppf_context_t *ctx) {
    mpz_t z;
    int cmp;
    mpz_init_set_si(z, value);
    cmp = compare_object_to_dyadic(op, z, exponent, 0, ctx);
    mpz_clear(z);
    return cmp;
}

int ppf_cmpabs(const ppf_t *a, const ppf_t *b, ppf_context_t *ctx) {
    if (!a || !b || ppf_nan_p(a) || ppf_nan_p(b)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    return ppf_compare_magnitudes(a, b);
}

int ppf_cmpabs_ui(const ppf_t *op, unsigned long value,
                  ppf_context_t *ctx) {
    mpz_t z;
    int cmp;
    mpz_init_set_ui(z, value);
    cmp = compare_object_to_dyadic(op, z, 0, 1, ctx);
    mpz_clear(z);
    return cmp;
}

int ppf_greater_p(const ppf_t *a, const ppf_t *b) {
    return a && b && !ppf_nan_p(a) && !ppf_nan_p(b) && ppf_cmp(a, b, NULL) > 0;
}
int ppf_greaterequal_p(const ppf_t *a, const ppf_t *b) {
    return a && b && !ppf_nan_p(a) && !ppf_nan_p(b) && ppf_cmp(a, b, NULL) >= 0;
}
int ppf_less_p(const ppf_t *a, const ppf_t *b) {
    return a && b && !ppf_nan_p(a) && !ppf_nan_p(b) && ppf_cmp(a, b, NULL) < 0;
}
int ppf_lessequal_p(const ppf_t *a, const ppf_t *b) {
    return a && b && !ppf_nan_p(a) && !ppf_nan_p(b) && ppf_cmp(a, b, NULL) <= 0;
}
int ppf_lessgreater_p(const ppf_t *a, const ppf_t *b) {
    return a && b && !ppf_nan_p(a) && !ppf_nan_p(b) && ppf_cmp(a, b, NULL) != 0;
}
int ppf_unordered_p(const ppf_t *a, const ppf_t *b) {
    return !a || !b || ppf_nan_p(a) || ppf_nan_p(b);
}

static int total_category(const ppf_t *x) {
    int negative = ppf_signbit(x) != 0;
    if (ppf_nan_p(x)) return negative ? 0 : 7;
    if (ppf_inf_p(x)) return negative ? 1 : 6;
    if (ppf_zero_p(x)) return negative ? 3 : 4;
    return negative ? 2 : 5;
}

int ppf_total_order_p(const ppf_t *a, const ppf_t *b) {
    int ca, cb;
    if (!a || !b) return 0;
    ca = total_category(a);
    cb = total_category(b);
    if (ca != cb) return ca < cb;
    if (ca == 2 || ca == 5) return ppf_cmp(a, b, NULL) <= 0;
    return 1;
}

int ppf_setsign(ppf_t *rop, const ppf_t *op, int signbit,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    ppf_exp_t e;
    int target_sign, t;
    ctx = resolve_ctx_rnd(ctx, &rnd);
    if (!rop || !op || !ctx || !ppf_rounding_valid(rnd))
        return utility_error(rop, ctx);
    target_sign = signbit ? -1 : 1;
    if (ppf_nan_p(op)) {
        ppf_set_nan(rop, target_sign);
        ppf_raise(ctx, PPF_FLAG_NAN);
        return 0;
    }
    if (ppf_inf_p(op)) { ppf_set_inf(rop, target_sign); return 0; }
    if (ppf_zero_p(op)) { ppf_set_zero(rop, target_sign); return 0; }
    mpz_init(z);
    e = ppf_get_z_2exp(z, op);
    mpz_abs(z, z);
    if (target_sign < 0) mpz_neg(z, z);
    t = ppf_set_z_2exp(rop, z, e, rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_copysign(ppf_t *rop, const ppf_t *op1, const ppf_t *op2,
                 ppf_rnd_t rnd, ppf_context_t *ctx) {
    if (!op2) return utility_error(rop, ctx);
    return ppf_setsign(rop, op1, ppf_signbit(op2), rnd, ctx);
}

static integer_round_kind_t kind_for_rnd(ppf_rnd_t rnd, int sign) {
    switch (rnd) {
        case PPF_RNDU: return INT_CEIL;
        case PPF_RNDD: return INT_FLOOR;
        case PPF_RNDZ: return INT_TRUNC;
        case PPF_RNDA: return sign < 0 ? INT_FLOOR : INT_CEIL;
        case PPF_RNDF: return INT_TRUNC;
        case PPF_RNDN:
        default: return INT_NEAREST_EVEN;
    }
}

static int abs_remainder_vs_half(mpz_srcptr magnitude, uint64_t shift) {
    size_t bits;
    if (shift == 0) return -1;
    bits = mpz_sizeinbase(magnitude, 2);
    if (shift > (uint64_t)bits + 1u) return -1;
    if (shift - 1u > (uint64_t)ULONG_MAX) return -1;
    {
        int half_bit = mpz_tstbit(magnitude, (mp_bitcnt_t)(shift - 1u));
        if (!half_bit) return -1;
        if (shift - 1u == 0) return 0;
        {
            mp_bitcnt_t first = mpz_scan1(magnitude, 0);
            if (first == (mp_bitcnt_t)(shift - 1u) &&
                mpz_popcount(magnitude) == 1) return 0;
        }
        {
            mpz_t rem, half;
            int cmp;
            mpz_inits(rem, half, NULL);
            mpz_fdiv_r_2exp(rem, magnitude, (mp_bitcnt_t)shift);
            mpz_set_ui(half, 1);
            mpz_mul_2exp(half, half, (mp_bitcnt_t)(shift - 1u));
            cmp = mpz_cmp(rem, half);
            mpz_clears(rem, half, NULL);
            return cmp;
        }
    }
}

static int round_dyadic_to_integer(mpz_ptr out, mpz_srcptr z,
                                   ppf_exp_t exponent,
                                   integer_round_kind_t kind,
                                   int *relation, int *was_integer) {
    mpz_t mag, q, rem;
    int sign = mpz_sgn(z) < 0 ? -1 : 1;
    int increment = 0;
    uint64_t shift;

    if (relation) *relation = 0;
    if (was_integer) *was_integer = 1;
    if (mpz_sgn(z) == 0) { mpz_set_ui(out, 0); return 0; }
    mpz_inits(mag, q, rem, NULL);
    mpz_abs(mag, z);
    if (exponent >= 0) {
        if ((uint64_t)exponent > (uint64_t)ULONG_MAX) {
            mpz_clears(mag, q, rem, NULL);
            return -1;
        }
        mpz_mul_2exp(out, mag, (mp_bitcnt_t)exponent);
        if (sign < 0) mpz_neg(out, out);
        mpz_clears(mag, q, rem, NULL);
        return 0;
    }

    shift = (uint64_t)(-(exponent + 1)) + 1u;
    if (was_integer) {
        if (shift > (uint64_t)ULONG_MAX)
            *was_integer = 0;
        else
            *was_integer = mpz_divisible_2exp_p(mag, (mp_bitcnt_t)shift) != 0;
    }
    if (shift > (uint64_t)ULONG_MAX) {
        mpz_set_ui(q, 0);
        mpz_set(rem, mag);
    } else {
        mpz_fdiv_q_2exp(q, mag, (mp_bitcnt_t)shift);
        mpz_fdiv_r_2exp(rem, mag, (mp_bitcnt_t)shift);
    }
    if (mpz_sgn(rem) != 0) {
        switch (kind) {
            case INT_CEIL: increment = sign > 0; break;
            case INT_FLOOR: increment = sign < 0; break;
            case INT_TRUNC: increment = 0; break;
            case INT_NEAREST_AWAY:
            case INT_NEAREST_EVEN: {
                int cmp = abs_remainder_vs_half(mag, shift);
                if (cmp > 0) increment = 1;
                else if (cmp == 0)
                    increment = kind == INT_NEAREST_AWAY || mpz_odd_p(q);
                break;
            }
        }
        if (increment) mpz_add_ui(q, q, 1);
        if (relation) {
            if (sign > 0) *relation = increment ? 1 : -1;
            else *relation = increment ? -1 : 1;
        }
    }
    mpz_set(out, q);
    if (sign < 0) mpz_neg(out, out);
    mpz_clears(mag, q, rem, NULL);
    return 0;
}


static int limb_any_below(const mp_limb_t *limbs, size_t n,
                          uint64_t shift) {
    size_t full, i;
    unsigned rem;
    mp_limb_t mask;
    if (!limbs || n == 0 || shift == 0) return 0;
    if (shift >= (uint64_t)n * PPF_LIMB_BITS) {
        for (i = 0; i < n; ++i) if (limbs[i] != 0) return 1;
        return 0;
    }
    full = (size_t)(shift / PPF_LIMB_BITS);
    rem = (unsigned)(shift % PPF_LIMB_BITS);
    for (i = 0; i < full; ++i) if (limbs[i] != 0) return 1;
    if (!rem) return 0;
    mask = (((mp_limb_t)1 << (rem - 1)) - 1) * 2 + 1;
    return (limbs[full] & mask) != 0;
}

static int limb_test_bit(const mp_limb_t *limbs, size_t n, uint64_t bit) {
    size_t limb = (size_t)(bit / PPF_LIMB_BITS);
    unsigned off = (unsigned)(bit % PPF_LIMB_BITS);
    return limb < n ? (int)((limbs[limb] >> off) & 1u) : 0;
}

static int limb_round_increment(integer_round_kind_t kind, int sign,
                                const mp_limb_t *limbs, size_t n,
                                uint64_t shift) {
    int half, lower;
    if (!limb_any_below(limbs, n, shift)) return 0;
    switch (kind) {
        case INT_CEIL: return sign > 0;
        case INT_FLOOR: return sign < 0;
        case INT_TRUNC: return 0;
        case INT_NEAREST_AWAY:
        case INT_NEAREST_EVEN:
            if (shift == 0) return 0;
            half = limb_test_bit(limbs, n, shift - 1u);
            if (!half) return 0;
            lower = limb_any_below(limbs, n, shift - 1u);
            if (lower) return 1;
            return kind == INT_NEAREST_AWAY ||
                   limb_test_bit(limbs, n, shift);
    }
    return 0;
}

static void limb_copy_clear_below(mp_limb_t *dst, const mp_limb_t *src,
                                  size_t n, uint64_t shift) {
    size_t full, i;
    unsigned rem;
    if (dst != src) memmove(dst, src, n * sizeof(*dst));
    if (shift == 0) return;
    if (shift >= (uint64_t)n * PPF_LIMB_BITS) {
        memset(dst, 0, n * sizeof(*dst));
        return;
    }
    full = (size_t)(shift / PPF_LIMB_BITS);
    rem = (unsigned)(shift % PPF_LIMB_BITS);
    for (i = 0; i < full; ++i) dst[i] = 0;
    if (rem) {
        mp_limb_t low_mask = (((mp_limb_t)1 << (rem - 1)) - 1) * 2 + 1;
        dst[full] &= ~low_mask;
    }
}

static void limb_set_min_significand(ppf_t *x) {
    size_t n = ppf_limbs_for_precision(x->precision);
    memset(x->limbs, 0, n * sizeof(*x->limbs));
    x->limbs[(size_t)((x->precision - 1u) / PPF_LIMB_BITS)] =
        (mp_limb_t)1 << (unsigned)((x->precision - 1u) % PPF_LIMB_BITS);
}

/* Same-precision integer rounding is a direct significand edit. */
static int rint_same_precision_fast(ppf_t *rop, const ppf_t *op,
                                    integer_round_kind_t kind,
                                    ppf_exp_t lattice,
                                    ppf_context_t *ctx,
                                    int *handled) {
    ppf_exp_t base;
    uint64_t shift, total;
    int increment, relation;
    size_t limb;
    unsigned off;
    mp_limb_t unit, carry;
#if defined(__SIZEOF_INT128__)
    __extension__ typedef __int128 i128;
    i128 need;
#endif
    *handled = 0;
    if (!rop || !op || !ctx || rop->precision != op->precision ||
        !ppf_regular_p(op)) return 0;
    if (ppf_exp_sub_prec(op->exponent, op->precision, &base) != 0) return 0;
#if defined(__SIZEOF_INT128__)
    need = (i128)lattice - (i128)base;
    if (need <= 0) {
        if (rop != op) {
            memmove(rop->limbs, op->limbs, op->used_limbs * sizeof(*op->limbs));
            rop->used_limbs = op->used_limbs; rop->exponent = op->exponent;
            rop->sign = op->sign; rop->kind = PPF_KIND_REGULAR;
        }
        *handled = 1; return 0;
    }
    shift = need > (i128)UINT64_MAX ? UINT64_MAX : (uint64_t)need;
#else
    if (base >= lattice) {
        if (rop != op) {
            memmove(rop->limbs, op->limbs, op->used_limbs * sizeof(*op->limbs));
            rop->used_limbs = op->used_limbs; rop->exponent = op->exponent;
            rop->sign = op->sign; rop->kind = PPF_KIND_REGULAR;
        }
        *handled = 1; return 0;
    }
    shift = (uint64_t)(lattice - base);
#endif
    total = (uint64_t)op->precision;
    if (!limb_any_below(op->limbs, op->used_limbs, shift)) {
        if (rop != op) {
            memmove(rop->limbs, op->limbs, op->used_limbs * sizeof(*op->limbs));
            rop->used_limbs = op->used_limbs; rop->exponent = op->exponent;
            rop->sign = op->sign; rop->kind = PPF_KIND_REGULAR;
        }
        *handled = 1; return 0;
    }
    increment = limb_round_increment(kind, op->sign, op->limbs,
                                     op->used_limbs, shift);
    if (shift >= total) {
        int t;
        if (!increment) {
            ppf_set_zero(rop, op->sign);
            t = op->sign > 0 ? -2 : 2;
        } else {
            t = ppf_set_si_2exp(rop, op->sign, lattice, PPF_RNDN, ctx);
            if (t == PPF_TERNARY_ERROR) return 0;
            t = op->sign > 0 ? 2 : -2;
        }
        ppf_raise(ctx, PPF_FLAG_INEXACT);
        *handled = 1; return t;
    }
    {
        int all_ones = 0;
        if (increment) {
            uint64_t bit;
            all_ones = 1;
            for (bit = shift; bit < total; ++bit) {
                if (!limb_test_bit(op->limbs, op->used_limbs, bit)) {
                    all_ones = 0; break;
                }
            }
            if (all_ones && op->exponent >= ctx->emax) return 0;
        }
        limb_copy_clear_below(rop->limbs, op->limbs, op->used_limbs, shift);
        rop->used_limbs = op->used_limbs; rop->exponent = op->exponent;
        rop->sign = op->sign; rop->kind = PPF_KIND_REGULAR;
        if (increment) {
            if (all_ones) {
                limb_set_min_significand(rop);
                ++rop->exponent;
            } else {
                limb = (size_t)(shift / PPF_LIMB_BITS);
                off = (unsigned)(shift % PPF_LIMB_BITS);
                unit = (mp_limb_t)1 << off;
                carry = mpn_add_1(rop->limbs + limb, rop->limbs + limb,
                                  (mp_size_t)(rop->used_limbs - limb), unit);
                if (carry) {
                    limb_set_min_significand(rop);
                    ++rop->exponent;
                }
            }
        }
    }
    ppf_zero_unused_bits(rop);
    relation = increment ? op->sign : -op->sign;
    ppf_raise(ctx, PPF_FLAG_INEXACT);
    *handled = 1;
    return relation > 0 ? 2 : -2;
}


static uint64_t raw_low_bit_length(const ppf_t *op, uint64_t limit) {
    size_t last;
    unsigned rem;
    mp_limb_t top;
    if (!op || limit == 0) return 0;
    if (limit > op->precision) limit = op->precision;
    last = (size_t)((limit - 1u) / PPF_LIMB_BITS);
    rem = (unsigned)(limit % PPF_LIMB_BITS);
    top = op->limbs[last];
    if (rem) top &= ((mp_limb_t)1 << rem) - 1u;
    while (top == 0 && last > 0) top = op->limbs[--last];
    if (top == 0) return 0;
#if ULONG_MAX == UINT64_MAX
    return (uint64_t)last * PPF_LIMB_BITS +
           (uint64_t)(PPF_LIMB_BITS -
           (unsigned)__builtin_clzl((unsigned long)top));
#else
    {
        uint64_t bits = 0;
        while (top) { ++bits; top >>= 1; }
        return (uint64_t)last * PPF_LIMB_BITS + bits;
    }
#endif
}

static void raw_copy_low_left_aligned(mp_ptr dst, size_t dst_n,
                                      mp_srcptr src, uint64_t residue_bits,
                                      ppf_prec_t precision) {
    uint64_t left = (uint64_t)precision - residue_bits;
    size_t word_shift = (size_t)(left / PPF_LIMB_BITS);
    unsigned bit_shift = (unsigned)(left % PPF_LIMB_BITS);
    size_t src_n = (size_t)((residue_bits + PPF_LIMB_BITS - 1u) /
                            PPF_LIMB_BITS);
    unsigned top_width = (unsigned)(residue_bits % PPF_LIMB_BITS);
    memset(dst, 0, dst_n * sizeof(*dst));
    for (size_t i = 0; i < src_n; ++i) {
        mp_limb_t v = src[i];
        size_t j = i + word_shift;
        if (i + 1 == src_n && top_width)
            v &= ((mp_limb_t)1 << top_width) - 1u;
        if (j < dst_n) dst[j] |= v << bit_shift;
        if (bit_shift && j + 1 < dst_n)
            dst[j + 1] |= v >> (PPF_LIMB_BITS - bit_shift);
    }
}

static int frac_same_precision_fast(ppf_t *rop, const ppf_t *op,
                                    ppf_context_t *ctx, int *handled) {
    ppf_exp_t base, result_exp;
    uint64_t shift, residue_bits;
    size_t n;
    *handled = 0;
    if (!rop || !op || !ctx || rop == op || !ppf_regular_p(op) ||
        rop->precision != op->precision) return 0;
    if (ppf_exp_sub_prec(op->exponent, op->precision, &base) != 0) return 0;
    if (base >= 0) {
        ppf_set_zero(rop, op->sign);
        *handled = 1;
        return 0;
    }
    shift = (uint64_t)(-(base + 1)) + 1u;
    if (shift >= op->precision) {
        *handled = 1;
        return ppf_set(rop, op, PPF_RNDN, ctx);
    }
    residue_bits = raw_low_bit_length(op, shift);
    if (residue_bits == 0) {
        ppf_set_zero(rop, op->sign);
        *handled = 1;
        return 0;
    }
    if (residue_bits > (uint64_t)INT64_MAX ||
        ppf_exp_add(base, (ppf_exp_t)residue_bits, &result_exp) != 0 ||
        result_exp < ctx->emin || result_exp > ctx->emax) return 0;
    n = ppf_limbs_for_precision(rop->precision);
    if (ppf_reserve(rop, rop->precision) != 0) return 0;
    raw_copy_low_left_aligned(rop->limbs, n, op->limbs,
                              residue_bits, rop->precision);
    if (ppf_commit_regular(rop, op->sign, result_exp, ctx) != 0) return 0;
    *handled = 1;
    return 0;
}

static int modf_same_precision_fast(ppf_t *iop, ppf_t *fop,
                                    const ppf_t *op,
                                    ppf_context_t *ctx, int *handled) {
    ppf_exp_t base, fraction_exp;
    uint64_t shift, residue_bits;
    size_t n;
    int frac_handled = 0;
    *handled = 0;
    if (!iop || !fop || !op || !ctx || iop == fop || iop == op || fop == op ||
        !ppf_regular_p(op) || iop->precision != op->precision ||
        fop->precision != op->precision) return 0;
    if (ppf_exp_sub_prec(op->exponent, op->precision, &base) != 0) return 0;
    if (base >= 0) {
        (void)ppf_set(iop, op, PPF_RNDN, ctx);
        ppf_set_zero(fop, op->sign);
        *handled = 1;
        return 0;
    }
    shift = (uint64_t)(-(base + 1)) + 1u;
    if (shift >= op->precision) {
        ppf_set_zero(iop, op->sign);
        (void)ppf_set(fop, op, PPF_RNDN, ctx);
        *handled = 1;
        return 0;
    }
    residue_bits = raw_low_bit_length(op, shift);
    if (residue_bits &&
        (residue_bits > (uint64_t)INT64_MAX ||
         ppf_exp_add(base, (ppf_exp_t)residue_bits, &fraction_exp) != 0 ||
         fraction_exp < ctx->emin || fraction_exp > ctx->emax)) return 0;
    n = op->used_limbs;
    if (ppf_reserve(iop, iop->precision) != 0) return 0;
    limb_copy_clear_below(iop->limbs, op->limbs, n, shift);
    if (ppf_commit_regular(iop, op->sign, op->exponent, ctx) != 0) return 0;
    (void)frac_same_precision_fast(fop, op, ctx, &frac_handled);
    if (!frac_handled) return 0;
    *handled = 1;
    return 0;
}

static int rint_no_double(ppf_t *rop, const ppf_t *op,
                          integer_round_kind_t kind, ppf_rnd_t range_rnd,
                          ppf_context_t *ctx) {
    mpz_t z, selected;
    ppf_exp_t e, lattice = 0;
    int source_was_integer, cmp, magnitude;
#if defined(__SIZEOF_INT128__)
    __extension__ typedef __int128 i128;
#endif

    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return utility_error(rop, ctx);
    if (ppf_nan_p(op)) return utility_nan(rop, op->sign, ctx);
    if (ppf_inf_p(op)) { ppf_set_inf(rop, op->sign); return 0; }
    if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
#if defined(__SIZEOF_INT128__)
    {
        i128 candidate = (i128)op->exponent - (i128)rop->precision;
        if (candidate > 0) {
            if (candidate > (i128)INT64_MAX) return utility_error(rop, ctx);
            lattice = (ppf_exp_t)candidate;
        }
    }
#else
    if (rop->precision <= (ppf_prec_t)INT64_MAX &&
        op->exponent > (ppf_exp_t)rop->precision)
        lattice = op->exponent - (ppf_exp_t)rop->precision;
#endif

    {
        int handled = 0;
        int fast = rint_same_precision_fast(rop, op, kind, lattice, ctx,
                                            &handled);
        if (handled) return fast;
    }

    source_was_integer = ppf_integer_p(op);
    mpz_inits(z, selected, NULL);
    e = ppf_get_z_2exp(z, op);
    if (add_exp_checked(e, -lattice, &e) != 0 ||
        round_dyadic_to_integer(selected, z, e, kind,
                                NULL, NULL) != 0) {
        mpz_clears(z, selected, NULL);
        return utility_error(rop, ctx);
    }
    if (mpz_sgn(selected) == 0) {
        ppf_set_zero(rop, op->sign);
    } else {
        (void)ppf_set_z_2exp(rop, selected, lattice, range_rnd, ctx);
    }
    cmp = ppf_cmp(rop, op, NULL);
    mpz_clears(z, selected, NULL);
    if (cmp == 0) return 0;
    ppf_raise(ctx, PPF_FLAG_INEXACT);
    magnitude = source_was_integer ? 1 : 2;
    return cmp > 0 ? magnitude : -magnitude;
}

static int rint_two_stage(ppf_t *rop, const ppf_t *op,
                          integer_round_kind_t first,
                          ppf_rnd_t second, ppf_context_t *ctx) {
    mpz_t z, integer;
    ppf_exp_t e;
    int relation = 0, was_integer = 1, t;
    ctx = resolve_ctx_rnd(ctx, &second);
    if (!rop || !op || !ctx || !ppf_rounding_valid(second))
        return utility_error(rop, ctx);
    if (ppf_nan_p(op)) return utility_nan(rop, op->sign, ctx);
    if (ppf_inf_p(op)) { ppf_set_inf(rop, op->sign); return 0; }
    if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }

    mpz_inits(z, integer, NULL);
    e = ppf_get_z_2exp(z, op);
    if (e >= 0) {
        mpz_clears(z, integer, NULL);
        return ppf_set(rop, op, second, ctx);
    }
    if (round_dyadic_to_integer(integer, z, e, first,
                                &relation, &was_integer) != 0) {
        mpz_clears(z, integer, NULL);
        return utility_error(rop, ctx);
    }
    if (mpz_sgn(integer) == 0) {
        ppf_set_zero(rop, op->sign);
        t = 0;
    } else {
        t = ppf_set_z(rop, integer, second, ctx);
    }
    mpz_clears(z, integer, NULL);
    return t;
}

int ppf_rint(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    ctx = resolve_ctx_rnd(ctx, &rnd);
    if (!ctx || !ppf_rounding_valid(rnd)) return utility_error(rop, ctx);
    return rint_no_double(rop, op, kind_for_rnd(rnd, op ? op->sign : 1),
                          rnd, ctx);
}
int ppf_ceil(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx) {
    return rint_no_double(rop, op, INT_CEIL, PPF_RNDU, ctx);
}
int ppf_floor(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx) {
    return rint_no_double(rop, op, INT_FLOOR, PPF_RNDD, ctx);
}
int ppf_round(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx) {
    return rint_no_double(rop, op, INT_NEAREST_AWAY, PPF_RNDN, ctx);
}
int ppf_roundeven(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx) {
    return rint_no_double(rop, op, INT_NEAREST_EVEN, PPF_RNDN, ctx);
}
int ppf_trunc(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx) {
    return rint_no_double(rop, op, INT_TRUNC, PPF_RNDZ, ctx);
}

int ppf_rint_ceil(ppf_t *rop, const ppf_t *op,
                  ppf_rnd_t rnd, ppf_context_t *ctx) {
    return rint_two_stage(rop, op, INT_CEIL, rnd, ctx);
}
int ppf_rint_floor(ppf_t *rop, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx) {
    return rint_two_stage(rop, op, INT_FLOOR, rnd, ctx);
}
int ppf_rint_round(ppf_t *rop, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx) {
    return rint_two_stage(rop, op, INT_NEAREST_AWAY, rnd, ctx);
}
int ppf_rint_roundeven(ppf_t *rop, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx) {
    return rint_two_stage(rop, op, INT_NEAREST_EVEN, rnd, ctx);
}
int ppf_rint_trunc(ppf_t *rop, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx) {
    return rint_two_stage(rop, op, INT_TRUNC, rnd, ctx);
}

int ppf_integer_p(const ppf_t *op) {
    mpz_t z;
    ppf_exp_t e;
    uint64_t shift;
    int answer;
    if (!op || !ppf_number_p(op) || ppf_inf_p(op)) return 0;
    if (ppf_zero_p(op)) return 1;
    mpz_init(z);
    e = ppf_get_z_2exp(z, op);
    if (e >= 0) answer = 1;
    else {
        shift = (uint64_t)(-(e + 1)) + 1u;
        answer = shift <= (uint64_t)ULONG_MAX &&
                 mpz_divisible_2exp_p(z, (mp_bitcnt_t)shift) != 0;
    }
    mpz_clear(z);
    return answer;
}

int ppf_frac(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z, mag, rem;
    ppf_exp_t e;
    uint64_t shift;
    int t;
    ctx = resolve_ctx_rnd(ctx, &rnd);
    if (!rop || !op || !ctx || !ppf_rounding_valid(rnd))
        return utility_error(rop, ctx);
    if (ppf_nan_p(op)) return utility_nan(rop, op->sign, ctx);
    if (ppf_inf_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
    if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
    {
        int handled = 0;
        int fast = frac_same_precision_fast(rop, op, ctx, &handled);
        if (handled) return fast;
    }

    mpz_inits(z, mag, rem, NULL);
    e = ppf_get_z_2exp(z, op);
    if (e >= 0) {
        ppf_set_zero(rop, op->sign);
        mpz_clears(z, mag, rem, NULL);
        return 0;
    }
    mpz_abs(mag, z);
    shift = (uint64_t)(-(e + 1)) + 1u;
    if (shift > (uint64_t)ULONG_MAX) mpz_set(rem, mag);
    else mpz_fdiv_r_2exp(rem, mag, (mp_bitcnt_t)shift);
    if (mpz_sgn(rem) == 0) {
        ppf_set_zero(rop, op->sign);
        mpz_clears(z, mag, rem, NULL);
        return 0;
    }
    if (op->sign < 0) mpz_neg(rem, rem);
    t = ppf_set_z_2exp(rop, rem, e, rnd, ctx);
    mpz_clears(z, mag, rem, NULL);
    return t;
}

static int combined_code(int t) { return t > 0 ? 1 : (t < 0 ? 2 : 0); }

int ppf_modf(ppf_t *iop, ppf_t *fop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z, integer, rem, mag;
    ppf_exp_t e;
    uint64_t shift;
    int ti = 0, tf = 0, relation = 0, was_integer = 1;
    ctx = resolve_ctx_rnd(ctx, &rnd);
    if (!iop || !fop || !op || !ctx || !ppf_rounding_valid(rnd))
        return utility_error(iop, ctx);
    if (iop == fop) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (ppf_nan_p(op)) {
        utility_nan(iop, op->sign, ctx);
        ppf_set_nan(fop, op->sign);
        return 0;
    }
    if (ppf_inf_p(op)) {
        ppf_set_inf(iop, op->sign);
        ppf_set_zero(fop, op->sign);
        return 0;
    }
    if (ppf_zero_p(op)) {
        ppf_set_zero(iop, op->sign);
        ppf_set_zero(fop, op->sign);
        return 0;
    }
    {
        int handled = 0;
        int fast = modf_same_precision_fast(iop, fop, op, ctx, &handled);
        if (handled) return fast;
    }

    mpz_inits(z, integer, rem, mag, NULL);
    e = ppf_get_z_2exp(z, op);
    if (e >= 0) {
        ti = ppf_set(iop, op, rnd, ctx);
        ppf_set_zero(fop, op->sign);
        tf = 0;
    } else {
        if (round_dyadic_to_integer(integer, z, e, INT_TRUNC,
                                    &relation, &was_integer) != 0) {
            mpz_clears(z, integer, rem, mag, NULL);
            return utility_error(iop, ctx);
        }
        mpz_abs(mag, z);
        shift = (uint64_t)(-(e + 1)) + 1u;
        if (shift > (uint64_t)ULONG_MAX) mpz_set(rem, mag);
        else mpz_fdiv_r_2exp(rem, mag, (mp_bitcnt_t)shift);
        if (op->sign < 0) mpz_neg(rem, rem);
        if (mpz_sgn(integer) == 0) { ppf_set_zero(iop, op->sign); ti = 0; }
        else ti = ppf_set_z(iop, integer, rnd, ctx);
        if (mpz_sgn(rem) == 0) { ppf_set_zero(fop, op->sign); tf = 0; }
        else tf = ppf_set_z_2exp(fop, rem, e, rnd, ctx);
    }
    mpz_clears(z, integer, rem, mag, NULL);
    return combined_code(ti) + 4 * combined_code(tf);
}

ppf_prec_t ppf_min_prec(const ppf_t *op) {
    mpz_t z;
    mp_bitcnt_t zeros;
    size_t bits;
    ppf_prec_t result;
    if (!op || !ppf_regular_p(op)) return 0;
    mpz_init(z);
    (void)ppf_get_z_2exp(z, op);
    mpz_abs(z, z);
    zeros = mpz_scan1(z, 0);
    mpz_fdiv_q_2exp(z, z, zeros);
    bits = mpz_sizeinbase(z, 2);
    result = bits > (size_t)UINT64_MAX ? 0 : (ppf_prec_t)bits;
    mpz_clear(z);
    return result;
}

static int is_min_significand(const ppf_t *x) {
    size_t i, top;
    unsigned bit;
    if (!x || !ppf_regular_p(x)) return 0;
    top = (size_t)((x->precision - 1) / PPF_LIMB_BITS);
    bit = (unsigned)((x->precision - 1) % PPF_LIMB_BITS);
    for (i = 0; i < x->used_limbs; ++i) {
        mp_limb_t expected = i == top ? ((mp_limb_t)1 << bit) : 0;
        if (x->limbs[i] != expected) return 0;
    }
    return 1;
}

static int is_max_significand(const ppf_t *x) {
    size_t i, top;
    unsigned width;
    mp_limb_t mask;
    if (!x || !ppf_regular_p(x)) return 0;
    top = (size_t)((x->precision - 1) / PPF_LIMB_BITS);
    width = (unsigned)(x->precision - (ppf_prec_t)top * PPF_LIMB_BITS);
    mask = width == PPF_LIMB_BITS ? ~(mp_limb_t)0
                                  : (((mp_limb_t)1 << width) - 1);
    for (i = 0; i < x->used_limbs; ++i) {
        mp_limb_t expected = i == top ? mask : ~(mp_limb_t)0;
        if (x->limbs[i] != expected) return 0;
    }
    return 1;
}

static void set_min_significand(ppf_t *x) {
    size_t top = (size_t)((x->precision - 1) / PPF_LIMB_BITS);
    unsigned bit = (unsigned)((x->precision - 1) % PPF_LIMB_BITS);
    memset(x->limbs, 0, x->used_limbs * sizeof(*x->limbs));
    x->limbs[top] = (mp_limb_t)1 << bit;
}

static void set_max_significand(ppf_t *x) {
    size_t top = (size_t)((x->precision - 1) / PPF_LIMB_BITS);
    unsigned width = (unsigned)(x->precision - (ppf_prec_t)top * PPF_LIMB_BITS);
    size_t i;
    for (i = 0; i < x->used_limbs; ++i) x->limbs[i] = ~(mp_limb_t)0;
    if (width != PPF_LIMB_BITS)
        x->limbs[top] = ((mp_limb_t)1 << width) - 1;
}

static void next_direction(ppf_t *x, int upward, ppf_context_t *ctx) {
    int grow;
    ctx = ppf_resolve_context(ctx);
    if (!x || !ctx) return;
    if (ppf_nan_p(x)) { ppf_raise(ctx, PPF_FLAG_NAN); return; }
    if (ppf_inf_p(x)) {
        if ((x->sign > 0 && upward) || (x->sign < 0 && !upward)) return;
        (void)ppf_set_max_finite(x, x->sign, ctx);
        return;
    }
    if (ppf_zero_p(x)) {
        (void)ppf_set_min_normal(x, upward ? 1 : -1, ctx);
        return;
    }
    grow = x->sign > 0 ? upward : !upward;
    if (grow) {
        if (is_max_significand(x)) {
            if (x->exponent >= ctx->emax) { ppf_set_inf(x, x->sign); return; }
            set_min_significand(x);
            ++x->exponent;
        } else {
            (void)mpn_add_1(x->limbs, x->limbs, (mp_size_t)x->used_limbs, 1);
        }
    } else {
        if (is_min_significand(x)) {
            if (x->exponent <= ctx->emin) { ppf_set_zero(x, x->sign); return; }
            set_max_significand(x);
            --x->exponent;
        } else {
            (void)mpn_sub_1(x->limbs, x->limbs, (mp_size_t)x->used_limbs, 1);
        }
    }
}

void ppf_nextabove(ppf_t *x, ppf_context_t *ctx) { next_direction(x, 1, ctx); }
void ppf_nextbelow(ppf_t *x, ppf_context_t *ctx) { next_direction(x, 0, ctx); }

void ppf_nexttoward(ppf_t *x, const ppf_t *y, ppf_context_t *ctx) {
    int cmp;
    ctx = ppf_resolve_context(ctx);
    if (!x || !y || !ctx) return;
    if (ppf_nan_p(x) || ppf_nan_p(y)) {
        ppf_set_nan(x, ppf_nan_p(x) ? x->sign : y->sign);
        ppf_raise(ctx, PPF_FLAG_NAN);
        return;
    }
    cmp = ppf_cmp(x, y, NULL);
    if (cmp < 0) next_direction(x, 1, ctx);
    else if (cmp > 0) next_direction(x, 0, ctx);
}

int ppf_min(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    const ppf_t *chosen;
    ctx = resolve_ctx_rnd(ctx, &rnd);
    if (!rop || !a || !b || !ctx || !ppf_rounding_valid(rnd))
        return utility_error(rop, ctx);
    if (ppf_nan_p(a) && ppf_nan_p(b)) return utility_nan(rop, a->sign, ctx);
    if (ppf_nan_p(a)) return ppf_set(rop, b, rnd, ctx);
    if (ppf_nan_p(b)) return ppf_set(rop, a, rnd, ctx);
    if (ppf_zero_p(a) && ppf_zero_p(b) && a->sign != b->sign) {
        ppf_set_zero(rop, -1);
        return 0;
    }
    chosen = ppf_cmp(a, b, NULL) <= 0 ? a : b;
    return ppf_set(rop, chosen, rnd, ctx);
}

int ppf_max(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    const ppf_t *chosen;
    ctx = resolve_ctx_rnd(ctx, &rnd);
    if (!rop || !a || !b || !ctx || !ppf_rounding_valid(rnd))
        return utility_error(rop, ctx);
    if (ppf_nan_p(a) && ppf_nan_p(b)) return utility_nan(rop, a->sign, ctx);
    if (ppf_nan_p(a)) return ppf_set(rop, b, rnd, ctx);
    if (ppf_nan_p(b)) return ppf_set(rop, a, rnd, ctx);
    if (ppf_zero_p(a) && ppf_zero_p(b) && a->sign != b->sign) {
        ppf_set_zero(rop, 1);
        return 0;
    }
    chosen = ppf_cmp(a, b, NULL) >= 0 ? a : b;
    return ppf_set(rop, chosen, rnd, ctx);
}

int ppf_dim(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    if (!a || !b || ppf_nan_p(a) || ppf_nan_p(b))
        return utility_nan(rop, 1, ctx);
    if (ppf_cmp(a, b, NULL) <= 0) { ppf_set_zero(rop, 1); return 0; }
    return ppf_sub(rop, a, b, rnd, ctx);
}


ppf_exp_t ppf_get_exp(const ppf_t *x, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (!x || !ctx || !ppf_regular_p(x)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    return x->exponent;
}

int ppf_set_exp(ppf_t *x, ppf_exp_t exponent, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (!x || !ctx || !ppf_regular_p(x)) return -1;
    if (exponent < ctx->emin || exponent > ctx->emax) return -1;
    x->exponent = exponent;
    return 0;
}

ppf_rnd_t ppf_get_default_rounding_mode(void) {
    ppf_context_t *ctx = ppf_default_context();
    return ctx ? ctx->default_rounding : PPF_RNDN;
}

int ppf_set_default_rounding_mode(ppf_rnd_t rnd) {
    return ppf_context_set_rounding(NULL, rnd);
}
