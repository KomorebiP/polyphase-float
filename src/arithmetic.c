/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "ppf_openmp_compat.h"

#ifndef PPF_MUL_PARALLEL_MIN_LIMBS
#define PPF_MUL_PARALLEL_MIN_LIMBS 1536u
#endif

#define PPF_GUARD_BITS 24u
#define PPF_INTERVAL_ATTEMPTS 24u

typedef struct {
    mpz_t z;
    ppf_exp_t e;
} exact_term_t;

static int arithmetic_error(ppf_t *rop, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, 1);
    ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

static int arithmetic_memory_error(ppf_t *rop, ppf_context_t *ctx) {
    if (ctx && (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0)
        return ppf_allocation_error(rop, ctx);
    return arithmetic_error(rop, ctx);
}

static int invalid_result(ppf_t *rop, ppf_context_t *ctx, int sign) {
    if (rop) ppf_set_nan(rop, sign);
    ppf_raise(ctx, PPF_FLAG_NAN);
    return 0;
}

int ppf_get_exact_dyadic(mpz_ptr significand, ppf_exp_t *exponent,
                         const ppf_t *op) {
    if (!significand || !exponent || !op) return -1;
    if (ppf_zero_p(op)) {
        mpz_set_ui(significand, 0);
        *exponent = 0;
        return 0;
    }
    if (!ppf_regular_p(op)) return -1;
    *exponent = ppf_get_z_2exp(significand, op);
    return 0;
}

static int add_exp_bits(ppf_exp_t e, size_t bits, ppf_exp_t *out) {
    if (bits > (size_t)INT64_MAX) return -1;
    return ppf_exp_add(e, (ppf_exp_t)bits, out);
}


static int term_bounds(mpz_ptr lower, mpz_ptr upper,
                       mpz_srcptr z, ppf_exp_t exponent,
                       ppf_exp_t scale) {
    uint64_t distance;
    int sign = mpz_sgn(z);
    if (sign == 0) {
        mpz_set_ui(lower, 0);
        mpz_set_ui(upper, 0);
        return 0;
    }
    if (exponent >= scale) {
        distance = (uint64_t)exponent - (uint64_t)scale;
        if (distance > (uint64_t)ULONG_MAX) return -1;
        mpz_mul_2exp(lower, z, (mp_bitcnt_t)distance);
        mpz_set(upper, lower);
        return 0;
    }
    distance = (uint64_t)scale - (uint64_t)exponent;
    if (distance > (uint64_t)ULONG_MAX) {
        if (sign > 0) {
            mpz_set_ui(lower, 0);
            mpz_set_ui(upper, 1);
        } else {
            mpz_set_si(lower, -1);
            mpz_set_ui(upper, 0);
        }
        return 0;
    }
    mpz_fdiv_q_2exp(lower, z, (mp_bitcnt_t)distance);
    mpz_cdiv_q_2exp(upper, z, (mp_bitcnt_t)distance);
    return 0;
}

static int exact_accumulate(mpz_ptr result, ppf_exp_t *result_e,
                            const exact_term_t *terms, size_t count,
                            int *did_exact) {
    ppf_exp_t common = 0;
    uint64_t span = 0, limit = 0;
    size_t i, bits_total = 0;
    int have = 0;
    *did_exact = 0;
    mpz_set_ui(result, 0);
    for (i = 0; i < count; ++i) {
        size_t bits;
        if (mpz_sgn(terms[i].z) == 0) continue;
        bits = mpz_sizeinbase(terms[i].z, 2);
        if (SIZE_MAX - bits_total < bits) bits_total = SIZE_MAX;
        else bits_total += bits;
        if (!have || terms[i].e < common) common = terms[i].e;
        have = 1;
    }
    if (!have) {
        *result_e = 0;
        *did_exact = 1;
        return 0;
    }
    for (i = 0; i < count; ++i) {
        uint64_t d;
        if (mpz_sgn(terms[i].z) == 0) continue;
        d = (uint64_t)terms[i].e - (uint64_t)common;
        if (d > span) span = d;
    }
    limit = bits_total == SIZE_MAX ? UINT64_MAX : (uint64_t)bits_total + 4096u;
    if (limit < (1u << 20)) limit = (1u << 20);
    if (span > limit || span > (uint64_t)ULONG_MAX) return 0;
    for (i = 0; i < count; ++i) {
        uint64_t d;
        mpz_t tmp;
        if (mpz_sgn(terms[i].z) == 0) continue;
        d = (uint64_t)terms[i].e - (uint64_t)common;
        mpz_init_set(tmp, terms[i].z);
        if (d) mpz_mul_2exp(tmp, tmp, (mp_bitcnt_t)d);
        mpz_add(result, result, tmp);
        mpz_clear(tmp);
    }
    *result_e = common;
    *did_exact = 1;
    return 0;
}

static int round_terms(ppf_t *rop, const exact_term_t *terms, size_t count,
                       int zero_sign, ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t exact, lower, upper, lo, hi;
    ppf_exp_t exact_e = 0, top = 0, min_e = 0, scale;
    ppf_interval_status_t status;
    int ternary = 0, did_exact = 0, have = 0;
    size_t i;

    mpz_inits(exact, lower, upper, lo, hi, NULL);
    if (exact_accumulate(exact, &exact_e, terms, count, &did_exact) != 0) {
        mpz_clears(exact, lower, upper, lo, hi, NULL);
        return arithmetic_error(rop, ctx);
    }
    if (did_exact) {
        if (mpz_sgn(exact) == 0) {
            ppf_set_zero(rop, zero_sign);
            mpz_clears(exact, lower, upper, lo, hi, NULL);
            return 0;
        }
        ternary = ppf_set_z_2exp(rop, exact, exact_e, rnd, ctx);
        mpz_clears(exact, lower, upper, lo, hi, NULL);
        return ternary;
    }

    for (i = 0; i < count; ++i) {
        mpz_t mag;
        size_t bits;
        ppf_exp_t t;
        if (mpz_sgn(terms[i].z) == 0) continue;
        mpz_init(mag);
        mpz_abs(mag, terms[i].z);
        bits = mpz_sizeinbase(mag, 2);
        mpz_clear(mag);
        if (add_exp_bits(terms[i].e, bits, &t) != 0) {
            mpz_clears(exact, lower, upper, lo, hi, NULL);
            return arithmetic_error(rop, ctx);
        }
        if (!have || t > top) top = t;
        if (!have || terms[i].e < min_e) min_e = terms[i].e;
        have = 1;
    }
    if (!have) {
        ppf_set_zero(rop, zero_sign);
        mpz_clears(exact, lower, upper, lo, hi, NULL);
        return 0;
    }
    if (rop->precision > (ppf_prec_t)INT64_MAX - PPF_GUARD_BITS ||
        ppf_exp_add(top, -(ppf_exp_t)(rop->precision + PPF_GUARD_BITS),
                    &scale) != 0) {
        mpz_clears(exact, lower, upper, lo, hi, NULL);
        return arithmetic_error(rop, ctx);
    }

    for (unsigned attempt = 0; attempt < PPF_INTERVAL_ATTEMPTS; ++attempt) {
        mpz_set_ui(lower, 0);
        mpz_set_ui(upper, 0);
        for (i = 0; i < count; ++i) {
            if (term_bounds(lo, hi, terms[i].z, terms[i].e, scale) != 0) {
                mpz_clears(exact, lower, upper, lo, hi, NULL);
                return arithmetic_error(rop, ctx);
            }
            mpz_add(lower, lower, lo);
            mpz_add(upper, upper, hi);
        }
        if (mpz_cmp(lower, upper) == 0) {
            if (mpz_sgn(lower) == 0) ppf_set_zero(rop, zero_sign);
            else ternary = ppf_set_z_2exp(rop, lower, scale, rnd, ctx);
            mpz_clears(exact, lower, upper, lo, hi, NULL);
            return ternary;
        }
        status = ppf_set_dyadic_interval(rop, lower, upper, scale,
                                         rnd, &ternary, ctx);
        if (status == PPF_INTERVAL_DECIDED) {
            mpz_clears(exact, lower, upper, lo, hi, NULL);
            return ternary;
        }
        if (scale <= min_e || scale < INT64_MIN + 64) break;
        scale -= 64;
    }

    /* Last resort: exact alignment when its shift is representable. */
    {
        ppf_exp_t common = min_e;
        mpz_set_ui(exact, 0);
        for (i = 0; i < count; ++i) {
            uint64_t d;
            mpz_t tmp;
            if (mpz_sgn(terms[i].z) == 0) continue;
            d = (uint64_t)terms[i].e - (uint64_t)common;
            if (d > (uint64_t)ULONG_MAX) {
                mpz_clears(exact, lower, upper, lo, hi, NULL);
                return arithmetic_error(rop, ctx);
            }
            mpz_init_set(tmp, terms[i].z);
            if (d) mpz_mul_2exp(tmp, tmp, (mp_bitcnt_t)d);
            mpz_add(exact, exact, tmp);
            mpz_clear(tmp);
        }
        if (mpz_sgn(exact) == 0) ppf_set_zero(rop, zero_sign);
        else ternary = ppf_set_z_2exp(rop, exact, common, rnd, ctx);
    }
    mpz_clears(exact, lower, upper, lo, hi, NULL);
    return ternary;
}

static int zero_sign_for_sum(int sign_a, int sign_b, ppf_rnd_t rnd) {
    if (sign_a == sign_b) return sign_a;
    return rnd == PPF_RNDD ? -1 : 1;
}

static int direct_same_phase_addsub(ppf_t *rop,
                                    const ppf_t *a, const ppf_t *b,
                                    int effective_b_sign,
                                    ppf_rnd_t rnd,
                                    ppf_context_t *ctx) {
    size_t n = a->used_limbs;
    ppf_arena_mark_t mark = ppf_arena_mark(ctx);
    mp_ptr temp;
    ppf_exp_t base;
    int sign, cmp, t;
    mp_limb_t carry;
    if (!ppf_regular_p(a) || !ppf_regular_p(b) ||
        a->precision != b->precision || a->exponent != b->exponent ||
        a->used_limbs != b->used_limbs) return PPF_TERNARY_ERROR;
    temp = ppf_arena_alloc(ctx, (n + 1) * sizeof(mp_limb_t),
                           _Alignof(mp_limb_t));
    if (!temp) return PPF_TERNARY_ERROR;
    memset(temp, 0, (n + 1) * sizeof(*temp));
    if (ppf_exp_sub_prec(a->exponent, a->precision, &base) != 0) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    if (a->sign == effective_b_sign) {
        carry = mpn_add_n(temp, a->limbs, b->limbs, (mp_size_t)n);
        temp[n] = carry;
        sign = a->sign;
        t = ppf_round_limbs_2exp(rop, temp, n + (carry != 0), sign,
                                 base, 0, rnd, ctx);
    } else {
        cmp = mpn_cmp(a->limbs, b->limbs, (mp_size_t)n);
        if (cmp == 0) {
            ppf_set_zero(rop, rnd == PPF_RNDD ? -1 : 1);
            t = 0;
        } else if (cmp > 0) {
            (void)mpn_sub_n(temp, a->limbs, b->limbs, (mp_size_t)n);
            t = ppf_round_limbs_2exp(rop, temp, n, a->sign,
                                     base, 0, rnd, ctx);
        } else {
            (void)mpn_sub_n(temp, b->limbs, a->limbs, (mp_size_t)n);
            t = ppf_round_limbs_2exp(rop, temp, n, effective_b_sign,
                                     base, 0, rnd, ctx);
        }
    }
    ppf_arena_release(ctx, mark);
    return t;
}


#ifndef PPF_ADD_DIRECT_EXTRA_BITS
#define PPF_ADD_DIRECT_EXTRA_BITS 4096u
#endif

static void addsub_copy_lshift(mp_ptr out, size_t out_size,
                               mp_srcptr value, size_t used,
                               size_t shift) {
    size_t limb_offset = shift / PPF_LIMB_BITS;
    unsigned bit_offset = (unsigned)(shift % PPF_LIMB_BITS);
    mp_limb_t carry = 0;
    memset(out, 0, out_size * sizeof(*out));
    if (!used || limb_offset >= out_size) return;
    if (used > out_size - limb_offset) used = out_size - limb_offset;
    if (bit_offset) {
        carry = mpn_lshift(out + limb_offset, value,
                           (mp_size_t)used, bit_offset);
        if (carry && limb_offset + used < out_size)
            out[limb_offset + used] = carry;
    } else {
        memcpy(out + limb_offset, value, used * sizeof(*out));
    }
}

static size_t addsub_trim(mp_srcptr x, size_t n) {
    while (n && x[n - 1] == 0) --n;
    return n;
}


static int direct_same_precision_shift_addsub(
    ppf_t *rop, const ppf_t *a, const ppf_t *b,
    int effective_b_sign, int zero_sign,
    ppf_rnd_t rnd, ppf_context_t *ctx) {
    const ppf_t *high, *low;
    int high_sign, low_sign, sign, ternary;
    uint64_t shift64;
    size_t result_bits, result_limbs, result_used;
    ppf_exp_t base;
    ppf_arena_mark_t mark;
    mp_ptr result;
    mp_limb_t carry_or_borrow;

    if (!ppf_regular_p(a) || !ppf_regular_p(b) ||
        a->precision != b->precision || a->exponent == b->exponent)
        return PPF_TERNARY_ERROR;
    if (a->exponent > b->exponent) {
        high = a; low = b;
        high_sign = a->sign; low_sign = effective_b_sign;
        shift64 = (uint64_t)a->exponent - (uint64_t)b->exponent;
        if (ppf_exp_sub_prec(b->exponent, b->precision, &base) != 0)
            return PPF_TERNARY_ERROR;
    } else {
        high = b; low = a;
        high_sign = effective_b_sign; low_sign = a->sign;
        shift64 = (uint64_t)b->exponent - (uint64_t)a->exponent;
        if (ppf_exp_sub_prec(a->exponent, a->precision, &base) != 0)
            return PPF_TERNARY_ERROR;
    }
    if (!shift64 || shift64 > (uint64_t)SIZE_MAX ||
        shift64 > (uint64_t)(a->precision + PPF_ADD_DIRECT_EXTRA_BITS) ||
        (size_t)a->precision > SIZE_MAX - (size_t)shift64 - 1u)
        return PPF_TERNARY_ERROR;
    result_bits = (size_t)a->precision + (size_t)shift64 + 1u;
    result_limbs = (result_bits + PPF_LIMB_BITS - 1u) / PPF_LIMB_BITS;
    mark = ppf_arena_mark(ctx);
    result = ppf_arena_alloc(ctx, result_limbs * sizeof(*result),
                             _Alignof(mp_limb_t));
    if (!result) return PPF_TERNARY_ERROR;
    addsub_copy_lshift(result, result_limbs,
                       high->limbs, high->used_limbs, (size_t)shift64);
    if (high_sign == low_sign) {
        carry_or_borrow = mpn_add(result, result, (mp_size_t)result_limbs,
                                  low->limbs, (mp_size_t)low->used_limbs);
        if (carry_or_borrow) {
            ppf_arena_release(ctx, mark);
            return PPF_TERNARY_ERROR;
        }
        sign = high_sign;
    } else {
        /* A one-or-more-bit phase lead makes HIGH strictly larger. */
        carry_or_borrow = mpn_sub(result, result,
                                  (mp_size_t)result_limbs,
                                  low->limbs, (mp_size_t)low->used_limbs);
        if (carry_or_borrow) {
            ppf_arena_release(ctx, mark);
            return PPF_TERNARY_ERROR;
        }
        sign = high_sign;
    }
    result_used = addsub_trim(result, result_limbs);
    if (!result_used) {
        ppf_set_zero(rop, zero_sign);
        ppf_arena_release(ctx, mark);
        return 0;
    }
    ternary = ppf_round_limbs_2exp(rop, result, result_used,
                                   sign, base, 0, rnd, ctx);
    ppf_arena_release(ctx, mark);
    return ternary;
}

static int direct_general_addsub(ppf_t *rop,
                                 const ppf_t *a, const ppf_t *b,
                                 int effective_b_sign, int zero_sign,
                                 ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_exp_t ea, eb, common;
    uint64_t shift_a64, shift_b64, maximum_shift;
    size_t bits_a, bits_b, result_bits, result_limbs, needed;
    mp_ptr memory, aa, bb, result;
    ppf_arena_mark_t mark;
    int cmp, sign, ternary;
    mp_limb_t carry;

    if (!ppf_regular_p(a) || !ppf_regular_p(b) ||
        ppf_exp_sub_prec(a->exponent, a->precision, &ea) != 0 ||
        ppf_exp_sub_prec(b->exponent, b->precision, &eb) != 0)
        return PPF_TERNARY_ERROR;
    common = ea < eb ? ea : eb;
    shift_a64 = (uint64_t)ea - (uint64_t)common;
    shift_b64 = (uint64_t)eb - (uint64_t)common;
    maximum_shift = shift_a64 > shift_b64 ? shift_a64 : shift_b64;
    if (maximum_shift > (uint64_t)SIZE_MAX ||
        maximum_shift > (uint64_t)(a->precision + b->precision +
                                    PPF_ADD_DIRECT_EXTRA_BITS))
        return PPF_TERNARY_ERROR;
    if ((size_t)a->precision > SIZE_MAX - (size_t)shift_a64 ||
        (size_t)b->precision > SIZE_MAX - (size_t)shift_b64)
        return PPF_TERNARY_ERROR;
    bits_a = (size_t)a->precision + (size_t)shift_a64;
    bits_b = (size_t)b->precision + (size_t)shift_b64;
    result_bits = bits_a > bits_b ? bits_a : bits_b;
    if (result_bits == SIZE_MAX) return PPF_TERNARY_ERROR;
    ++result_bits;
    result_limbs = (result_bits + PPF_LIMB_BITS - 1u) / PPF_LIMB_BITS;
    if (!result_limbs || result_limbs > SIZE_MAX / 3u)
        return PPF_TERNARY_ERROR;
    needed = 3u * result_limbs;
    mark = ppf_arena_mark(ctx);
    memory = ppf_arena_alloc(ctx, needed * sizeof(*memory),
                             _Alignof(mp_limb_t));
    if (!memory) return PPF_TERNARY_ERROR;
    aa = memory;
    bb = aa + result_limbs;
    result = bb + result_limbs;
    addsub_copy_lshift(aa, result_limbs, a->limbs, a->used_limbs,
                       (size_t)shift_a64);
    addsub_copy_lshift(bb, result_limbs, b->limbs, b->used_limbs,
                       (size_t)shift_b64);
    memset(result, 0, result_limbs * sizeof(*result));
    if (a->sign == effective_b_sign) {
        carry = mpn_add_n(result, aa, bb, (mp_size_t)result_limbs);
        if (carry) {
            ppf_arena_release(ctx, mark);
            return PPF_TERNARY_ERROR;
        }
        sign = a->sign;
    } else {
        cmp = mpn_cmp(aa, bb, (mp_size_t)result_limbs);
        if (cmp == 0) {
            ppf_set_zero(rop, zero_sign);
            ppf_arena_release(ctx, mark);
            return 0;
        }
        if (cmp > 0) {
            (void)mpn_sub_n(result, aa, bb, (mp_size_t)result_limbs);
            sign = a->sign;
        } else {
            (void)mpn_sub_n(result, bb, aa, (mp_size_t)result_limbs);
            sign = effective_b_sign;
        }
    }
    result_limbs = addsub_trim(result, result_limbs);
    ternary = ppf_round_limbs_2exp(rop, result, result_limbs,
                                   sign, common, 0, rnd, ctx);
    ppf_arena_release(ctx, mark);
    return ternary;
}

static int add_or_sub(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                      int subtract, ppf_rnd_t rnd, ppf_context_t *ctx) {
    exact_term_t terms[2];
    int bsign, zero_sign, result;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !b || !ctx || !ppf_rounding_valid(rnd))
        return arithmetic_error(rop, ctx);
    bsign = subtract ? -b->sign : b->sign;
    if (ppf_nan_p(a) || ppf_nan_p(b))
        return invalid_result(rop, ctx,
                              ppf_nan_p(a) ? a->sign : b->sign);
    if (ppf_inf_p(a) || ppf_inf_p(b)) {
        if (ppf_inf_p(a) && ppf_inf_p(b) && a->sign != bsign)
            return invalid_result(rop, ctx, 1);
        if (ppf_inf_p(a)) ppf_set_inf(rop, a->sign);
        else ppf_set_inf(rop, bsign);
        return 0;
    }
    zero_sign = zero_sign_for_sum(a->sign, bsign, rnd);
    if (ppf_regular_p(a) && ppf_regular_p(b) &&
        a->precision == b->precision && a->exponent == b->exponent) {
        result = direct_same_phase_addsub(rop, a, b, bsign, rnd, ctx);
        if (result != PPF_TERNARY_ERROR) return result;
    }
    if (ppf_regular_p(a) && ppf_regular_p(b) &&
        a->precision == b->precision && a->exponent != b->exponent) {
        result = direct_same_precision_shift_addsub(
            rop, a, b, bsign, zero_sign, rnd, ctx);
        if (result != PPF_TERNARY_ERROR) return result;
    }
    if (ppf_regular_p(a) && ppf_regular_p(b)) {
        result = direct_general_addsub(rop, a, b, bsign, zero_sign,
                                       rnd, ctx);
        if (result != PPF_TERNARY_ERROR) return result;
    }
    mpz_init(terms[0].z);
    mpz_init(terms[1].z);
    if (ppf_get_exact_dyadic(terms[0].z, &terms[0].e, a) != 0 ||
        ppf_get_exact_dyadic(terms[1].z, &terms[1].e, b) != 0) {
        mpz_clear(terms[0].z); mpz_clear(terms[1].z);
        return arithmetic_error(rop, ctx);
    }
    if (subtract) mpz_neg(terms[1].z, terms[1].z);
    result = round_terms(rop, terms, 2, zero_sign, rnd, ctx);
    mpz_clear(terms[0].z); mpz_clear(terms[1].z);
    return result;
}

