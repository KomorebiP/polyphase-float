/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "ppf_openmp_compat.h"

#ifndef PPF_PLAN_QUOTIENT_EXTRA_BITS
#define PPF_PLAN_QUOTIENT_EXTRA_BITS 2u
#endif

#ifndef PPF_PLAN_RECIPROCAL_MIN_BITS
#define PPF_PLAN_RECIPROCAL_MIN_BITS 262144u
#endif
#ifndef PPF_PLAN_RECIPROCAL_MAX_BITS
#define PPF_PLAN_RECIPROCAL_MAX_BITS 2097152u
#endif

struct ppf_divisor_plan {
    mp_limb_t *denominator;
    mp_limb_t *reciprocal;
    size_t denominator_limbs;
    size_t reciprocal_limbs;
    ppf_prec_t denominator_precision;
    ppf_prec_t destination_precision;
    ppf_prec_t quotient_bits;
    ppf_exp_t denominator_exponent;
    int denominator_sign;
    mp_bitcnt_t reciprocal_scale_bits;
    int reciprocal_ready;
    ppf_allocator_t allocator;
    atomic_ulong applications;
};

static int plan_error(ppf_t *rop, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, 1);
    if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

static int plan_memory_or_error(ppf_t *rop, ppf_context_t *ctx) {
    if (ctx && (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0)
        return ppf_allocation_error(rop, ctx);
    return plan_error(rop, ctx);
}

static int plan_invalid(ppf_t *rop, int sign, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, sign);
    if (ctx) ppf_raise(ctx, PPF_FLAG_NAN);
    return 0;
}

static size_t trim_limbs(mp_srcptr value, size_t used) {
    while (used && value[used - 1] == 0) --used;
    return used;
}

