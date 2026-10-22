/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include "special_internal.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); \
} } while (0)

/* Verify containment against independently directed, higher-precision MPFR
   enclosures. This exercises interval certificates, beyond final rounding. */
static void gamma_rational_case(unsigned numerator, unsigned precision) {
    ppf_context_t ctx;
    ppf_special_iv_t argument, value;
    mpq_t exact;
    mpfr_t ad, au, expected_lo, expected_hi, actual_lo, actual_hi;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_special_iv_init(&argument, precision, &ctx) == 0);
    CHECK(ppf_special_iv_init(&value, precision, &ctx) == 0);
    mpq_init(exact); mpq_set_ui(exact, numerator, 3);
    mpfr_inits2(1024, ad, au, expected_lo, expected_hi, actual_lo, actual_hi, (mpfr_ptr)0);
    (void)mpfr_set_q(ad, exact, MPFR_RNDD);
    (void)mpfr_set_q(au, exact, MPFR_RNDU);
    (void)mpfr_gamma(expected_lo, au, MPFR_RNDD);
    (void)mpfr_gamma(expected_hi, ad, MPFR_RNDU);
    CHECK(ppf_special_iv_set_q(&argument, exact, &ctx) == 0);
    CHECK(ppf_special_iv_gamma_decreasing(&value, &argument, &ctx) == 0);
    CHECK(ppf_get_mpfr(actual_lo, &value.lo, MPFR_RNDN) != PPF_TERNARY_ERROR);
    CHECK(ppf_get_mpfr(actual_hi, &value.hi, MPFR_RNDN) != PPF_TERNARY_ERROR);
    if (mpfr_cmp(actual_lo, expected_lo) > 0 || mpfr_cmp(actual_hi, expected_hi) < 0) {
        fprintf(stderr, "Gamma(%u/3) at %u bits fails interval containment\n", numerator, precision);
        mpfr_printf("actual=[%Ra,%Ra]\noracle=[%Ra,%Ra]\n",
                    actual_lo, actual_hi, expected_lo, expected_hi);
        exit(1);
    }
    ppf_special_iv_clear(&argument); ppf_special_iv_clear(&value);
    ppf_context_clear(&ctx); mpq_clear(exact);
    mpfr_clears(ad, au, expected_lo, expected_hi, actual_lo, actual_hi, (mpfr_ptr)0);
}

static void positive_tail_case(int alias) {
    ppf_context_t ctx;
    ppf_special_iv_t sum, tail, result;
    mpq_t third;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_special_iv_init(&sum, 53, &ctx) == 0);
    CHECK(ppf_special_iv_init(&tail, 53, &ctx) == 0);
    CHECK(ppf_special_iv_init(&result, 53, &ctx) == 0);
    mpq_init(third); mpq_set_ui(third, 1, 3);
    CHECK(ppf_set_ui(&sum.lo, 1, PPF_RNDD, &ctx) == 0);
    CHECK(ppf_set_ui(&sum.hi, 2, PPF_RNDU, &ctx) == 0);
    CHECK(ppf_set_q(&tail.lo, third, PPF_RNDD, &ctx) != PPF_TERNARY_ERROR);
    CHECK(ppf_set_ui_2exp(&tail.hi, 1, -1, PPF_RNDU, &ctx) == 0);
    ppf_special_iv_t *out = alias == 1 ? &sum : alias == 2 ? &tail : &result;
    CHECK(ppf_special_iv_add_positive_tail(out, &sum, &tail, &ctx) == 0);
    /* The unknown positive remainder may be zero: retaining 1 is necessary.
       Its upper bound is 1/2, so 2+1/2 is the exact upper certificate. */
    CHECK(ppf_cmp_ui(&out->lo, 1, &ctx) == 0);
    CHECK(ppf_cmp_ui_2exp(&out->hi, 5, -1, &ctx) == 0);
    ppf_special_iv_clear(&sum); ppf_special_iv_clear(&tail); ppf_special_iv_clear(&result);
    ppf_context_clear(&ctx); mpq_clear(third);
}

int main(void) {
    const unsigned precisions[] = {2, 4, 8, 16, 24, 53, 80, 149, 256, 512};
    for (unsigned i = 0; i < sizeof(precisions) / sizeof(precisions[0]); ++i)
        for (unsigned numerator = 1; numerator < 3; ++numerator)
            gamma_rational_case(numerator, precisions[i]);
    for (int alias = 0; alias < 3; ++alias) positive_tail_case(alias);
    ppf_free_constant_caches(); mpfr_mp_memory_cleanup();
    puts("special interval audit passed: 20 independent Gamma certificates and 3 positive-tail certificates");
    return 0;
}
