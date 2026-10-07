/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <polyphase_float.h>

#include <stdio.h>

int main(void) {
    ppf_context_t context;
    ppf_t denominator, numerator, quotient, root;
    ppf_divisor_plan_t *plan = NULL;
    char *text = NULL;
    ppf_exp_t exponent = 0;

    if (ppf_context_init(&context) != 0) return 1;
    if (ppf_context_set_range(&context, -1000000, 1000000) != 0) return 1;
    if (ppf_init2_ctx(&denominator, 512, &context) != 0 ||
        ppf_init2_ctx(&numerator, 512, &context) != 0 ||
        ppf_init2_ctx(&quotient, 512, &context) != 0 ||
        ppf_init2_ctx(&root, 512, &context) != 0) return 1;

    ppf_set_ui(&denominator, 37, PPF_RNDN, &context);
    ppf_set_str(&numerator,
                "3.1415926535897932384626433832795028841971693993751",
                10, PPF_RNDN, &context);

    if (ppf_divisor_plan_create_ex(&plan, &denominator, 512, 16,
                                   &context) != 0) return 1;
    if (ppf_div_with_plan(&quotient, &numerator, plan,
                          PPF_RNDN, &context) == PPF_TERNARY_ERROR) return 1;
    if (ppf_rootn_ui(&root, &numerator, 7, PPF_RNDN,
                     &context) == PPF_TERNARY_ERROR) return 1;

    text = ppf_get_str(NULL, &exponent, 10, 80, &quotient, PPF_RNDN, &context);
    if (!text) return 1;
    printf("pi/37 = 0.%se%lld\n", text, (long long) exponent);
    ppf_free_str(text);

    text = ppf_get_str(NULL, &exponent, 10, 80, &root, PPF_RNDN, &context);
    if (!text) return 1;
    printf("seventh_root(pi) = 0.%se%lld\n", text, (long long) exponent);
    ppf_free_str(text);

    printf("plan applications = %lu\n",
           ppf_divisor_plan_applications(plan));
    printf("flags = 0x%x\n", (unsigned) ppf_context_flags(&context));

    ppf_divisor_plan_destroy(plan);
    ppf_clear(&root);
    ppf_clear(&quotient);
    ppf_clear(&numerator);
    ppf_clear(&denominator);
    ppf_context_clear(&context);
    return 0;
}
