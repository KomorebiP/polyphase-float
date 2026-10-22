/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Requires the MPFR adapter, official MPFR headers, and the core library. */
#include <polyphase_float_mpfr.h>

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t value = {0};
    mpfr_t input, output;
    int status = EXIT_FAILURE;

    if (ppf_context_init(&ctx) != 0) return status;
    mpfr_init2(input, 128);
    mpfr_init2(output, 53);
    if (ppf_init2_ctx(&value, 53, &ctx) != 0) goto cleanup;
    if (mpfr_set_ui(input, 5, MPFR_RNDN) != 0 ||
        mpfr_div_ui(input, input, 4, MPFR_RNDN) != 0) goto cleanup;
    if (ppf_import_mpfr_exact(&value, input, &ctx) != 0 ||
        ppf_get_prec(&value) != 128 ||
        ppf_mul_ui(&value, &value, 8, PPF_RNDN, &ctx) != 0 ||
        ppf_get_mpfr(output, &value, MPFR_RNDN) != 0 ||
        mpfr_cmp_ui(output, 10) != 0) goto cleanup;
    status = EXIT_SUCCESS;

cleanup:
    ppf_clear(&value);
    mpfr_clear(output);
    mpfr_clear(input);
    mpfr_free_cache();
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    if (status == EXIT_SUCCESS) {
        if (puts("MPFR -> Polyphase -> MPFR: 10") == EOF)
            status = EXIT_FAILURE;
    } else {
        fputs("MPFR bridge example failed\n", stderr);
    }
    return status;
}
