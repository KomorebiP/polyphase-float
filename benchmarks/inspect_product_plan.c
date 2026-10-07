/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *algorithm_name(ppf_product_algorithm_t algorithm) {
    switch (algorithm) {
        case PPF_PRODUCT_FULL: return "full";
        case PPF_PRODUCT_KARATSUBA3: return "rank3";
        case PPF_PRODUCT_TOOM3: return "rank5";
        case PPF_PRODUCT_WINDOW: return "window";
        default: return "auto";
    }
}

static int parse_operation(const char *text, ppf_product_operation_t *operation) {
    if (strcmp(text, "multiply") == 0) *operation = PPF_PRODUCT_OP_MULTIPLY;
    else if (strcmp(text, "square") == 0) *operation = PPF_PRODUCT_OP_SQUARE;
    else if (strcmp(text, "fma") == 0) *operation = PPF_PRODUCT_OP_FMA;
    else if (strcmp(text, "rec_sqrt") == 0)
        *operation = PPF_PRODUCT_OP_REC_SQRT;
    else return -1;
    return 0;
}

int main(int argc, char **argv) {
    unsigned long bits;
    unsigned long workers;
    ppf_product_operation_t operation;
    ppf_product_plan_t plan;
    ppf_context_t context;
    ppf_t a, b;
    mpz_t value;
    const ppf_t *second;

    if (argc != 4 || parse_operation(argv[2], &operation) != 0) {
        fprintf(stderr, "usage: %s BITS multiply|square|fma|rec_sqrt WORKERS\n",
                argv[0]);
        return 2;
    }
    bits = strtoul(argv[1], NULL, 10);
    workers = strtoul(argv[3], NULL, 10);
    if (bits < PPF_PREC_MIN || !workers) return 2;
    if (ppf_context_init(&context) != 0 ||
        ppf_init2_ctx(&a, bits, &context) != 0 ||
        ppf_init2_ctx(&b, bits, &context) != 0)
        return 2;
    mpz_init(value);
    mpz_setbit(value, bits - 1u);
    mpz_setbit(value, 17u);
    if (ppf_set_z(&a, value, PPF_RNDN, &context) == PPF_TERNARY_ERROR ||
        ppf_set_z(&b, value, PPF_RNDN, &context) == PPF_TERNARY_ERROR)
        return 2;
    second = operation == PPF_PRODUCT_OP_REC_SQRT ? NULL : &b;
    if (operation == PPF_PRODUCT_OP_SQUARE) second = &a;
    if (ppf_plan_product(&plan, &a, second, operation, workers) != 0)
        return 2;
    printf("{\"profile_name\":\"%s\","
           "\"profile_sha256\":\"%s\","
           "\"bits\":%lu,\"operation\":\"%s\","
           "\"requested_workers\":%lu,\"algorithm\":\"%s\","
           "\"effective_workers\":%lu,\"profile_rule_matched\":%s}\n",
           ppf_product_profile_name(), ppf_product_profile_sha256(),
           bits, argv[2], plan.requested_workers,
           algorithm_name(plan.algorithm), plan.effective_workers,
           plan.profile_rule_matched ? "true" : "false");
    mpz_clear(value);
    ppf_clear(&a);
    ppf_clear(&b);
    ppf_context_clear(&context);
    return 0;
}
