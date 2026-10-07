/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static const char *g_label = "startup";

static int signum(int x) { return (x > 0) - (x < 0); }

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        return mpfr_nan_p(a) && mpfr_nan_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    if (mpfr_zero_p(a) || mpfr_zero_p(b))
        return mpfr_zero_p(a) && mpfr_zero_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    return mpfr_equal_p(a, b) != 0;
}

typedef struct {
    int underflow, overflow, divby0, nan, inexact, erange;
} mpfr_flag_snapshot_t;

static mpfr_flag_snapshot_t snapshot_mpfr_flags(void) {
    mpfr_flag_snapshot_t f;
    f.underflow = mpfr_underflow_p() != 0;
    f.overflow = mpfr_overflow_p() != 0;
    f.divby0 = mpfr_divby0_p() != 0;
    f.nan = mpfr_nanflag_p() != 0;
    f.inexact = mpfr_inexflag_p() != 0;
    f.erange = mpfr_erangeflag_p() != 0;
    return f;
}

static void compare_flags(ppf_context_t *ctx, mpfr_flag_snapshot_t f) {
    ppf_flags_t p = ppf_context_flags(ctx);
    CHECK(f.underflow == ((p & PPF_FLAG_UNDERFLOW) != 0));
    CHECK(f.overflow == ((p & PPF_FLAG_OVERFLOW) != 0));
    CHECK(f.divby0 == ((p & PPF_FLAG_DIVBY0) != 0));
    if (f.nan != ((p & PPF_FLAG_NAN) != 0)) {
        fprintf(stderr, "flag mismatch mpfr(nan=%d inex=%d erange=%d under=%d over=%d div=%d) ppf=0x%x\n",
                f.nan, f.inexact, f.erange, f.underflow, f.overflow, f.divby0, (unsigned)p);
        exit(1);
    }
    CHECK(f.inexact == ((p & PPF_FLAG_INEXACT) != 0));
    CHECK(f.erange == ((p & PPF_FLAG_ERANGE) != 0));
}

static void compare_result(ppf_context_t *ctx, const ppf_t *got,
                           mpfr_srcptr expected, int pt, int mt,
                           int exact_return) {
    mpfr_t out;
    mpfr_flag_snapshot_t flags = snapshot_mpfr_flags();
    mpfr_init2(out, mpfr_get_prec(expected));
    (void)ppf_get_mpfr(out, got, MPFR_RNDN);
    if (!same_value(out, expected)) {
        fprintf(stderr, "label=%s\n", g_label);
        mpfr_printf("expected=%Ra\nout=%Ra\n", expected, out);
        ppf_dump_file(stderr, got);
        exit(1);
    }
    if (exact_return) {
        if (pt != mt) {
            fprintf(stderr, "return mismatch label=%s pt=%d mt=%d\n", g_label, pt, mt);
            exit(1);
        }
    } else if (signum(pt) != signum(mt)) {
        fprintf(stderr, "return sign mismatch label=%s pt=%d mt=%d\n", g_label, pt, mt);
        exit(1);
    }
    compare_flags(ctx, flags);
    mpfr_clear(out);
}

static void random_exact(gmp_randstate_t rs, mpfr_ptr m, ppf_t *p,
                         ppf_context_t *ctx, mpfr_prec_t precision,
                         long emin, long emax) {
    mpz_t z;
    unsigned bits = 1u + (unsigned)gmp_urandomm_ui(rs, precision + 47u);
    long span = emax - emin + 1;
    long e = emin + (long)gmp_urandomm_ui(rs, (unsigned long)span);
    mpz_init(z);
    mpz_urandomb(z, rs, bits);
    if (mpz_sgn(z) == 0) mpz_set_ui(z, 1);
    if (gmp_urandomb_ui(rs, 1)) mpz_neg(z, z);
    mpfr_set_prec(m, precision);
    (void)mpfr_set_z_2exp(m, z, e, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(p, m, ctx) == 0);
    mpz_clear(z);
}

