/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <stdio.h>

static int print_result(const char *label, const ppf_t *value,
                        int ternary, ppf_context_t *ctx) {
    const char *direction;
    if (ternary == PPF_TERNARY_ERROR) return -1;
    direction = ternary < 0 ? "below" : ternary > 0 ? "above" : "exact";
    if (fputs(label, stdout) == EOF ||
        ppf_out_str(stdout, 10, 9, value, PPF_RNDN, ctx) == 0 ||
        fprintf(stdout, " (%s)\n", direction) < 0)
        return -1;
    return 0;
}

int main(void) {
    ppf_context_t ctx;
    ppf_t one = {0}, three = {0};
    ppf_t lower = {0}, nearest = {0}, upper = {0};
    int down, near, up;
    int status = 1;

    if (ppf_context_init(&ctx) != 0) return 1;
    if (ppf_init2_ctx(&one, 8, &ctx) != 0 ||
        ppf_init2_ctx(&three, 8, &ctx) != 0 ||
        ppf_init2_ctx(&lower, 8, &ctx) != 0 ||
        ppf_init2_ctx(&nearest, 8, &ctx) != 0 ||
        ppf_init2_ctx(&upper, 8, &ctx) != 0)
        goto cleanup;
    if (ppf_set_ui(&one, 1, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&three, 3, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        goto cleanup;
    down = ppf_div(&lower, &one, &three, PPF_RNDD, &ctx);
    near = ppf_div(&nearest, &one, &three, PPF_RNDN, &ctx);
    up = ppf_div(&upper, &one, &three, PPF_RNDU, &ctx);
    if (down == PPF_TERNARY_ERROR || near == PPF_TERNARY_ERROR ||
        up == PPF_TERNARY_ERROR)
        goto cleanup;
    if (!(down < 0 && near > 0 && up > 0) ||
        ppf_cmp(&lower, &upper, &ctx) >= 0 ||
        ppf_cmp(&nearest, &upper, &ctx) != 0)
        goto cleanup;
    if (print_result("down    = ", &lower, down, &ctx) != 0 ||
        print_result("nearest = ", &nearest, near, &ctx) != 0 ||
        print_result("up      = ", &upper, up, &ctx) != 0 ||
        fflush(stdout) == EOF)
        goto cleanup;
    status = 0;

cleanup:
    if (status != 0) fputs("The rounding example failed.\n", stderr);
    ppf_clear(&upper);
    ppf_clear(&nearest);
    ppf_clear(&lower);
    ppf_clear(&three);
    ppf_clear(&one);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    return status;
}
