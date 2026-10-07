/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

typedef int (*ppf_unary_fn)(ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_unary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
typedef int (*ppf_binary_fn)(ppf_t *, const ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_binary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);

typedef struct { int underflow, overflow, divby0, nan, inexact, erange; } flags_t;
static const ppf_rnd_t prnd[5] = { PPF_RNDN, PPF_RNDZ, PPF_RNDU, PPF_RNDD, PPF_RNDA };
static const mpfr_rnd_t mrnd[5] = { MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA };
static int signum(int x) { return (x > 0) - (x < 0); }

static flags_t mpfr_flags_now(void) {
    flags_t f;
    f.underflow = mpfr_underflow_p() != 0;
    f.overflow = mpfr_overflow_p() != 0;
    f.divby0 = mpfr_divby0_p() != 0;
    f.nan = mpfr_nanflag_p() != 0;
    f.inexact = mpfr_inexflag_p() != 0;
    f.erange = mpfr_erangeflag_p() != 0;
    return f;
}

static int flags_equal(const ppf_context_t *ctx, flags_t f) {
    ppf_flags_t p = ctx->flags;
    return f.underflow == !!(p & PPF_FLAG_UNDERFLOW) &&
           f.overflow  == !!(p & PPF_FLAG_OVERFLOW) &&
           f.divby0    == !!(p & PPF_FLAG_DIVBY0) &&
           f.nan       == !!(p & PPF_FLAG_NAN) &&
           f.inexact   == !!(p & PPF_FLAG_INEXACT) &&
           f.erange    == !!(p & PPF_FLAG_ERANGE);
}

static int same_value(const ppf_t *p, mpfr_srcptr m) {
    mpfr_t got;
    int ok;
    mpfr_init2(got, mpfr_get_prec(m));
    CHECK(ppf_get_mpfr(got, p, MPFR_RNDN) != PPF_TERNARY_ERROR);
    if (mpfr_nan_p(m)) ok = mpfr_nan_p(got);
    else if (mpfr_inf_p(m)) ok = mpfr_inf_p(got) && mpfr_signbit(got) == mpfr_signbit(m);
    else if (mpfr_zero_p(m)) ok = mpfr_zero_p(got) && mpfr_signbit(got) == mpfr_signbit(m);
    else ok = mpfr_equal_p(got, m) != 0;
    if (!ok) mpfr_printf("got=%.*Rg ref=%.*Rg\n", 80, got, 80, m);
    mpfr_clear(got);
    return ok;
}

static void set_both_str(ppf_t *p, mpfr_ptr m, const char *s, ppf_context_t *ctx) {
    CHECK(mpfr_set_str(m, s, 0, MPFR_RNDN) == 0);
    CHECK(ppf_set_mpfr(p, m, PPF_RNDN, ctx) != PPF_TERNARY_ERROR);
}

static void compare_unary_case(const char *name, ppf_unary_fn pf, mpfr_unary_fn mf,
                               const char *value, unsigned long inprec,
                               unsigned long outprec) {
    ppf_context_t ctx;
    ppf_t x, y;
    mpfr_t mx, my;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, inprec, &ctx) == 0);
    CHECK(ppf_init2_ctx(&y, outprec, &ctx) == 0);
    mpfr_init2(mx, inprec); mpfr_init2(my, outprec);
    set_both_str(&x, mx, value, &ctx);
    for (int i = 0; i < 5; ++i) {
        int pt, mt;
        flags_t f;
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); mpfr_clear_flags();
        pt = pf(&y, &x, prnd[i], &ctx);
        mt = mf(my, mx, mrnd[i]);
        f = mpfr_flags_now();
        if (pt == PPF_TERNARY_ERROR || !same_value(&y, my) ||
            signum(pt) != signum(mt) || !flags_equal(&ctx, f)) {
            fprintf(stderr, "%s value=%s ip=%lu op=%lu rnd=%d pt=%d mt=%d pflags=0x%x mf=%d%d%d%d%d%d\n",
                    name, value, inprec, outprec, i, pt, mt, (unsigned)ctx.flags,
                    f.underflow, f.overflow, f.divby0, f.nan, f.inexact, f.erange);
            exit(1);
        }
        if (inprec == outprec) {
            /* Input/output alias uses the input object's precision. */
            CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); mpfr_clear_flags();
            pt = pf(&x, &x, prnd[i], &ctx);
            mt = mf(my, mx, mrnd[i]);
            f = mpfr_flags_now();
            if (pt == PPF_TERNARY_ERROR || !same_value(&x, my) ||
                signum(pt) != signum(mt) || !flags_equal(&ctx, f)) {
                fprintf(stderr, "%s alias value=%s ip=%lu op=%lu rnd=%d\n",
                        name, value, inprec, outprec, i);
                exit(1);
            }
            CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        }
    }
    ppf_clear(&x); ppf_clear(&y); ppf_context_clear(&ctx);
    mpfr_clear(mx); mpfr_clear(my);
}

