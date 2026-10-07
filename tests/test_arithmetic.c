/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static int sign_of(int x) { return (x > 0) - (x < 0); }

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        return mpfr_nan_p(a) && mpfr_nan_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
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
}

static void random_operand(gmp_randstate_t rs, mpfr_ptr m, ppf_t *p,
                           ppf_context_t *ctx, mpfr_prec_t precision) {
    mpz_t z;
    unsigned bits = 1u + (unsigned)gmp_urandomm_ui(rs, precision + 37);
    long e = (long)gmp_urandomm_ui(rs, 801) - 400;
    mpz_init(z);
    mpz_urandomb(z, rs, bits);
    if (mpz_sgn(z) == 0) mpz_set_ui(z, 1);
    if (gmp_urandomb_ui(rs, 1)) mpz_neg(z, z);
    mpfr_set_z_2exp(m, z, e, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(p, m, ctx) == 0);
    mpz_clear(z);
}

static void compare_binary(gmp_randstate_t rs, int which) {
    ppf_context_t ctx;
    ppf_t a, b, got;
    mpfr_t ma, mb, mr, out;
    const mpfr_prec_t source_precs[] = {17,53,64,113,257,521};
    const mpfr_prec_t output_precs[] = {2,11,24,53,80,113,300};

    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, ma, mb, mr, out, (mpfr_ptr)0);

    for (int i = 0; i < 700; ++i) {
        mpfr_prec_t pa = source_precs[gmp_urandomm_ui(rs, 6)];
        mpfr_prec_t pb = source_precs[gmp_urandomm_ui(rs, 6)];
        mpfr_prec_t pr = output_precs[gmp_urandomm_ui(rs, 7)];
        mpfr_set_prec(ma, pa); mpfr_set_prec(mb, pb);
        mpfr_set_prec(mr, pr); mpfr_set_prec(out, pr);
        random_operand(rs, ma, &a, &ctx, pa);
        random_operand(rs, mb, &b, &ctx, pb);
        CHECK(ppf_set_prec(&got, pr) == 0);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            int mt, pt;
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            if (which == 0) {
                mt = mpfr_add(mr, ma, mb, (mpfr_rnd_t)r);
                pt = ppf_add(&got, &a, &b, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
            } else if (which == 1) {
                mt = mpfr_sub(mr, ma, mb, (mpfr_rnd_t)r);
                pt = ppf_sub(&got, &a, &b, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
            } else if (which == 2) {
                mt = mpfr_mul(mr, ma, mb, (mpfr_rnd_t)r);
                pt = ppf_mul(&got, &a, &b, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
            } else {
                mt = mpfr_div(mr, ma, mb, (mpfr_rnd_t)r);
                pt = ppf_div(&got, &a, &b, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
            }
            CHECK(pt != PPF_TERNARY_ERROR);
            (void)ppf_get_mpfr(out, &got, MPFR_RNDN);
            if (!same_value(mr, out)) {
                fprintf(stderr, "binary mismatch which=%d iter=%d rnd=%d\n", which, i, r);
                mpfr_printf("ref=%Ra\ngot=%Ra\na=%Ra\nb=%Ra\n", mr, out, ma, mb);
                exit(1);
            }
            CHECK(sign_of(mt) == sign_of(pt));
            compare_flags(&ctx);
        }
    }

    mpfr_clears(ma, mb, mr, out, (mpfr_ptr)0);
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&got);
    ppf_context_clear(&ctx);
}

static void compare_unary(gmp_randstate_t rs, int which) {
    ppf_context_t ctx;
    ppf_t a, got;
    mpfr_t ma, mr, out;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, ma, mr, out, (mpfr_ptr)0);
    for (int i = 0; i < 500; ++i) {
        mpfr_prec_t pa = 20 + gmp_urandomm_ui(rs, 400);
        mpfr_prec_t pr = 2 + gmp_urandomm_ui(rs, 300);
        mpfr_set_prec(ma, pa); mpfr_set_prec(mr, pr); mpfr_set_prec(out, pr);
        random_operand(rs, ma, &a, &ctx, pa);
        if (which == 1 && mpfr_sgn(ma) < 0) {
            mpfr_neg(ma, ma, MPFR_RNDN);
            CHECK(ppf_import_mpfr_exact(&a, ma, &ctx) == 0);
        }
        CHECK(ppf_set_prec(&got, pr) == 0);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            int mt, pt;
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            if (which == 0) {
                mt = mpfr_sqr(mr, ma, (mpfr_rnd_t)r);
                pt = ppf_sqr(&got, &a, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
            } else {
                mt = mpfr_sqrt(mr, ma, (mpfr_rnd_t)r);
                pt = ppf_sqrt(&got, &a, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
            }
            CHECK(pt != PPF_TERNARY_ERROR);
            (void)ppf_get_mpfr(out, &got, MPFR_RNDN);
            CHECK(same_value(mr, out));
            CHECK(sign_of(mt) == sign_of(pt));
            compare_flags(&ctx);
        }
    }
    mpfr_clears(ma, mr, out, (mpfr_ptr)0);
    ppf_clear(&a); ppf_clear(&got); ppf_context_clear(&ctx);
}

static void compare_fused(gmp_randstate_t rs, int subtract) {
    ppf_context_t ctx;
    ppf_t a,b,c,got;
    mpfr_t ma,mb,mc,mr,out;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a,53,&ctx)==0);
    CHECK(ppf_init2_ctx(&b,53,&ctx)==0);
    CHECK(ppf_init2_ctx(&c,53,&ctx)==0);
    CHECK(ppf_init2_ctx(&got,53,&ctx)==0);
    mpfr_inits2(53,ma,mb,mc,mr,out,(mpfr_ptr)0);
    for(int i=0;i<350;++i){
        mpfr_prec_t pa=20+gmp_urandomm_ui(rs,250);
        mpfr_prec_t pb=20+gmp_urandomm_ui(rs,250);
        mpfr_prec_t pc=20+gmp_urandomm_ui(rs,250);
        mpfr_prec_t pr=2+gmp_urandomm_ui(rs,220);
        mpfr_set_prec(ma,pa);mpfr_set_prec(mb,pb);mpfr_set_prec(mc,pc);
        mpfr_set_prec(mr,pr);mpfr_set_prec(out,pr);
        random_operand(rs,ma,&a,&ctx,pa);random_operand(rs,mb,&b,&ctx,pb);
        random_operand(rs,mc,&c,&ctx,pc);CHECK(ppf_set_prec(&got,pr)==0);
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){
            int mt,pt;mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            if(subtract){mt=mpfr_fms(mr,ma,mb,mc,(mpfr_rnd_t)r);
                pt=ppf_fms(&got,&a,&b,&c,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);}
            else{mt=mpfr_fma(mr,ma,mb,mc,(mpfr_rnd_t)r);
                pt=ppf_fma(&got,&a,&b,&c,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);}
            CHECK(pt!=PPF_TERNARY_ERROR);(void)ppf_get_mpfr(out,&got,MPFR_RNDN);
            CHECK(same_value(mr,out));CHECK(sign_of(mt)==sign_of(pt));compare_flags(&ctx);
        }
    }
    mpfr_clears(ma,mb,mc,mr,out,(mpfr_ptr)0);
    ppf_clear(&a);ppf_clear(&b);ppf_clear(&c);ppf_clear(&got);ppf_context_clear(&ctx);
}

