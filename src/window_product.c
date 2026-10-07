/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ppf_openmp_compat.h"

#ifndef PPF_WINDOW_GUARD_LIMBS
#define PPF_WINDOW_GUARD_LIMBS 2u
#endif

static size_t window_trim(mp_srcptr value, size_t used) {
    while (used && value[used - 1] == 0) --used;
    return used;
}

static size_t window_bit_length(mp_srcptr value, size_t used) {
    mp_limb_t top;
    unsigned leading;
    used = window_trim(value, used);
    if (!used) return 0;
    top = value[used - 1];
#if ULONG_MAX == UINT64_MAX
    leading = (unsigned)__builtin_clzl((unsigned long)top);
    return (used - 1u) * PPF_LIMB_BITS
         + (sizeof(unsigned long) * CHAR_BIT - leading);
#else
    {
        size_t bits = 0;
        while (top) { ++bits; top >>= 1; }
        return (used - 1u) * PPF_LIMB_BITS + bits;
    }
#endif
}

static void window_mul(mp_ptr out, mp_srcptr a, size_t an,
                       mp_srcptr b, size_t bn, int square) {
    an = window_trim(a, an);
    bn = window_trim(b, bn);
    if (!an || !bn) return;
    if (square && a == b && an == bn)
        mpn_sqr(out, a, (mp_size_t)an);
    else if (an >= bn)
        mpn_mul(out, a, (mp_size_t)an, b, (mp_size_t)bn);
    else
        mpn_mul(out, b, (mp_size_t)bn, a, (mp_size_t)an);
}

static int window_add_at(mp_ptr out, size_t out_n,
                         mp_srcptr add, size_t add_n, size_t offset) {
    mp_limb_t carry;
    size_t at;
    add_n = window_trim(add, add_n);
    if (!add_n) return 0;
    if (offset > out_n || add_n > out_n - offset) return -1;
    carry = mpn_add_n(out + offset, out + offset,
                      add, (mp_size_t)add_n);
    at = offset + add_n;
    while (carry && at < out_n) {
        ++out[at];
        carry = out[at] == 0;
        ++at;
    }
    return carry ? -1 : 0;
}

/* Compute all product channels except low*low.  The omitted term is positive
   and rigorously bounded by 2^error_bits.  Return 1 when the interval proves
   the final rounded value and ternary, 0 when a complete product is required,
   and -1 on allocation/range failure. */