static void comparison_tests(gmp_randstate_t rs) {
    g_label = "comparison";
    ppf_context_t ctx;
    ppf_t a, b;
    mpfr_t ma, mb;
    mpz_t z;
    mpq_t q;
    mpf_t f;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 53, &ctx) == 0);
    mpfr_inits2(53, ma, mb, (mpfr_ptr)0);
    mpz_init(z); mpq_init(q); mpf_init2(f, 192);

    for (int i = 0; i < 1200; ++i) {
        mpfr_prec_t pa = 2 + gmp_urandomm_ui(rs, 500);
        mpfr_prec_t pb = 2 + gmp_urandomm_ui(rs, 500);
        random_exact(rs, ma, &a, &ctx, pa, -500, 500);
        random_exact(rs, mb, &b, &ctx, pb, -500, 500);
        CHECK(signum(ppf_cmp(&a, &b, &ctx)) == signum(mpfr_cmp(ma, mb)));
        CHECK(signum(ppf_cmpabs(&a, &b, &ctx)) == signum(mpfr_cmpabs(ma, mb)));
        CHECK(ppf_greater_p(&a, &b) == !!mpfr_greater_p(ma, mb));
        CHECK(ppf_greaterequal_p(&a, &b) == !!mpfr_greaterequal_p(ma, mb));
        CHECK(ppf_less_p(&a, &b) == !!mpfr_less_p(ma, mb));
        CHECK(ppf_lessequal_p(&a, &b) == !!mpfr_lessequal_p(ma, mb));
        CHECK(ppf_equal_p(&a, &b) == !!mpfr_equal_p(ma, mb));
        CHECK(ppf_lessgreater_p(&a, &b) == !!mpfr_lessgreater_p(ma, mb));
        CHECK(ppf_unordered_p(&a, &b) == !!mpfr_unordered_p(ma, mb));
        CHECK(ppf_total_order_p(&a, &b) == !!mpfr_total_order_p(ma, mb));

        {
            unsigned long u = gmp_urandomb_ui(rs, 8 * sizeof(unsigned long));
            long s = (long)gmp_urandomb_ui(rs, 8 * sizeof(long) - 1);
            if (gmp_urandomb_ui(rs, 1)) s = -s;
            long e = (long)gmp_urandomm_ui(rs, 401) - 200;
            CHECK(signum(ppf_cmp_ui(&a, u, &ctx)) == signum(mpfr_cmp_ui(ma, u)));
            CHECK(signum(ppf_cmp_si(&a, s, &ctx)) == signum(mpfr_cmp_si(ma, s)));
            CHECK(signum(ppf_cmp_ui_2exp(&a, u, e, &ctx)) ==
                  signum(mpfr_cmp_ui_2exp(ma, u, e)));
            CHECK(signum(ppf_cmp_si_2exp(&a, s, e, &ctx)) ==
                  signum(mpfr_cmp_si_2exp(ma, s, e)));
            CHECK(signum(ppf_cmpabs_ui(&a, u, &ctx)) ==
                  signum(mpfr_cmpabs_ui(ma, u)));
        }
        mpz_urandomb(z, rs, 1 + gmp_urandomm_ui(rs, 400));
        if (gmp_urandomb_ui(rs, 1)) mpz_neg(z, z);
        CHECK(signum(ppf_cmp_z(&a, z, &ctx)) == signum(mpfr_cmp_z(ma, z)));
        {
            mpz_t den;
            mpz_init(den);
            mpz_urandomb(den, rs, 1 + gmp_urandomm_ui(rs, 180));
            if (mpz_sgn(den) == 0) mpz_set_ui(den, 1);
            mpq_set_num(q, z); mpq_set_den(q, den); mpq_canonicalize(q);
            CHECK(signum(ppf_cmp_q(&a, q, &ctx)) == signum(mpfr_cmp_q(ma, q)));
            mpf_set_q(f, q);
            CHECK(signum(ppf_cmp_f(&a, f, &ctx)) == signum(mpfr_cmp_f(ma, f)));
            mpz_clear(den);
        }
        {
            double d = ldexp((double)((int)gmp_urandomm_ui(rs, 2001) - 1000),
                             (int)gmp_urandomm_ui(rs, 40) - 20);
            long double ld = (long double)d / 7.0L;
            CHECK(signum(ppf_cmp_d(&a, d, &ctx)) == signum(mpfr_cmp_d(ma, d)));
            CHECK(signum(ppf_cmp_ld(&a, ld, &ctx)) == signum(mpfr_cmp_ld(ma, ld)));
        }
    }

    mpfr_set_nan(ma); ppf_set_nan(&a, -1);
    mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    CHECK(ppf_cmp(&a, &b, &ctx) == mpfr_cmp(ma, mb));
    compare_flags(&ctx, snapshot_mpfr_flags());

    mpf_clear(f); mpq_clear(q); mpz_clear(z);
    mpfr_clears(ma, mb, (mpfr_ptr)0);
    ppf_clear(&a); ppf_clear(&b); ppf_context_clear(&ctx);
}