int ppf_add(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return add_or_sub(rop, a, b, 0, rnd, ctx);
}

int ppf_sub(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return add_or_sub(rop, a, b, 1, rnd, ctx);
}

static int regular_product(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                           ppf_rnd_t rnd, ppf_context_t *ctx) {
    size_t na = a->used_limbs, nb = b->used_limbs, used = na + nb;
    ppf_arena_mark_t mark = ppf_arena_mark(ctx);
    mp_ptr product = ppf_arena_alloc(ctx, used * sizeof(mp_limb_t),
                                     _Alignof(mp_limb_t));
    ppf_exp_t ea, eb, base;
    int t;
    if (!product) return arithmetic_memory_error(rop, ctx);
    memset(product, 0, used * sizeof(*product));
    if (na >= nb) mpn_mul(product, a->limbs, (mp_size_t)na,
                           b->limbs, (mp_size_t)nb);
    else mpn_mul(product, b->limbs, (mp_size_t)nb,
                 a->limbs, (mp_size_t)na);
    if (ppf_exp_sub_prec(a->exponent, a->precision, &ea) != 0 ||
        ppf_exp_sub_prec(b->exponent, b->precision, &eb) != 0 ||
        ppf_exp_add(ea, eb, &base) != 0) {
        ppf_arena_release(ctx, mark);
        return arithmetic_error(rop, ctx);
    }
    t = ppf_round_limbs_2exp(rop, product, used, a->sign * b->sign,
                             base, 0, rnd, ctx);
    ppf_arena_release(ctx, mark);
    return t;
}

