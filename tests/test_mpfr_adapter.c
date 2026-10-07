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

static void compare_case(gmp_randstate_t rs, ppf_prec_t precision,
                         unsigned bits, ppf_exp_t exponent,
                         ppf_rnd_t rnd) {
    ppf_context_t ctx;
    ppf_t value;
    mpz_t z;
    mpfr_t reference, exported;
    int pt, mt;

    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&value, precision, &ctx) == 0);
    mpz_init(z);
    mpz_urandomb(z, rs, bits);
    if (mpz_sgn(z) == 0) mpz_set_ui(z, 1);
    if (gmp_urandomb_ui(rs, 1)) mpz_neg(z, z);

    mpfr_init2(reference, (mpfr_prec_t)precision);
    mpfr_init2(exported, (mpfr_prec_t)precision);
    mpfr_clear_flags();
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    mt = mpfr_set_z_2exp(reference, z, (mpfr_exp_t)exponent,
                         ppf_rounding_to_mpfr(rnd));
    pt = ppf_set_z_2exp(&value, z, exponent, rnd, &ctx);
    (void)ppf_get_mpfr(exported, &value, MPFR_RNDN);

    CHECK(mpfr_equal_p(reference, exported));
    CHECK(sign_of(mt) == sign_of(pt));
    CHECK((mpfr_inexflag_p() != 0) ==
          ((ppf_context_flags(&ctx) & PPF_FLAG_INEXACT) != 0));
    CHECK(ppf_validate(&value));

    mpfr_clear(reference); mpfr_clear(exported);
    mpz_clear(z);
    ppf_clear(&value);
    ppf_context_clear(&ctx);
}

static void random_normal_tests(void) {
    gmp_randstate_t rs;
    const ppf_prec_t precisions[] = {2,3,7,24,53,64,65,113,257,1000};
    gmp_randinit_mt(rs);
    gmp_randseed_ui(rs, 0x50504631UL);
    for (size_t p = 0; p < sizeof(precisions)/sizeof(precisions[0]); ++p) {
        for (int rnd = PPF_RNDN; rnd <= PPF_RNDA; ++rnd) {
            for (int i = 0; i < 80; ++i) {
                unsigned bits = 1u + (unsigned)gmp_urandomm_ui(rs, 1500);
                ppf_exp_t e = (ppf_exp_t)gmp_urandomm_ui(rs, 2001) - 1000;
                compare_case(rs, precisions[p], bits, e, (ppf_rnd_t)rnd);
            }
        }
    }
    gmp_randclear(rs);
}


static void faithful_tests(void) {
    gmp_randstate_t rs;
    ppf_context_t ctx;
    ppf_t value;
    mpfr_t down, up, got;
    mpz_t z;
    gmp_randinit_mt(rs);
    gmp_randseed_ui(rs, 0x46414954UL);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&value, 113, &ctx) == 0);
    mpfr_init2(down, 113); mpfr_init2(up, 113); mpfr_init2(got, 113);
    mpz_init(z);
    for (int i = 0; i < 500; ++i) {
        ppf_exp_t e = (ppf_exp_t)gmp_urandomm_ui(rs, 1000) - 500;
        mpz_urandomb(z, rs, 300);
        if (mpz_sgn(z) == 0) mpz_set_ui(z, 1);
        if (gmp_urandomb_ui(rs, 1)) mpz_neg(z, z);
        (void)mpfr_set_z_2exp(down, z, (mpfr_exp_t)e, MPFR_RNDD);
        (void)mpfr_set_z_2exp(up, z, (mpfr_exp_t)e, MPFR_RNDU);
        (void)ppf_set_z_2exp(&value, z, e, PPF_RNDF, &ctx);
        (void)ppf_get_mpfr(got, &value, MPFR_RNDN);
        CHECK(mpfr_equal_p(got, down) || mpfr_equal_p(got, up));
    }
    mpz_clear(z);
    mpfr_clear(down); mpfr_clear(up); mpfr_clear(got);
    ppf_clear(&value); ppf_context_clear(&ctx);
    gmp_randclear(rs);
}

static void range_tests(void) {
    mpfr_exp_t old_emin = mpfr_get_emin();
    mpfr_exp_t old_emax = mpfr_get_emax();
    ppf_context_t ctx;
    ppf_t value;
    mpfr_t reference, exported;
    mpz_t z;
    struct row { long z; long e; } rows[] = {
        {4,0}, {1,-5}, {3,-5}, {1,-4}, {-3,-5}, {-4,0}
    };
    CHECK(mpfr_set_emin(-2) == 0);
    CHECK(mpfr_set_emax(2) == 0);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_context_set_range(&ctx, -2, 2) == 0);
    CHECK(ppf_init2_ctx(&value, 4, &ctx) == 0);
    mpfr_init2(reference, 4);
    mpfr_init2(exported, 4);
    mpz_init(z);

    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); ++i) {
        for (int rnd = PPF_RNDN; rnd <= PPF_RNDA; ++rnd) {
            int mt, pt;
            mpz_set_si(z, rows[i].z);
            mpfr_clear_flags();
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_set_z_2exp(reference, z, rows[i].e,
                                 ppf_rounding_to_mpfr((ppf_rnd_t)rnd));
            pt = ppf_set_z_2exp(&value, z, rows[i].e,
                                (ppf_rnd_t)rnd, &ctx);
            (void)ppf_get_mpfr(exported, &value, MPFR_RNDN);
            CHECK(mpfr_equal_p(reference, exported));
            CHECK(sign_of(mt) == sign_of(pt));
            CHECK((mpfr_overflow_p()!=0) ==
                  ((ppf_context_flags(&ctx)&PPF_FLAG_OVERFLOW)!=0));
            CHECK((mpfr_underflow_p()!=0) ==
                  ((ppf_context_flags(&ctx)&PPF_FLAG_UNDERFLOW)!=0));
        }
    }

    mpz_clear(z);
    mpfr_clear(reference); mpfr_clear(exported);
    ppf_clear(&value);
    ppf_context_clear(&ctx);
    CHECK(mpfr_set_emin(old_emin) == 0);
    CHECK(mpfr_set_emax(old_emax) == 0);
}

static void import_tests(void) {
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t a, b;
    mpz_t z;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    mpfr_init2(a, 333);
    mpfr_init2(b, 333);
    mpz_init(z);
    mpz_ui_pow_ui(z, 7, 140);
    (void)mpfr_set_z_2exp(a, z, -77, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(&x, a, &ctx) == 0);
    CHECK(x.precision == 333 && ppf_validate(&x));
    (void)ppf_get_mpfr(b, &x, MPFR_RNDN);
    CHECK(mpfr_equal_p(a, b));

    mpfr_set_zero(a, -1);
    CHECK(ppf_import_mpfr_exact(&x, a, &ctx) == 0);
    CHECK(ppf_zero_p(&x) && ppf_signbit(&x));
    mpfr_set_inf(a, -1);
    CHECK(ppf_import_mpfr_exact(&x, a, &ctx) == 0);
    CHECK(ppf_inf_p(&x) && ppf_signbit(&x));
    mpfr_set_nan(a);
    CHECK(ppf_import_mpfr_exact(&x, a, &ctx) == 0);
    CHECK(ppf_nan_p(&x));

    mpz_clear(z);
    mpfr_clear(a); mpfr_clear(b);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
}

int main(void) {
    random_normal_tests();
    faithful_tests();
    range_tests();
    import_tests();
    puts("MPFR ADAPTER DIFFERENTIAL TESTS PASSED");
    return 0;
}
