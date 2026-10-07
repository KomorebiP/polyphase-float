/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static int sgn(int x) { return (x > 0) - (x < 0); }

static void export_ppf(mpfr_ptr out, const ppf_t *x) {
    (void)ppf_get_mpfr(out, x, MPFR_RNDN);
}

static void compare_result(const ppf_t *x, mpfr_srcptr ref, int pt, int mt) {
    mpfr_t got;
    mpfr_init2(got, mpfr_get_prec(ref));
    export_ppf(got, x);
    CHECK(mpfr_equal_p(got, ref));
    CHECK(sgn(pt) == sgn(mt));
    mpfr_clear(got);
}

static void initialization_tests(void) {
    ppf_t a, b, c, d;
    ppf_prec_t old = ppf_get_default_prec();
    CHECK(ppf_set_default_prec(137) == 0);
    CHECK(ppf_get_default_prec() == 137);
    CHECK(ppf_inits(&a, &b, (ppf_t *)0) == 0);
    CHECK(a.precision == 137 && b.precision == 137);
    ppf_clears(&a, &b, (ppf_t *)0);

    CHECK(ppf_inits2(79, &a, &b, &c, (ppf_t *)0) == 0);
    CHECK(a.precision == 79 && b.precision == 79 && c.precision == 79);
    ppf_clears(&a, &b, &c, (ppf_t *)0);

    CHECK(ppf_init_set_ui(&a, 17, PPF_RNDN, NULL) == 0);
    CHECK(a.precision == 137);
    CHECK(ppf_init_set_si(&b, -19, PPF_RNDN, NULL) == 0);
    CHECK(ppf_init_set_uj(&c, UINTMAX_MAX, PPF_RNDN, NULL) != PPF_TERNARY_ERROR);
    CHECK(ppf_init_set_flt(&d, -0.125f, PPF_RNDN, NULL) == 0);
    ppf_clears(&a, &b, &c, &d, (ppf_t *)0);
    CHECK(ppf_set_default_prec(old) == 0);
}

static void scalar_assignment_tests(void) {
    const ppf_prec_t ps[] = {2, 3, 7, 24, 53, 64, 113, 257};
    const double ds[] = {0.0, -0.0, 0.1, -0.1, 1.0/3.0, DBL_MIN,
                         nextafter(1.0, 2.0), DBL_MAX};
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t ref;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    mpfr_init2(ref, 53);

    for (size_t pi = 0; pi < sizeof(ps)/sizeof(ps[0]); ++pi) {
        CHECK(ppf_set_prec(&x, ps[pi]) == 0);
        mpfr_set_prec(ref, (mpfr_prec_t)ps[pi]);
        for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
            ppf_rnd_t pr = (ppf_rnd_t)r;
            mpfr_rnd_t mr = ppf_rounding_to_mpfr(pr);
            int pt, mt;
            {
                mpz_t zz;
                uintmax_t uv = UINTMAX_MAX - (uintmax_t)pi;
                mpz_init(zz); mpz_import(zz, 1, -1, sizeof(uv), 0, 0, &uv);
                pt = ppf_set_uj(&x, uv, pr, &ctx);
                mt = mpfr_set_z(ref, zz, mr);
                compare_result(&x, ref, pt, mt);
                mpz_clear(zz);
            }
            {
                mpz_t zz;
                intmax_t sv = INTMAX_MIN + (intmax_t)pi;
                uintmax_t mag = sv < 0 ? (uintmax_t)(-(sv + 1)) + 1u : (uintmax_t)sv;
                mpz_init(zz); mpz_import(zz, 1, -1, sizeof(mag), 0, 0, &mag);
                if (sv < 0) mpz_neg(zz, zz);
                pt = ppf_set_sj(&x, sv, pr, &ctx);
                mt = mpfr_set_z(ref, zz, mr);
                compare_result(&x, ref, pt, mt);
                mpz_clear(zz);
            }
            pt = ppf_set_ui_2exp(&x, 13UL, -79, pr, &ctx);
            mt = mpfr_set_ui_2exp(ref, 13UL, -79, mr);
            compare_result(&x, ref, pt, mt);
            {
                mpz_t zz;
                mpz_init_set_si(zz, -37);
                pt = ppf_set_sj_2exp(&x, -37, 41, pr, &ctx);
                mt = mpfr_set_z_2exp(ref, zz, 41, mr);
                compare_result(&x, ref, pt, mt);
                mpz_clear(zz);
            }
            for (size_t i = 0; i < sizeof(ds)/sizeof(ds[0]); ++i) {
                pt = ppf_set_d(&x, ds[i], pr, &ctx);
                mt = mpfr_set_d(ref, ds[i], mr);
                compare_result(&x, ref, pt, mt);
                if (ds[i] == 0.0)
                    CHECK(ppf_signbit(&x) == !!signbit(ds[i]));
            }
        }
    }
    mpfr_clear(ref); ppf_clear(&x); ppf_context_clear(&ctx);
}

