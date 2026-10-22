/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t x = {0}, y = {0};
    int status = 1;

    if (ppf_context_init(&ctx) != 0) {
        fputs("Could not initialize the arithmetic context.\n", stderr);
        return 1;
    }
    if (ppf_context_set_default_precision(&ctx, 256) != 0 ||
        ppf_init2_ctx(&x, ctx.default_precision, &ctx) != 0 ||
        ppf_init2_ctx(&y, 80, &ctx) != 0) goto cleanup;

    /* Precision is measured in binary bits. The same context controls both
       objects; copying into y rounds to y's 80-bit precision. */
    if (ppf_set_ui(&x, 31, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&y, &x, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR) goto cleanup;

    /* Arithmetic returns a signed rounding direction, so a nonzero return
       alone is not an error. PPF_TERNARY_ERROR is the failure sentinel. */
    if (fputs("x (256 bits) = ", stdout) == EOF ||
        ppf_out_str(stdout, 10, 6, &x, PPF_RNDN, &ctx) == 0 ||
        fputs("\ny (80 bits)  = ", stdout) == EOF ||
        ppf_out_str(stdout, 10, 6, &y, PPF_RNDN, &ctx) == 0 ||
        fputc('\n', stdout) == EOF || fflush(stdout) == EOF) goto cleanup;
    status = 0;

cleanup:
    if (status != 0) fputs("The example could not complete.\n", stderr);
    /* Zero initialization also makes cleanup safe after partial setup. */
    ppf_clear(&x);
    ppf_clear(&y);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    return status;
}
