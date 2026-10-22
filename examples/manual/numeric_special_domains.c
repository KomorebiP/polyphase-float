/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <stdio.h>
#define REQUIRE(c) do { if (!(c)) goto cleanup; } while (0)
int main(void) {
    ppf_context_t ctx;
    ppf_t x = {0}, result = {0};
    int status = 1, t, gamma_sign = 0;
    if (ppf_context_init(&ctx) != 0) return 1;
    REQUIRE(ppf_init2_ctx(&x, 80, &ctx) == 0);
    REQUIRE(ppf_init2_ctx(&result, 80, &ctx) == 0);
    REQUIRE(ppf_set_ui(&x, 5, PPF_RNDN, &ctx) == 0);
    REQUIRE(ppf_gamma(&result, &x, PPF_RNDN, &ctx) == 0);
    REQUIRE(ppf_cmp_ui(&result, 24, &ctx) == 0);
    puts("Gamma(5) = 24 exactly.");
    REQUIRE(ppf_set_si_2exp(&x, -1, -1, PPF_RNDN, &ctx) == 0);
    t = ppf_lgamma(&result, &gamma_sign, &x, PPF_RNDN, &ctx);
    REQUIRE(t != PPF_TERNARY_ERROR && ppf_regular_p(&result));
    REQUIRE(gamma_sign == -1);
    puts("lgamma(-1/2) returns a finite log magnitude and sign -1.");
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    t = ppf_lngamma(&result, &x, PPF_RNDN, &ctx);
    REQUIRE(t != PPF_TERNARY_ERROR && ppf_nan_p(&result));
    REQUIRE((ppf_context_flags(&ctx) & PPF_FLAG_NAN) != 0);
    puts("lngamma(-1/2) reports a real-domain NaN.");
    REQUIRE(ppf_set_ui(&x, 501, PPF_RNDN, &ctx) == 0);
    REQUIRE(ppf_cmpabs_ui(&x, 500, &ctx) > 0);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    t = ppf_ai(&result, &x, PPF_RNDN, &ctx);
    REQUIRE(t == PPF_TERNARY_ERROR);
    REQUIRE((ppf_context_flags(&ctx) & PPF_FLAG_ERANGE) != 0);
    REQUIRE((ppf_context_flags(&ctx) & PPF_FLAG_ALLOC) == 0);
    puts("Ai(501) exceeds the implementation domain: ERANGE, not ALLOC.");
    status = 0;
cleanup:
    if (status) fputs("Special-function example failed.\n", stderr);
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    ppf_free_constant_caches(); ppf_thread_cleanup();
    return status;
}
