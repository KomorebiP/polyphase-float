/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static int sign_of(int x) { return (x > 0) - (x < 0); }

static void one_case(ppf_prec_t precision, long value, long exponent,
                     ppf_rnd_t rnd, ppf_context_t *ctx,
                     mpfr_t reference, mpfr_t exported, ppf_t *x) {
    mpz_t z;
    int mt, pt;
    mpz_init_set_si(z, value);
    mpfr_clear_flags();
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
    mt = mpfr_set_z_2exp(reference, z, exponent, ppf_rounding_to_mpfr(rnd));
    pt = ppf_set_z_2exp(x, z, exponent, rnd, ctx);
    (void)ppf_get_mpfr(exported, x, MPFR_RNDN);
    CHECK(mpfr_equal_p(reference, exported));
    CHECK(sign_of(mt) == sign_of(pt));
    CHECK((mpfr_inexflag_p()!=0) ==
          ((ppf_context_flags(ctx)&PPF_FLAG_INEXACT)!=0));
    CHECK((mpfr_underflow_p()!=0) ==
          ((ppf_context_flags(ctx)&PPF_FLAG_UNDERFLOW)!=0));
    CHECK((mpfr_overflow_p()!=0) ==
          ((ppf_context_flags(ctx)&PPF_FLAG_OVERFLOW)!=0));
    CHECK(ppf_validate(x));
    mpz_clear(z);
    (void)precision;
}

int main(void) {
    unsigned long normal_cases = 0, range_cases = 0;
    ppf_context_t ctx;
    CHECK(ppf_context_init(&ctx) == 0);

    for (ppf_prec_t p = 2; p <= 8; ++p) {
        ppf_t x;
        mpfr_t ref, got;
        CHECK(ppf_init2_ctx(&x, p, &ctx) == 0);
        mpfr_init2(ref, p); mpfr_init2(got, p);
        for (long z = -255; z <= 255; ++z) {
            for (long e = -8; e <= 8; ++e) {
                for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
                    one_case(p, z, e, (ppf_rnd_t)r, &ctx, ref, got, &x);
                    normal_cases++;
                }
            }
        }
        mpfr_clear(ref); mpfr_clear(got); ppf_clear(&x);
    }

    {
        mpfr_exp_t old_emin = mpfr_get_emin();
        mpfr_exp_t old_emax = mpfr_get_emax();
        CHECK(mpfr_set_emin(-2) == 0);
        CHECK(mpfr_set_emax(2) == 0);
        CHECK(ppf_context_set_range(&ctx, -2, 2) == 0);
        for (ppf_prec_t p = 2; p <= 6; ++p) {
            ppf_t x;
            mpfr_t ref, got;
            CHECK(ppf_init2_ctx(&x, p, &ctx) == 0);
            mpfr_init2(ref, p); mpfr_init2(got, p);
            for (long z = -32; z <= 32; ++z) {
                for (long e = -10; e <= 10; ++e) {
                    for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
                        one_case(p, z, e, (ppf_rnd_t)r, &ctx, ref, got, &x);
                        range_cases++;
                    }
                }
            }
            mpfr_clear(ref); mpfr_clear(got); ppf_clear(&x);
        }
        CHECK(mpfr_set_emin(old_emin) == 0);
        CHECK(mpfr_set_emax(old_emax) == 0);
    }

    ppf_context_clear(&ctx);
    printf("EXHAUSTIVE SMALL-PRECISION TESTS PASSED: %lu normal, %lu constrained-range cases\n",
           normal_cases, range_cases);
    return 0;
}
