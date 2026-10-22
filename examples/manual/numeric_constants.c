/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <stdio.h>
#define REQUIRE(c) do { if (!(c)) goto cleanup; } while (0)
int main(void) {
    ppf_context_t ctx;
    ppf_t direct = {0}, certified = {0};
    mpz_t lower, upper;
    ppf_exp_t exponent = 0;
    int status = 1, direct_t, interval_t = 0;
    if (ppf_context_init(&ctx) != 0) return 1;
    mpz_inits(lower, upper, NULL);
    REQUIRE(ppf_init2_ctx(&direct, 80, &ctx) == 0);
    REQUIRE(ppf_init2_ctx(&certified, 80, &ctx) == 0);
    direct_t = ppf_const_pi(&direct, PPF_RNDN, &ctx);
    REQUIRE(direct_t != PPF_TERNARY_ERROR);
    /* Interval export is a status API using the thread-default context. */
    REQUIRE(ppf_get_constant_interval(PPF_CONSTANT_PI, lower, upper,
                                      &exponent, 160, 1) == 0);
    REQUIRE(mpz_cmp(lower, upper) <= 0);
    REQUIRE(ppf_set_dyadic_interval(&certified, lower, upper, exponent,
                PPF_RNDN, &interval_t, &ctx) == PPF_INTERVAL_DECIDED);
    REQUIRE(ppf_equal_p(&direct, &certified));
    REQUIRE((direct_t > 0) == (interval_t > 0));
    REQUIRE((direct_t < 0) == (interval_t < 0));
    puts("Pi: direct evaluation and exported interval agree at 80 bits.");
    puts("The interval proves the value and its rounding direction.");
    status = 0;
cleanup:
    if (status) fputs("Constant example failed.\n", stderr);
    mpz_clears(lower, upper, NULL);
    ppf_clear(&direct); ppf_clear(&certified);
    ppf_context_clear(&ctx); ppf_free_constant_caches(); ppf_thread_cleanup();
    return status;
}