static void rational_assignment_tests(void) {
    gmp_randstate_t rs;
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t ref;
    mpq_t q;
    mpf_t f;
    mpz_t n, d;
    gmp_randinit_mt(rs); gmp_randseed_ui(rs, 0xA5519EUL);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 173, &ctx) == 0);
    mpfr_init2(ref, 173);
    mpq_init(q); mpf_init2(f, 211); mpz_inits(n, d, NULL);
    for (int i = 0; i < 500; ++i) {
        mpz_urandomb(n, rs, 1 + gmp_urandomm_ui(rs, 400));
        mpz_urandomb(d, rs, 1 + gmp_urandomm_ui(rs, 250));
        if (mpz_sgn(n) == 0) mpz_set_ui(n, 1);
        if (mpz_sgn(d) == 0) mpz_set_ui(d, 1);
        if (gmp_urandomb_ui(rs, 1)) mpz_neg(n, n);
        mpq_set_num(q, n); mpq_set_den(q, d); mpq_canonicalize(q);
        for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
            int pt = ppf_set_q(&x, q, (ppf_rnd_t)r, &ctx);
            int mt = mpfr_set_q(ref, q, ppf_rounding_to_mpfr((ppf_rnd_t)r));
            compare_result(&x, ref, pt, mt);
        }
        mpf_set_q(f, q);
        {
            int pt = ppf_set_f(&x, f, PPF_RNDN, &ctx);
            int mt = mpfr_set_f(ref, f, MPFR_RNDN);
            compare_result(&x, ref, pt, mt);
        }
    }
    mpz_clears(n, d, NULL); mpq_clear(q); mpf_clear(f);
    mpfr_clear(ref); ppf_clear(&x); ppf_context_clear(&ctx);
    gmp_randclear(rs);
}

struct text_case { const char *s; int base; };
static void parsing_tests(void) {
    const struct text_case rows[] = {
        {"3.1415926535897932384626433832795028841971", 10},
        {"  -0x1.921fb54442d18469898cc51701b8p+1tail", 0},
        {"+0b1.010101010101p-17rest", 0},
        {"1.23456789@37!", 10},
        {"Zz@-4?", 62},
        {"-@Inf@x", 17},
        {"-nan(This_Is_17)_rest", 10},
        {"0x0.0000000000000000000001p-100", 0}
    };
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t ref, got;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 197, &ctx) == 0);
    mpfr_init2(ref, 197); mpfr_init2(got, 197);
    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); ++i) {
        for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
            char *pe = NULL, *me = NULL;
            int pt = ppf_strtofr(&x, rows[i].s, &pe, rows[i].base,
                                 (ppf_rnd_t)r, &ctx);
            int mt = mpfr_strtofr(ref, rows[i].s, &me, rows[i].base,
                                  ppf_rounding_to_mpfr((ppf_rnd_t)r));
            export_ppf(got, &x);
            if (mpfr_nan_p(ref)) CHECK(ppf_nan_p(&x));
            else CHECK(mpfr_equal_p(got, ref));
            CHECK((size_t)(pe - rows[i].s) == (size_t)(me - rows[i].s));
            if (!mpfr_nan_p(ref)) CHECK(sgn(pt) == sgn(mt));
        }
    }
    CHECK(ppf_set_str(&x, "1.25", 10, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_str(&x, "1.25 trailing", 10, PPF_RNDN, &ctx) == -1);
    mpfr_clear(ref); mpfr_clear(got); ppf_clear(&x); ppf_context_clear(&ctx);
}

