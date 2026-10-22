/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t one = {0}, three = {0}, zero = {0}, result = {0};
    ppf_flags_t flags;
    int status = 1;
    int ternary;

    if (ppf_context_init(&ctx) != 0) return 1;
    if (ppf_init2_ctx(&one, 8, &ctx) != 0 ||
        ppf_init2_ctx(&three, 8, &ctx) != 0 ||
        ppf_init2_ctx(&zero, 8, &ctx) != 0 ||
        ppf_init2_ctx(&result, 8, &ctx) != 0)
        goto cleanup;
    if (ppf_set_ui(&one, 1, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&three, 3, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        goto cleanup;
    ppf_set_zero(&zero, 1);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);

    ternary = ppf_div(&result, &one, &three, PPF_RNDN, &ctx);
    if (ternary == PPF_TERNARY_ERROR) goto cleanup;
    flags = ppf_context_flags(&ctx);
    if (ternary == 0 || (flags & PPF_FLAG_INEXACT) == 0)
        goto cleanup;
    if (puts("1 / 3: INEXACT is set") == EOF) goto cleanup;

    ternary = ppf_set_ui(&result, 2, PPF_RNDN, &ctx);
    if (ternary == PPF_TERNARY_ERROR) goto cleanup;
    if (ternary != 0 ||
        (ppf_context_flags(&ctx) & PPF_FLAG_INEXACT) == 0)
        goto cleanup;
    if (puts("exact assignment: INEXACT is still set") == EOF)
        goto cleanup;

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    if (ppf_context_flags(&ctx) != 0) goto cleanup;
    if (puts("after clearing: no flags are set") == EOF) goto cleanup;

    ternary = ppf_div(&result, &one, &zero, PPF_RNDN, &ctx);
    if (ternary == PPF_TERNARY_ERROR) goto cleanup;
    flags = ppf_context_flags(&ctx);
    if (!ppf_inf_p(&result) || ppf_signbit(&result) ||
        (flags & PPF_FLAG_DIVBY0) == 0)
        goto cleanup;
    if (puts("1 / 0: positive infinity; DIVBY0 is set") == EOF)
        goto cleanup;

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    ternary = ppf_div(&result, &zero, &zero, PPF_RNDN, &ctx);
    if (ternary == PPF_TERNARY_ERROR) goto cleanup;
    flags = ppf_context_flags(&ctx);
    if (!ppf_nan_p(&result) || (flags & PPF_FLAG_NAN) == 0)
        goto cleanup;
    if (puts("0 / 0: NaN; NAN is set") == EOF || fflush(stdout) == EOF)
        goto cleanup;
    status = 0;

cleanup:
    if (status != 0) fputs("The flag example failed.\n", stderr);
    ppf_clear(&result);
    ppf_clear(&zero);
    ppf_clear(&three);
    ppf_clear(&one);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    return status;
}