static void compare_binary_case(const char *name, ppf_binary_fn pf, mpfr_binary_fn mf,
                                const char *av, const char *bv,
                                unsigned long inprec, unsigned long outprec) {
    ppf_context_t ctx;
    ppf_t a, b, y;
    mpfr_t ma, mb, my;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, inprec, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, inprec, &ctx) == 0);
    CHECK(ppf_init2_ctx(&y, outprec, &ctx) == 0);
    mpfr_init2(ma, inprec); mpfr_init2(mb, inprec); mpfr_init2(my, outprec);
    set_both_str(&a, ma, av, &ctx); set_both_str(&b, mb, bv, &ctx);
    for (int i = 0; i < 5; ++i) {
        int pt, mt;
        flags_t f;
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); mpfr_clear_flags();
        pt = pf(&y, &a, &b, prnd[i], &ctx);
        mt = mf(my, ma, mb, mrnd[i]);
        f = mpfr_flags_now();
        if (pt == PPF_TERNARY_ERROR || !same_value(&y, my) ||
            signum(pt) != signum(mt) || !flags_equal(&ctx, f)) {
            fprintf(stderr, "%s a=%s b=%s ip=%lu op=%lu rnd=%d pt=%d mt=%d pflags=0x%x mf=%d%d%d%d%d%d\n",
                    name, av, bv, inprec, outprec, i, pt, mt, (unsigned)ctx.flags,
                    f.underflow, f.overflow, f.divby0, f.nan, f.inexact, f.erange);
            exit(1);
        }
    }
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&y); ppf_context_clear(&ctx);
    mpfr_clear(ma); mpfr_clear(mb); mpfr_clear(my);
}

static void check_faithful_unary(const char *name, ppf_unary_fn pf,
                                 mpfr_unary_fn mf, const char *value,
                                 unsigned long p) {
    ppf_context_t ctx; ppf_t x, y; mpfr_t mx, lo, hi, got;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, p, &ctx) == 0); CHECK(ppf_init2_ctx(&y, p, &ctx) == 0);
    mpfr_init2(mx,p); mpfr_init2(lo,p); mpfr_init2(hi,p); mpfr_init2(got,p);
    set_both_str(&x,mx,value,&ctx);
    CHECK(pf(&y,&x,PPF_RNDF,&ctx) != PPF_TERNARY_ERROR);
    (void)mf(lo,mx,MPFR_RNDD); (void)mf(hi,mx,MPFR_RNDU);
    CHECK(ppf_get_mpfr(got,&y,MPFR_RNDN) != PPF_TERNARY_ERROR);
    if (!(mpfr_equal_p(got,lo) || mpfr_equal_p(got,hi) ||
          (mpfr_nan_p(got) && mpfr_nan_p(lo)))) {
        fprintf(stderr,"faithful %s(%s) failed\n",name,value); exit(1);
    }
    ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&ctx);
    mpfr_clear(mx);mpfr_clear(lo);mpfr_clear(hi);mpfr_clear(got);
}

static void compare_log_ui(unsigned long x, unsigned long p) {
    ppf_context_t ctx; ppf_t y; mpfr_t my;
    CHECK(ppf_context_init(&ctx)==0); CHECK(ppf_init2_ctx(&y,p,&ctx)==0); mpfr_init2(my,p);
    for(int i=0;i<5;i++){
        int a,b;flags_t f;ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);mpfr_clear_flags();
        a=ppf_log_ui(&y,x,prnd[i],&ctx);b=mpfr_log_ui(my,x,mrnd[i]);f=mpfr_flags_now();
        CHECK(a!=PPF_TERNARY_ERROR);CHECK(same_value(&y,my));CHECK(signum(a)==signum(b));CHECK(flags_equal(&ctx,f));
    }
    ppf_clear(&y);ppf_context_clear(&ctx);mpfr_clear(my);
}