static void conversion_tests(void) {
    gmp_randstate_t rs;
    ppf_context_t ctx;
    ppf_t x, mant;
    mpfr_t ref, got, mref, mgot;
    mpz_t z, pz, mz;
    mpq_t pq, mq;
    mpf_t pf, mf;
    gmp_randinit_mt(rs); gmp_randseed_ui(rs, 0xC0A9E25UL);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 211, &ctx) == 0);
    CHECK(ppf_init2_ctx(&mant, 67, &ctx) == 0);
    mpfr_init2(ref, 211); mpfr_init2(got, 211);
    mpfr_init2(mref, 67); mpfr_init2(mgot, 67);
    mpz_inits(z, pz, mz, NULL); mpq_inits(pq, mq, NULL);
    mpf_init2(pf, 139); mpf_init2(mf, 139);

    for (int i = 0; i < 500; ++i) {
        ppf_exp_t e = (ppf_exp_t)gmp_urandomm_ui(rs, 801) - 400;
        mpz_urandomb(z, rs, 1 + gmp_urandomm_ui(rs, 350));
        if (mpz_sgn(z) == 0) mpz_set_ui(z, 1);
        if (gmp_urandomb_ui(rs, 1)) mpz_neg(z, z);
        (void)ppf_set_z_2exp(&x, z, e, PPF_RNDN, &ctx);
        (void)mpfr_set_z_2exp(ref, z, (mpfr_exp_t)e, MPFR_RNDN);
        export_ppf(got, &x); CHECK(mpfr_equal_p(got, ref));

        for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
            int pt = ppf_get_z(pz, &x, (ppf_rnd_t)r, &ctx);
            int mt = mpfr_get_z(mz, ref, ppf_rounding_to_mpfr((ppf_rnd_t)r));
            CHECK(mpz_cmp(pz, mz) == 0); CHECK(sgn(pt) == sgn(mt));

            CHECK(ppf_get_ui(&x, (ppf_rnd_t)r, &ctx) ==
                  mpfr_get_ui(ref, ppf_rounding_to_mpfr((ppf_rnd_t)r)));
            CHECK(ppf_get_si(&x, (ppf_rnd_t)r, &ctx) ==
                  mpfr_get_si(ref, ppf_rounding_to_mpfr((ppf_rnd_t)r)));

            {
                double pd = ppf_get_d(&x, (ppf_rnd_t)r, &ctx);
                double md = mpfr_get_d(ref, ppf_rounding_to_mpfr((ppf_rnd_t)r));
                CHECK(memcmp(&pd, &md, sizeof(pd)) == 0 || (isnan(pd) && isnan(md)));
            }
            {
                long pe = 0, me = 0;
                double pd = ppf_get_d_2exp(&pe, &x, (ppf_rnd_t)r, &ctx);
                double md = mpfr_get_d_2exp(&me, ref, ppf_rounding_to_mpfr((ppf_rnd_t)r));
                CHECK(pe == me);
                CHECK(memcmp(&pd, &md, sizeof(pd)) == 0 || (isnan(pd) && isnan(md)));
            }
        }

        ppf_get_q(pq, &x, &ctx); mpfr_get_q(mq, ref);
        CHECK(mpq_equal(pq, mq));
        {
            int pft = ppf_get_f(pf, &x, PPF_RNDN, &ctx);
            int mft = mpfr_get_f(mf, ref, MPFR_RNDN);
            CHECK(pft == mft);
            CHECK(mpf_cmp(pf, mf) == 0);
        }

        {
            ppf_exp_t pe = 0; mpfr_exp_t me = 0;
            int pt = ppf_frexp(&pe, &mant, &x, PPF_RNDN, &ctx);
            int mt = mpfr_frexp(&me, mref, ref, MPFR_RNDN);
            export_ppf(mgot, &mant);
            CHECK(pe == (ppf_exp_t)me); CHECK(mpfr_equal_p(mgot, mref));
            CHECK(sgn(pt) == sgn(mt));
        }

        for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
            mpfr_rnd_t mr = ppf_rounding_to_mpfr((ppf_rnd_t)r);
            CHECK(ppf_fits_ulong_p(&x, (ppf_rnd_t)r) == mpfr_fits_ulong_p(ref, mr));
            CHECK(ppf_fits_slong_p(&x, (ppf_rnd_t)r) == mpfr_fits_slong_p(ref, mr));
            CHECK(ppf_fits_uintmax_p(&x, (ppf_rnd_t)r) == mpfr_fits_uintmax_p(ref, mr));
            CHECK(ppf_fits_intmax_p(&x, (ppf_rnd_t)r) == mpfr_fits_intmax_p(ref, mr));
        }
    }

    mpf_clear(pf); mpf_clear(mf); mpq_clears(pq, mq, NULL); mpz_clears(z, pz, mz, NULL);
    mpfr_clear(ref); mpfr_clear(got); mpfr_clear(mref); mpfr_clear(mgot);
    ppf_clear(&x); ppf_clear(&mant); ppf_context_clear(&ctx); gmp_randclear(rs);
}

