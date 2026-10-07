/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#define PPF_QUOTIENT_BITS ((unsigned)(sizeof(long) * CHAR_BIT - 1u))
#define PPF_QUOTIENT_MASK (ULONG_MAX >> 1)
#define PPF_DIRECT_SHIFT_EXTRA 4096u

typedef struct {
    mpz_t remainder;
    mpz_t denominator;
    unsigned long quotient_low;
    ppf_exp_t common_exponent;
    int denominator_valid;
    int half_compare;
} ppf_divrem_info_t;

static int remainder_error(ppf_t *rop, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (rop) ppf_set_nan(rop, 1);
    if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

static int remainder_nan(ppf_t *rop, int sign, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (rop) ppf_set_nan(rop, sign);
    if (ctx) ppf_raise(ctx, PPF_FLAG_NAN);
    return 0;
}

static void info_init(ppf_divrem_info_t *info) {
    mpz_init(info->remainder);
    mpz_init(info->denominator);
    info->quotient_low = 0;
    info->common_exponent = 0;
    info->denominator_valid = 0;
    info->half_compare = -1;
}

static void info_clear(ppf_divrem_info_t *info) {
    mpz_clear(info->remainder);
    mpz_clear(info->denominator);
}

static unsigned long quotient_low_bits(mpz_srcptr quotient) {
    mpz_t low;
    unsigned long value;
    mpz_init(low);
    mpz_fdiv_r_2exp(low, quotient, PPF_QUOTIENT_BITS);
    value = mpz_get_ui(low) & PPF_QUOTIENT_MASK;
    mpz_clear(low);
    return value;
}

static int compare_twice_to_shifted(mpz_srcptr a, mpz_srcptr b,
                                    uint64_t shift) {
    size_t ba, bb;
#if defined(__SIZEOF_INT128__)
    __extension__ typedef __int128 ppf_i128;
    ppf_i128 ta, tb;
#else
    long double ta, tb;
#endif
    mpz_t left, right;
    int cmp;
    if (mpz_sgn(a) == 0) return -1;
    ba = mpz_sizeinbase(a, 2);
    bb = mpz_sizeinbase(b, 2);
#if defined(__SIZEOF_INT128__)
    ta = (ppf_i128)ba + 1;
    tb = (ppf_i128)bb + (ppf_i128)shift;
#else
    ta = (long double)ba + 1.0L;
    tb = (long double)bb + (long double)shift;
#endif
    if (ta != tb) return ta > tb ? 1 : -1;
    if (shift > (uint64_t)ULONG_MAX) return -1;
    mpz_inits(left, right, NULL);
    mpz_mul_2exp(left, a, 1);
    mpz_mul_2exp(right, b, (mp_bitcnt_t)shift);
    cmp = mpz_cmp(left, right);
    mpz_clears(left, right, NULL);
    return cmp;
}

static int materialize_shifted_denominator(ppf_divrem_info_t *info,
                                           mpz_srcptr denominator,
                                           uint64_t shift) {
    if (shift > (uint64_t)ULONG_MAX) return -1;
    mpz_mul_2exp(info->denominator, denominator, (mp_bitcnt_t)shift);
    info->denominator_valid = 1;
    return 0;
}

static void set_half_compare(ppf_divrem_info_t *info) {
    mpz_t twice;
    mpz_init(twice);
    mpz_mul_2exp(twice, info->remainder, 1);
    info->half_compare = mpz_cmp(twice, info->denominator);
    mpz_clear(twice);
}

static int positive_shift_direct(ppf_divrem_info_t *info,
                                 mpz_srcptr numerator,
                                 mpz_srcptr denominator,
                                 uint64_t shift) {
    mpz_t scaled, quotient;
    if (shift > (uint64_t)ULONG_MAX) return -1;
    mpz_inits(scaled, quotient, NULL);
    mpz_mul_2exp(scaled, numerator, (mp_bitcnt_t)shift);
    mpz_fdiv_qr(quotient, info->remainder, scaled, denominator);
    info->quotient_low = quotient_low_bits(quotient);
    mpz_set(info->denominator, denominator);
    info->denominator_valid = 1;
    set_half_compare(info);
    mpz_clears(scaled, quotient, NULL);
    return 0;
}

static int positive_shift_modular(ppf_divrem_info_t *info,
                                  mpz_srcptr numerator,
                                  mpz_srcptr denominator,
                                  uint64_t shift) {
    mpz_t modulus, nmod, two, exponent, power, qlow;
    mpz_inits(modulus, nmod, two, exponent, power, qlow, NULL);
    mpz_mul_2exp(modulus, denominator, PPF_QUOTIENT_BITS);
    mpz_mod(nmod, numerator, modulus);
    mpz_set_ui(two, 2);
    mpz_import(exponent, 1, -1, sizeof(shift), 0, 0, &shift);
    mpz_powm(power, two, exponent, modulus);
    mpz_mul(nmod, nmod, power);
    mpz_mod(nmod, nmod, modulus);
    mpz_fdiv_qr(qlow, info->remainder, nmod, denominator);
    info->quotient_low = mpz_get_ui(qlow) & PPF_QUOTIENT_MASK;
    mpz_set(info->denominator, denominator);
    info->denominator_valid = 1;
    set_half_compare(info);
    mpz_clears(modulus, nmod, two, exponent, power, qlow, NULL);
    return 0;
}

static int compute_floor_divrem(ppf_divrem_info_t *info,
                                mpz_srcptr numerator,
                                ppf_exp_t numerator_exp,
                                mpz_srcptr denominator,
                                ppf_exp_t denominator_exp) {
#if defined(__SIZEOF_INT128__)
    __extension__ typedef __int128 ppf_i128;
    ppf_i128 difference = (ppf_i128)numerator_exp -
                          (ppf_i128)denominator_exp;
#else
    long double difference = (long double)numerator_exp -
                             (long double)denominator_exp;
#endif
    if (difference >= 0) {
        uint64_t shift = (uint64_t)difference;
        uint64_t direct_limit = (uint64_t)mpz_sizeinbase(denominator, 2) * 2u
                              + PPF_DIRECT_SHIFT_EXTRA;
        info->common_exponent = denominator_exp;
        if (shift <= direct_limit)
            return positive_shift_direct(info, numerator, denominator, shift);
        return positive_shift_modular(info, numerator, denominator, shift);
    }
    {
        uint64_t shift = (uint64_t)(-difference);
        size_t numerator_bits = mpz_sizeinbase(numerator, 2);
        size_t denominator_bits = mpz_sizeinbase(denominator, 2);
#if defined(__SIZEOF_INT128__)
        ppf_i128 denominator_top = (ppf_i128)denominator_bits +
                                   (ppf_i128)shift;
        int quotient_zero = denominator_top > (ppf_i128)numerator_bits;
#else
        long double denominator_top = (long double)denominator_bits +
                                      (long double)shift;
        int quotient_zero = denominator_top > (long double)numerator_bits;
#endif
        info->common_exponent = numerator_exp;
        if (quotient_zero) {
            mpz_set(info->remainder, numerator);
            info->quotient_low = 0;
            info->half_compare = compare_twice_to_shifted(
                numerator, denominator, shift);
            if (info->half_compare >= 0 &&
                materialize_shifted_denominator(info, denominator, shift) != 0)
                return -1;
            return 0;
        }
        if (materialize_shifted_denominator(info, denominator, shift) != 0)
            return -1;
        {
            mpz_t quotient;
            mpz_init(quotient);
            mpz_fdiv_qr(quotient, info->remainder,
                        numerator, info->denominator);
            info->quotient_low = quotient_low_bits(quotient);
            mpz_clear(quotient);
        }
        set_half_compare(info);
        return 0;
    }
}

static long signed_quotient_bits(unsigned long low, int sign) {
    low &= PPF_QUOTIENT_MASK;
    if (low == 0) return 0;
    return sign < 0 ? -(long)low : (long)low;
}

static int remainder_core(ppf_t *rop, long *quotient_bits,
                          const ppf_t *x, const ppf_t *y,
                          int nearest, ppf_rnd_t rnd,
                          ppf_context_t *ctx) {
    mpz_t ax, ay, exact_remainder;
    ppf_exp_t ex, ey;
    ppf_divrem_info_t info;
    int quotient_sign, increment = 0, t;
    unsigned long qlow;

    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (quotient_bits) *quotient_bits = 0;
    if (!rop || !x || !y || !ctx || !ppf_rounding_valid(rnd))
        return remainder_error(rop, ctx);
    if (ppf_nan_p(x) || ppf_nan_p(y))
        return remainder_nan(rop, ppf_nan_p(x) ? x->sign : y->sign, ctx);
    if (ppf_inf_p(x) || ppf_zero_p(y))
        return remainder_nan(rop, 1, ctx);
    if (ppf_inf_p(y)) return ppf_set(rop, x, rnd, ctx);
    if (ppf_zero_p(x)) { ppf_set_zero(rop, x->sign); return 0; }

    mpz_roinit_n(ax, x->limbs, (mp_size_t)x->used_limbs);
    mpz_roinit_n(ay, y->limbs, (mp_size_t)y->used_limbs);
    mpz_init(exact_remainder);
    if (ppf_exp_sub_prec(x->exponent, x->precision, &ex) != 0 ||
        ppf_exp_sub_prec(y->exponent, y->precision, &ey) != 0) {
        mpz_clear(exact_remainder);
        return remainder_error(rop, ctx);
    }
    info_init(&info);
    if (compute_floor_divrem(&info, ax, ex, ay, ey) != 0) {
        info_clear(&info);
        mpz_clear(exact_remainder);
        return remainder_error(rop, ctx);
    }
    qlow = info.quotient_low;
    if (nearest && (info.half_compare > 0 ||
        (info.half_compare == 0 && (qlow & 1u)))) {
        increment = 1;
        if (!info.denominator_valid) {
            info_clear(&info);
            mpz_clear(exact_remainder);
            return remainder_error(rop, ctx);
        }
    }
    if (increment) {
        qlow = (qlow + 1u) & PPF_QUOTIENT_MASK;
        mpz_sub(exact_remainder, info.remainder, info.denominator);
    } else {
        mpz_set(exact_remainder, info.remainder);
    }
    if (x->sign < 0) mpz_neg(exact_remainder, exact_remainder);
    quotient_sign = x->sign * y->sign;
    if (quotient_bits)
        *quotient_bits = signed_quotient_bits(qlow, quotient_sign);

    if (mpz_sgn(exact_remainder) == 0) {
        ppf_set_zero(rop, x->sign);
        t = 0;
    } else {
        t = ppf_set_z_2exp(rop, exact_remainder,
                           info.common_exponent, rnd, ctx);
    }
    info_clear(&info);
    mpz_clear(exact_remainder);
    return t;
}

int ppf_fmod(ppf_t *rop, const ppf_t *x, const ppf_t *y,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return remainder_core(rop, NULL, x, y, 0, rnd, ctx);
}

int ppf_fmodquo(ppf_t *rop, long *quotient_bits,
                const ppf_t *x, const ppf_t *y,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    return remainder_core(rop, quotient_bits, x, y, 0, rnd, ctx);
}

int ppf_remainder(ppf_t *rop, const ppf_t *x, const ppf_t *y,
                  ppf_rnd_t rnd, ppf_context_t *ctx) {
    return remainder_core(rop, NULL, x, y, 1, rnd, ctx);
}

int ppf_remquo(ppf_t *rop, long *quotient_bits,
               const ppf_t *x, const ppf_t *y,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return remainder_core(rop, quotient_bits, x, y, 1, rnd, ctx);
}

int ppf_fmod_ui(ppf_t *rop, const ppf_t *x, unsigned long y,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_context_t wide;
    ppf_t denominator;
    ppf_prec_t p = PPF_PREC_MIN;
    unsigned long temp = y;
    int t;
    ctx = ppf_resolve_context(ctx);
    if (!ctx) return remainder_error(rop, ctx);
    while (temp) { ++p; temp >>= 1; }
    if (p < PPF_PREC_MIN) p = PPF_PREC_MIN;
    if (ppf_context_clone(&wide, ctx) != 0)
        return remainder_error(rop, ctx);
    wide.emin = PPF_EMIN_MIN;
    wide.emax = PPF_EMAX_MAX;
    wide.flags = 0;
    if (ppf_init2_ctx(&denominator, p, &wide) != 0) {
        ppf_context_clear(&wide);
        return remainder_error(rop, ctx);
    }
    if (y == 0) ppf_set_zero(&denominator, 1);
    else (void)ppf_set_ui(&denominator, y, PPF_RNDN, &wide);
    t = ppf_fmod(rop, x, &denominator, rnd, ctx);
    ppf_clear(&denominator);
    ppf_context_clear(&wide);
    return t;
}