static void compare_reductions(gmp_randstate_t rs) {
    enum {N=8};
    ppf_context_t ctx; ppf_t pv[N], qv[N], got;
    const ppf_t *pp[N], *qq[N];
    mpfr_t mv[N], nv[N], mr, out;
    mpfr_ptr mp[N], np[N];
    CHECK(ppf_context_init(&ctx)==0); CHECK(ppf_init2_ctx(&got,113,&ctx)==0);
    mpfr_init2(mr,113);mpfr_init2(out,113);
    for(int j=0;j<N;++j){CHECK(ppf_init2_ctx(&pv[j],53,&ctx)==0);CHECK(ppf_init2_ctx(&qv[j],53,&ctx)==0);
        mpfr_init2(mv[j],53);mpfr_init2(nv[j],53);pp[j]=&pv[j];qq[j]=&qv[j];mp[j]=mv[j];np[j]=nv[j];}
    for(int iter=0;iter<180;++iter){
        mpfr_prec_t pr=10+gmp_urandomm_ui(rs,240);mpfr_set_prec(mr,pr);mpfr_set_prec(out,pr);CHECK(ppf_set_prec(&got,pr)==0);
        for(int j=0;j<N;++j){mpfr_prec_t p=20+gmp_urandomm_ui(rs,180);mpfr_set_prec(mv[j],p);mpfr_set_prec(nv[j],p);
            random_operand(rs,mv[j],&pv[j],&ctx,p);random_operand(rs,nv[j],&qv[j],&ctx,p);}
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){int mt,pt;mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            mt=mpfr_sum(mr,mp,N,(mpfr_rnd_t)r);pt=ppf_sum(&got,pp,N,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&got,MPFR_RNDN);CHECK(same_value(mr,out));CHECK(sign_of(mt)==sign_of(pt));compare_flags(&ctx);
            mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            mt=mpfr_dot(mr,mp,np,N,(mpfr_rnd_t)r);pt=ppf_dot(&got,pp,qq,N,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&got,MPFR_RNDN);CHECK(same_value(mr,out));CHECK(sign_of(mt)==sign_of(pt));compare_flags(&ctx);
        }
    }
    for(int j=0;j<N;++j){ppf_clear(&pv[j]);ppf_clear(&qv[j]);mpfr_clear(mv[j]);mpfr_clear(nv[j]);}
    ppf_clear(&got);ppf_context_clear(&ctx);mpfr_clear(mr);mpfr_clear(out);
}