static void compare_pow_scalars(unsigned long p) {
    ppf_context_t ctx; ppf_t b,y,e; mpfr_t mb,my,me; mpz_t z;
    CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&b,p,&ctx)==0);CHECK(ppf_init2_ctx(&y,p,&ctx)==0);CHECK(ppf_init2_ctx(&e,p,&ctx)==0);
    mpfr_init2(mb,p);mpfr_init2(my,p);mpfr_init2(me,p);mpz_init_set_si(z,-7);
    set_both_str(&b,mb,"-1.25",&ctx);set_both_str(&e,me,"3.5",&ctx);
    for(int i=0;i<5;i++){
#define TEST_SCALAR(PF,MF,ARGS_P,ARGS_M) do { int a,bx;flags_t f;ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);mpfr_clear_flags();a=PF ARGS_P;bx=MF ARGS_M;f=mpfr_flags_now();if(a==PPF_TERNARY_ERROR||!same_value(&y,my)||signum(a)!=signum(bx)||!flags_equal(&ctx,f)){fprintf(stderr,#PF " scalar fail p=%lu rnd=%d\n",p,i);exit(1);}} while(0)
        TEST_SCALAR(ppf_pow_ui,mpfr_pow_ui,(&y,&b,7UL,prnd[i],&ctx),(my,mb,7UL,mrnd[i]));
        TEST_SCALAR(ppf_pow_si,mpfr_pow_si,(&y,&b,-7L,prnd[i],&ctx),(my,mb,-7L,mrnd[i]));
        TEST_SCALAR(ppf_pow_uj,mpfr_pow_ui,(&y,&b,(uintmax_t)7,prnd[i],&ctx),(my,mb,7UL,mrnd[i]));
        TEST_SCALAR(ppf_pow_sj,mpfr_pow_si,(&y,&b,(intmax_t)-7,prnd[i],&ctx),(my,mb,-7L,mrnd[i]));
        TEST_SCALAR(ppf_pown,mpfr_pow_si,(&y,&b,(intmax_t)-7,prnd[i],&ctx),(my,mb,-7L,mrnd[i]));
        TEST_SCALAR(ppf_pow_z,mpfr_pow_z,(&y,&b,z,prnd[i],&ctx),(my,mb,z,mrnd[i]));
        TEST_SCALAR(ppf_ui_pow_ui,mpfr_ui_pow_ui,(&y,37UL,13UL,prnd[i],&ctx),(my,37UL,13UL,mrnd[i]));
        TEST_SCALAR(ppf_ui_pow,mpfr_ui_pow,(&y,37UL,&e,prnd[i],&ctx),(my,37UL,me,mrnd[i]));
        TEST_SCALAR(ppf_compound_si,mpfr_compound_si,(&y,&b,7L,prnd[i],&ctx),(my,mb,7L,mrnd[i]));
        (void)mpfr_set_d(mb,0.5,MPFR_RNDN); CHECK(ppf_set_mpfr(&b,mb,PPF_RNDN,&ctx)!=PPF_TERNARY_ERROR);
        TEST_SCALAR(ppf_compound_si,mpfr_compound_si,(&y,&b,2L,prnd[i],&ctx),(my,mb,2L,mrnd[i]));
        (void)mpfr_set_d(mb,-1.25,MPFR_RNDN); CHECK(ppf_set_mpfr(&b,mb,PPF_RNDN,&ctx)!=PPF_TERNARY_ERROR);
#undef TEST_SCALAR
    }
    mpz_clear(z);ppf_clear(&b);ppf_clear(&y);ppf_clear(&e);ppf_context_clear(&ctx);mpfr_clear(mb);mpfr_clear(my);mpfr_clear(me);
}

