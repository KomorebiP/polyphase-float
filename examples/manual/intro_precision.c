/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <stdio.h>

static int print_value(const char *label, const ppf_t *value,
                       ppf_context_t *ctx) {
    if (fputs(label, stdout) == EOF ||
        ppf_out_str(stdout, 10, 12, value, PPF_RNDN, ctx) == 0 ||
        fputc('\n', stdout) == EOF)
        return -1;
    return 0;
}

int main(void) {
    ppf_context_t ctx;
    ppf_t coarse = {0}, widened = {0}, fresh = {0};
    int status = 1;
    int ternary;

    if (ppf_context_init(&ctx) != 0) return 1;
    if (ppf_init2_ctx(&coarse, 8, &ctx) != 0 ||
        ppf_init2_ctx(&widened, 128, &ctx) != 0 ||
        ppf_init2_ctx(&fresh, 128, &ctx) != 0)
        goto cleanup;
    /* set_str returns a status, unlike the ternary-returning set. */
    if (ppf_set_str(&coarse, "0.1", 10, PPF_RNDN, &ctx) != 0 ||
        ppf_set_str(&fresh, "0.1", 10, PPF_RNDN, &ctx) != 0)
        goto cleanup;
    ternary = ppf_set(&widened, &coarse, PPF_RNDN, &ctx);
    if (ternary == PPF_TERNARY_ERROR) goto cleanup;
    if (ternary != 0 || ppf_cmp(&coarse, &widened, &ctx) != 0 ||
        ppf_cmp(&widened, &fresh, &ctx) == 0)
        goto cleanup;
    if (ppf_context_set_default_precision(&ctx, 256) != 0 ||
        ppf_get_prec(&coarse) != 8 || ppf_get_prec(&fresh) != 128)
        goto cleanup;
    if (print_value("parsed at 8 bits     = ", &coarse, &ctx) != 0 ||
        print_value("copied to 128 bits   = ", &widened, &ctx) != 0 ||
        print_value("parsed at 128 bits   = ", &fresh, &ctx) != 0 ||
        puts("changing the default leaves existing precisions unchanged") == EOF ||
        fflush(stdout) == EOF)
        goto cleanup;
    status = 0;

cleanup:
    if (status != 0) fputs("The precision example failed.\n", stderr);
    ppf_clear(&fresh);
    ppf_clear(&widened);
    ppf_clear(&coarse);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    return status;
}
