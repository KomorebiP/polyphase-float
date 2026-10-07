/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static int sign_of(int x) { return (x > 0) - (x < 0); }

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        /* MPFR does not specify the sign of every NaN created by an
           invalid arithmetic operation.  Match the value category and
           exception state, not an implementation-specific NaN sign. */
        return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_zero_p(a) || mpfr_zero_p(b))
        return mpfr_zero_p(a) && mpfr_zero_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    return mpfr_equal_p(a, b) != 0;
}

static void compare_flags(ppf_context_t *ctx) {
    ppf_flags_t f = ppf_context_flags(ctx);
    CHECK((mpfr_inexflag_p() != 0) == ((f & PPF_FLAG_INEXACT) != 0));
    CHECK((mpfr_underflow_p() != 0) == ((f & PPF_FLAG_UNDERFLOW) != 0));
    CHECK((mpfr_overflow_p() != 0) == ((f & PPF_FLAG_OVERFLOW) != 0));
    CHECK((mpfr_divby0_p() != 0) == ((f & PPF_FLAG_DIVBY0) != 0));
    CHECK((mpfr_nanflag_p() != 0) == ((f & PPF_FLAG_NAN) != 0));
    CHECK((mpfr_erangeflag_p() != 0) == ((f & PPF_FLAG_ERANGE) != 0));
}

static void random_regular(gmp_randstate_t rs, mpfr_ptr m, ppf_t *p,
                           ppf_context_t *ctx, mpfr_prec_t precision) {
    mpz_t z;
    unsigned bits = 1u + (unsigned)gmp_urandomm_ui(rs, precision + 40u);
    long e = (long)gmp_urandomm_ui(rs, 801u) - 400L;
    mpz_init(z);
    mpz_urandomb(z, rs, bits);
    if (mpz_sgn(z) == 0) mpz_set_ui(z, 1);
    if (gmp_urandomb_ui(rs, 1u)) mpz_neg(z, z);
    mpfr_set_prec(m, precision);
    mpfr_set_z_2exp(m, z, e, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(p, m, ctx) == 0);
    mpz_clear(z);
}

static void check_result(const char *name, int mt, int pt,
                         mpfr_srcptr expected, const ppf_t *got,
                         mpfr_ptr exported, ppf_context_t *ctx) {
    CHECK(pt != PPF_TERNARY_ERROR);
    (void)ppf_get_mpfr(exported, got, MPFR_RNDN);
    if (!same_value(expected, exported) || sign_of(mt) != sign_of(pt)) {
        fprintf(stderr, "%s mismatch mt=%d pt=%d\n", name, mt, pt);
        mpfr_printf("expected=%Ra\nactual  =%Ra\n", expected, exported);
        exit(1);
    }
    compare_flags(ctx);
}

static double random_double(gmp_randstate_t rs) {
    long v = (long)gmp_urandomm_ui(rs, 2000001u) - 1000000L;
    int e = (int)gmp_urandomm_ui(rs, 81u) - 40;
    return ldexp((double)v, e);
}

static void random_z(gmp_randstate_t rs, mpz_ptr z) {
    mpz_urandomb(z, rs, 1u + gmp_urandomm_ui(rs, 320u));
    if (gmp_urandomb_ui(rs, 1u)) mpz_neg(z, z);
}

static void random_q(gmp_randstate_t rs, mpq_ptr q) {
    random_z(rs, mpq_numref(q));
    mpz_urandomb(mpq_denref(q), rs, 1u + gmp_urandomm_ui(rs, 180u));
    if (mpz_sgn(mpq_denref(q)) == 0) mpz_set_ui(mpq_denref(q), 1);
    mpq_canonicalize(q);
}