static void string_output_tests(void) {
    const int bases[] = {2, 3, 7, 10, 16, 36, 62, -16};
    const size_t ns[] = {1, 7, 31, 0};
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t ref;
    mpz_t z;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 193, &ctx) == 0);
    mpfr_init2(ref, 193); mpz_init(z);
    mpz_ui_pow_ui(z, 7, 83); mpz_add_ui(z, z, 1234567);
    (void)ppf_set_z_2exp(&x, z, -177, PPF_RNDN, &ctx);
    (void)mpfr_set_z_2exp(ref, z, -177, MPFR_RNDN);
    for (size_t bi = 0; bi < sizeof(bases)/sizeof(bases[0]); ++bi) {
        int base = bases[bi];
        for (size_t ni = 0; ni < sizeof(ns)/sizeof(ns[0]); ++ni) {
            for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
                ppf_exp_t pe = 0; mpfr_exp_t me = 0;
                char *ps = ppf_get_str(NULL, &pe, base, ns[ni], &x,
                                       (ppf_rnd_t)r, &ctx);
                char *ms = mpfr_get_str(NULL, &me, base, ns[ni], ref,
                                        ppf_rounding_to_mpfr((ppf_rnd_t)r));
                CHECK(ps != NULL && ms != NULL);
                CHECK(pe == (ppf_exp_t)me);
                CHECK(strcmp(ps, ms) == 0);
                ppf_free_str(ps); mpfr_free_str(ms);
            }
        }
    }
    CHECK(ppf_get_str_ndigits(10, 193) == mpfr_get_str_ndigits(10, 193));
    mpz_clear(z); mpfr_clear(ref); ppf_clear(&x); ppf_context_clear(&ctx);
}


static void extended_binary_conversion_tests(void) {
    const ppf_exp_t exponents[] = {-1200, -1075, -1074, -1073, -1022,
                                    -1021, -1, 0, 1023, 1024, 1100};
    const long double lds[] = {0.0L, -0.0L, 0.1L, -0.1L,
                               1.0L/3.0L, LDBL_MIN, LDBL_MAX};
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t ref;
    mpz_t z;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 211, &ctx) == 0);
    mpfr_init2(ref, 211); mpz_init_set_ui(z, 3);
    for (size_t i = 0; i < sizeof(exponents)/sizeof(exponents[0]); ++i) {
        (void)ppf_set_z_2exp(&x, z, exponents[i], PPF_RNDN, &ctx);
        (void)mpfr_set_z_2exp(ref, z, exponents[i], MPFR_RNDN);
        for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
            double pd = ppf_get_d(&x, (ppf_rnd_t)r, &ctx);
            double md = mpfr_get_d(ref, ppf_rounding_to_mpfr((ppf_rnd_t)r));
            CHECK(memcmp(&pd, &md, sizeof(pd)) == 0 || (isnan(pd) && isnan(md)));
        }
    }
    for (size_t i = 0; i < sizeof(lds)/sizeof(lds[0]); ++i) {
        for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
            int pt = ppf_set_ld(&x, lds[i], (ppf_rnd_t)r, &ctx);
            int mt = mpfr_set_ld(ref, lds[i], ppf_rounding_to_mpfr((ppf_rnd_t)r));
            compare_result(&x, ref, pt, mt);
            {
                long double pl = ppf_get_ld(&x, (ppf_rnd_t)r, &ctx);
                long double ml = mpfr_get_ld(ref, ppf_rounding_to_mpfr((ppf_rnd_t)r));
                CHECK((pl == ml && (pl != 0.0L || signbit(pl) == signbit(ml))) ||
                      (isnan(pl) && isnan(ml)));
            }
        }
    }
    mpz_clear(z); mpfr_clear(ref); ppf_clear(&x); ppf_context_clear(&ctx);
}

