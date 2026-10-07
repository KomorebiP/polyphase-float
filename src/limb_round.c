/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef PPF_TERNARY_ERROR
#define PPF_TERNARY_ERROR INT_MIN
#endif

static size_t limb_bit_length(mp_srcptr value, size_t used) {
    mp_limb_t top;
    unsigned leading;
    while (used && value[used - 1] == 0) --used;
    if (!used) return 0;
    top = value[used - 1];
#if ULONG_MAX == UINT64_MAX
    leading = (unsigned)__builtin_clzl((unsigned long)top);
    return (used - 1) * PPF_LIMB_BITS +
           (sizeof(unsigned long) * CHAR_BIT - leading);
#else
    {
        size_t bits = 0;
        while (top) { ++bits; top >>= 1; }
        return (used - 1) * PPF_LIMB_BITS + bits;
    }
#endif
}

static int bit_at(mp_srcptr value, size_t used, size_t bit) {
    size_t limb = bit / PPF_LIMB_BITS;
    unsigned offset = (unsigned)(bit % PPF_LIMB_BITS);
    if (limb >= used) return 0;
    return (int)((value[limb] >> offset) & 1u);
}

static int any_bits_below(mp_srcptr value, size_t used, size_t count) {
    size_t full = count / PPF_LIMB_BITS;
    unsigned partial = (unsigned)(count % PPF_LIMB_BITS);
    size_t i;
    for (i = 0; i < full && i < used; ++i)
        if (value[i]) return 1;
    if (partial && full < used) {
        mp_limb_t mask = partial == PPF_LIMB_BITS
            ? ~(mp_limb_t)0
            : (((mp_limb_t)1 << partial) - 1);
        if (value[full] & mask) return 1;
    }
    return 0;
}

static mp_limb_t shifted_limb(mp_srcptr value, size_t used,
                              size_t bit_position) {
    size_t limb = bit_position / PPF_LIMB_BITS;
    unsigned offset = (unsigned)(bit_position % PPF_LIMB_BITS);
    mp_limb_t out = limb < used ? value[limb] >> offset : 0;
    if (offset && limb + 1 < used)
        out |= value[limb + 1] << (PPF_LIMB_BITS - offset);
    return out;
}

static void copy_right_shift(mp_ptr out, size_t out_size,
                             mp_srcptr value, size_t used,
                             size_t shift) {
    size_t i;
    for (i = 0; i < out_size; ++i)
        out[i] = shifted_limb(value, used,
                              shift + i * (size_t)PPF_LIMB_BITS);
}

static void copy_left_shift(mp_ptr out, size_t out_size,
                            mp_srcptr value, size_t used,
                            size_t shift) {
    size_t limb_offset = shift / PPF_LIMB_BITS;
    unsigned bit_offset = (unsigned)(shift % PPF_LIMB_BITS);
    size_t written = 0;

    if (shift == 0) {
        size_t count = used < out_size ? used : out_size;
        if (count) memmove(out, value, count * sizeof(*out));
        if (out_size > count)
            memset(out + count, 0, (out_size - count) * sizeof(*out));
        return;
    }

    if (limb_offset) {
        size_t zeros = limb_offset < out_size ? limb_offset : out_size;
        memset(out, 0, zeros * sizeof(*out));
    }
    if (limb_offset >= out_size) return;

    if (bit_offset == 0) {
        size_t count = used < out_size - limb_offset
            ? used : out_size - limb_offset;
        if (count) memmove(out + limb_offset, value, count * sizeof(*out));
        written = limb_offset + count;
    } else {
        size_t count = used < out_size - limb_offset
            ? used : out_size - limb_offset;
        mp_limb_t carry = 0;
        if (count)
            carry = mpn_lshift(out + limb_offset, value,
                               (mp_size_t)count, bit_offset);
        written = limb_offset + count;
        if (written < out_size) out[written++] = carry;
    }
    if (written < out_size)
        memset(out + written, 0, (out_size - written) * sizeof(*out));
}

static int all_precision_bits_are_one(mp_srcptr value, size_t limbs,
                                      ppf_prec_t precision) {
    size_t i;
    unsigned width;
    mp_limb_t mask;
    if (!limbs) return 0;
    for (i = 0; i + 1 < limbs; ++i)
        if (value[i] != ~(mp_limb_t)0) return 0;
    width = (unsigned)(precision -
            (ppf_prec_t)(limbs - 1) * PPF_LIMB_BITS);
    mask = width == PPF_LIMB_BITS
        ? ~(mp_limb_t)0
        : (((mp_limb_t)1 << width) - 1);
    return value[limbs - 1] == mask;
}