static size_t ppf_product_trim(mp_srcptr value, size_t used) {
    while (used && value[used - 1] == 0) --used;
    return used;
}

static void ppf_product_add_at(mp_ptr destination, size_t destination_size,
                               mp_srcptr source, size_t source_size,
                               size_t offset) {
    mp_limb_t carry;
    size_t position;
    if (!source_size) return;
    carry = mpn_add_n(destination + offset, destination + offset,
                      source, (mp_size_t)source_size);
    position = offset + source_size;
    while (carry && position < destination_size) {
        ++destination[position];
        carry = destination[position] == 0;
        ++position;
    }
}

static size_t ppf_product_add_parts(mp_ptr out,
                                    mp_srcptr low_value, size_t low_size,
                                    mp_srcptr high_value, size_t high_size) {
    size_t maximum = low_size > high_size ? low_size : high_size;
    size_t minimum = low_size < high_size ? low_size : high_size;
    mp_limb_t carry = 0;
    memset(out, 0, (maximum + 1) * sizeof(*out));
    if (minimum) {
        carry = mpn_add_n(out, low_value, high_value,
                          (mp_size_t)minimum);
    }
    if (low_size > minimum) {
        memcpy(out + minimum, low_value + minimum,
               (low_size - minimum) * sizeof(*out));
        if (carry) {
            carry = mpn_add_1(out + minimum, out + minimum,
                              (mp_size_t)(low_size - minimum), carry);
        }
    } else if (high_size > minimum) {
        memcpy(out + minimum, high_value + minimum,
               (high_size - minimum) * sizeof(*out));
        if (carry) {
            carry = mpn_add_1(out + minimum, out + minimum,
                              (mp_size_t)(high_size - minimum), carry);
        }
    }
    if (carry) out[maximum++] = carry;
    return ppf_product_trim(out, maximum);
}

static int ppf_product_sub_extended(mp_ptr target, size_t target_size,
                                    mp_srcptr subtract,
                                    size_t subtract_size) {
    mp_limb_t borrow = 0;
    if (subtract_size) {
        borrow = mpn_sub_n(target, target, subtract,
                           (mp_size_t)subtract_size);
    }
    if (borrow && target_size > subtract_size) {
        borrow = mpn_sub_1(target + subtract_size,
                           target + subtract_size,
                           (mp_size_t)(target_size - subtract_size),
                           borrow);
    }
    return borrow != 0;
}

/* Exact rank-3 direct-limb product packet.  The caller owns PRODUCT;
   this helper allocates only reconstruction scratch in the context arena. */
