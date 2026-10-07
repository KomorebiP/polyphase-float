/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t x, y, gamma_value, zeta_value, airy_value, euler, catalan;
    char *digits;
    ppf_exp_t exponent;

    if (ppf_context_init(&ctx) != 0 ||
        ppf_init2_ctx(&x, 256, &ctx) != 0 ||
        ppf_init2_ctx(&y, 256, &ctx) != 0 ||
        ppf_init2_ctx(&gamma_value, 256, &ctx) != 0 ||
        ppf_init2_ctx(&zeta_value, 256, &ctx) != 0 ||
        ppf_init2_ctx(&airy_value, 256, &ctx) != 0 ||
        ppf_init2_ctx(&euler, 256, &ctx) != 0 ||
        ppf_init2_ctx(&catalan, 256, &ctx) != 0) return 1;

    if (ppf_set_str(&x, "2.5", 10, PPF_RNDN, &ctx) != 0 ||
        ppf_set_str(&y, "0.5", 10, PPF_RNDN, &ctx) != 0 ||
        ppf_gamma(&gamma_value, &x, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_zeta(&zeta_value, &x, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_ai(&airy_value, &y, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_const_euler(&euler, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_const_catalan(&catalan, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR) return 2;

    ppf_t *values[] = {&gamma_value, &zeta_value, &airy_value, &euler, &catalan};
    const char *names[] = {"Gamma(2.5)", "zeta(2.5)", "Ai(0.5)", "Euler", "Catalan"};
    for (unsigned i = 0; i < 5; ++i) {
        digits = ppf_get_str(NULL, &exponent, 10, 40, values[i], PPF_RNDN, &ctx);
        if (!digits) return 3;
        printf("%s = %s e%ld\n", names[i], digits, (long)exponent);
        ppf_free_str(digits);
    }

    ppf_clear(&x); ppf_clear(&y); ppf_clear(&gamma_value); ppf_clear(&zeta_value);
    ppf_clear(&airy_value); ppf_clear(&euler); ppf_clear(&catalan);
    ppf_context_clear(&ctx);
    return 0;
}