int ppf_window_product_round_internal(
    ppf_t *rop, const ppf_t *a, const ppf_t *b,
    ppf_rnd_t rnd, unsigned long workers, int square,
    int *ternary_out, ppf_context_t *ctx) {
    size_t an, bn, abits, bbits, p, discarded_min;
    size_t guard_bits, low_limbs, max_low, h, ah, bh, total;
    size_t hh_n, cross1_n, cross2_n, scratch_n, used;
    size_t alow_bits, blow_bits, error_bits;
    ppf_arena_mark_t mark;
    mp_ptr memory, high, scratch, hh, cross1, cross2;
    ppf_exp_t ea, eb, exponent;
    mpz_t high_z, lower, upper, error;
    int ternary = 0, sign;
    ppf_interval_status_t status;
    ppf_flags_t saved_flags;

#ifndef _OPENMP
    (void)workers;
#endif
    if (!rop || !a || !b || !ctx || !ppf_regular_p(a) ||
        !ppf_regular_p(b)) return 0;
    an = a->used_limbs;
    bn = b->used_limbs;
    abits = window_bit_length(a->limbs, an);
    bbits = window_bit_length(b->limbs, bn);
    p = (size_t)rop->precision;
    if (!abits || !bbits || abits + bbits - 1u <= p) return 0;
    discarded_min = abits + bbits - 1u - p;
    guard_bits = (size_t)PPF_WINDOW_GUARD_LIMBS * PPF_LIMB_BITS;
    if (discarded_min <= guard_bits + 2u * PPF_LIMB_BITS) return 0;
    low_limbs = (discarded_min - guard_bits) / (2u * PPF_LIMB_BITS);
    max_low = an < bn ? an : bn;
    if (!low_limbs || low_limbs >= max_low) return 0;

    h = low_limbs;
    ah = an - h;
    bh = bn - h;
    total = an + bn;
    hh_n = ah + bh;
    cross1_n = ah + h;
    cross2_n = h + bh;
    scratch_n = hh_n + cross1_n + (square ? 0u : cross2_n);
    if (total > SIZE_MAX / sizeof(mp_limb_t) ||
        scratch_n > SIZE_MAX / sizeof(mp_limb_t) ||
        total > SIZE_MAX - scratch_n) return -1;

    mark = ppf_arena_mark(ctx);
    memory = ppf_arena_alloc(ctx, (total + scratch_n) * sizeof(*memory),
                             _Alignof(mp_limb_t));
    if (!memory) return -1;
    high = memory;
    scratch = high + total;
    hh = scratch;
    cross1 = hh + hh_n;
    cross2 = square ? NULL : cross1 + cross1_n;
    memset(high, 0, total * sizeof(*high));
    memset(scratch, 0, scratch_n * sizeof(*scratch));

    if (square) {
#ifdef _OPENMP
        if (workers >= 2 && !omp_in_parallel()) {
PPF_OMP_PRAGMA(omp parallel sections num_threads(2))
            {
PPF_OMP_PRAGMA(omp section)
                { window_mul(hh, a->limbs + h, ah,
                             a->limbs + h, ah, 1); }
PPF_OMP_PRAGMA(omp section)
                { window_mul(cross1, a->limbs + h, ah,
                             a->limbs, h, 0); }
            }
        } else
#endif
        {
            window_mul(hh, a->limbs + h, ah, a->limbs + h, ah, 1);
            window_mul(cross1, a->limbs + h, ah, a->limbs, h, 0);
        }
        if (window_add_at(high, total, hh, hh_n, 2u * h) ||
            window_add_at(high, total, cross1, cross1_n, h) ||
            window_add_at(high, total, cross1, cross1_n, h)) {
            ppf_arena_release(ctx, mark);
            return -1;
        }
    } else {
#ifdef _OPENMP
        if (workers >= 3 && !omp_in_parallel()) {
PPF_OMP_PRAGMA(omp parallel sections num_threads(3))
            {
PPF_OMP_PRAGMA(omp section)
                { window_mul(hh, a->limbs + h, ah,
                             b->limbs + h, bh, 0); }
PPF_OMP_PRAGMA(omp section)
                { window_mul(cross1, a->limbs + h, ah,
                             b->limbs, h, 0); }
PPF_OMP_PRAGMA(omp section)
                { window_mul(cross2, a->limbs, h,
                             b->limbs + h, bh, 0); }
            }
        } else
#endif
        {
            window_mul(hh, a->limbs + h, ah, b->limbs + h, bh, 0);
            window_mul(cross1, a->limbs + h, ah, b->limbs, h, 0);
            window_mul(cross2, a->limbs, h, b->limbs + h, bh, 0);
        }
        if (window_add_at(high, total, hh, hh_n, 2u * h) ||
            window_add_at(high, total, cross1, cross1_n, h) ||
            window_add_at(high, total, cross2, cross2_n, h)) {
            ppf_arena_release(ctx, mark);
            return -1;
        }
    }

    if (ppf_exp_sub_prec(a->exponent, a->precision, &ea) != 0 ||
        ppf_exp_sub_prec(b->exponent, b->precision, &eb) != 0 ||
        ppf_exp_add(ea, eb, &exponent) != 0) {
        ppf_arena_release(ctx, mark);
        return -1;
    }
    used = window_trim(high, total);
    alow_bits = window_bit_length(a->limbs, h);
    blow_bits = square ? alow_bits : window_bit_length(b->limbs, h);
    if (!alow_bits || !blow_bits) {
        int rc = ppf_round_limbs_2exp(
            rop, high, used, square ? 1 : a->sign * b->sign,
            exponent, 0, rnd, ctx);
        ppf_arena_release(ctx, mark);
        return rc == PPF_TERNARY_ERROR ? -1 : 1;
    }
    if (alow_bits > SIZE_MAX - blow_bits) {
        ppf_arena_release(ctx, mark);
        return -1;
    }
    error_bits = alow_bits + blow_bits;
    if (error_bits > (size_t)ULONG_MAX) {
        ppf_arena_release(ctx, mark);
        return 0;
    }

    mpz_inits(high_z, lower, upper, error, NULL);
    mpz_import(high_z, used, -1, sizeof(mp_limb_t), 0, 0, high);
    sign = square ? 1 : a->sign * b->sign;
    mpz_set_ui(error, 1);
    mpz_mul_2exp(error, error, (mp_bitcnt_t)error_bits);
    if (sign > 0) {
        mpz_set(lower, high_z);
        mpz_add(upper, high_z, error);
    } else {
        mpz_neg(upper, high_z);          /* -H */
        mpz_add(lower, high_z, error);
        mpz_neg(lower, lower);           /* -(H + 2^error_bits) */
    }
    saved_flags = ctx->flags;
    status = ppf_set_dyadic_interval(rop, lower, upper, exponent,
                                     rnd, &ternary, ctx);
    if (status != PPF_INTERVAL_DECIDED) ctx->flags = saved_flags;
    mpz_clears(high_z, lower, upper, error, NULL);
    ppf_arena_release(ctx, mark);
    if (status == PPF_INTERVAL_DECIDED) {
        if (ternary_out) *ternary_out = ternary;
        return 1;
    }
    if (status == PPF_INTERVAL_UNDECIDED) return 0;
    return -1;
}