static int rank3_product_into(mp_ptr product,
                              const ppf_t *a, const ppf_t *b,
                              unsigned long workers, int square,
                              ppf_context_t *ctx) {
    size_t n = a->used_limbs;
    size_t low, high, product_size, scratch_size;
    size_t z0_size, z2_size, sum_capacity, middle_capacity;
    ppf_arena_mark_t mark;
    mp_ptr scratch;

    product_size = a->used_limbs + b->used_limbs;
    if (a->used_limbs != b->used_limbs ||
        n < PPF_MUL_PARALLEL_MIN_LIMBS || workers < 3
#ifdef _OPENMP
        || omp_in_parallel()
#endif
        ) {
        memset(product, 0, product_size * sizeof(*product));
        if (square) mpn_sqr(product, a->limbs, (mp_size_t)n);
        else if (a->used_limbs >= b->used_limbs)
            mpn_mul(product, a->limbs, (mp_size_t)a->used_limbs,
                    b->limbs, (mp_size_t)b->used_limbs);
        else
            mpn_mul(product, b->limbs, (mp_size_t)b->used_limbs,
                    a->limbs, (mp_size_t)a->used_limbs);
        return 0;
    }

    low = n / 2;
    high = n - low;
    if (!low) {
        memset(product, 0, product_size * sizeof(*product));
        if (square) mpn_sqr(product, a->limbs, (mp_size_t)n);
        else mpn_mul(product, a->limbs, (mp_size_t)n,
                     b->limbs, (mp_size_t)n);
        return 0;
    }
    z0_size = 2 * low;
    z2_size = 2 * high;
    mark = ppf_arena_mark(ctx);

    if (square) {
        size_t cross_size = n;
        scratch_size = z0_size + cross_size + z2_size;
        scratch = ppf_arena_alloc(ctx, scratch_size * sizeof(*scratch),
                                  _Alignof(mp_limb_t));
        if (!scratch) return -1;
        {
            mp_ptr z0 = scratch;
            mp_ptr cross = z0 + z0_size;
            mp_ptr z2 = cross + cross_size;
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel sections num_threads(3))
            {
PPF_OMP_PRAGMA(omp section)
                { mpn_sqr(z0, a->limbs, (mp_size_t)low); }
PPF_OMP_PRAGMA(omp section)
                { mpn_mul(cross, a->limbs + low, (mp_size_t)high,
                          a->limbs, (mp_size_t)low); }
PPF_OMP_PRAGMA(omp section)
                { mpn_sqr(z2, a->limbs + low, (mp_size_t)high); }
            }
#else
            mpn_sqr(z0, a->limbs, (mp_size_t)low);
            mpn_mul(cross, a->limbs + low, (mp_size_t)high,
                    a->limbs, (mp_size_t)low);
            mpn_sqr(z2, a->limbs + low, (mp_size_t)high);
#endif
            memset(product, 0, product_size * sizeof(*product));
            ppf_product_add_at(product, product_size, z0, z0_size, 0);
            ppf_product_add_at(product, product_size, cross, cross_size, low);
            ppf_product_add_at(product, product_size, cross, cross_size, low);
            ppf_product_add_at(product, product_size, z2, z2_size, 2 * low);
        }
    } else {
        size_t sum_a_size, sum_b_size, middle_size;
        sum_capacity = high + 1;
        middle_capacity = 2 * sum_capacity;
        scratch_size = z0_size + z2_size + 2 * sum_capacity
                     + middle_capacity;
        scratch = ppf_arena_alloc(ctx, scratch_size * sizeof(*scratch),
                                  _Alignof(mp_limb_t));
        if (!scratch) return -1;
        {
            mp_ptr z0 = scratch;
            mp_ptr z2 = z0 + z0_size;
            mp_ptr sum_a = z2 + z2_size;
            mp_ptr sum_b = sum_a + sum_capacity;
            mp_ptr middle = sum_b + sum_capacity;
            sum_a_size = ppf_product_add_parts(
                sum_a, a->limbs, low, a->limbs + low, high);
            sum_b_size = ppf_product_add_parts(
                sum_b, b->limbs, low, b->limbs + low, high);
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel sections num_threads(3))
            {
PPF_OMP_PRAGMA(omp section)
                { mpn_mul(z0, a->limbs, (mp_size_t)low,
                          b->limbs, (mp_size_t)low); }
PPF_OMP_PRAGMA(omp section)
                { mpn_mul(z2, a->limbs + low, (mp_size_t)high,
                          b->limbs + low, (mp_size_t)high); }
PPF_OMP_PRAGMA(omp section)
                {
                    if (sum_a_size >= sum_b_size)
                        mpn_mul(middle, sum_a, (mp_size_t)sum_a_size,
                                sum_b, (mp_size_t)sum_b_size);
                    else
                        mpn_mul(middle, sum_b, (mp_size_t)sum_b_size,
                                sum_a, (mp_size_t)sum_a_size);
                }
            }
#else
            mpn_mul(z0, a->limbs, (mp_size_t)low,
                    b->limbs, (mp_size_t)low);
            mpn_mul(z2, a->limbs + low, (mp_size_t)high,
                    b->limbs + low, (mp_size_t)high);
            if (sum_a_size >= sum_b_size)
                mpn_mul(middle, sum_a, (mp_size_t)sum_a_size,
                        sum_b, (mp_size_t)sum_b_size);
            else
                mpn_mul(middle, sum_b, (mp_size_t)sum_b_size,
                        sum_a, (mp_size_t)sum_a_size);
#endif
            middle_size = sum_a_size + sum_b_size;
            if (middle_size < middle_capacity)
                memset(middle + middle_size, 0,
                       (middle_capacity - middle_size) * sizeof(*middle));
            if (ppf_product_sub_extended(middle, middle_capacity,
                                         z0, z0_size) ||
                ppf_product_sub_extended(middle, middle_capacity,
                                         z2, z2_size)) {
                ppf_arena_release(ctx, mark);
                memset(product, 0, product_size * sizeof(*product));
                mpn_mul(product, a->limbs, (mp_size_t)n,
                        b->limbs, (mp_size_t)n);
                return 0;
            }
            middle_size = ppf_product_trim(middle, middle_capacity);
            memset(product, 0, product_size * sizeof(*product));
            ppf_product_add_at(product, product_size, z0, z0_size, 0);
            ppf_product_add_at(product, product_size,
                               middle, middle_size, low);
            ppf_product_add_at(product, product_size,
                               z2, z2_size, 2 * low);
        }
    }
    ppf_arena_release(ctx, mark);
    return 0;
}

/* Exact direct-limb product packets.  Reconstruction and the single final
   rounding happen after the product team joins. */
static int threaded_product_limbs(ppf_t *rop,
                                  const ppf_t *a, const ppf_t *b,
                                  ppf_rnd_t rnd, unsigned long workers,
                                  int square, ppf_context_t *ctx) {
    size_t product_size = a->used_limbs + b->used_limbs;
    ppf_arena_mark_t mark = ppf_arena_mark(ctx);
    mp_ptr product = ppf_arena_alloc(ctx, product_size * sizeof(*product),
                                     _Alignof(mp_limb_t));
    ppf_exp_t ea, eb, base;
    int ternary;
    if (!product || rank3_product_into(product, a, b, workers,
                                       square, ctx) != 0) {
        ppf_arena_release(ctx, mark);
        return arithmetic_memory_error(rop, ctx);
    }
    if (ppf_exp_sub_prec(a->exponent, a->precision, &ea) != 0 ||
        ppf_exp_sub_prec(b->exponent, b->precision, &eb) != 0 ||
        ppf_exp_add(ea, eb, &base) != 0) {
        ppf_arena_release(ctx, mark);
        return arithmetic_error(rop, ctx);
    }
    ternary = ppf_round_limbs_2exp(
        rop, product, product_size,
        square ? 1 : a->sign * b->sign,
        base, 0, rnd, ctx);
    ppf_arena_release(ctx, mark);
    return ternary;
}

static int toom3_product_limbs(ppf_t *rop,
                               const ppf_t *a, const ppf_t *b,
                               ppf_rnd_t rnd, unsigned long workers,
                               int square, ppf_context_t *ctx) {
    size_t n, product_size;
    ppf_arena_mark_t mark;
    mp_ptr product;
    ppf_exp_t ea, eb, base;
    int ternary;

    if (a->used_limbs != b->used_limbs || workers < 1
#ifdef _OPENMP
        || omp_in_parallel()
#endif
        ) {
        return workers >= 3
            ? threaded_product_limbs(rop, a, b, rnd, workers, square, ctx)
            : regular_product(rop, a, b, rnd, ctx);
    }
    n = a->used_limbs;
    if (n < 3) return regular_product(rop, a, b, rnd, ctx);
    product_size = 2u * n;
    mark = ppf_arena_mark(ctx);
    product = ppf_arena_alloc(ctx, product_size * sizeof(*product),
                              _Alignof(mp_limb_t));
    if (!product) return arithmetic_memory_error(rop, ctx);
    (void)ppf_toom3_limb_product_internal(
        product, a->limbs, b->limbs, (mp_size_t)n,
        workers, square);
    if (ppf_exp_sub_prec(a->exponent, a->precision, &ea) != 0 ||
        ppf_exp_sub_prec(b->exponent, b->precision, &eb) != 0 ||
        ppf_exp_add(ea, eb, &base) != 0) {
        ppf_arena_release(ctx, mark);
        return arithmetic_error(rop, ctx);
    }
    ternary = ppf_round_limbs_2exp(
        rop, product, product_size,
        square ? 1 : a->sign * b->sign,
        base, 0, rnd, ctx);
    ppf_arena_release(ctx, mark);
    return ternary;
}

static int product_algorithm_regular(
    ppf_t *rop, const ppf_t *a, const ppf_t *b,
    ppf_rnd_t rnd, unsigned long workers,
    ppf_product_algorithm_t algorithm,
    ppf_product_operation_t operation, int square,
    ppf_context_t *ctx) {
    ppf_product_plan_t plan;
    if (ppf_product_plan_internal(&plan, a, b, operation, workers,
                                  algorithm) != 0)
        return arithmetic_error(rop, ctx);
    algorithm = plan.algorithm;
    workers = plan.effective_workers;
    switch (algorithm) {
        case PPF_PRODUCT_WINDOW: {
            int ternary = 0;
            int decided = ppf_window_product_round_internal(
                rop, a, b, rnd, workers, square, &ternary, ctx);
            if (decided > 0) return ternary;
            if (decided < 0) return arithmetic_error(rop, ctx);
            return workers >= 3
                ? threaded_product_limbs(rop, a, b, rnd, workers,
                                         square, ctx)
                : regular_product(rop, a, b, rnd, ctx);
        }
        case PPF_PRODUCT_TOOM3:
            return toom3_product_limbs(rop, a, b, rnd, workers,
                                       square, ctx);
        case PPF_PRODUCT_KARATSUBA3:
            return threaded_product_limbs(rop, a, b, rnd, workers,
                                          square, ctx);
        case PPF_PRODUCT_FULL:
        default:
            return regular_product(rop, a, b, rnd, ctx);
    }
}


/* Build an exact unrounded product into caller-provided limbs.  This is shared
   by fused arithmetic so product scheduling and final rounding remain separate. */
static int exact_product_into(
    mp_ptr product, const ppf_t *a, const ppf_t *b,
    unsigned long workers, ppf_product_algorithm_t algorithm,
    ppf_product_operation_t operation,
    ppf_context_t *ctx) {
    ppf_product_plan_t plan;
    size_t product_size;
    if (ppf_product_plan_internal(&plan, a, b, operation, workers,
                                  algorithm) != 0)
        return -1;
    algorithm = plan.algorithm;
    workers = plan.effective_workers;
    product_size = a->used_limbs + b->used_limbs;
    if (algorithm == PPF_PRODUCT_TOOM3 &&
        a->used_limbs == b->used_limbs && workers >= 1 &&
        a->used_limbs >= 3
#ifdef _OPENMP
        && !omp_in_parallel()
#endif
        ) {
        (void)ppf_toom3_limb_product_internal(
            product, a->limbs, b->limbs,
            (mp_size_t)a->used_limbs, workers, 0);
        return 0;
    }
    if (algorithm == PPF_PRODUCT_KARATSUBA3 ||
        algorithm == PPF_PRODUCT_WINDOW) {
        return rank3_product_into(product, a, b, workers, 0, ctx);
    }
    memset(product, 0, product_size * sizeof(*product));
    if (a->used_limbs >= b->used_limbs)
        mpn_mul(product, a->limbs, (mp_size_t)a->used_limbs,
                b->limbs, (mp_size_t)b->used_limbs);
    else
        mpn_mul(product, b->limbs, (mp_size_t)b->used_limbs,
                a->limbs, (mp_size_t)a->used_limbs);
    return 0;
}

