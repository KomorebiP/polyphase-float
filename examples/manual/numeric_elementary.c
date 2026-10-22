/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <stdio.h>
#define REQUIRE(c) do { if (!(c)) goto cleanup; } while (0)
#define NUMBER_CALL(c) REQUIRE((c) != PPF_TERNARY_ERROR)
int main(void) {
    ppf_context_t ctx;
    ppf_t x = {0}, one = {0}, sum = {0}, result = {0}, pi = {0};
    int status = 1;
    if (ppf_context_init(&ctx) != 0) return 1;
    REQUIRE(ppf_init2_ctx(&x, 53, &ctx) == 0);
    REQUIRE(ppf_init2_ctx(&one, 53, &ctx) == 0);
    REQUIRE(ppf_init2_ctx(&sum, 53, &ctx) == 0);
    REQUIRE(ppf_init2_ctx(&result, 53, &ctx) == 0);
    REQUIRE(ppf_init2_ctx(&pi, 53, &ctx) == 0);
    REQUIRE(ppf_set_ui_2exp(&x, 1, -80, PPF_RNDN, &ctx) == 0);
    REQUIRE(ppf_set_ui(&one, 1, PPF_RNDN, &ctx) == 0);
    NUMBER_CALL(ppf_add(&sum, &one, &x, PPF_RNDN, &ctx));
    REQUIRE(ppf_equal_p(&sum, &one));
    NUMBER_CALL(ppf_log(&result, &sum, PPF_RNDN, &ctx));
    REQUIRE(ppf_zero_p(&result));
    puts("log of the rounded 1 + 2^-80 is zero at 53 bits.");
    NUMBER_CALL(ppf_log1p(&result, &x, PPF_RNDN, &ctx));
    REQUIRE(ppf_regular_p(&result) && !ppf_signbit(&result));
    NUMBER_CALL(ppf_expm1(&result, &x, PPF_RNDN, &ctx));
    REQUIRE(ppf_regular_p(&result) && !ppf_signbit(&result));
    puts("log1p(2^-80) and expm1(2^-80) remain positive.");
    REQUIRE(ppf_sinpi(&result, &one, PPF_RNDN, &ctx) == 0);
    REQUIRE(ppf_zero_p(&result));
    NUMBER_CALL(ppf_const_pi(&pi, PPF_RNDN, &ctx));
    NUMBER_CALL(ppf_sin(&result, &pi, PPF_RNDN, &ctx));
    REQUIRE(ppf_regular_p(&result));
    puts("sinpi(1) is exactly zero; sin of rounded pi is not.");
    status = 0;
cleanup:
    if (status) fputs("Elementary-function example failed.\n", stderr);
    ppf_clear(&x); ppf_clear(&one); ppf_clear(&sum);
    ppf_clear(&result); ppf_clear(&pi);
    ppf_context_clear(&ctx); ppf_free_constant_caches(); ppf_thread_cleanup();
    return status;
}
