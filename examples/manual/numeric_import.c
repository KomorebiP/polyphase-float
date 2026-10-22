/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "polyphase_float.h"
#include <float.h>
#include <stdio.h>
#define REQUIRE(c) do { if (!(c)) goto cleanup; } while (0)
int main(void) {
    ppf_context_t ctx;
    ppf_t decimal = {0}, native = {0};
    mpq_t target, qdecimal, qnative, edecimal, enative, widened;
    int status = 1;
    if (FLT_RADIX != 2 || DBL_MANT_DIG != 53) {
        fputs("This example requires a 53-bit binary double.\n", stderr);
        return 1;
    }
    if (ppf_context_init(&ctx) != 0) return 1;
    mpq_inits(target, qdecimal, qnative, edecimal, enative, widened, NULL);
    REQUIRE(ppf_init2_ctx(&decimal, 256, &ctx) == 0);
    REQUIRE(ppf_init2_ctx(&native, 256, &ctx) == 0);
    REQUIRE(ppf_set_str(&decimal, "0.1", 10, PPF_RNDN, &ctx) == 0);
    REQUIRE(ppf_set_d(&native, 0.1, PPF_RNDN, &ctx) == 0);
    mpq_set_ui(target, 1, 10);
    ppf_get_q(qdecimal, &decimal, &ctx);
    ppf_get_q(qnative, &native, &ctx);
    mpq_sub(edecimal, qdecimal, target); mpq_abs(edecimal, edecimal);
    mpq_sub(enative, qnative, target); mpq_abs(enative, enative);
    REQUIRE(mpq_sgn(edecimal) != 0 && mpq_sgn(enative) != 0);
    REQUIRE(mpq_cmp(edecimal, enative) < 0);
    REQUIRE(ppf_prec_round(&native, 512, PPF_RNDN, &ctx) == 0);
    ppf_get_q(widened, &native, &ctx);
    REQUIRE(mpq_equal(qnative, widened));
    puts("Neither binary value is exactly 1/10.");
    puts("The 256-bit decimal parse is closer to 1/10.");
    puts("Widening the native import preserves its old value.");
    status = 0;
cleanup:
    if (status) fputs("Import example failed.\n", stderr);
    mpq_clears(target, qdecimal, qnative, edecimal, enative, widened, NULL);
    ppf_clear(&decimal); ppf_clear(&native);
    ppf_context_clear(&ctx); ppf_thread_cleanup();
    return status;
}
