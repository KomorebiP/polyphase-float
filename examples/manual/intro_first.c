/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t a = {0}, b = {0}, product = {0};
    int status = 1;
    int ternary;

    if (ppf_context_init(&ctx) != 0) {
        fputs("Cannot initialize the context.\n", stderr);
        return 1;
    }
    if (ppf_init2_ctx(&a, 128, &ctx) != 0 ||
        ppf_init2_ctx(&b, 128, &ctx) != 0 ||
        ppf_init2_ctx(&product, 128, &ctx) != 0)
        goto cleanup;
    if (ppf_set_ui(&a, 2, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&b, 3, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        goto cleanup;

    ternary = ppf_mul(&product, &a, &b, PPF_RNDN, &ctx);
    if (ternary == PPF_TERNARY_ERROR) goto cleanup;
    if (ternary != 0 || ppf_cmp_ui(&product, 6, &ctx) != 0)
        goto cleanup;
    if (fputs("2 * 3 = ", stdout) == EOF ||
        ppf_out_str(stdout, 10, 6, &product, PPF_RNDN, &ctx) == 0 ||
        fputc('\n', stdout) == EOF || fflush(stdout) == EOF)
        goto cleanup;
    status = 0;

cleanup:
    if (status != 0) fputs("The calculation or output failed.\n", stderr);
    ppf_clear(&product);
    ppf_clear(&b);
    ppf_clear(&a);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    return status;
}
