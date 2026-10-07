/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"
#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static int sign_of(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_inf_p(a) || mpfr_inf_p(b))
        return mpfr_inf_p(a) && mpfr_inf_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    if (!mpfr_equal_p(a, b)) return 0;
    return !mpfr_zero_p(a) || mpfr_signbit(a) == mpfr_signbit(b);
}

typedef struct {
    int underflow, overflow, divby0, nanflag, inexact, erange;
} flag_snapshot_t;

static flag_snapshot_t snapshot_mpfr_flags(void) {
    flag_snapshot_t f;
    f.underflow = mpfr_underflow_p() != 0;
    f.overflow = mpfr_overflow_p() != 0;
    f.divby0 = mpfr_divby0_p() != 0;
    f.nanflag = mpfr_nanflag_p() != 0;
    f.inexact = mpfr_inexflag_p() != 0;
    f.erange = mpfr_erangeflag_p() != 0;
    return f;
}

static void compare_snapshot(ppf_context_t *ctx, flag_snapshot_t f) {
    ppf_flags_t p = ppf_context_flags(ctx);
    CHECK(((p & PPF_FLAG_UNDERFLOW) != 0) == f.underflow);
    CHECK(((p & PPF_FLAG_OVERFLOW) != 0) == f.overflow);
    CHECK(((p & PPF_FLAG_DIVBY0) != 0) == f.divby0);
    CHECK(((p & PPF_FLAG_NAN) != 0) == f.nanflag);
    CHECK(((p & PPF_FLAG_INEXACT) != 0) == f.inexact);
    CHECK(((p & PPF_FLAG_ERANGE) != 0) == f.erange);
}

static void wrapper_tests(void) {
    ppf_clear_flags();
    CHECK(ppf_flags_test(PPF_FLAGS_ALL) == 0);
    ppf_set_underflow(); ppf_set_overflow(); ppf_set_divby0();
    ppf_set_nanflag(); ppf_set_inexflag(); ppf_set_erangeflag();
    ppf_set_allocflag();
    CHECK(ppf_underflow_p() && ppf_overflow_p() && ppf_divby0_p());
    CHECK(ppf_nanflag_p() && ppf_inexflag_p() && ppf_erangeflag_p());
    CHECK(ppf_allocflag_p());
    {
        ppf_flags_t saved = ppf_flags_save();
        CHECK((saved & PPF_FLAGS_ALL) == PPF_FLAGS_ALL);
        ppf_flags_clear(PPF_FLAG_OVERFLOW | PPF_FLAG_NAN);
        CHECK(!ppf_overflow_p() && !ppf_nanflag_p());
        ppf_flags_restore(saved, PPF_FLAGS_ALL);
        CHECK(ppf_flags_test(PPF_FLAGS_ALL) == PPF_FLAGS_ALL);
    }
    ppf_clear_underflow(); ppf_clear_overflow(); ppf_clear_divby0();
    ppf_clear_nanflag(); ppf_clear_inexflag(); ppf_clear_erangeflag();
    ppf_clear_allocflag();
    CHECK(ppf_flags_test(PPF_FLAGS_ALL) == 0);

    {
        ppf_exp_t oldemin = ppf_get_emin(), oldemax = ppf_get_emax();
        CHECK(ppf_set_emin(-1234) == 0);
        CHECK(ppf_set_emax(4321) == 0);
        CHECK(ppf_get_emin() == -1234 && ppf_get_emax() == 4321);
        CHECK(ppf_set_emin(5000) != 0);
        CHECK(ppf_set_emax(-2000) != 0);
        CHECK(ppf_set_emin(oldemin) == 0);
        CHECK(ppf_set_emax(oldemax) == 0);
        CHECK(ppf_get_emin_min() <= oldemin);
        CHECK(ppf_get_emax_max() >= oldemax);
    }
}

static void set_threshold_value(mpfr_ptr y, mpfr_exp_t emin, int which,
                                int negative) {
    mpfr_set_ui_2exp(y, 1, emin - 2, MPFR_RNDN); /* halfway */
    if (which != 0) {
        mpfr_t eps;
        mpfr_init2(eps, mpfr_get_prec(y));
        mpfr_set_ui_2exp(eps, 1, emin - 30, MPFR_RNDN);
        if (which > 0) mpfr_add(y, y, eps, MPFR_RNDN);
        else mpfr_sub(y, y, eps, MPFR_RNDN);
        mpfr_clear(eps);
    }
    if (negative) mpfr_neg(y, y, MPFR_RNDN);
}