static size_t limb_bit_length(mp_srcptr value, size_t used) {
    mp_limb_t top;
    unsigned leading;
    used = trim_limbs(value, used);
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

static int limbs_nonzero(mp_srcptr value, size_t used) {
    for (size_t i = 0; i < used; ++i)
        if (value[i] != 0) return 1;
    return 0;
}

static void copy_lshift(mp_ptr out, size_t out_size,
                        mp_srcptr value, size_t used, size_t shift) {
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
                             mp_srcptr value, size_t used, size_t shift) {
    for (size_t i = 0; i < out_size; ++i)
        out[i] = shifted_limb(value, used,
                              shift + i * (size_t)PPF_LIMB_BITS);
}

static int compare_variable(mp_srcptr a, size_t an,
                            mp_srcptr b, size_t bn) {
    an = trim_limbs(a, an);
    bn = trim_limbs(b, bn);
    if (an != bn) return an > bn ? 1 : -1;
    return an ? mpn_cmp(a, b, (mp_size_t)an) : 0;
}

static int subtract_variable(mp_ptr target, size_t target_size,
                             mp_srcptr subtract, size_t subtract_size) {
    mp_limb_t borrow;
    if (!subtract_size) return 0;
    if (subtract_size > target_size) return -1;
    borrow = mpn_sub_n(target, target, subtract,
                       (mp_size_t)subtract_size);
    if (borrow && target_size > subtract_size)
        borrow = mpn_sub_1(target + subtract_size,
                           target + subtract_size,
                           (mp_size_t)(target_size - subtract_size),
                           borrow);
    return borrow ? -1 : 0;
}

static int compare_normalized(const ppf_t *a,
                              const ppf_divisor_plan_t *plan,
                              mp_ptr aligned_a, mp_ptr aligned_d,
                              size_t common_limbs,
                              ppf_prec_t common_bits) {
    size_t shift_a = (size_t)(common_bits - a->precision);
    size_t shift_d = (size_t)(common_bits - plan->denominator_precision);
    copy_lshift(aligned_a, common_limbs,
                a->limbs, a->used_limbs, shift_a);
    copy_lshift(aligned_d, common_limbs,
                plan->denominator, plan->denominator_limbs, shift_d);
    return mpn_cmp(aligned_a, aligned_d, (mp_size_t)common_limbs);
}

static int plan_build_reciprocal(ppf_divisor_plan_t *plan, ppf_context_t *ctx) {
    mpz_t denominator, power, reciprocal;
    size_t count = 0, capacity;
    uint64_t scale;
    if (!plan || !plan->denominator || !plan->denominator_limbs)
        return -1;
    if (plan->denominator_precision > UINT64_MAX - plan->quotient_bits)
        return -1;
    scale = plan->denominator_precision + plan->quotient_bits;
    if (scale > ULONG_MAX) return -1;
    plan->reciprocal_scale_bits = (mp_bitcnt_t)scale;
    mpz_inits(denominator, power, reciprocal, NULL);
    mpz_import(denominator, plan->denominator_limbs, -1,
               sizeof(mp_limb_t), 0, 0, plan->denominator);
    mpz_set_ui(power, 0);
    mpz_setbit(power, plan->reciprocal_scale_bits);
    mpz_fdiv_q(reciprocal, power, denominator);
    capacity = ppf_limbs_for_precision(plan->quotient_bits + 1u);
    if (!capacity) {
        mpz_clears(denominator, power, reciprocal, NULL);
        return -1;
    }
    plan->reciprocal = ppf_context_malloc_internal(
        ctx, capacity * sizeof(mp_limb_t));
    if (!plan->reciprocal) {
        mpz_clears(denominator, power, reciprocal, NULL);
        return -1;
    }
    memset(plan->reciprocal, 0, capacity * sizeof(mp_limb_t));
    mpz_export(plan->reciprocal, &count, -1,
               sizeof(mp_limb_t), 0, 0, reciprocal);
    mpz_clears(denominator, power, reciprocal, NULL);
    if (!count || count > capacity) return -1;
    plan->reciprocal_limbs = count;
    plan->reciprocal_ready = 1;
    return 0;
}

int ppf_divisor_plan_create_ex(
    ppf_divisor_plan_t **out, const ppf_t *denominator,
    ppf_prec_t destination_precision, unsigned long expected_applications,
    ppf_context_t *ctx) {
    ppf_divisor_plan_t *plan;
    size_t bytes;
    ctx = ppf_resolve_context(ctx);
    if (!out || !denominator || !ctx ||
        !ppf_precision_valid(destination_precision)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return -1;
    }
    *out = NULL;
    if (!ppf_regular_p(denominator) || ppf_zero_p(denominator) ||
        destination_precision > UINT64_MAX - PPF_PLAN_QUOTIENT_EXTRA_BITS) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return -1;
    }
    plan = ppf_context_malloc_internal(ctx, sizeof(*plan));
    if (!plan) return -1;
    memset(plan, 0, sizeof(*plan));
    plan->allocator = ctx->allocator;
    plan->denominator_limbs = denominator->used_limbs;
    bytes = plan->denominator_limbs * sizeof(mp_limb_t);
    plan->denominator = ppf_context_malloc_internal(ctx, bytes);
    if (!plan->denominator) {
        plan->allocator.free_fn(plan->allocator.opaque, plan, sizeof(*plan));
        return -1;
    }
    memcpy(plan->denominator, denominator->limbs, bytes);
    plan->denominator_precision = denominator->precision;
    plan->destination_precision = destination_precision;
    plan->quotient_bits = destination_precision + PPF_PLAN_QUOTIENT_EXTRA_BITS;
    plan->denominator_exponent = denominator->exponent;
    plan->denominator_sign = denominator->sign;
    atomic_init(&plan->applications, 0UL);
    /* The portable reciprocal plan has a measured middle-precision
       crossover.  Outside that range the direct quotient is both cheaper to
       construct and faster to apply, so the plan deliberately stays light. */
    if (expected_applications >= 2UL &&
        destination_precision >= PPF_PLAN_RECIPROCAL_MIN_BITS &&
        destination_precision <= PPF_PLAN_RECIPROCAL_MAX_BITS &&
        plan_build_reciprocal(plan, ctx) != 0) {
        int allocation_failed =
            (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0;
        ppf_divisor_plan_destroy(plan);
        if (!allocation_failed) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return -1;
    }
    *out = plan;
    return 0;
}

int ppf_divisor_plan_create(
    ppf_divisor_plan_t **out, const ppf_t *denominator,
    ppf_prec_t destination_precision, ppf_context_t *ctx) {
    return ppf_divisor_plan_create_ex(out, denominator,
                                      destination_precision, 8UL, ctx);
}

void ppf_divisor_plan_destroy(ppf_divisor_plan_t *plan) {
    if (!plan) return;
    if (plan->denominator)
        plan->allocator.free_fn(plan->allocator.opaque, plan->denominator,
            plan->denominator_limbs * sizeof(mp_limb_t));
    if (plan->reciprocal)
        plan->allocator.free_fn(plan->allocator.opaque, plan->reciprocal,
            ppf_limbs_for_precision(plan->quotient_bits + 1u)
            * sizeof(mp_limb_t));
    plan->allocator.free_fn(plan->allocator.opaque, plan, sizeof(*plan));
}

ppf_prec_t ppf_divisor_plan_precision(const ppf_divisor_plan_t *plan) {
    return plan ? plan->destination_precision : 0;
}

unsigned long ppf_divisor_plan_applications(
    const ppf_divisor_plan_t *plan) {
    return plan ? atomic_load_explicit(
        (const atomic_ulong *)&plan->applications, memory_order_relaxed) : 0UL;
}

static int planned_divide_reciprocal(ppf_t *rop, const ppf_t *numerator_value,
                                     const ppf_divisor_plan_t *plan,
                                     ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_arena_mark_t mark;
    ppf_prec_t common_bits;
    size_t common_limbs, scale_shift, scaled_bits, scaled_limbs;
    size_t product_limbs, q_capacity, q_limbs, qd_limbs, rem_limbs;
    size_t needed, high_shift;
    mp_ptr mem, aligned_n, aligned_d, scaled, estimate_product;
    mp_ptr quotient, qd, remainder;
    int compare, sign, correction = 0, sticky, ternary;
    ppf_exp_t base;

    common_bits = numerator_value->precision > plan->denominator_precision
        ? numerator_value->precision : plan->denominator_precision;
    common_limbs = ppf_limbs_for_precision(common_bits);
    if (!common_limbs) return PPF_TERNARY_ERROR;
    mark = ppf_arena_mark(ctx);
    needed = 2u * common_limbs;
    mem = ppf_arena_alloc(ctx, needed * sizeof(*mem),
                          _Alignof(mp_limb_t));
    if (!mem) return PPF_TERNARY_ERROR;
    aligned_n = mem;
    aligned_d = aligned_n + common_limbs;
    compare = compare_normalized(numerator_value, plan,
                                 aligned_n, aligned_d,
                                 common_limbs, common_bits);
    /* q=floor((normalized numerator/denominator)*2^scale_shift). */
    if ((size_t)plan->quotient_bits < (compare >= 0 ? 1u : 0u)) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    scale_shift = (size_t)plan->quotient_bits - (compare >= 0 ? 1u : 0u);
    if (common_bits > (ppf_prec_t)SIZE_MAX - scale_shift) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    scaled_bits = (size_t)common_bits + scale_shift;
    scaled_limbs = (scaled_bits + PPF_LIMB_BITS - 1u) / PPF_LIMB_BITS;
    product_limbs = scaled_limbs + plan->reciprocal_limbs;
    q_capacity = ppf_limbs_for_precision(plan->quotient_bits + 1u);
    qd_limbs = q_capacity + common_limbs;
    rem_limbs = scaled_limbs > qd_limbs ? scaled_limbs : qd_limbs;
    if (!q_capacity || scaled_limbs > SIZE_MAX - product_limbs ||
        q_capacity > SIZE_MAX - (scaled_limbs + product_limbs) ||
        qd_limbs > SIZE_MAX - (scaled_limbs + product_limbs + q_capacity) ||
        rem_limbs > SIZE_MAX - (scaled_limbs + product_limbs + q_capacity + qd_limbs)) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    needed = scaled_limbs + product_limbs + q_capacity + qd_limbs + rem_limbs;
    mem = ppf_arena_alloc(ctx, needed * sizeof(*mem),
                          _Alignof(mp_limb_t));
    if (!mem) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    scaled = mem;
    estimate_product = scaled + scaled_limbs;
    quotient = estimate_product + product_limbs;
    qd = quotient + q_capacity;
    remainder = qd + qd_limbs;
    copy_lshift(scaled, scaled_limbs, aligned_n, common_limbs, scale_shift);
    scaled_limbs = trim_limbs(scaled, scaled_limbs);
    memset(estimate_product, 0, product_limbs * sizeof(*estimate_product));
    if (scaled_limbs >= plan->reciprocal_limbs)
        mpn_mul(estimate_product, scaled, (mp_size_t)scaled_limbs,
                plan->reciprocal, (mp_size_t)plan->reciprocal_limbs);
    else
        mpn_mul(estimate_product, plan->reciprocal,
                (mp_size_t)plan->reciprocal_limbs,
                scaled, (mp_size_t)scaled_limbs);
    product_limbs = trim_limbs(estimate_product, product_limbs);
    high_shift = (size_t)plan->reciprocal_scale_bits;
    memset(quotient, 0, q_capacity * sizeof(*quotient));
    copy_right_shift(quotient, q_capacity,
                     estimate_product, product_limbs, high_shift);
    q_limbs = trim_limbs(quotient, q_capacity);
    if (!q_limbs) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    memset(qd, 0, qd_limbs * sizeof(*qd));
    if (q_limbs >= common_limbs)
        mpn_mul(qd, quotient, (mp_size_t)q_limbs,
                aligned_d, (mp_size_t)common_limbs);
    else
        mpn_mul(qd, aligned_d, (mp_size_t)common_limbs,
                quotient, (mp_size_t)q_limbs);
    qd_limbs = trim_limbs(qd, q_limbs + common_limbs);
    if (compare_variable(qd, qd_limbs, scaled, scaled_limbs) > 0) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    memset(remainder, 0, rem_limbs * sizeof(*remainder));
    memcpy(remainder, scaled, scaled_limbs * sizeof(*remainder));
    if (subtract_variable(remainder, rem_limbs, qd, qd_limbs) != 0) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    rem_limbs = trim_limbs(remainder, rem_limbs);
    if (compare_variable(remainder, rem_limbs,
                         aligned_d, common_limbs) >= 0) {
        if (subtract_variable(remainder, rem_limbs ? rem_limbs : common_limbs,
                              aligned_d, common_limbs) != 0 ||
            mpn_add_1(quotient, quotient, (mp_size_t)q_capacity, 1) != 0) {
            ppf_arena_release(ctx, mark);
            return PPF_TERNARY_ERROR;
        }
        correction = 1;
        rem_limbs = trim_limbs(remainder,
            rem_limbs ? rem_limbs : common_limbs);
    }
    if (compare_variable(remainder, rem_limbs,
                         aligned_d, common_limbs) >= 0) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    (void)correction;
    q_limbs = trim_limbs(quotient, q_capacity);
    if (limb_bit_length(quotient, q_limbs) !=
        (size_t)plan->quotient_bits ||
        ppf_exp_add(numerator_value->exponent,
                    -plan->denominator_exponent, &base) != 0 ||
        scale_shift > (size_t)INT64_MAX ||
        ppf_exp_add(base, -(ppf_exp_t)scale_shift, &base) != 0) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    sign = numerator_value->sign * plan->denominator_sign;
    sticky = limbs_nonzero(remainder, rem_limbs);
    ternary = ppf_round_limbs_2exp(rop, quotient, q_limbs,
                                   sign, base, sticky, rnd, ctx);
    ppf_arena_release(ctx, mark);
    return ternary;
}

int ppf_div_with_plan(ppf_t *rop, const ppf_t *numerator,
                      const ppf_divisor_plan_t *plan,
                      ppf_rnd_t rnd, ppf_context_t *ctx) {
    int rc;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !numerator || !plan || !ctx ||
        !ppf_rounding_valid(rnd) ||
        rop->precision != plan->destination_precision)
        return plan_error(rop, ctx);
    if (ppf_nan_p(numerator)) return plan_invalid(rop, numerator->sign, ctx);
    if (ppf_inf_p(numerator)) {
        ppf_set_inf(rop, numerator->sign * plan->denominator_sign);
        atomic_fetch_add_explicit((atomic_ulong *)&plan->applications, 1UL,
                                  memory_order_relaxed);
        return 0;
    }
    if (ppf_zero_p(numerator)) {
        ppf_set_zero(rop, numerator->sign * plan->denominator_sign);
        atomic_fetch_add_explicit((atomic_ulong *)&plan->applications, 1UL,
                                  memory_order_relaxed);
        return 0;
    }
    if (!plan->reciprocal_ready) {
        /* A lightweight plan still remains fully native. */
        ppf_t denominator;
        if (ppf_init2_ctx(&denominator, plan->denominator_precision, ctx) != 0)
            return plan_memory_or_error(rop, ctx);
        memcpy(denominator.limbs, plan->denominator,
               plan->denominator_limbs * sizeof(mp_limb_t));
        denominator.used_limbs = plan->denominator_limbs;
        denominator.kind = PPF_KIND_REGULAR;
        denominator.sign = plan->denominator_sign;
        denominator.exponent = plan->denominator_exponent;
        rc = ppf_div(rop, numerator, &denominator, rnd, ctx);
        ppf_clear(&denominator);
    } else {
        rc = planned_divide_reciprocal(rop, numerator, plan, rnd, ctx);
        if (rc == PPF_TERNARY_ERROR) {
            /* The exact direct quotient is the native safety path. */
            ppf_t denominator;
            if (ppf_init2_ctx(&denominator, plan->denominator_precision, ctx) != 0)
                return plan_memory_or_error(rop, ctx);
            memcpy(denominator.limbs, plan->denominator,
                   plan->denominator_limbs * sizeof(mp_limb_t));
            denominator.used_limbs = plan->denominator_limbs;
            denominator.kind = PPF_KIND_REGULAR;
            denominator.sign = plan->denominator_sign;
            denominator.exponent = plan->denominator_exponent;
            rc = ppf_div(rop, numerator, &denominator, rnd, ctx);
            ppf_clear(&denominator);
        }
    }
    if (rc != PPF_TERNARY_ERROR)
        atomic_fetch_add_explicit((atomic_ulong *)&plan->applications, 1UL,
                                  memory_order_relaxed);
    return rc;
}