int ppf_mul_algorithm_threads(
    ppf_t *rop, const ppf_t *a, const ppf_t *b,
    ppf_rnd_t rnd, unsigned long workers,
    ppf_product_algorithm_t algorithm, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !b || !ctx || !ppf_rounding_valid(rnd) ||
        algorithm < PPF_PRODUCT_AUTO || algorithm > PPF_PRODUCT_WINDOW)
        return arithmetic_error(rop, ctx);
    if (ppf_nan_p(a) || ppf_nan_p(b))
        return invalid_result(rop, ctx,
                              ppf_nan_p(a) ? a->sign : b->sign);
    if ((ppf_inf_p(a) && ppf_zero_p(b)) ||
        (ppf_inf_p(b) && ppf_zero_p(a)))
        return invalid_result(rop, ctx, 1);
    if (ppf_inf_p(a) || ppf_inf_p(b)) {
        ppf_set_inf(rop, a->sign * b->sign);
        return 0;
    }
    if (ppf_zero_p(a) || ppf_zero_p(b)) {
        ppf_set_zero(rop, a->sign * b->sign);
        return 0;
    }
    return product_algorithm_regular(rop, a, b, rnd,
                                     workers ? workers : 1,
                                     algorithm, PPF_PRODUCT_OP_MULTIPLY,
                                     0, ctx);
}

int ppf_mul_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    ppf_rnd_t rnd, unsigned long workers,
                    ppf_context_t *ctx) {
    return ppf_mul_algorithm_threads(rop, a, b, rnd, workers,
                                     PPF_PRODUCT_AUTO, ctx);
}

int ppf_mul(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_mul_threads(rop, a, b, rnd, 1, ctx);
}

int ppf_sqr(ppf_t *rop, const ppf_t *a,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    size_t n, used;
    ppf_arena_mark_t mark;
    mp_ptr product;
    ppf_exp_t ea, base;
    int t;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !ctx || !ppf_rounding_valid(rnd))
        return arithmetic_error(rop, ctx);
    if (ppf_nan_p(a)) return invalid_result(rop, ctx, a->sign);
    if (ppf_inf_p(a)) { ppf_set_inf(rop, 1); return 0; }
    if (ppf_zero_p(a)) { ppf_set_zero(rop, 1); return 0; }
    n = a->used_limbs; used = 2 * n;
    mark = ppf_arena_mark(ctx);
    product = ppf_arena_alloc(ctx, used * sizeof(*product),
                              _Alignof(mp_limb_t));
    if (!product) return arithmetic_memory_error(rop, ctx);
    memset(product, 0, used * sizeof(*product));
    mpn_sqr(product, a->limbs, (mp_size_t)n);
    if (ppf_exp_sub_prec(a->exponent, a->precision, &ea) != 0 ||
        ppf_exp_add(ea, ea, &base) != 0) {
        ppf_arena_release(ctx, mark);
        return arithmetic_error(rop, ctx);
    }
    t = ppf_round_limbs_2exp(rop, product, used, 1, base, 0, rnd, ctx);
    ppf_arena_release(ctx, mark);
    return t;
}

int ppf_sqr_algorithm_threads(
    ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd,
    unsigned long workers, ppf_product_algorithm_t algorithm,
    ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !ctx || !ppf_rounding_valid(rnd) ||
        algorithm < PPF_PRODUCT_AUTO || algorithm > PPF_PRODUCT_WINDOW)
        return arithmetic_error(rop, ctx);
    if (ppf_nan_p(a)) return invalid_result(rop, ctx, a->sign);
    if (ppf_inf_p(a)) { ppf_set_inf(rop, 1); return 0; }
    if (ppf_zero_p(a)) { ppf_set_zero(rop, 1); return 0; }
    return product_algorithm_regular(rop, a, a, rnd,
                                     workers ? workers : 1,
                                     algorithm, PPF_PRODUCT_OP_SQUARE,
                                     1, ctx);
}

int ppf_sqr_threads(ppf_t *rop, const ppf_t *a,
                    ppf_rnd_t rnd, unsigned long workers,
                    ppf_context_t *ctx) {
    return ppf_sqr_algorithm_threads(rop, a, rnd, workers,
                                     PPF_PRODUCT_AUTO, ctx);
}


static size_t ppf_trim_limb_count(mp_srcptr value, size_t used) {
    while (used && value[used - 1] == 0) --used;
    return used;
}

static size_t ppf_limb_bit_length_local(mp_srcptr value, size_t used) {
    mp_limb_t top;
    unsigned leading;
    used = ppf_trim_limb_count(value, used);
    if (!used) return 0;
    top = value[used - 1];
#if ULONG_MAX == UINT64_MAX
    leading = (unsigned)__builtin_clzl((unsigned long)top);
    return (used - 1) * PPF_LIMB_BITS
         + (sizeof(unsigned long) * CHAR_BIT - leading);
#else
    {
        size_t bits = 0;
        while (top) { ++bits; top >>= 1; }
        return (used - 1) * PPF_LIMB_BITS + bits;
    }
#endif
}

static int ppf_limbs_any_nonzero(mp_srcptr value, size_t used) {
    for (size_t i = 0; i < used; ++i)
        if (value[i] != 0) return 1;
    return 0;
}

static void ppf_copy_lshift_limbs(mp_ptr out, size_t out_size,
                                  mp_srcptr value, size_t used,
                                  size_t shift) {
    size_t limb_offset = shift / PPF_LIMB_BITS;
    unsigned bit_offset = (unsigned)(shift % PPF_LIMB_BITS);
    mp_limb_t carry = 0;
    memset(out, 0, out_size * sizeof(*out));
    if (!used || limb_offset >= out_size) return;
    if (used > out_size - limb_offset) used = out_size - limb_offset;
    if (bit_offset) {
        carry = mpn_lshift(out + limb_offset, value,
                           (mp_size_t)used, bit_offset);
        if (carry && limb_offset + used < out_size)
            out[limb_offset + used] = carry;
    } else {
        memcpy(out + limb_offset, value, used * sizeof(*out));
    }
}

static int ppf_compare_normalized_fractions(const ppf_t *a, const ppf_t *b,
                                            mp_ptr aligned_a,
                                            mp_ptr aligned_b,
                                            size_t common_limbs,
                                            ppf_prec_t common_bits) {
    size_t shift_a = (size_t)(common_bits - a->precision);
    size_t shift_b = (size_t)(common_bits - b->precision);
    ppf_copy_lshift_limbs(aligned_a, common_limbs,
                          a->limbs, a->used_limbs, shift_a);
    ppf_copy_lshift_limbs(aligned_b, common_limbs,
                          b->limbs, b->used_limbs, shift_b);
    return mpn_cmp(aligned_a, aligned_b, (mp_size_t)common_limbs);
}

int ppf_div(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_arena_mark_t mark;
    ppf_prec_t common_bits, quotient_bits;
    size_t common_limbs, shift, numerator_bits, numerator_limbs;
    size_t quotient_limbs_capacity, quotient_limbs, needed;
    mp_ptr memory, aligned_a, aligned_b, numerator, quotient, remainder;
    int compare, sign, remainder_nonzero, ternary;
    ppf_exp_t base;

    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !b || !ctx || !ppf_rounding_valid(rnd))
        return arithmetic_error(rop, ctx);
    if (ppf_nan_p(a) || ppf_nan_p(b))
        return invalid_result(rop, ctx,
                              ppf_nan_p(a) ? a->sign : b->sign);
    if ((ppf_zero_p(a) && ppf_zero_p(b)) ||
        (ppf_inf_p(a) && ppf_inf_p(b)))
        return invalid_result(rop, ctx, 1);
    sign = a->sign * b->sign;
    if (ppf_inf_p(a)) { ppf_set_inf(rop, sign); return 0; }
    if (ppf_zero_p(b)) {
        ppf_set_inf(rop, sign);
        ppf_raise(ctx, PPF_FLAG_DIVBY0);
        return 0;
    }
    if (ppf_zero_p(a) || ppf_inf_p(b)) {
        ppf_set_zero(rop, sign);
        return 0;
    }
    if (!ppf_regular_p(a) || !ppf_regular_p(b) ||
        rop->precision > UINT64_MAX - 2u)
        return arithmetic_error(rop, ctx);

    common_bits = a->precision > b->precision ? a->precision : b->precision;
    common_limbs = ppf_limbs_for_precision(common_bits);
    quotient_bits = rop->precision + 2u;
    if (!common_limbs || quotient_bits > (ppf_prec_t)SIZE_MAX)
        return arithmetic_error(rop, ctx);

    mark = ppf_arena_mark(ctx);
    /* First allocate the two normalized fractions, then size the quotient. */
    needed = 2u * common_limbs;
    memory = ppf_arena_alloc(ctx, needed * sizeof(*memory),
                             _Alignof(mp_limb_t));
    if (!memory) return arithmetic_memory_error(rop, ctx);
    aligned_a = memory;
    aligned_b = aligned_a + common_limbs;
    compare = ppf_compare_normalized_fractions(a, b, aligned_a, aligned_b,
                                               common_limbs, common_bits);
    shift = (size_t)quotient_bits - (compare >= 0 ? 1u : 0u);
    if ((size_t)common_bits > SIZE_MAX - shift) {
        ppf_arena_release(ctx, mark);
        return arithmetic_error(rop, ctx);
    }
    numerator_bits = (size_t)common_bits + shift;
    numerator_limbs = (numerator_bits + PPF_LIMB_BITS - 1u) / PPF_LIMB_BITS;
    if (numerator_limbs < common_limbs ||
        numerator_limbs > SIZE_MAX - common_limbs - 2u) {
        ppf_arena_release(ctx, mark);
        return arithmetic_error(rop, ctx);
    }
    quotient_limbs_capacity = numerator_limbs - common_limbs + 1u;
    needed = numerator_limbs + quotient_limbs_capacity + common_limbs;
    memory = ppf_arena_alloc(ctx, needed * sizeof(*memory),
                             _Alignof(mp_limb_t));
    if (!memory) {
        ppf_arena_release(ctx, mark);
        return arithmetic_memory_error(rop, ctx);
    }
    numerator = memory;
    quotient = numerator + numerator_limbs;
    remainder = quotient + quotient_limbs_capacity;
    ppf_copy_lshift_limbs(numerator, numerator_limbs,
                          aligned_a, common_limbs, shift);
    numerator_limbs = ppf_trim_limb_count(numerator, numerator_limbs);
    if (numerator_limbs < common_limbs) {
        ppf_arena_release(ctx, mark);
        return arithmetic_error(rop, ctx);
    }
    memset(quotient, 0, quotient_limbs_capacity * sizeof(*quotient));
    memset(remainder, 0, common_limbs * sizeof(*remainder));
    mpn_tdiv_qr(quotient, remainder, 0,
                numerator, (mp_size_t)numerator_limbs,
                aligned_b, (mp_size_t)common_limbs);
    quotient_limbs = ppf_trim_limb_count(quotient,
        numerator_limbs - common_limbs + 1u);
    if (ppf_limb_bit_length_local(quotient, quotient_limbs) !=
        (size_t)quotient_bits ||
        ppf_exp_add(a->exponent, -b->exponent, &base) != 0 ||
        shift > (size_t)INT64_MAX ||
        ppf_exp_add(base, -(ppf_exp_t)shift, &base) != 0) {
        ppf_arena_release(ctx, mark);
        return arithmetic_error(rop, ctx);
    }
    remainder_nonzero = ppf_limbs_any_nonzero(remainder, common_limbs);
    ternary = ppf_round_limbs_2exp(rop, quotient, quotient_limbs,
                                   sign, base, remainder_nonzero,
                                   rnd, ctx);
    ppf_arena_release(ctx, mark);
    return ternary;
}