static void special_and_alias_tests(void) {
    ppf_context_t ctx; ppf_t a,b,c; mpfr_t ma,mb,mr,out;
    CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&a,53,&ctx)==0);CHECK(ppf_init2_ctx(&b,53,&ctx)==0);CHECK(ppf_init2_ctx(&c,53,&ctx)==0);
    mpfr_inits2(53,ma,mb,mr,out,(mpfr_ptr)0);
    ppf_set_ui(&a,3,PPF_RNDN,&ctx);ppf_set_ui(&b,7,PPF_RNDN,&ctx);
    mpfr_set_ui(ma,3,MPFR_RNDN);mpfr_set_ui(mb,7,MPFR_RNDN);
    CHECK(sign_of(ppf_mul(&a,&a,&b,PPF_RNDN,&ctx))==sign_of(mpfr_mul(ma,ma,mb,MPFR_RNDN)));
    ppf_get_mpfr(out,&a,MPFR_RNDN);CHECK(mpfr_equal_p(ma,out));
    ppf_set_zero(&a,-1);CHECK(ppf_sqrt(&b,&a,PPF_RNDN,&ctx)==0);CHECK(ppf_zero_p(&b)&&ppf_signbit(&b));
    ppf_set_inf(&a,1);ppf_set_zero(&b,1);ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);ppf_mul(&c,&a,&b,PPF_RNDN,&ctx);CHECK(ppf_nan_p(&c));CHECK(ppf_context_flags(&ctx)&PPF_FLAG_NAN);
    ppf_set_ui(&a,1,PPF_RNDN,&ctx);ppf_set_zero(&b,-1);ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);ppf_div(&c,&a,&b,PPF_RNDN,&ctx);CHECK(ppf_inf_p(&c)&&ppf_signbit(&c));CHECK(ppf_context_flags(&ctx)&PPF_FLAG_DIVBY0);
    mpfr_clears(ma,mb,mr,out,(mpfr_ptr)0);ppf_clear(&a);ppf_clear(&b);ppf_clear(&c);ppf_context_clear(&ctx);
}

int main(void) {
    gmp_randstate_t rs;
    gmp_randinit_mt(rs); gmp_randseed_ui(rs,0x41524954484dUL);
    compare_binary(rs,0); compare_binary(rs,1); compare_binary(rs,2); compare_binary(rs,3);
    compare_unary(rs,0); compare_unary(rs,1);
    compare_fused(rs,0); compare_fused(rs,1);
    compare_reductions(rs); special_and_alias_tests();
    gmp_randclear(rs);
    puts("INDEPENDENT ARITHMETIC DIFFERENTIAL TESTS PASSED");
    return 0;
}
