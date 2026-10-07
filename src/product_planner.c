/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <stddef.h>

#ifndef PPF_MUL_PARALLEL_MIN_LIMBS
#define PPF_MUL_PARALLEL_MIN_LIMBS 1536u
#endif
#ifndef PPF_TOOM3_AUTO_SQR_MIN_LIMBS
#define PPF_TOOM3_AUTO_SQR_MIN_LIMBS 8192u
#endif
#ifndef PPF_RECSQRT_PROFILE_MIN_BITS
#define PPF_RECSQRT_PROFILE_MIN_BITS 32768u
#endif

typedef struct {
    int operation;
    ppf_prec_t min_precision;
    ppf_prec_t max_precision;
    size_t min_limbs;
    size_t max_limbs;
    unsigned long min_workers;
    unsigned long max_workers;
    ppf_product_algorithm_t algorithm;
} ppf_product_profile_rule_t;

#include <ppf_product_profile_build.h>

static int operation_valid(ppf_product_operation_t operation) {
    return operation >= PPF_PRODUCT_OP_MULTIPLY &&
           operation <= PPF_PRODUCT_OP_REC_SQRT;
}

static unsigned long effective_workers_for(
    ppf_product_algorithm_t algorithm,
    ppf_product_operation_t operation,
    unsigned long requested_workers) {
    unsigned long workers = requested_workers ? requested_workers : 1u;
    switch (algorithm) {
        case PPF_PRODUCT_FULL:
            return 1u;
        case PPF_PRODUCT_KARATSUBA3:
            return workers >= 3u ? 3u : 1u;
        case PPF_PRODUCT_TOOM3:
            return workers < 5u ? workers : 5u;
        case PPF_PRODUCT_WINDOW:
            if (operation == PPF_PRODUCT_OP_SQUARE)
                return workers < 2u ? workers : 2u;
            return workers < 3u ? workers : 3u;
        case PPF_PRODUCT_AUTO:
        default:
            return workers;
    }
}

static int balanced_product_inputs(
    const ppf_t *a, const ppf_t *b,
    ppf_product_operation_t operation) {
    if (!a) return 0;
    if (operation == PPF_PRODUCT_OP_REC_SQRT) return 1;
    if (operation == PPF_PRODUCT_OP_SQUARE) {
        if (!b) b = a;
    }
    return b && a->used_limbs == b->used_limbs &&
           a->precision == b->precision;
}

static ppf_product_algorithm_t portable_algorithm(
    const ppf_t *a, const ppf_t *b,
    ppf_product_operation_t operation,
    unsigned long workers) {
    size_t n;
    if (!balanced_product_inputs(a, b, operation))
        return PPF_PRODUCT_FULL;
    n = a->used_limbs;
    if (operation == PPF_PRODUCT_OP_REC_SQRT) {
        if (workers >= 3u && a->precision >= PPF_RECSQRT_PROFILE_MIN_BITS)
            return PPF_PRODUCT_KARATSUBA3;
        return PPF_PRODUCT_FULL;
    }
    if (operation == PPF_PRODUCT_OP_SQUARE && workers >= 5u &&
        n >= PPF_TOOM3_AUTO_SQR_MIN_LIMBS)
        return PPF_PRODUCT_TOOM3;
    if (workers >= 3u && n >= PPF_MUL_PARALLEL_MIN_LIMBS)
        return PPF_PRODUCT_KARATSUBA3;
    return PPF_PRODUCT_FULL;
}

static int profile_lookup(
    const ppf_t *a, const ppf_t *b,
    ppf_product_operation_t operation,
    unsigned long workers,
    ppf_product_algorithm_t *algorithm) {
    size_t limbs;
    ppf_prec_t precision;
#if PPF_PRODUCT_PROFILE_RULE_COUNT > 0
    size_t i;
#endif
    if (!algorithm || !balanced_product_inputs(a, b, operation)) return 0;
    limbs = a->used_limbs;
    precision = a->precision;
#if PPF_PRODUCT_PROFILE_RULE_COUNT > 0
    for (i = 0; i < (size_t)PPF_PRODUCT_PROFILE_RULE_COUNT; ++i) {
        const ppf_product_profile_rule_t *rule = &ppf_product_profile_rules[i];
        if (rule->operation != (int)operation ||
            precision < rule->min_precision ||
            precision > rule->max_precision ||
            limbs < rule->min_limbs || limbs > rule->max_limbs ||
            workers < rule->min_workers || workers > rule->max_workers)
            continue;
        if (rule->algorithm < PPF_PRODUCT_FULL ||
            rule->algorithm > PPF_PRODUCT_TOOM3)
            continue;
        *algorithm = rule->algorithm;
        return 1;
    }
#else
    (void)limbs;
    (void)precision;
    (void)operation;
    (void)workers;
#endif
    return 0;
}

int ppf_product_plan_internal(
    ppf_product_plan_t *plan, const ppf_t *a, const ppf_t *b,
    ppf_product_operation_t operation, unsigned long workers,
    ppf_product_algorithm_t requested) {
    ppf_product_algorithm_t algorithm;
    int matched = 0;
    if (!plan || !operation_valid(operation) ||
        requested < PPF_PRODUCT_AUTO || requested > PPF_PRODUCT_WINDOW)
        return -1;
    if (!workers) workers = 1u;
    if (requested == PPF_PRODUCT_AUTO) {
        matched = profile_lookup(a, b, operation, workers, &algorithm);
        if (!matched)
            algorithm = portable_algorithm(a, b, operation, workers);
    } else {
        algorithm = requested;
    }
    plan->algorithm = algorithm;
    plan->requested_workers = workers;
    plan->effective_workers = effective_workers_for(
        algorithm, operation, workers);
    plan->profile_rule_matched = matched;
    return 0;
}

int ppf_plan_product(
    ppf_product_plan_t *plan, const ppf_t *a, const ppf_t *b,
    ppf_product_operation_t operation, unsigned long workers) {
    return ppf_product_plan_internal(plan, a, b, operation, workers,
                                     PPF_PRODUCT_AUTO);
}

ppf_product_algorithm_t ppf_plan_product_algorithm(
    const ppf_t *a, const ppf_t *b, int square,
    unsigned long workers) {
    ppf_product_plan_t plan;
    ppf_product_operation_t operation = square
        ? PPF_PRODUCT_OP_SQUARE : PPF_PRODUCT_OP_MULTIPLY;
    if (ppf_plan_product(&plan, a, b, operation, workers) != 0)
        return PPF_PRODUCT_FULL;
    return plan.algorithm;
}

const char *ppf_product_profile_name(void) {
    return PPF_PRODUCT_PROFILE_NAME;
}

const char *ppf_product_profile_sha256(void) {
    return PPF_PRODUCT_PROFILE_SHA256;
}