int ppf_sqrt(ppf_t *rop, const ppf_t *a,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_arena_mark_t mark;
    ppf_exp_t exact_exponent, original_exponent, half_exponent, base;
    ppf_prec_t desired;
    size_t adjusted_bits, root_bits, scale, total_shift;
    size_t radicand_bits, radicand_limbs, root_limbs, root_used, needed;
    mp_ptr memory, radicand, root;
    mp_size_t remainder_size;
    int ternary;

    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !ctx || !ppf_rounding_valid(rnd))
        return arithmetic_error(rop, ctx);
    if (ppf_nan_p(a)) return invalid_result(rop, ctx, a->sign);
    if (ppf_zero_p(a)) { ppf_set_zero(rop, a->sign); return 0; }
    if (a->sign < 0) return invalid_result(rop, ctx, 1);
    if (ppf_inf_p(a)) { ppf_set_inf(rop, 1); return 0; }
    if (!ppf_regular_p(a) ||
        ppf_exp_sub_prec(a->exponent, a->precision, &exact_exponent) != 0 ||
        rop->precision > UINT64_MAX - PPF_GUARD_BITS)
        return arithmetic_error(rop, ctx);

    /* Make the exact dyadic exponent even by moving one bit into M. */
    original_exponent = exact_exponent;
    adjusted_bits = (size_t)a->precision + ((exact_exponent & 1) != 0);
    if (exact_exponent & 1) --exact_exponent;
    half_exponent = exact_exponent / 2;
    root_bits = (adjusted_bits + 1u) / 2u;
    desired = rop->precision + PPF_GUARD_BITS;
    scale = desired > root_bits ? (size_t)(desired - root_bits) : 0u;

    mark = ppf_arena_mark(ctx);
    for (unsigned attempt = 0; attempt < PPF_INTERVAL_ATTEMPTS; ++attempt) {
        if (scale > (SIZE_MAX - 1u) / 2u) break;
        total_shift = 2u * scale + (original_exponent != exact_exponent ? 1u : 0u);
        if ((size_t)a->precision > SIZE_MAX - total_shift) break;
        radicand_bits = (size_t)a->precision + total_shift;
        radicand_limbs = (radicand_bits + PPF_LIMB_BITS - 1u) /
                          PPF_LIMB_BITS;
        root_limbs = (radicand_limbs + 1u) / 2u;
        if (!radicand_limbs || !root_limbs ||
            radicand_limbs > SIZE_MAX - root_limbs) break;
        needed = radicand_limbs + root_limbs;
        memory = ppf_arena_alloc(ctx, needed * sizeof(*memory),
                                 _Alignof(mp_limb_t));
        if (!memory) break;
        radicand = memory;
        root = radicand + radicand_limbs;
        ppf_copy_lshift_limbs(radicand, radicand_limbs,
                              a->limbs, a->used_limbs, total_shift);
        radicand_limbs = ppf_trim_limb_count(radicand, radicand_limbs);
        memset(root, 0, root_limbs * sizeof(*root));
        remainder_size = mpn_sqrtrem(root, NULL, radicand,
                                     (mp_size_t)radicand_limbs);
        root_used = ppf_trim_limb_count(root, root_limbs);
        if (!root_used || scale > (size_t)INT64_MAX ||
            ppf_exp_add(half_exponent, -(ppf_exp_t)scale, &base) != 0) {
            ppf_arena_release(ctx, mark);
            return arithmetic_error(rop, ctx);
        }
        ternary = ppf_round_limbs_2exp(rop, root, root_used, 1,
                                       base, remainder_size != 0,
                                       rnd, ctx);
        if (ternary != PPF_TERNARY_ERROR) {
            ppf_arena_release(ctx, mark);
            return ternary;
        }
        ppf_arena_release(ctx, mark);
        mark = ppf_arena_mark(ctx);
        if (scale > SIZE_MAX - 64u) break;
        scale += 64u;
    }
    ppf_arena_release(ctx, mark);
    return arithmetic_error(rop, ctx);
}

#ifndef PPF_FMA_DIRECT_EXTRA_BITS
#define PPF_FMA_DIRECT_EXTRA_BITS 8192u
#endif

static int fused_product_add_exact_terms(
    ppf_t *rop, const ppf_t *a, const ppf_t *b,
    const ppf_t *c, int subtract_c,
    ppf_rnd_t rnd, ppf_context_t *ctx) {
    exact_term_t terms[2];
    int product_sign, csign, zero_sign, result;
    ppf_exp_t ea, eb;

    product_sign = a->sign * b->sign;
    csign = subtract_c ? -c->sign : c->sign;
    mpz_init(terms[0].z);
    mpz_init(terms[1].z);
    terms[0].e = 0;
    terms[1].e = 0;
    if (ppf_zero_p(a) || ppf_zero_p(b)) {
        mpz_set_ui(terms[0].z, 0);
    } else {
        mpz_t za, zb;
        mpz_roinit_n(za, a->limbs, (mp_size_t)a->used_limbs);
        mpz_roinit_n(zb, b->limbs, (mp_size_t)b->used_limbs);
        mpz_mul(terms[0].z, za, zb);
        if (product_sign < 0) mpz_neg(terms[0].z, terms[0].z);
        if (ppf_exp_sub_prec(a->exponent, a->precision, &ea) != 0 ||
            ppf_exp_sub_prec(b->exponent, b->precision, &eb) != 0 ||
            ppf_exp_add(ea, eb, &terms[0].e) != 0) {
            mpz_clear(terms[0].z);
            mpz_clear(terms[1].z);
            return arithmetic_error(rop, ctx);
        }
    }
    if (ppf_get_exact_dyadic(terms[1].z, &terms[1].e, c) != 0) {
        mpz_clear(terms[0].z);
        mpz_clear(terms[1].z);
        return arithmetic_error(rop, ctx);
    }
    if (subtract_c) mpz_neg(terms[1].z, terms[1].z);
    zero_sign = zero_sign_for_sum(product_sign, csign, rnd);
    result = round_terms(rop, terms, 2, zero_sign, rnd, ctx);
    mpz_clear(terms[0].z);
    mpz_clear(terms[1].z);
    return result;
}

/* Direct exact product-plus-addend accumulation.  This avoids constructing
   two general-purpose mpz_t terms and then realigning them a second time.
   Very large sparse phase gaps retain the interval-capable exact-term path. */