static void compare_check_range_case(mpfr_srcptr y, mpfr_prec_t precision,
                                     mpfr_exp_t emin, mpfr_exp_t emax,
                                     mpfr_rnd_t rnd) {
    mpfr_exp_t oldemin = mpfr_get_emin(), oldemax = mpfr_get_emax();
    mpfr_t mx, mout;
    ppf_context_t ctx;
    ppf_t px;
    int mt, pt;
    flag_snapshot_t flags;

    mpfr_init2(mx, precision); mpfr_init2(mout, precision);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&px, precision, &ctx) == 0);

    CHECK(mpfr_set_emin(oldemin) == 0 && mpfr_set_emax(oldemax) == 0);
    mt = mpfr_set(mx, y, rnd);
    CHECK(ppf_import_mpfr_exact(&px, mx, &ctx) == 0);

    CHECK(mpfr_set_emin(emin) == 0 && mpfr_set_emax(emax) == 0);
    CHECK(ppf_context_set_range(&ctx, (ppf_exp_t)emin, (ppf_exp_t)emax) == 0);
    mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    {
        int incoming = mt;
        mt = mpfr_check_range(mx, incoming, rnd);
        flags = snapshot_mpfr_flags();
        pt = ppf_check_range(&px, incoming,
                             ppf_rounding_from_mpfr(rnd), &ctx);
    }
    mpfr_clear_flags();
    (void)ppf_get_mpfr(mout, &px, MPFR_RNDN);
    if (!same_value(mx, mout) || sign_of(mt) != sign_of(pt)) {
        fprintf(stderr, "check_range mismatch p=%lu emin=%ld emax=%ld rnd=%d\n",
                (unsigned long)precision, (long)emin, (long)emax, (int)rnd);
        mpfr_printf("y=%Ra\nmpfr=%Ra\nppf=%Ra\n", y, mx, mout);
        exit(1);
    }
    compare_snapshot(&ctx, flags);

    CHECK(mpfr_set_emin(oldemin) == 0 && mpfr_set_emax(oldemax) == 0);
    ppf_clear(&px); ppf_context_clear(&ctx);
    mpfr_clear(mx); mpfr_clear(mout);
}

static void check_range_tests(void) {
    mpfr_t y;
    const mpfr_exp_t emin = -20, emax = 20;
    mpfr_init2(y, 160);
    for (int neg = 0; neg < 2; ++neg) {
        for (int which = -1; which <= 1; ++which) {
            set_threshold_value(y, emin, which, neg);
            for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r)
                compare_check_range_case(y, 17, emin, emax, (mpfr_rnd_t)r);
        }
    }
    for (int neg = 0; neg < 2; ++neg) {
        mpfr_set_ui_2exp(y, 1, emax, MPFR_RNDN);
        if (neg) mpfr_neg(y, y, MPFR_RNDN);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r)
            compare_check_range_case(y, 23, emin, emax, (mpfr_rnd_t)r);
        mpfr_set_ui_2exp(y, 3, emax - 1, MPFR_RNDN);
        if (neg) mpfr_neg(y, y, MPFR_RNDN);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r)
            compare_check_range_case(y, 23, emin, emax, (mpfr_rnd_t)r);
    }
    mpfr_clear(y);
}

static void compare_subnormalize_case(mpfr_srcptr y, mpfr_prec_t precision,
                                      mpfr_exp_t emin, mpfr_exp_t emax,
                                      mpfr_rnd_t rnd) {
    mpfr_exp_t oldemin = mpfr_get_emin(), oldemax = mpfr_get_emax();
    mpfr_t mx, mout;
    ppf_context_t ctx;
    ppf_t px;
    int incoming, mt, pt;
    flag_snapshot_t flags;

    mpfr_init2(mx, precision); mpfr_init2(mout, precision);
    CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&px,precision,&ctx)==0);
    CHECK(mpfr_set_emin(oldemin)==0 && mpfr_set_emax(oldemax)==0);
    incoming=mpfr_set(mx,y,rnd);
    CHECK(ppf_import_mpfr_exact(&px,mx,&ctx)==0);
    CHECK(mpfr_set_emin(emin)==0 && mpfr_set_emax(emax)==0);
    CHECK(ppf_context_set_range(&ctx,(ppf_exp_t)emin,(ppf_exp_t)emax)==0);
    mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
    mt=mpfr_subnormalize(mx,incoming,rnd);
    flags=snapshot_mpfr_flags();
    pt=ppf_subnormalize(&px,incoming,ppf_rounding_from_mpfr(rnd),&ctx);
    mpfr_clear_flags();(void)ppf_get_mpfr(mout,&px,MPFR_RNDN);
    if(!same_value(mx,mout)||sign_of(mt)!=sign_of(pt)){
        fprintf(stderr,"subnormalize mismatch p=%lu emin=%ld rnd=%d incoming=%d\n",
                (unsigned long)precision,(long)emin,(int)rnd,incoming);
        mpfr_printf("y=%Ra\nx=%Ra\nppf=%Ra\n",y,mx,mout);exit(1);
    }
    compare_snapshot(&ctx,flags);
    CHECK(mpfr_set_emin(oldemin)==0 && mpfr_set_emax(oldemax)==0);
    ppf_clear(&px);ppf_context_clear(&ctx);mpfr_clears(mx,mout,(mpfr_ptr)0);
}

static void subnormalize_tests(void) {
    gmp_randstate_t rs;
    mpfr_t y;
    const mpfr_exp_t emin=-40, emax=100;
    gmp_randinit_mt(rs);gmp_randseed_ui(rs,0x5355424eUL);
    mpfr_init2(y,180);
    for(int iter=0;iter<450;++iter){
        mpfr_prec_t p=2+gmp_urandomm_ui(rs,60);
        mpfr_exp_t e=emin+(mpfr_exp_t)gmp_urandomm_ui(rs,(unsigned long)p-1);
        mpfr_set_prec(y,p+80);mpfr_urandomb(y,rs);if(mpfr_zero_p(y))mpfr_set_ui(y,1,MPFR_RNDN);
        CHECK(mpfr_set_exp(y,e)==0);if(gmp_urandomm_ui(rs,2))mpfr_neg(y,y,MPFR_RNDN);
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r)
            compare_subnormalize_case(y,p,emin,emax,(mpfr_rnd_t)r);
    }
    mpfr_clear(y);gmp_randclear(rs);
}

int main(void) {
    wrapper_tests();
    check_range_tests();
    subnormalize_tests();
    puts("RANGE AND FLAG CONTROL TESTS PASSED");
    return 0;
}
