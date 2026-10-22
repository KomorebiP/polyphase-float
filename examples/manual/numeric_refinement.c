/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <stdio.h>
#define REQUIRE(c) do { if (!(c)) goto cleanup; } while (0)
int main(void) {
    const unsigned scales[] = {8, 16, 32, 64, 128};
    ppf_context_t ctx;
    ppf_t result = {0};
    mpz_t radicand, lower, upper, remainder;
    int status = 1, ternary = 777, decided = 0;
    if (ppf_context_init(&ctx) != 0) return 1;
    mpz_inits(radicand, lower, upper, remainder, NULL);
    REQUIRE(ppf_init2_ctx(&result, 53, &ctx) == 0);
    REQUIRE(ppf_set_ui(&result, 1, PPF_RNDN, &ctx) == 0);
    for (size_t i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        unsigned k = scales[i];
        mpz_set_ui(radicand, 2);
        mpz_mul_2exp(radicand, radicand, 2u * k);
        mpz_sqrtrem(lower, remainder, radicand);
        REQUIRE(mpz_sgn(remainder) > 0);
        mpz_add_ui(upper, lower, 1);
        ppf_interval_status_t s = ppf_set_dyadic_interval(
            &result, lower, upper, -(ppf_exp_t)k, PPF_RNDN, &ternary, &ctx);
        REQUIRE(s != PPF_INTERVAL_ERROR);
        if (s == PPF_INTERVAL_UNDECIDED) {
            REQUIRE(ternary == 777 && ppf_cmp_ui(&result, 1, &ctx) == 0);
            printf("scale %u: refine\n", k);
        } else {
            REQUIRE(ternary > 0);
            printf("scale %u: certified 53-bit sqrt(2), rounded upward\n", k);
            decided = 1;
            break;
        }
    }
    REQUIRE(decided);
    status = 0;
cleanup:
    if (status) fputs("Refinement example did not obtain its certificate.\n", stderr);
    mpz_clears(radicand, lower, upper, remainder, NULL);
    ppf_clear(&result); ppf_context_clear(&ctx); ppf_thread_cleanup();
    return status;
}