static int fused_product_add_direct(
    ppf_t *rop, const ppf_t *a, const ppf_t *b,
    const ppf_t *c, int subtract_c,
    ppf_rnd_t rnd, unsigned long workers,
    ppf_product_algorithm_t algorithm, ppf_context_t *ctx) {
    ppf_exp_t ea, eb, product_base, c_base, common;
    uint64_t product_shift64, c_shift64, maximum_shift;
    size_t na, nb, product_limbs, product_used;
    size_t product_bits, c_bits, result_bits, result_limbs;
    size_t needed;
    ppf_arena_mark_t mark;
    mp_ptr memory, product, product_aligned, c_aligned, result;
    int product_sign = a->sign * b->sign;
    int csign = subtract_c ? -c->sign : c->sign;
    int cmp, sign, ternary;
    mp_limb_t carry;

    if (ppf_zero_p(a) || ppf_zero_p(b)) {
        if (ppf_zero_p(c)) {
            ppf_set_zero(rop, zero_sign_for_sum(product_sign, csign, rnd));
            return 0;
        }
        if (subtract_c)
            return ppf_neg(rop, c, rnd, ctx);
        return ppf_set(rop, c, rnd, ctx);
    }
    if (ppf_zero_p(c))
        return product_algorithm_regular(
            rop, a, b, rnd, workers, algorithm,
            PPF_PRODUCT_OP_FMA, 0, ctx);
    if (!ppf_regular_p(a) || !ppf_regular_p(b) || !ppf_regular_p(c) ||
        ppf_exp_sub_prec(a->exponent, a->precision, &ea) != 0 ||
        ppf_exp_sub_prec(b->exponent, b->precision, &eb) != 0 ||
        ppf_exp_add(ea, eb, &product_base) != 0 ||
        ppf_exp_sub_prec(c->exponent, c->precision, &c_base) != 0)
        return PPF_TERNARY_ERROR;

    common = product_base < c_base ? product_base : c_base;
    product_shift64 = (uint64_t)product_base - (uint64_t)common;
    c_shift64 = (uint64_t)c_base - (uint64_t)common;
    maximum_shift = product_shift64 > c_shift64
                  ? product_shift64 : c_shift64;
    if (maximum_shift > (uint64_t)SIZE_MAX ||
        maximum_shift > (uint64_t)(a->precision + b->precision +
                                    c->precision + PPF_FMA_DIRECT_EXTRA_BITS))
        return PPF_TERNARY_ERROR;

    na = a->used_limbs;
    nb = b->used_limbs;
    if (!na || !nb || na > SIZE_MAX - nb) return PPF_TERNARY_ERROR;
    product_limbs = na + nb;
    product_bits = (size_t)a->precision + (size_t)b->precision;
    c_bits = (size_t)c->precision;
    if (product_bits > SIZE_MAX - (size_t)product_shift64 ||
        c_bits > SIZE_MAX - (size_t)c_shift64)
        return PPF_TERNARY_ERROR;
    product_bits += (size_t)product_shift64;
    c_bits += (size_t)c_shift64;
    result_bits = product_bits > c_bits ? product_bits : c_bits;
    if (result_bits > SIZE_MAX - 2u) return PPF_TERNARY_ERROR;
    result_bits += 2u;
    result_limbs = (result_bits + PPF_LIMB_BITS - 1u) / PPF_LIMB_BITS;
    if (!result_limbs || result_limbs > (SIZE_MAX - product_limbs) / 3u)
        return PPF_TERNARY_ERROR;
    needed = product_limbs + 3u * result_limbs;

    mark = ppf_arena_mark(ctx);
    memory = ppf_arena_alloc(ctx, needed * sizeof(*memory),
                             _Alignof(mp_limb_t));
    if (!memory) return PPF_TERNARY_ERROR;
    product = memory;
    product_aligned = product + product_limbs;
    c_aligned = product_aligned + result_limbs;
    result = c_aligned + result_limbs;
    if (exact_product_into(product, a, b, workers, algorithm,
                           PPF_PRODUCT_OP_FMA, ctx) != 0) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    product_used = addsub_trim(product, product_limbs);
    addsub_copy_lshift(product_aligned, result_limbs,
                       product, product_used, (size_t)product_shift64);
    addsub_copy_lshift(c_aligned, result_limbs,
                       c->limbs, c->used_limbs, (size_t)c_shift64);

    if (product_sign == csign) {
        carry = mpn_add_n(result, product_aligned, c_aligned,
                          (mp_size_t)result_limbs);
        if (carry) {
            ppf_arena_release(ctx, mark);
            return PPF_TERNARY_ERROR;
        }
        sign = product_sign;
    } else {
        cmp = mpn_cmp(product_aligned, c_aligned,
                      (mp_size_t)result_limbs);
        if (cmp == 0) {
            ppf_set_zero(rop, zero_sign_for_sum(product_sign, csign, rnd));
            ppf_arena_release(ctx, mark);
            return 0;
        }
        if (cmp > 0) {
            (void)mpn_sub_n(result, product_aligned, c_aligned,
                            (mp_size_t)result_limbs);
            sign = product_sign;
        } else {
            (void)mpn_sub_n(result, c_aligned, product_aligned,
                            (mp_size_t)result_limbs);
            sign = csign;
        }
    }
    result_limbs = addsub_trim(result, result_limbs);
    ternary = ppf_round_limbs_2exp(rop, result, result_limbs,
                                   sign, common, 0, rnd, ctx);
    ppf_arena_release(ctx, mark);
    return ternary;
}

static int fused_product_add(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                             const ppf_t *c, int subtract_c,
                             ppf_rnd_t rnd, unsigned long workers,
                             ppf_product_algorithm_t algorithm,
                             ppf_context_t *ctx) {
    int product_sign, csign, direct;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !b || !c || !ctx || !ppf_rounding_valid(rnd))
        return arithmetic_error(rop, ctx);
    if (ppf_nan_p(a) || ppf_nan_p(b) || ppf_nan_p(c))
        return invalid_result(rop, ctx, 1);
    if ((ppf_inf_p(a) && ppf_zero_p(b)) ||
        (ppf_inf_p(b) && ppf_zero_p(a)))
        return invalid_result(rop, ctx, 1);
    product_sign = a->sign * b->sign;
    csign = subtract_c ? -c->sign : c->sign;
    if (ppf_inf_p(a) || ppf_inf_p(b)) {
        if (ppf_inf_p(c) && product_sign != csign)
            return invalid_result(rop, ctx, 1);
        ppf_set_inf(rop, product_sign);
        return 0;
    }
    if (ppf_inf_p(c)) {
        ppf_set_inf(rop, csign);
        return 0;
    }

    direct = fused_product_add_direct(rop, a, b, c, subtract_c,
                                      rnd, workers ? workers : 1,
                                      algorithm, ctx);
    if (direct != PPF_TERNARY_ERROR) return direct;
    return fused_product_add_exact_terms(rop, a, b, c, subtract_c,
                                         rnd, ctx);
}

int ppf_fma_algorithm_threads(
    ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c,
    ppf_rnd_t rnd, unsigned long workers,
    ppf_product_algorithm_t algorithm, ppf_context_t *ctx) {
    if (algorithm < PPF_PRODUCT_AUTO || algorithm > PPF_PRODUCT_WINDOW)
        return arithmetic_error(rop, ppf_resolve_context(ctx));
    return fused_product_add(rop, a, b, c, 0, rnd,
                             workers ? workers : 1, algorithm, ctx);
}

int ppf_fma_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    const ppf_t *c, ppf_rnd_t rnd,
                    unsigned long workers, ppf_context_t *ctx) {
    return ppf_fma_algorithm_threads(rop, a, b, c, rnd, workers,
                                     PPF_PRODUCT_AUTO, ctx);
}

int ppf_fma(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            const ppf_t *c, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_fma_threads(rop, a, b, c, rnd, 1, ctx);
}

int ppf_fms_algorithm_threads(
    ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c,
    ppf_rnd_t rnd, unsigned long workers,
    ppf_product_algorithm_t algorithm, ppf_context_t *ctx) {
    if (algorithm < PPF_PRODUCT_AUTO || algorithm > PPF_PRODUCT_WINDOW)
        return arithmetic_error(rop, ppf_resolve_context(ctx));
    return fused_product_add(rop, a, b, c, 1, rnd,
                             workers ? workers : 1, algorithm, ctx);
}

int ppf_fms_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    const ppf_t *c, ppf_rnd_t rnd,
                    unsigned long workers, ppf_context_t *ctx) {
    return ppf_fms_algorithm_threads(rop, a, b, c, rnd, workers,
                                     PPF_PRODUCT_AUTO, ctx);
}

int ppf_fms(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            const ppf_t *c, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_fms_threads(rop, a, b, c, rnd, 1, ctx);
}

static int exact_product_term(exact_term_t *term,
                              const ppf_t *a, const ppf_t *b) {
    mpz_t za, zb;
    ppf_exp_t ea = 0, eb = 0;
    int rc = -1;
    mpz_inits(za, zb, NULL);
    if (ppf_get_exact_dyadic(za, &ea, a) != 0 ||
        ppf_get_exact_dyadic(zb, &eb, b) != 0 ||
        ppf_exp_add(ea, eb, &term->e) != 0)
        goto done;
    mpz_mul(term->z, za, zb);
    rc = 0;
done:
    mpz_clears(za, zb, NULL);
    return rc;
}

static int fused_two_products(ppf_t *rop,
                              const ppf_t *a, const ppf_t *b,
                              const ppf_t *c, const ppf_t *d,
                              int subtract_second,
                              ppf_rnd_t rnd, ppf_context_t *ctx) {
    exact_term_t terms[2];
    int sign1, sign2, inf1, inf2, t;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !b || !c || !d || !ctx || !ppf_rounding_valid(rnd))
        return arithmetic_error(rop, ctx);
    if (ppf_nan_p(a) || ppf_nan_p(b) || ppf_nan_p(c) || ppf_nan_p(d))
        return invalid_result(rop, ctx, 1);
    if ((ppf_inf_p(a) && ppf_zero_p(b)) ||
        (ppf_inf_p(b) && ppf_zero_p(a)) ||
        (ppf_inf_p(c) && ppf_zero_p(d)) ||
        (ppf_inf_p(d) && ppf_zero_p(c)))
        return invalid_result(rop, ctx, 1);

    sign1 = a->sign * b->sign;
    sign2 = c->sign * d->sign;
    if (subtract_second) sign2 = -sign2;
    inf1 = ppf_inf_p(a) || ppf_inf_p(b);
    inf2 = ppf_inf_p(c) || ppf_inf_p(d);
    if (inf1 || inf2) {
        if (inf1 && inf2 && sign1 != sign2)
            return invalid_result(rop, ctx, 1);
        ppf_set_inf(rop, inf1 ? sign1 : sign2);
        return 0;
    }

    mpz_init(terms[0].z);
    mpz_init(terms[1].z);
    terms[0].e = terms[1].e = 0;
    if (exact_product_term(&terms[0], a, b) != 0 ||
        exact_product_term(&terms[1], c, d) != 0) {
        mpz_clear(terms[0].z);
        mpz_clear(terms[1].z);
        return arithmetic_error(rop, ctx);
    }
    if (subtract_second) mpz_neg(terms[1].z, terms[1].z);
    t = round_terms(rop, terms, 2,
                    zero_sign_for_sum(sign1, sign2, rnd),
                    rnd, ctx);
    mpz_clear(terms[0].z);
    mpz_clear(terms[1].z);
    return t;
}