static void sign_tests(void) {
    g_label = "sign";
    ppf_context_t ctx;
    ppf_t x, y, s;
    mpfr_t mx, my, ms;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 37, &ctx) == 0);
    CHECK(ppf_init2_ctx(&y, 11, &ctx) == 0);
    CHECK(ppf_init2_ctx(&s, 19, &ctx) == 0);
    mpfr_init2(mx, 37); mpfr_init2(my, 11); mpfr_init2(ms, 19);
    mpfr_set_str(mx, "-1.234567890123", 10, MPFR_RNDN);
    mpfr_set_str(ms, "-7.5", 10, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(&x, mx, &ctx) == 0);
    CHECK(ppf_import_mpfr_exact(&s, ms, &ctx) == 0);
    CHECK(ppf_set_prec(&y, 11) == 0);
    for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
        int mt, pt;
        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        mt = mpfr_setsign(my, mx, 0, (mpfr_rnd_t)r);
        pt = ppf_setsign(&y, &x, 0, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
        compare_result(&ctx, &y, my, pt, mt, 0);

        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        mt = mpfr_copysign(my, mx, ms, (mpfr_rnd_t)r);
        pt = ppf_copysign(&y, &x, &s, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
        compare_result(&ctx, &y, my, pt, mt, 0);
    }
    mpfr_clears(mx, my, ms, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&y); ppf_clear(&s); ppf_context_clear(&ctx);
}

