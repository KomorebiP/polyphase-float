/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <polyphase_float_mpfr.h>

int main(void) {
    ppf_context_t context;
    ppf_t value;
    mpfr_t input;
    mpfr_t output;
    int rc = 1;

    if (ppf_context_init(&context) != 0) return 2;
    if (ppf_init2_ctx(&value, 128, &context) != 0) {
        ppf_context_clear(&context);
        return 3;
    }

    mpfr_init2(input, 128);
    mpfr_init2(output, 128);
    (void)mpfr_set_ui(input, 7ul, MPFR_RNDN);

    if (ppf_set_mpfr(&value, input, PPF_RNDN, &context) == PPF_TERNARY_ERROR)
        goto done;
    if (ppf_get_mpfr(output, &value, MPFR_RNDN) == PPF_TERNARY_ERROR)
        goto done;
    if (mpfr_cmp_ui(output, 7ul) != 0) goto done;
    rc = 0;

done:
    mpfr_clear(input);
    mpfr_clear(output);
    ppf_clear(&value);
    ppf_context_clear(&context);
    return rc;
}
