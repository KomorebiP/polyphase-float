/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdio.h>

#include <polyphase_float.h>

int main(void)
{
    ppf_context_t context;
    ppf_t x, y, remainder, integer_part, fractional_part;
    long quotient_bits = 0;

    if (ppf_context_init(&context) != 0)
        return 1;

    if (ppf_init2_ctx(&x, 256, &context) != 0 ||
        ppf_init2_ctx(&y, 256, &context) != 0 ||
        ppf_init2_ctx(&remainder, 256, &context) != 0 ||
        ppf_init2_ctx(&integer_part, 256, &context) != 0 ||
        ppf_init2_ctx(&fractional_part, 256, &context) != 0)
        return 1;

    if (ppf_set_str(&x, "12345.75", 10, PPF_RNDN, &context) < 0 ||
        ppf_set_str(&y, "97.5", 10, PPF_RNDN, &context) < 0)
        return 1;

    ppf_modf(&integer_part, &fractional_part, &x, PPF_RNDN, &context);
    ppf_remquo(&remainder, &quotient_bits, &x, &y, PPF_RNDN, &context);

    printf("x > y: %s\n", ppf_greater_p(&x, &y) ? "yes" : "no");
    printf("x is an integer: %s\n", ppf_integer_p(&x) ? "yes" : "no");
    printf("low quotient bits: %ld\n", quotient_bits);
    printf("integer part: %.0Lf\n", ppf_get_ld(&integer_part, PPF_RNDN, &context));
    printf("fractional part: %.2Lf\n", ppf_get_ld(&fractional_part, PPF_RNDN, &context));
    printf("IEEE remainder: %.2Lf\n", ppf_get_ld(&remainder, PPF_RNDN, &context));

    ppf_clear(&fractional_part);
    ppf_clear(&integer_part);
    ppf_clear(&remainder);
    ppf_clear(&y);
    ppf_clear(&x);
    ppf_context_clear(&context);
    return 0;
}