static int slow_interval(ppf_t *rop, mp_srcptr magnitude, size_t used,
                         int sign, ppf_exp_t base_exponent,
                         int external_sticky, ppf_rnd_t rnd,
                         ppf_context_t *ctx) {
    mpz_t mag, lower, upper;
    int ternary = 0;
    ppf_interval_status_t status;
    mpz_inits(mag, lower, upper, NULL);
    mpz_import(mag, used, -1, sizeof(mp_limb_t), 0, 0, magnitude);
    if (!external_sticky) {
        if (sign < 0) mpz_neg(mag, mag);
        ternary = ppf_set_z_2exp(rop, mag, base_exponent, rnd, ctx);
        mpz_clears(mag, lower, upper, NULL);
        return ternary;
    }
    if (sign > 0) {
        mpz_set(lower, mag);
        mpz_add_ui(upper, mag, 1);
    } else {
        mpz_neg(upper, mag);
        mpz_sub_ui(lower, upper, 1);
    }
    status = ppf_set_dyadic_interval(rop, lower, upper,
                                     base_exponent, rnd, &ternary, ctx);
    mpz_clears(mag, lower, upper, NULL);
    return status == PPF_INTERVAL_DECIDED ? ternary : PPF_TERNARY_ERROR;
}

int ppf_round_limbs_2exp(ppf_t *rop,
                         mp_srcptr magnitude,
                         size_t used,
                         int sign,
                         ppf_exp_t base_exponent,
                         int external_sticky,
                         ppf_rnd_t rnd,
                         ppf_context_t *ctx) {
    size_t source_bits, out_limbs, discarded;
    ppf_exp_t result_exponent;
    mp_ptr out;
    int inexact = 0, increment = 0, ternary = 0;

    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !ctx || !magnitude || !ppf_rounding_valid(rnd) ||
        !ppf_precision_valid(rop->precision)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    while (used && magnitude[used - 1] == 0) --used;
    if (!used) {
        ppf_set_zero(rop, sign < 0 ? -1 : 1);
        return external_sticky ? PPF_TERNARY_ERROR : 0;
    }

    source_bits = limb_bit_length(magnitude, used);
    if (source_bits > (size_t)INT64_MAX ||
        ppf_exp_add(base_exponent, (ppf_exp_t)source_bits,
                    &result_exponent) != 0 ||
        result_exponent < ctx->emin || result_exponent > ctx->emax) {
        return slow_interval(rop, magnitude, used, sign, base_exponent,
                             external_sticky, rnd, ctx);
    }

    out_limbs = ppf_limbs_for_precision(rop->precision);
    if (!out_limbs || ppf_reserve(rop, rop->precision) != 0) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    out = rop->limbs;
    if (!out) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    /* Every branch below overwrites all destination limbs.  Avoid the
       unconditional clear in ppf_limbs_write(), which otherwise adds a full
       extra memory pass to exact assignment and most arithmetic results. */
    rop->kind = PPF_KIND_NAN;
    rop->sign = 1;
    rop->exponent = 0;
    rop->used_limbs = out_limbs;

    if ((ppf_prec_t)source_bits > rop->precision) {
        discarded = source_bits - (size_t)rop->precision;
        copy_right_shift(out, out_limbs, magnitude, used, discarded);
        inexact = any_bits_below(magnitude, used, discarded) || external_sticky;
        if (inexact) {
            switch (rnd) {
                case PPF_RNDA: increment = 1; break;
                case PPF_RNDU: increment = sign > 0; break;
                case PPF_RNDD: increment = sign < 0; break;
                case PPF_RNDN: {
                    int half = bit_at(magnitude, used, discarded - 1);
                    int below = any_bits_below(magnitude, used,
                                               discarded - 1) ||
                                external_sticky;
                    increment = half && (below || (out[0] & 1u));
                    break;
                }
                case PPF_RNDZ:
                case PPF_RNDF:
                default: increment = 0; break;
            }
        }
        if (increment) {
            if (all_precision_bits_are_one(out, out_limbs,
                                           rop->precision)) {
                memset(out, 0, out_limbs * sizeof(*out));
                out[(rop->precision - 1) / PPF_LIMB_BITS] =
                    (mp_limb_t)1 <<
                    (unsigned)((rop->precision - 1) % PPF_LIMB_BITS);
                if (result_exponent == INT64_MAX ||
                    ++result_exponent > ctx->emax)
                    return slow_interval(rop, magnitude, used, sign,
                                         base_exponent, external_sticky,
                                         rnd, ctx);
            } else {
                (void)mpn_add_1(out, out, (mp_size_t)out_limbs, 1);
            }
        }
    } else {
        size_t shift = (size_t)(rop->precision - (ppf_prec_t)source_bits);
        if (external_sticky)
            return slow_interval(rop, magnitude, used, sign,
                                 base_exponent, 1, rnd, ctx);
        copy_left_shift(out, out_limbs, magnitude, used, shift);
    }

    ppf_zero_unused_bits(rop);
    if (ppf_commit_regular(rop, sign, result_exponent, ctx) != 0) {
        ppf_set_nan(rop, 1);
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (inexact) {
        ppf_raise(ctx, PPF_FLAG_INEXACT);
        if (sign > 0) ternary = increment ? 1 : -1;
        else ternary = increment ? -1 : 1;
    }
    return ternary;
}