static void scalar_variants(gmp_randstate_t rs) {
    ppf_context_t ctx;
    ppf_t a, got;
    mpfr_t ma, mr, out;
    mpz_t z;
    mpq_t q;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, ma, mr, out, (mpfr_ptr)0);
    mpz_init(z); mpq_init(q);

    for (int i = 0; i < 140; ++i) {
        mpfr_prec_t pa = 20u + gmp_urandomm_ui(rs, 280u);
        mpfr_prec_t pr = 2u + gmp_urandomm_ui(rs, 230u);
        unsigned long u = gmp_urandomb_ui(rs, sizeof(unsigned long) * 8u);
        long si = (long)gmp_urandomb_ui(rs, sizeof(long) * 8u - 1u);
        double d = random_double(rs);
        if (gmp_urandomb_ui(rs, 1u)) si = -si;
        random_z(rs, z); random_q(rs, q);
        random_regular(rs, ma, &a, &ctx, pa);
        mpfr_set_prec(mr, pr); mpfr_set_prec(out, pr);
        CHECK(ppf_set_prec(&got, pr) == 0);

        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            ppf_rnd_t prnd = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
            int mt, pt;
#define RUN(NAME, MCALL, PCALL) do { \
    mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); \
    mt = (MCALL); pt = (PCALL); \
    check_result((NAME), mt, pt, mr, &got, out, &ctx); \
} while (0)
            RUN("add_ui", mpfr_add_ui(mr, ma, u, (mpfr_rnd_t)r),
                ppf_add_ui(&got, &a, u, prnd, &ctx));
            RUN("add_si", mpfr_add_si(mr, ma, si, (mpfr_rnd_t)r),
                ppf_add_si(&got, &a, si, prnd, &ctx));
            RUN("add_d", mpfr_add_d(mr, ma, d, (mpfr_rnd_t)r),
                ppf_add_d(&got, &a, d, prnd, &ctx));
            RUN("add_z", mpfr_add_z(mr, ma, z, (mpfr_rnd_t)r),
                ppf_add_z(&got, &a, z, prnd, &ctx));
            RUN("add_q", mpfr_add_q(mr, ma, q, (mpfr_rnd_t)r),
                ppf_add_q(&got, &a, q, prnd, &ctx));

            RUN("ui_sub", mpfr_ui_sub(mr, u, ma, (mpfr_rnd_t)r),
                ppf_ui_sub(&got, u, &a, prnd, &ctx));
            RUN("sub_ui", mpfr_sub_ui(mr, ma, u, (mpfr_rnd_t)r),
                ppf_sub_ui(&got, &a, u, prnd, &ctx));
            RUN("si_sub", mpfr_si_sub(mr, si, ma, (mpfr_rnd_t)r),
                ppf_si_sub(&got, si, &a, prnd, &ctx));
            RUN("sub_si", mpfr_sub_si(mr, ma, si, (mpfr_rnd_t)r),
                ppf_sub_si(&got, &a, si, prnd, &ctx));
            RUN("d_sub", mpfr_d_sub(mr, d, ma, (mpfr_rnd_t)r),
                ppf_d_sub(&got, d, &a, prnd, &ctx));
            RUN("sub_d", mpfr_sub_d(mr, ma, d, (mpfr_rnd_t)r),
                ppf_sub_d(&got, &a, d, prnd, &ctx));
            RUN("z_sub", mpfr_z_sub(mr, z, ma, (mpfr_rnd_t)r),
                ppf_z_sub(&got, z, &a, prnd, &ctx));
            RUN("sub_z", mpfr_sub_z(mr, ma, z, (mpfr_rnd_t)r),
                ppf_sub_z(&got, &a, z, prnd, &ctx));
            RUN("sub_q", mpfr_sub_q(mr, ma, q, (mpfr_rnd_t)r),
                ppf_sub_q(&got, &a, q, prnd, &ctx));

            RUN("mul_ui", mpfr_mul_ui(mr, ma, u, (mpfr_rnd_t)r),
                ppf_mul_ui(&got, &a, u, prnd, &ctx));
            RUN("mul_si", mpfr_mul_si(mr, ma, si, (mpfr_rnd_t)r),
                ppf_mul_si(&got, &a, si, prnd, &ctx));
            RUN("mul_d", mpfr_mul_d(mr, ma, d, (mpfr_rnd_t)r),
                ppf_mul_d(&got, &a, d, prnd, &ctx));
            RUN("mul_z", mpfr_mul_z(mr, ma, z, (mpfr_rnd_t)r),
                ppf_mul_z(&got, &a, z, prnd, &ctx));
            RUN("mul_q", mpfr_mul_q(mr, ma, q, (mpfr_rnd_t)r),
                ppf_mul_q(&got, &a, q, prnd, &ctx));

            if (u == 0) u = 1;
            if (si == 0) si = 1;
            if (d == 0.0) d = 0.5;
            if (mpz_sgn(z) == 0) mpz_set_ui(z, 1);
            if (mpq_sgn(q) == 0) mpq_set_ui(q, 1, 3);
            RUN("ui_div", mpfr_ui_div(mr, u, ma, (mpfr_rnd_t)r),
                ppf_ui_div(&got, u, &a, prnd, &ctx));
            RUN("div_ui", mpfr_div_ui(mr, ma, u, (mpfr_rnd_t)r),
                ppf_div_ui(&got, &a, u, prnd, &ctx));
            RUN("si_div", mpfr_si_div(mr, si, ma, (mpfr_rnd_t)r),
                ppf_si_div(&got, si, &a, prnd, &ctx));
            RUN("div_si", mpfr_div_si(mr, ma, si, (mpfr_rnd_t)r),
                ppf_div_si(&got, &a, si, prnd, &ctx));
            RUN("d_div", mpfr_d_div(mr, d, ma, (mpfr_rnd_t)r),
                ppf_d_div(&got, d, &a, prnd, &ctx));
            RUN("div_d", mpfr_div_d(mr, ma, d, (mpfr_rnd_t)r),
                ppf_div_d(&got, &a, d, prnd, &ctx));
            RUN("div_z", mpfr_div_z(mr, ma, z, (mpfr_rnd_t)r),
                ppf_div_z(&got, &a, z, prnd, &ctx));
            RUN("div_q", mpfr_div_q(mr, ma, q, (mpfr_rnd_t)r),
                ppf_div_q(&got, &a, q, prnd, &ctx));