static void randomized_string_tests(void) {
    const int bases[] = {2, 3, 7, 10, 16, 31, 36, 37, 62, -2, -16, -36};
    gmp_randstate_t rs;
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t ref, got;
    mpz_t z;
    gmp_randinit_mt(rs); gmp_randseed_ui(rs, 0x57A1A6UL);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 241, &ctx) == 0);
    mpfr_init2(ref, 241); mpfr_init2(got, 241); mpz_init(z);
    for (int i = 0; i < 300; ++i) {
        ppf_exp_t e = (ppf_exp_t)gmp_urandomm_ui(rs, 1401) - 700;
        mpz_urandomb(z, rs, 1 + gmp_urandomm_ui(rs, 500));
        if (mpz_sgn(z) == 0) mpz_set_ui(z, 1);
        if (gmp_urandomb_ui(rs, 1)) mpz_neg(z, z);
        (void)ppf_set_z_2exp(&x, z, e, PPF_RNDN, &ctx);
        (void)mpfr_set_z_2exp(ref, z, e, MPFR_RNDN);
        for (size_t bi = 0; bi < sizeof(bases)/sizeof(bases[0]); ++bi) {
            int base = bases[bi];
            size_t n = 1 + (size_t)gmp_urandomm_ui(rs, 80);
            for (int r = PPF_RNDN; r <= PPF_RNDA; ++r) {
                ppf_exp_t pe = 0; mpfr_exp_t me = 0;
                char *ps = ppf_get_str(NULL, &pe, base, n, &x,
                                       (ppf_rnd_t)r, &ctx);
                char *ms = mpfr_get_str(NULL, &me, base, n, ref,
                                        ppf_rounding_to_mpfr((ppf_rnd_t)r));
                CHECK(ps && ms);
                CHECK(pe == (ppf_exp_t)me);
                CHECK(strcmp(ps, ms) == 0);
                ppf_free_str(ps); mpfr_free_str(ms);
            }
        }
    }
    for (int base = 2; base <= 62; ++base) {
        const ppf_prec_t ps[] = {2, 3, 24, 53, 113, 241, 1000};
        for (size_t i = 0; i < sizeof(ps)/sizeof(ps[0]); ++i)
            CHECK(ppf_get_str_ndigits(base, ps[i]) ==
                  mpfr_get_str_ndigits(base, (mpfr_prec_t)ps[i]));
    }
    ppf_set_zero(&x, -1); mpfr_set_zero(ref, -1);
    {
        ppf_exp_t pe; mpfr_exp_t me;
        char *ps = ppf_get_str(NULL, &pe, 10, 0, &x, PPF_RNDN, &ctx);
        char *ms = mpfr_get_str(NULL, &me, 10, 0, ref, MPFR_RNDN);
        CHECK(ps && ms && pe == (ppf_exp_t)me && strcmp(ps, ms) == 0);
        ppf_free_str(ps); mpfr_free_str(ms);
    }
    mpz_clear(z); mpfr_clear(ref); mpfr_clear(got);
    ppf_clear(&x); ppf_context_clear(&ctx); gmp_randclear(rs);
}

static void special_conversion_tests(void) {
    ppf_context_t ctx;
    ppf_t x;
    mpz_t z;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    mpz_init(z);
    ppf_set_nan(&x, -1); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    CHECK(ppf_get_z(z, &x, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR);
    CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ERANGE) != 0);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    CHECK(ppf_get_ui(&x, PPF_RNDN, &ctx) == 0);
    CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ERANGE) != 0);
    ppf_set_inf(&x, -1); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    CHECK(ppf_get_si(&x, PPF_RNDN, &ctx) == 0);
    CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ERANGE) != 0);
    ppf_set_zero(&x, -1);
    {
        double d = ppf_get_d(&x, PPF_RNDN, &ctx);
        CHECK(d == 0.0 && signbit(d));
    }
    mpz_clear(z); ppf_clear(&x); ppf_context_clear(&ctx);
}

int main(void) {
    initialization_tests();
    scalar_assignment_tests();
    rational_assignment_tests();
    parsing_tests();
    conversion_tests();
    extended_binary_conversion_tests();
    string_output_tests();
    randomized_string_tests();
    special_conversion_tests();
    puts("INITIALIZATION, ASSIGNMENT, AND CONVERSION TESTS PASSED");
    return 0;
}