static void special_values(void) {
    const char *uvals[] = {"nan","inf","-inf","0","-0","-2","-1","-0.5","0.5","1","2"};
    for(size_t i=0;i<sizeof(uvals)/sizeof(uvals[0]);i++){
        compare_unary_case("log",ppf_log,mpfr_log,uvals[i],53,53);
        compare_unary_case("log1p",ppf_log1p,mpfr_log1p,uvals[i],53,53);
        compare_unary_case("exp",ppf_exp,mpfr_exp,uvals[i],53,53);
        compare_unary_case("expm1",ppf_expm1,mpfr_expm1,uvals[i],53,53);
    }
    const char *pvals[]={"nan","inf","-inf","0","-0","1","-1","2","-2"};
    for(size_t i=0;i<9;i++)for(size_t j=0;j<9;j++){
        compare_binary_case("pow",ppf_pow,mpfr_pow,pvals[i],pvals[j],53,53);
        compare_binary_case("powr",ppf_powr,mpfr_powr,pvals[i],pvals[j],53,53);
    }
}

int main(void) {
    static const unsigned long precisions[] = {2,3,5,12,24,53,80,128,256};
    static const char *logs[] = {"0x1p-40","0.0009765625","0.125","0.5","0.999999999999","1.000000000001","1.1","2","3","10","12345.75"};
    static const char *log1ps[] = {"-0.999999999999","-0.75","-0.1","-0x1p-40","0","0x1p-40","0.1","1","9","99","1000"};
    static const char *exps[] = {"-100","-20","-3","-1","-0x1p-40","0","0x1p-40","0.1","1","3","8","20"};
    static const char *powa[][2] = {{"2.5","3.25"},{"0.75","-2.5"},{"-2","7"},{"-2","8"},{"1.0001","1000"},{"0.9999","-1000"},{"0x1p-10","2.5"},{"9","0.5"},{"81","0.25"}};
    for(size_t pi=0;pi<sizeof(precisions)/sizeof(precisions[0]);pi++){
        unsigned long p=precisions[pi];
        unsigned long ip=p+7;
        for(size_t i=0;i<sizeof(logs)/sizeof(logs[0]);i++){
            compare_unary_case("log",ppf_log,mpfr_log,logs[i],ip,p);
            compare_unary_case("log2",ppf_log2,mpfr_log2,logs[i],ip,p);
            compare_unary_case("log10",ppf_log10,mpfr_log10,logs[i],ip,p);
        }
        for(size_t i=0;i<sizeof(log1ps)/sizeof(log1ps[0]);i++){
            compare_unary_case("log1p",ppf_log1p,mpfr_log1p,log1ps[i],ip,p);
            compare_unary_case("log2p1",ppf_log2p1,mpfr_log2p1,log1ps[i],ip,p);
            compare_unary_case("log10p1",ppf_log10p1,mpfr_log10p1,log1ps[i],ip,p);
        }
        for(size_t i=0;i<sizeof(exps)/sizeof(exps[0]);i++){
            compare_unary_case("exp",ppf_exp,mpfr_exp,exps[i],ip,p);
            compare_unary_case("exp2",ppf_exp2,mpfr_exp2,exps[i],ip,p);
            compare_unary_case("exp10",ppf_exp10,mpfr_exp10,exps[i],ip,p);
            compare_unary_case("expm1",ppf_expm1,mpfr_expm1,exps[i],ip,p);
            compare_unary_case("exp2m1",ppf_exp2m1,mpfr_exp2m1,exps[i],ip,p);
            compare_unary_case("exp10m1",ppf_exp10m1,mpfr_exp10m1,exps[i],ip,p);
        }
        for(size_t i=0;i<sizeof(powa)/sizeof(powa[0]);i++)
            compare_binary_case("pow",ppf_pow,mpfr_pow,powa[i][0],powa[i][1],ip,p);
        compare_log_ui(0UL,p);compare_log_ui(1UL,p);compare_log_ui(2UL,p);compare_log_ui(ULONG_MAX,p);
        compare_pow_scalars(p);
    }
    special_values();
    check_faithful_unary("log",ppf_log,mpfr_log,"3.25",80);
    check_faithful_unary("exp",ppf_exp,mpfr_exp,"0.125",80);
    puts("complete transcendental differential passed");
    return 0;
}