#undef RUN
        }
    }

    mpq_clear(q); mpz_clear(z);
    mpfr_clears(ma, mr, out, (mpfr_ptr)0);
    ppf_clear(&a); ppf_clear(&got); ppf_context_clear(&ctx);
}

static void fused_and_misc(gmp_randstate_t rs) {
    ppf_context_t ctx;
    ppf_t a,b,c,d,got;
    mpfr_t ma,mb,mc,md,mr,out;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a,53,&ctx)==0); CHECK(ppf_init2_ctx(&b,53,&ctx)==0);
    CHECK(ppf_init2_ctx(&c,53,&ctx)==0); CHECK(ppf_init2_ctx(&d,53,&ctx)==0);
    CHECK(ppf_init2_ctx(&got,53,&ctx)==0);
    mpfr_inits2(53,ma,mb,mc,md,mr,out,(mpfr_ptr)0);
    for(int i=0;i<180;++i){
        mpfr_prec_t p=20+gmp_urandomm_ui(rs,220), pr=2+gmp_urandomm_ui(rs,220);
        random_regular(rs,ma,&a,&ctx,p); random_regular(rs,mb,&b,&ctx,p);
        random_regular(rs,mc,&c,&ctx,p); random_regular(rs,md,&d,&ctx,p);
        mpfr_set_prec(mr,pr);mpfr_set_prec(out,pr);CHECK(ppf_set_prec(&got,pr)==0);
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){
            int mt,pt;ppf_rnd_t prnd=ppf_rounding_from_mpfr((mpfr_rnd_t)r);
            mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            mt=mpfr_fmma(mr,ma,mb,mc,md,(mpfr_rnd_t)r);
            pt=ppf_fmma(&got,&a,&b,&c,&d,prnd,&ctx);
            check_result("fmma",mt,pt,mr,&got,out,&ctx);
            mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            mt=mpfr_fmms(mr,ma,mb,mc,md,(mpfr_rnd_t)r);
            pt=ppf_fmms(&got,&a,&b,&c,&d,prnd,&ctx);
            check_result("fmms",mt,pt,mr,&got,out,&ctx);
        }
    }
    for(unsigned long n=0;n<=120;++n){
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){
            int mt,pt;mpfr_set_prec(mr,80);mpfr_set_prec(out,80);CHECK(ppf_set_prec(&got,80)==0);
            mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            mt=mpfr_fac_ui(mr,n,(mpfr_rnd_t)r);
            pt=ppf_fac_ui(&got,n,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            check_result("fac_ui",mt,pt,mr,&got,out,&ctx);
        }
    }
    mpfr_set_ui(ma,3,MPFR_RNDN); CHECK(ppf_import_mpfr_exact(&a,ma,&ctx)==0);
    for(unsigned long sh=0;sh<300;sh+=17){
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){
            int mt,pt;mpfr_set_prec(mr,37);mpfr_set_prec(out,37);CHECK(ppf_set_prec(&got,37)==0);
            mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            mt=mpfr_mul_2ui(mr,ma,sh,(mpfr_rnd_t)r);
            pt=ppf_mul_2ui(&got,&a,sh,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            check_result("mul_2ui",mt,pt,mr,&got,out,&ctx);
            mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            mt=mpfr_div_2ui(mr,ma,sh,(mpfr_rnd_t)r);
            pt=ppf_div_2ui(&got,&a,sh,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            check_result("div_2ui",mt,pt,mr,&got,out,&ctx);
        }
    }
    mpfr_clears(ma,mb,mc,md,mr,out,(mpfr_ptr)0);
    ppf_clear(&a);ppf_clear(&b);ppf_clear(&c);ppf_clear(&d);ppf_clear(&got);ppf_context_clear(&ctx);
}

