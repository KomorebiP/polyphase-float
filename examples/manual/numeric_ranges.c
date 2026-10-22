/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <stdio.h>
#define REQUIRE(c) do { if (!(c)) goto cleanup; } while (0)
int main(void) {
    ppf_context_t ctx;
    ppf_t a = {0}, b = {0}, result = {0};
    int status = 1, t;
    if (ppf_context_init(&ctx) != 0) return 1;
    REQUIRE(ppf_init2_ctx(&a, 8, &ctx) == 0);
    REQUIRE(ppf_init2_ctx(&b, 8, &ctx) == 0);
    REQUIRE(ppf_init2_ctx(&result, 2, &ctx) == 0);
    REQUIRE(ppf_context_set_range(&ctx, -9, 9) == 0);
    REQUIRE(ppf_set_ui(&a, 7, PPF_RNDN, &ctx) == 0);
    REQUIRE(ppf_set_ui_2exp(&b, 15, -10, PPF_RNDN, &ctx) == 0);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    t = ppf_div(&result, &a, &b, PPF_RNDN, &ctx);
    REQUIRE(t != PPF_TERNARY_ERROR && t > 0 && ppf_inf_p(&result));
    REQUIRE(ppf_context_flags(&ctx) == (PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT));
    puts("Nearest quotient: +infinity; overflow and inexact.");
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    t = ppf_div(&result, &a, &b, PPF_RNDZ, &ctx);
    REQUIRE(t != PPF_TERNARY_ERROR && t < 0);
    REQUIRE(ppf_cmp_ui(&result, 384, &ctx) == 0);
    REQUIRE(ppf_context_flags(&ctx) == PPF_FLAG_INEXACT);
    puts("Toward-zero quotient: 384; inexact only.");
    REQUIRE(ppf_context_set_range(&ctx, -30, 100) == 0);
    REQUIRE(ppf_set_ui_2exp(&result, 1, -31, PPF_RNDN, &ctx) == 0);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    t = ppf_subnormalize(&result, 0, PPF_RNDN, &ctx);
    REQUIRE(t == 0 && ppf_context_flags(&ctx) == PPF_FLAG_UNDERFLOW);
    puts("Exact subnormal emulation: underflow without inexact.");
    status = 0;
cleanup:
    if (status) fputs("Range example failed.\n", stderr);
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&result);
    ppf_context_clear(&ctx); ppf_thread_cleanup();
    return status;
}
