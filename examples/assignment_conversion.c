/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <polyphase_float.h>
#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t x;
    ppf_exp_t exponent;
    char *digits;

    if (ppf_context_init(&ctx) != 0 || ppf_init2_ctx(&x, 256, &ctx) != 0)
        return 1;
    if (ppf_set_str(&x, "3.1415926535897932384626433832795028841971",
                    10, PPF_RNDN, &ctx) != 0)
        return 2;

    digits = ppf_get_str(NULL, &exponent, 10, 50, &x, PPF_RNDN, &ctx);
    if (!digits) return 3;
    printf("significand=%s exponent=%lld\n", digits, (long long)exponent);

    ppf_free_str(digits);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    return 0;
}