static void alias_smoke(void) {
    ppf_context_t ctx; ppf_t x; mpfr_t mx, ref, out; mpq_t q;
    CHECK(ppf_context_init(&ctx)==0); CHECK(ppf_init2_ctx(&x,53,&ctx)==0);
    mpfr_inits2(53,mx,ref,out,(mpfr_ptr)0); mpq_init(q);mpq_set_si(q,7,11);
    mpfr_set_d(mx,-3.25,MPFR_RNDN);CHECK(ppf_import_mpfr_exact(&x,mx,&ctx)==0);
    int mt=mpfr_add_q(ref,mx,q,MPFR_RNDN);int pt=ppf_add_q(&x,&x,q,PPF_RNDN,&ctx);
    check_result("alias add_q",mt,pt,ref,&x,out,&ctx);
    mpfr_set_d(mx,-3.25,MPFR_RNDN);CHECK(ppf_import_mpfr_exact(&x,mx,&ctx)==0);
    mt=mpfr_mul_d(ref,mx,-2.0,MPFR_RNDN);pt=ppf_mul_d(&x,&x,-2.0,PPF_RNDN,&ctx);
    check_result("alias mul_d",mt,pt,ref,&x,out,&ctx);
    mpq_clear(q);mpfr_clears(mx,ref,out,(mpfr_ptr)0);ppf_clear(&x);ppf_context_clear(&ctx);
}


static void special_and_range_cases(void) {
    ppf_context_t ctx;
    ppf_t a, b, c, d, got;
    mpfr_t ma, mb, mc, md, mr, out;
    mpq_t q;
    mpfr_exp_t old_emin, old_emax;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&c, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&d, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, ma, mb, mc, md, mr, out, (mpfr_ptr)0);
    mpq_init(q);