static int batch_alias_valid(ppf_t *outputs[],
                             const ppf_t *const numerators[], size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (!outputs[i] || !numerators[i]) return 0;
        for (size_t j = 0; j < length; ++j) {
            if (i != j && outputs[i] == numerators[j]) return 0;
            if (i != j && outputs[i] == outputs[j]) return 0;
        }
    }
    return 1;
}

int ppf_div_with_plan_many(
    ppf_t *outputs[], const ppf_t *const numerators[], size_t length,
    const ppf_divisor_plan_t *plan, ppf_rnd_t rnd, unsigned long workers,
    int ternaries[], ppf_context_t *ctx) {
    int failed = 0;
    ppf_flags_t combined_flags = 0;
    ctx = ppf_resolve_context(ctx);
    if (!ctx || !plan || (!outputs && length) || (!numerators && length) ||
        !batch_alias_valid(outputs, numerators, length)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return -1;
    }
    if (length == 0) return 0;
    if (workers == 0) workers = 1;
#ifndef _OPENMP
    workers = 1;
#endif
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel num_threads((int)workers) reduction(|:failed) reduction(|:combined_flags))
#endif
    {
        ppf_context_t local;
        int local_ok = ppf_context_clone(&local, ctx) == 0;
        if (local_ok) local.flags = 0;
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp for schedule(static))
#endif
        for (size_t i = 0; i < length; ++i) {
            int value = local_ok
                ? ppf_div_with_plan(outputs[i], numerators[i], plan,
                                    rnd, &local)
                : PPF_TERNARY_ERROR;
            if (ternaries) ternaries[i] = value;
            if (value == PPF_TERNARY_ERROR) failed = 1;
        }
        if (local_ok) {
            combined_flags |= ppf_context_flags(&local);
            ppf_context_clear(&local);
        } else failed = 1;
    }
    ppf_context_flags_set(ctx, combined_flags);
    return failed ? -1 : 0;
}