static void integer_rounding_tests(gmp_randstate_t rs) {
    g_label = "integer-rounding";
    ppf_context_t ctx;
    ppf_t x, got, ip, fp;
    mpfr_t mx, mr, mi, mf;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&ip, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&fp, 53, &ctx) == 0);
    mpfr_inits2(53, mx, mr, mi, mf, (mpfr_ptr)0);

    for (int i = 0; i < 1800; ++i) {
        mpfr_prec_t px = 2 + gmp_urandomm_ui(rs, 420);
        mpfr_prec_t pr = 2 + gmp_urandomm_ui(rs, 220);
        random_exact(rs, mx, &x, &ctx, px, -300, 300);
        CHECK(ppf_set_prec(&got, pr) == 0);
        CHECK(ppf_set_prec(&ip, pr) == 0);
        CHECK(ppf_set_prec(&fp, pr) == 0);
        mpfr_set_prec(mr, pr); mpfr_set_prec(mi, pr); mpfr_set_prec(mf, pr);

        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            int mt, pt;
            ppf_rnd_t rr = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            g_label = "rint";
            mt = mpfr_rint(mr, mx, (mpfr_rnd_t)r);
            pt = ppf_rint(&got, &x, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 1);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            g_label = "rint_ceil";
            mt = mpfr_rint_ceil(mr, mx, (mpfr_rnd_t)r);
            pt = ppf_rint_ceil(&got, &x, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            g_label = "rint_floor";
            mt = mpfr_rint_floor(mr, mx, (mpfr_rnd_t)r);
            pt = ppf_rint_floor(&got, &x, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            g_label = "rint_round";
            mt = mpfr_rint_round(mr, mx, (mpfr_rnd_t)r);
            pt = ppf_rint_round(&got, &x, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            g_label = "rint_roundeven";
            mt = mpfr_rint_roundeven(mr, mx, (mpfr_rnd_t)r);
            pt = ppf_rint_roundeven(&got, &x, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            g_label = "rint_trunc";
            mt = mpfr_rint_trunc(mr, mx, (mpfr_rnd_t)r);
            pt = ppf_rint_trunc(&got, &x, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            g_label = "frac";
            mt = mpfr_frac(mr, mx, (mpfr_rnd_t)r);
            pt = ppf_frac(&got, &x, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            g_label = "modf";
            mt = mpfr_modf(mi, mf, mx, (mpfr_rnd_t)r);
            pt = ppf_modf(&ip, &fp, &x, rr, &ctx);
            {
                mpfr_flag_snapshot_t flags = snapshot_mpfr_flags();
                mpfr_t oi, of;
                mpfr_init2(oi, pr); mpfr_init2(of, pr);
                (void)ppf_get_mpfr(oi, &ip, MPFR_RNDN);
                (void)ppf_get_mpfr(of, &fp, MPFR_RNDN);
                if (!same_value(oi, mi) || !same_value(of, mf)) {
                    fprintf(stderr, "modf mismatch rnd=%d pt=%d mt=%d\n", r, pt, mt);
                    mpfr_printf("x=%Ra\nexpected_i=%Ra got_i=%Ra\nexpected_f=%Ra got_f=%Ra\n", mx, mi, oi, mf, of);
                    ppf_dump_file(stderr, &ip); ppf_dump_file(stderr, &fp);
                    exit(1);
                }
                CHECK(pt == mt);
                compare_flags(&ctx, flags);
                mpfr_clear(oi); mpfr_clear(of);
            }
        }

        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        {
            g_label = "ceil";
            int mt = mpfr_ceil(mr, mx);
            int pt = ppf_ceil(&got, &x, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 1);
        }
        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        {
            g_label = "floor";
            int mt = mpfr_floor(mr, mx);
            int pt = ppf_floor(&got, &x, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 1);
        }
        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        {
            g_label = "round";
            int mt = mpfr_round(mr, mx);
            int pt = ppf_round(&got, &x, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 1);
        }
        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        {
            g_label = "roundeven";
            int mt = mpfr_roundeven(mr, mx);
            int pt = ppf_roundeven(&got, &x, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 1);
        }
        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        {
            g_label = "trunc";
            int mt = mpfr_trunc(mr, mx);
            int pt = ppf_trunc(&got, &x, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 1);
        }
        CHECK(ppf_integer_p(&x) == !!mpfr_integer_p(mx));
        CHECK(ppf_min_prec(&x) == (ppf_prec_t)mpfr_min_prec(mx));
    }

    mpfr_clears(mx, mr, mi, mf, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&got); ppf_clear(&ip); ppf_clear(&fp);
    ppf_context_clear(&ctx);
}


static void exhaustive_same_precision_utility_tests(void) {
    ppf_context_t ctx;
    ppf_t x, got, ip, fp, alias;
    mpfr_t mx, mr, mi, mf, ma;
    mpz_t z;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 2, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 2, &ctx) == 0);
    CHECK(ppf_init2_ctx(&ip, 2, &ctx) == 0);
    CHECK(ppf_init2_ctx(&fp, 2, &ctx) == 0);
    CHECK(ppf_init2_ctx(&alias, 2, &ctx) == 0);
    mpfr_inits2(2, mx, mr, mi, mf, ma, (mpfr_ptr)0);
    mpz_init(z);

    for (mpfr_prec_t p = 2; p <= 10; ++p) {
        unsigned long lo = 1UL << (p - 1);
        unsigned long hi = (1UL << p) - 1UL;
        CHECK(ppf_set_prec(&x, p) == 0);
        CHECK(ppf_set_prec(&got, p) == 0);
        CHECK(ppf_set_prec(&ip, p) == 0);
        CHECK(ppf_set_prec(&fp, p) == 0);
        CHECK(ppf_set_prec(&alias, p) == 0);
        mpfr_set_prec(mx, p); mpfr_set_prec(mr, p);
        mpfr_set_prec(mi, p); mpfr_set_prec(mf, p); mpfr_set_prec(ma, p);
        for (unsigned long m = lo; m <= hi; ++m) {
            for (int sign = -1; sign <= 1; sign += 2) {
                mpz_set_ui(z, m); if (sign < 0) mpz_neg(z, z);
                for (long exponent = -2; exponent <= (long)p + 2; ++exponent) {
                    ppf_exp_t base = (ppf_exp_t)exponent - (ppf_exp_t)p;
                    CHECK(ppf_set_z_2exp(&x, z, base, PPF_RNDN, &ctx) == 0);
                    CHECK(mpfr_set_z_2exp(mx, z, (mpfr_exp_t)base, MPFR_RNDN) == 0);
                    for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
                        ppf_rnd_t rr = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
                        int mt, pt;
                        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
                        g_label = "exhaustive-rint";
                        mt = mpfr_rint(mr, mx, (mpfr_rnd_t)r);
                        pt = ppf_rint(&got, &x, rr, &ctx);
                        compare_result(&ctx, &got, mr, pt, mt, 1);

                        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
                        g_label = "exhaustive-frac";
                        mt = mpfr_frac(mr, mx, (mpfr_rnd_t)r);
                        pt = ppf_frac(&got, &x, rr, &ctx);
                        compare_result(&ctx, &got, mr, pt, mt, 0);

                        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
                        g_label = "exhaustive-modf";
                        mt = mpfr_modf(mi, mf, mx, (mpfr_rnd_t)r);
                        pt = ppf_modf(&ip, &fp, &x, rr, &ctx);
                        {
                            mpfr_t oi, of;
                            mpfr_flag_snapshot_t flags = snapshot_mpfr_flags();
                            mpfr_init2(oi, p); mpfr_init2(of, p);
                            (void)ppf_get_mpfr(oi, &ip, MPFR_RNDN);
                            (void)ppf_get_mpfr(of, &fp, MPFR_RNDN);
                            CHECK(same_value(oi, mi)); CHECK(same_value(of, mf));
                            CHECK(pt == mt); compare_flags(&ctx, flags);
                            mpfr_clear(oi); mpfr_clear(of);
                        }
                    }
                    CHECK(ppf_set(&alias, &x, PPF_RNDN, &ctx) == 0);
                    mpfr_set(ma, mx, MPFR_RNDN);
                    mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
                    {
                        int mt = mpfr_roundeven(ma, ma);
                        int pt = ppf_roundeven(&alias, &alias, &ctx);
                        compare_result(&ctx, &alias, ma, pt, mt, 1);
                    }
                }
            }
        }
    }
    mpz_clear(z);
    mpfr_clears(mx, mr, mi, mf, ma, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&got); ppf_clear(&ip); ppf_clear(&fp); ppf_clear(&alias);
    ppf_context_clear(&ctx);
}

static void neighbor_minmax_tests(gmp_randstate_t rs) {
    g_label = "neighbor-minmax";
    ppf_context_t ctx;
    ppf_t x, y, got;
    mpfr_t mx, my, mr;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&y, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, mx, my, mr, (mpfr_ptr)0);

    for (int i = 0; i < 900; ++i) {
        mpfr_prec_t p = 2 + gmp_urandomm_ui(rs, 420);
        random_exact(rs, mx, &x, &ctx, p, -400, 400);
        random_exact(rs, my, &y, &ctx, p, -400, 400);
        CHECK(ppf_set_prec(&got, p) == 0);
        mpfr_set_prec(mr, p);

        CHECK(ppf_get_exp(&x, &ctx) == (ppf_exp_t)mpfr_get_exp(mx));
        {
            ppf_t ec;
            mpfr_t mec, out;
            ppf_exp_t target = x.exponent;
            if (target < ctx.emax) ++target;
            else if (target > ctx.emin) --target;
            CHECK(ppf_init2_ctx(&ec, p, &ctx) == 0);
            CHECK(ppf_set(&ec, &x, PPF_RNDN, &ctx) == 0);
            mpfr_init2(mec, p); mpfr_init2(out, p);
            mpfr_set(mec, mx, MPFR_RNDN);
            CHECK(ppf_set_exp(&ec, target, &ctx) == mpfr_set_exp(mec, (mpfr_exp_t)target));
            (void)ppf_get_mpfr(out, &ec, MPFR_RNDN);
            CHECK(same_value(out, mec));
            mpfr_clear(mec); mpfr_clear(out); ppf_clear(&ec);
        }

        {
            ppf_t copy;
            mpfr_t mcopy, out;
            CHECK(ppf_init2_ctx(&copy, p, &ctx) == 0);
            CHECK(ppf_set(&copy, &x, PPF_RNDN, &ctx) == 0);
            mpfr_init2(mcopy, p); mpfr_init2(out, p); mpfr_set(mcopy, mx, MPFR_RNDN);
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mpfr_nextabove(mcopy); ppf_nextabove(&copy, &ctx);
            (void)ppf_get_mpfr(out, &copy, MPFR_RNDN);
            CHECK(same_value(out, mcopy)); compare_flags(&ctx, snapshot_mpfr_flags());

            CHECK(ppf_set(&copy, &x, PPF_RNDN, &ctx) == 0); mpfr_set(mcopy, mx, MPFR_RNDN);
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mpfr_nextbelow(mcopy); ppf_nextbelow(&copy, &ctx);
            (void)ppf_get_mpfr(out, &copy, MPFR_RNDN);
            CHECK(same_value(out, mcopy)); compare_flags(&ctx, snapshot_mpfr_flags());

            CHECK(ppf_set(&copy, &x, PPF_RNDN, &ctx) == 0); mpfr_set(mcopy, mx, MPFR_RNDN);
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mpfr_nexttoward(mcopy, my); ppf_nexttoward(&copy, &y, &ctx);
            (void)ppf_get_mpfr(out, &copy, MPFR_RNDN);
            CHECK(same_value(out, mcopy)); compare_flags(&ctx, snapshot_mpfr_flags());
            mpfr_clear(mcopy); mpfr_clear(out); ppf_clear(&copy);
        }

        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            int mt, pt;
            ppf_rnd_t rr = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_min(mr, mx, my, (mpfr_rnd_t)r);
            pt = ppf_min(&got, &x, &y, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_max(mr, mx, my, (mpfr_rnd_t)r);
            pt = ppf_max(&got, &x, &y, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_dim(mr, mx, my, (mpfr_rnd_t)r);
            pt = ppf_dim(&got, &x, &y, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);
        }
    }

    mpfr_clears(mx, my, mr, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&y); ppf_clear(&got); ppf_context_clear(&ctx);
}

static void remainder_tests(gmp_randstate_t rs) {
    g_label = "remainder";
    ppf_context_t ctx;
    ppf_t x, y, got;
    mpfr_t mx, my, mr;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&y, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, mx, my, mr, (mpfr_ptr)0);

    for (int i = 0; i < 1800; ++i) {
        mpfr_prec_t px = 2 + gmp_urandomm_ui(rs, 420);
        mpfr_prec_t py = 2 + gmp_urandomm_ui(rs, 420);
        mpfr_prec_t pr = 2 + gmp_urandomm_ui(rs, 260);
        random_exact(rs, mx, &x, &ctx, px, -600, 600);
        do { random_exact(rs, my, &y, &ctx, py, -600, 600); }
        while (mpfr_zero_p(my));
        CHECK(ppf_set_prec(&got, pr) == 0); mpfr_set_prec(mr, pr);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            ppf_rnd_t rr = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
            int mt, pt; long mq = 0, pq = 0;
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_fmod(mr, mx, my, (mpfr_rnd_t)r);
            pt = ppf_fmod(&got, &x, &y, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_remainder(mr, mx, my, (mpfr_rnd_t)r);
            pt = ppf_remainder(&got, &x, &y, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_fmodquo(mr, &mq, mx, my, (mpfr_rnd_t)r);
            pt = ppf_fmodquo(&got, &pq, &x, &y, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);
            CHECK(pq == mq);

            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_remquo(mr, &mq, mx, my, (mpfr_rnd_t)r);
            pt = ppf_remquo(&got, &pq, &x, &y, rr, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);
            CHECK(pq == mq);
        }
        {
            unsigned long u = 1 + gmp_urandomm_ui(rs, ULONG_MAX);
            int mt, pt;
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_fmod_ui(mr, mx, u, MPFR_RNDN);
            pt = ppf_fmod_ui(&got, &x, u, PPF_RNDN, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);
        }
    }

    /* Huge phase gap: the native path must use modular powers instead of a
       giant shifted numerator. */
    {
        mpz_t a, b;
        mpz_inits(a, b, NULL);
        mpz_set_ui(a, 123456789); mpz_set_ui(b, 1000003);
        mpfr_set_prec(mx, 128); mpfr_set_prec(my, 128); mpfr_set_prec(mr, 128);
        (void)mpfr_set_z_2exp(mx, a, 200000, MPFR_RNDN);
        (void)mpfr_set_z_2exp(my, b, -17, MPFR_RNDN);
        CHECK(ppf_import_mpfr_exact(&x, mx, &ctx) == 0);
        CHECK(ppf_import_mpfr_exact(&y, my, &ctx) == 0);
        CHECK(ppf_set_prec(&got, 128) == 0);
        {
            int mt = mpfr_fmod(mr, mx, my, MPFR_RNDN);
            int pt = ppf_fmod(&got, &x, &y, PPF_RNDN, &ctx);
            compare_result(&ctx, &got, mr, pt, mt, 0);
        }
        mpz_clears(a, b, NULL);
    }

    mpfr_clears(mx, my, mr, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&y); ppf_clear(&got); ppf_context_clear(&ctx);
}

int main(void) {
    gmp_randstate_t rs;
    gmp_randinit_mt(rs);
    gmp_randseed_ui(rs, 0x6A31D2B5UL);
    comparison_tests(rs);
    sign_tests();
    integer_rounding_tests(rs);
    exhaustive_same_precision_utility_tests();
    neighbor_minmax_tests(rs);
    remainder_tests(rs);
    gmp_randclear(rs);
    ppf_default_context_cleanup();
    puts("independent utility and remainder tests passed");
    return 0;
}
