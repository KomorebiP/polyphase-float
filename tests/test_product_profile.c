/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message) {
    fprintf(stderr, "product profile test failure: %s\n", message);
    exit(2);
}

static void set_high_bit(ppf_t *value, ppf_prec_t precision,
                         ppf_context_t *ctx) {
    mpz_t integer;
    mpz_init(integer);
    mpz_setbit(integer, (mp_bitcnt_t)(precision - 1u));
    mpz_setbit(integer, 7u);
    if (ppf_set_z(value, integer, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) {
        mpz_clear(integer);
        fail("high-bit assignment");
    }
    mpz_clear(integer);
}

static void check_hash(const char *hash) {
    size_t i;
    if (!hash || strlen(hash) != 64u) fail("profile hash length");
    for (i = 0; i < 64u; ++i)
        if (!isxdigit((unsigned char)hash[i])) fail("profile hash syntax");
}

static void check_plan_shape(const ppf_product_plan_t *plan) {
    if (!plan) fail("null plan");
    if (plan->algorithm < PPF_PRODUCT_FULL ||
        plan->algorithm > PPF_PRODUCT_WINDOW)
        fail("algorithm range");
    if (!plan->requested_workers || !plan->effective_workers ||
        plan->effective_workers > plan->requested_workers)
        fail("worker range");
    if (plan->profile_rule_matched != 0 &&
        plan->profile_rule_matched != 1)
        fail("profile match flag");
    switch (plan->algorithm) {
        case PPF_PRODUCT_FULL:
            if (plan->effective_workers != 1u) fail("full worker cap");
            break;
        case PPF_PRODUCT_KARATSUBA3:
            if (plan->effective_workers != 1u &&
                plan->effective_workers != 3u)
                fail("rank-3 worker cap");
            break;
        case PPF_PRODUCT_TOOM3:
            if (plan->effective_workers > 5u) fail("rank-5 worker cap");
            break;
        case PPF_PRODUCT_WINDOW:
            if (plan->effective_workers > 3u) fail("window worker cap");
            break;
        default:
            fail("unresolved AUTO plan");
    }
}

static int same_direction(int first, int second) {
    return (first > 0) == (second > 0) &&
           (first < 0) == (second < 0);
}

int main(void) {
    const ppf_prec_t medium_bits = 100000u;
    const ppf_prec_t square_bits = 600000u;
    const int portable = strcmp(ppf_product_profile_name(), "portable") == 0;
    const int fixture = strcmp(ppf_product_profile_name(), "test-profile") == 0;
    ppf_context_t ctx;
    ppf_t small_a, small_b, medium_a, medium_b, uneven_b, square_a;
    ppf_t auto_result, selected_result;
    ppf_product_plan_t plan;
    ppf_product_algorithm_t legacy;

    if (!ppf_product_profile_name() || !*ppf_product_profile_name())
        fail("profile name");
    check_hash(ppf_product_profile_sha256());
    if (ppf_context_init(&ctx) != 0) fail("context init");
#define INIT(name, bits) \
    if (ppf_init2_ctx(&(name), (bits), &ctx) != 0) fail("object init")
    INIT(small_a, 128u);
    INIT(small_b, 128u);
    INIT(medium_a, medium_bits);
    INIT(medium_b, medium_bits);
    INIT(uneven_b, medium_bits - 64u);
    INIT(square_a, square_bits);
    INIT(auto_result, medium_bits);
    INIT(selected_result, medium_bits);
#undef INIT
    set_high_bit(&small_a, 128u, &ctx);
    set_high_bit(&small_b, 128u, &ctx);
    set_high_bit(&medium_a, medium_bits, &ctx);
    set_high_bit(&medium_b, medium_bits, &ctx);
    set_high_bit(&uneven_b, medium_bits - 64u, &ctx);
    set_high_bit(&square_a, square_bits, &ctx);

    if (ppf_plan_product(NULL, &small_a, &small_b,
                         PPF_PRODUCT_OP_MULTIPLY, 1u) == 0)
        fail("null destination accepted");
    if (ppf_plan_product(&plan, &small_a, &small_b,
                         (ppf_product_operation_t)0, 1u) == 0)
        fail("invalid operation accepted");

    if (ppf_plan_product(&plan, &small_a, &small_b,
                         PPF_PRODUCT_OP_MULTIPLY, 0u) != 0)
        fail("small plan");
    check_plan_shape(&plan);
    if (plan.requested_workers != 1u) fail("zero worker normalization");
    if (portable && (plan.algorithm != PPF_PRODUCT_FULL ||
                     plan.profile_rule_matched != 0))
        fail("portable small plan");

    if (ppf_plan_product(&plan, &medium_a, &medium_b,
                         PPF_PRODUCT_OP_MULTIPLY, 10u) != 0)
        fail("medium multiply plan");
    check_plan_shape(&plan);
    legacy = ppf_plan_product_algorithm(&medium_a, &medium_b, 0, 10u);
    if (legacy != plan.algorithm) fail("legacy planner consistency");
    if (portable && (plan.algorithm != PPF_PRODUCT_KARATSUBA3 ||
                     plan.effective_workers != 3u ||
                     plan.profile_rule_matched != 0))
        fail("portable multiply plan");
    if (fixture && (plan.algorithm != PPF_PRODUCT_TOOM3 ||
                    plan.effective_workers != 5u ||
                    plan.profile_rule_matched != 1))
        fail("fixture multiply plan");
    {
        int automatic = ppf_mul_algorithm_threads(
            &auto_result, &medium_a, &medium_b, PPF_RNDN, 10u,
            PPF_PRODUCT_AUTO, &ctx);
        int selected = ppf_mul_algorithm_threads(
            &selected_result, &medium_a, &medium_b, PPF_RNDN,
            plan.effective_workers, plan.algorithm, &ctx);
        if (automatic == PPF_TERNARY_ERROR || selected == PPF_TERNARY_ERROR ||
            !same_direction(automatic, selected) ||
            !ppf_equal_p(&auto_result, &selected_result))
            fail("AUTO multiply execution");
    }

    if (ppf_plan_product(&plan, &medium_a, &medium_b,
                         PPF_PRODUCT_OP_FMA, 10u) != 0)
        fail("medium FMA plan");
    check_plan_shape(&plan);
    if (portable && (plan.algorithm != PPF_PRODUCT_KARATSUBA3 ||
                     plan.effective_workers != 3u ||
                     plan.profile_rule_matched != 0))
        fail("portable FMA plan");
    if (fixture && (plan.algorithm != PPF_PRODUCT_TOOM3 ||
                    plan.effective_workers != 5u ||
                    plan.profile_rule_matched != 1))
        fail("fixture FMA plan");
    {
        int automatic = ppf_fma_algorithm_threads(
            &auto_result, &medium_a, &medium_b, &medium_a,
            PPF_RNDN, 10u, PPF_PRODUCT_AUTO, &ctx);
        int selected = ppf_fma_algorithm_threads(
            &selected_result, &medium_a, &medium_b, &medium_a,
            PPF_RNDN, plan.effective_workers, plan.algorithm, &ctx);
        if (automatic == PPF_TERNARY_ERROR || selected == PPF_TERNARY_ERROR ||
            !same_direction(automatic, selected) ||
            !ppf_equal_p(&auto_result, &selected_result))
            fail("AUTO FMA execution");
    }

    if (ppf_plan_product(&plan, &medium_a, &uneven_b,
                         PPF_PRODUCT_OP_MULTIPLY, 10u) != 0)
        fail("unbalanced plan");
    check_plan_shape(&plan);
    if (plan.algorithm != PPF_PRODUCT_FULL ||
        plan.effective_workers != 1u || plan.profile_rule_matched != 0)
        fail("unbalanced fallback");

    if (ppf_plan_product(&plan, &square_a, &square_a,
                         PPF_PRODUCT_OP_SQUARE, 10u) != 0)
        fail("square plan");
    check_plan_shape(&plan);
    if (portable && (plan.algorithm != PPF_PRODUCT_TOOM3 ||
                     plan.effective_workers != 5u))
        fail("portable square plan");

    if (ppf_plan_product(&plan, &medium_a, NULL,
                         PPF_PRODUCT_OP_REC_SQRT, 10u) != 0)
        fail("reciprocal square-root plan");
    check_plan_shape(&plan);
    if (portable && (plan.algorithm != PPF_PRODUCT_KARATSUBA3 ||
                     plan.effective_workers != 3u))
        fail("portable reciprocal square-root plan");

    ppf_clear(&small_a);
    ppf_clear(&small_b);
    ppf_clear(&medium_a);
    ppf_clear(&medium_b);
    ppf_clear(&uneven_b);
    ppf_clear(&square_a);
    ppf_clear(&auto_result);
    ppf_clear(&selected_result);
    ppf_context_clear(&ctx);
    puts("PRODUCT PROFILE INTROSPECTION TESTS PASSED");
    return 0;
}
