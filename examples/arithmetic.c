/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"
#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t x, y, product, quotient, root;

    if (ppf_context_init(&ctx) != 0) return 1;
    if (ppf_init2_ctx(&x, 256, &ctx) != 0 ||
        ppf_init2_ctx(&y, 256, &ctx) != 0 ||
        ppf_init2_ctx(&product, 256, &ctx) != 0 ||
        ppf_init2_ctx(&quotient, 256, &ctx) != 0 ||
        ppf_init2_ctx(&root, 256, &ctx) != 0) return 1;

    ppf_set_ui(&x, 2UL, PPF_RNDN, &ctx);
    ppf_set_ui(&y, 3UL, PPF_RNDN, &ctx);
    ppf_mul(&product, &x, &y, PPF_RNDN, &ctx);
    ppf_div(&quotient, &product, &y, PPF_RNDN, &ctx);
    ppf_sqrt(&root, &x, PPF_RNDN, &ctx);

    printf("product: "); ppf_dump_file(stdout, &product);
    printf("quotient: "); ppf_dump_file(stdout, &quotient);
    printf("sqrt(2): "); ppf_dump_file(stdout, &root);

    ppf_clear(&root); ppf_clear(&quotient); ppf_clear(&product);
    ppf_clear(&y); ppf_clear(&x); ppf_context_clear(&ctx);
    return 0;
}