int ppf_fmma(ppf_t *rop, const ppf_t *a, const ppf_t *b,
             const ppf_t *c, const ppf_t *d,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return fused_two_products(rop, a, b, c, d, 0, rnd, ctx);
}

int ppf_fmms(ppf_t *rop, const ppf_t *a, const ppf_t *b,
             const ppf_t *c, const ppf_t *d,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return fused_two_products(rop, a, b, c, d, 1, rnd, ctx);
}

static int scale_pow2(ppf_t *rop, const ppf_t *a, long shift,
                      ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_exp_t base;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !ctx || !ppf_rounding_valid(rnd))
        return arithmetic_error(rop, ctx);
    if (ppf_nan_p(a)) return invalid_result(rop, ctx, a->sign);
    if (ppf_inf_p(a)) { ppf_set_inf(rop, a->sign); return 0; }
    if (ppf_zero_p(a)) { ppf_set_zero(rop, a->sign); return 0; }
    if (!ppf_regular_p(a)) return arithmetic_error(rop, ctx);
    if (rop->precision == a->precision) {
        ppf_exp_t exponent;
        if (ppf_exp_add(a->exponent, (ppf_exp_t)shift, &exponent) == 0 &&
            exponent >= ctx->emin && exponent <= ctx->emax) {
            if (rop != a) {
                size_t bytes = a->used_limbs * sizeof(*a->limbs);
                if (ppf_reserve(rop, rop->precision) != 0)
                    return arithmetic_error(rop, ctx);
                memmove(rop->limbs, a->limbs, bytes);
                rop->used_limbs = a->used_limbs;
                rop->precision = a->precision;
                rop->sign = a->sign;
                rop->kind = PPF_KIND_REGULAR;
            }
            rop->exponent = exponent;
            return 0;
        }
    }
    if (ppf_exp_sub_prec(a->exponent, a->precision, &base) != 0 ||
        ppf_exp_add(base, (ppf_exp_t)shift, &base) != 0)
        return arithmetic_error(rop, ctx);
    return ppf_round_limbs_2exp(rop, a->limbs, a->used_limbs,
                                a->sign, base, 0, rnd, ctx);
}

int ppf_mul_2si(ppf_t *rop, const ppf_t *a, long shift,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    return scale_pow2(rop, a, shift, rnd, ctx);
}

int ppf_div_2si(ppf_t *rop, const ppf_t *a, long shift,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    if (shift == LONG_MIN) return arithmetic_error(rop, ctx);
    return scale_pow2(rop, a, -shift, rnd, ctx);
}

int ppf_sum(ppf_t *rop, const ppf_t *const values[], size_t length,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    exact_term_t *terms;
    int has_pos_inf = 0, has_neg_inf = 0, all_zero = 1;
    int all_zero_sign = 0, result;
    size_t i;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || (!values && length) || !ctx || !ppf_rounding_valid(rnd))
        return arithmetic_error(rop, ctx);
    if (!length) { ppf_set_zero(rop, 1); return 0; }
    terms = ppf_context_calloc_internal(ctx, length, sizeof(*terms));
    if (!terms) return ppf_allocation_error(rop, ctx);
    for (i = 0; i < length; ++i) {
        if (!values[i]) { ppf_context_free_internal(ctx, terms, length * sizeof(*terms)); return arithmetic_error(rop, ctx); }
        if (ppf_nan_p(values[i])) {
            ppf_context_free_internal(ctx, terms, length * sizeof(*terms)); return invalid_result(rop, ctx, values[i]->sign);
        }
        if (ppf_inf_p(values[i])) {
            if (values[i]->sign > 0) has_pos_inf = 1;
            else has_neg_inf = 1;
        }
        if (!ppf_zero_p(values[i])) all_zero = 0;
        else if (i == 0) all_zero_sign = values[i]->sign;
        else if (all_zero_sign != values[i]->sign) all_zero_sign = 0;
    }
    if (has_pos_inf && has_neg_inf) {
        ppf_context_free_internal(ctx, terms, length * sizeof(*terms)); return invalid_result(rop, ctx, 1);
    }
    if (has_pos_inf || has_neg_inf) {
        ppf_set_inf(rop, has_pos_inf ? 1 : -1); ppf_context_free_internal(ctx, terms, length * sizeof(*terms)); return 0;
    }
    if (all_zero) {
        int sign = all_zero_sign ? all_zero_sign : (rnd == PPF_RNDD ? -1 : 1);
        ppf_set_zero(rop, sign); ppf_context_free_internal(ctx, terms, length * sizeof(*terms)); return 0;
    }
    for (i = 0; i < length; ++i) {
        mpz_init(terms[i].z);
        if (ppf_get_exact_dyadic(terms[i].z, &terms[i].e, values[i]) != 0) {
            for (size_t j = 0; j <= i; ++j) mpz_clear(terms[j].z);
            ppf_context_free_internal(ctx, terms, length * sizeof(*terms)); return arithmetic_error(rop, ctx);
        }
    }
    result = round_terms(rop, terms, length,
                         rnd == PPF_RNDD ? -1 : 1, rnd, ctx);
    for (i = 0; i < length; ++i) mpz_clear(terms[i].z);
    ppf_context_free_internal(ctx, terms, length * sizeof(*terms));
    return result;
}

int ppf_dot_threads(ppf_t *rop, const ppf_t *const a[],
                    const ppf_t *const b[], size_t length,
                    ppf_rnd_t rnd, unsigned long workers,
                    ppf_context_t *ctx) {
#ifndef _OPENMP
    (void)workers;
#endif
    exact_term_t *terms;
    size_t i;
    int result, has_pos_inf = 0, has_neg_inf = 0;
    int all_zero = 1, zero_product_sign = 0, error = 0;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || (!a && length) || (!b && length) || !ctx ||
        !ppf_rounding_valid(rnd)) return arithmetic_error(rop, ctx);
    if (!length) { ppf_set_zero(rop, 1); return 0; }

    for (i = 0; i < length; ++i) {
        int product_sign;
        if (!a[i] || !b[i]) return arithmetic_error(rop, ctx);
        if (ppf_nan_p(a[i]) || ppf_nan_p(b[i]) ||
            (ppf_inf_p(a[i]) && ppf_zero_p(b[i])) ||
            (ppf_inf_p(b[i]) && ppf_zero_p(a[i])))
            return invalid_result(rop, ctx, 1);
        product_sign = a[i]->sign * b[i]->sign;
        if (ppf_inf_p(a[i]) || ppf_inf_p(b[i])) {
            if (product_sign > 0) has_pos_inf = 1;
            else has_neg_inf = 1;
        }
        if (!(ppf_zero_p(a[i]) || ppf_zero_p(b[i]))) all_zero = 0;
        else if (zero_product_sign == 0) zero_product_sign = product_sign;
        else if (zero_product_sign != product_sign) zero_product_sign = 2;
    }
    if (has_pos_inf && has_neg_inf) return invalid_result(rop, ctx, 1);
    if (has_pos_inf || has_neg_inf) {
        ppf_set_inf(rop, has_pos_inf ? 1 : -1);
        return 0;
    }
    if (all_zero) {
        int sign = zero_product_sign == 1 || zero_product_sign == -1
                 ? zero_product_sign : (rnd == PPF_RNDD ? -1 : 1);
        ppf_set_zero(rop, sign);
        return 0;
    }

    terms = ppf_context_calloc_internal(ctx, length, sizeof(*terms));
    if (!terms) return ppf_allocation_error(rop, ctx);
    for (i = 0; i < length; ++i) mpz_init(terms[i].z);

#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel for num_threads(workers > 1 ? (int)workers : 1) reduction(|:error))
#endif
    for (i = 0; i < length; ++i) {
        mpz_t za, zb;
        ppf_exp_t ea, eb;
        if (ppf_zero_p(a[i]) || ppf_zero_p(b[i])) {
            mpz_set_ui(terms[i].z, 0); terms[i].e = 0; continue;
        }
        /* Read-only GMP views avoid copying both full input significands for
           every dot-product leaf.  Only the exact product owns storage. */
        mpz_roinit_n(za, a[i]->limbs, (mp_size_t)a[i]->used_limbs);
        mpz_roinit_n(zb, b[i]->limbs, (mp_size_t)b[i]->used_limbs);
        mpz_mul(terms[i].z, za, zb);
        if (a[i]->sign * b[i]->sign < 0)
            mpz_neg(terms[i].z, terms[i].z);
        if (ppf_exp_sub_prec(a[i]->exponent, a[i]->precision, &ea) != 0 ||
            ppf_exp_sub_prec(b[i]->exponent, b[i]->precision, &eb) != 0 ||
            ppf_exp_add(ea, eb, &terms[i].e) != 0) error = 1;
    }
    if (error) {
        for (i = 0; i < length; ++i) mpz_clear(terms[i].z);
        ppf_context_free_internal(ctx, terms, length * sizeof(*terms)); return arithmetic_error(rop, ctx);
    }
    result = round_terms(rop, terms, length,
                         rnd == PPF_RNDD ? -1 : 1, rnd, ctx);
    for (i = 0; i < length; ++i) mpz_clear(terms[i].z);
    ppf_context_free_internal(ctx, terms, length * sizeof(*terms));
    return result;
}

int ppf_dot(ppf_t *rop, const ppf_t *const a[],
            const ppf_t *const b[], size_t length,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_dot_threads(rop, a, b, length, rnd, 1, ctx);
}
