/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <gmp.h>
#include <stdio.h>

int main(void) {
    ppf_context_t context;
    ppf_t a, b, c, d, result;
    mpz_t integer;
    mpq_t rational;

    if (ppf_context_init(&context) != 0) return 1;
    if (ppf_init2_ctx(&a, 256, &context) != 0 ||
        ppf_init2_ctx(&b, 256, &context) != 0 ||
        ppf_init2_ctx(&c, 256, &context) != 0 ||
        ppf_init2_ctx(&d, 256, &context) != 0 ||
        ppf_init2_ctx(&result, 256, &context) != 0) return 1;

    mpz_init_set_str(integer, "123456789012345678901234567890", 10);
    mpq_init(rational);
    mpq_set_si(rational, -355, 113);

    (void)ppf_set_d(&a, 1.25, PPF_RNDN, &context);
    (void)ppf_set_d(&b, -2.5, PPF_RNDN, &context);
    (void)ppf_set_d(&c, 3.75, PPF_RNDN, &context);
    (void)ppf_set_d(&d, -0.625, PPF_RNDN, &context);

    (void)ppf_fmma(&result, &a, &b, &c, &d, PPF_RNDN, &context);
    ppf_printf("fmma = %.40Rg\n", &result);

    (void)ppf_mul_z(&result, &a, integer, PPF_RNDN, &context);
    ppf_printf("a * z = %.40Rg\n", &result);

    (void)ppf_add_q(&result, &a, rational, PPF_RNDN, &context);
    ppf_printf("a + q = %.40Rg\n", &result);

    (void)ppf_fac_ui(&result, 100, PPF_RNDN, &context);
    ppf_printf("100! = %.40Rg\n", &result);

    mpq_clear(rational);
    mpz_clear(integer);
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&c); ppf_clear(&d);
    ppf_clear(&result);
    ppf_context_clear(&context);
    return 0;
}