#define RUN_SPECIAL(NAME, MCALL, PCALL) do { \
    int mt_, pt_; \
    mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); \
    mt_ = (MCALL); pt_ = (PCALL); \
    check_result((NAME), mt_, pt_, mr, &got, out, &ctx); \
} while (0)

    /* A scalar rational zero has no negative-zero encoding.  The binary
       operand's zero sign still matters in add/subtract and product cases. */
    mpq_set_ui(q, 0, 1);
    for (int sign = -1; sign <= 1; sign += 2) {
        mpfr_set_zero(ma, sign);
        ppf_set_zero(&a, sign);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            ppf_rnd_t prnd = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
            RUN_SPECIAL("zero add_q", mpfr_add_q(mr, ma, q, (mpfr_rnd_t)r),
                        ppf_add_q(&got, &a, q, prnd, &ctx));
            RUN_SPECIAL("zero sub_q", mpfr_sub_q(mr, ma, q, (mpfr_rnd_t)r),
                        ppf_sub_q(&got, &a, q, prnd, &ctx));
            RUN_SPECIAL("zero mul_q", mpfr_mul_q(mr, ma, q, (mpfr_rnd_t)r),
                        ppf_mul_q(&got, &a, q, prnd, &ctx));
            RUN_SPECIAL("zero div_q", mpfr_div_q(mr, ma, q, (mpfr_rnd_t)r),
                        ppf_div_q(&got, &a, q, prnd, &ctx));
        }
    }

    /* Rational and C-double special-value propagation. */
    mpq_set_si(q, -7, 9);
    mpfr_set_inf(ma, 1); ppf_set_inf(&a, 1);
    RUN_SPECIAL("inf add_q", mpfr_add_q(mr, ma, q, MPFR_RNDN),
                ppf_add_q(&got, &a, q, PPF_RNDN, &ctx));
    RUN_SPECIAL("inf mul_q", mpfr_mul_q(mr, ma, q, MPFR_RNDN),
                ppf_mul_q(&got, &a, q, PPF_RNDN, &ctx));
    mpq_set_ui(q, 0, 1);
    RUN_SPECIAL("inf times qzero", mpfr_mul_q(mr, ma, q, MPFR_RNDN),
                ppf_mul_q(&got, &a, q, PPF_RNDN, &ctx));
    mpfr_set_d(ma, -3.5, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(&a, ma, &ctx) == 0);
    RUN_SPECIAL("finite div qzero", mpfr_div_q(mr, ma, q, MPFR_RNDN),
                ppf_div_q(&got, &a, q, PPF_RNDN, &ctx));
    mpfr_set_nan(ma); ppf_set_nan(&a, -1);
    RUN_SPECIAL("nan add_d", mpfr_add_d(mr, ma, -0.0, MPFR_RNDN),
                ppf_add_d(&got, &a, -0.0, PPF_RNDN, &ctx));

    /* Every destination/input alias arrangement for the two-product fused
       operations. */
    mpfr_set_d(ma, 1.25, MPFR_RNDN);
    mpfr_set_d(mb, -2.5, MPFR_RNDN);
    mpfr_set_d(mc, 3.75, MPFR_RNDN);
    mpfr_set_d(md, -0.625, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(&a, ma, &ctx) == 0);
    CHECK(ppf_import_mpfr_exact(&b, mb, &ctx) == 0);
    CHECK(ppf_import_mpfr_exact(&c, mc, &ctx) == 0);
    CHECK(ppf_import_mpfr_exact(&d, md, &ctx) == 0);
    for (int which = 0; which < 4; ++which) {
        ppf_t *dst = which == 0 ? &a : which == 1 ? &b : which == 2 ? &c : &d;
        mpfr_ptr mdst = which == 0 ? ma : which == 1 ? mb : which == 2 ? mc : md;
        mpfr_set_d(ma, 1.25, MPFR_RNDN);
        mpfr_set_d(mb, -2.5, MPFR_RNDN);
        mpfr_set_d(mc, 3.75, MPFR_RNDN);
        mpfr_set_d(md, -0.625, MPFR_RNDN);
        CHECK(ppf_import_mpfr_exact(&a, ma, &ctx) == 0);
        CHECK(ppf_import_mpfr_exact(&b, mb, &ctx) == 0);
        CHECK(ppf_import_mpfr_exact(&c, mc, &ctx) == 0);
        CHECK(ppf_import_mpfr_exact(&d, md, &ctx) == 0);
        mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        int mt = mpfr_fmma(mdst, ma, mb, mc, md, MPFR_RNDN);
        int pt = ppf_fmma(dst, &a, &b, &c, &d, PPF_RNDN, &ctx);
        check_result("fmma alias", mt, pt, mdst, dst, out, &ctx);
    }

    /* Exercise individual product overflow/underflow and final range
       handling under a deliberately narrow exponent range. */
    old_emin = mpfr_get_emin();
    old_emax = mpfr_get_emax();
    CHECK(mpfr_set_emin(-12) == 0);
    CHECK(mpfr_set_emax(12) == 0);
    CHECK(ppf_context_set_range(&ctx, -12, 12) == 0);
    mpfr_set_ui_2exp(ma, 1, 8, MPFR_RNDN);
    mpfr_set_ui_2exp(mb, 1, 8, MPFR_RNDN);
    mpfr_set_si(mc, -1, MPFR_RNDN);
    mpfr_set_ui(md, 1, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(&a, ma, &ctx) == 0);
    CHECK(ppf_import_mpfr_exact(&b, mb, &ctx) == 0);
    CHECK(ppf_import_mpfr_exact(&c, mc, &ctx) == 0);
    CHECK(ppf_import_mpfr_exact(&d, md, &ctx) == 0);
    for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
        ppf_rnd_t prnd = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
        RUN_SPECIAL("narrow fmma", mpfr_fmma(mr, ma, mb, mc, md, (mpfr_rnd_t)r),
                    ppf_fmma(&got, &a, &b, &c, &d, prnd, &ctx));
        RUN_SPECIAL("narrow fmms", mpfr_fmms(mr, ma, mb, mc, md, (mpfr_rnd_t)r),
                    ppf_fmms(&got, &a, &b, &c, &d, prnd, &ctx));
    }

    /* On platforms where unsigned long exceeds signed long, force the
       enormous-shift branch without attempting a physically enormous
       allocation. */
#if ULONG_MAX > LONG_MAX
    mpfr_set_d(ma, -1.5, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(&a, ma, &ctx) == 0);
    for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
        ppf_rnd_t prnd = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
        RUN_SPECIAL("huge mul_2ui",
                    mpfr_mul_2ui(mr, ma, ULONG_MAX, (mpfr_rnd_t)r),
                    ppf_mul_2ui(&got, &a, ULONG_MAX, prnd, &ctx));
        RUN_SPECIAL("huge div_2ui",
                    mpfr_div_2ui(mr, ma, ULONG_MAX, (mpfr_rnd_t)r),
                    ppf_div_2ui(&got, &a, ULONG_MAX, prnd, &ctx));
    }
#endif
    CHECK(mpfr_set_emin(old_emin) == 0);
    CHECK(mpfr_set_emax(old_emax) == 0);
    CHECK(ppf_context_set_range(&ctx, PPF_EMIN_DEFAULT, PPF_EMAX_DEFAULT) == 0);

#undef RUN_SPECIAL
    mpq_clear(q);
    mpfr_clears(ma, mb, mc, md, mr, out, (mpfr_ptr)0);
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&c); ppf_clear(&d); ppf_clear(&got);
    ppf_context_clear(&ctx);
}

int main(void) {
    gmp_randstate_t rs;
    gmp_randinit_mt(rs); gmp_randseed_ui(rs, 0xA17A5EEDu);
    scalar_variants(rs);
    fused_and_misc(rs);
    alias_smoke();
    special_and_range_cases();
    gmp_randclear(rs);
    ppf_free_constant_caches();
    mpfr_mp_memory_cleanup();
    puts("arithmetic variant tests passed");
    return 0;
}
