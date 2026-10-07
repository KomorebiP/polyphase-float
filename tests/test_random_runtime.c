/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if(!(c)){fprintf(stderr,"CHECK FAILED %s:%d: %s\n",__FILE__,__LINE__,#c);exit(1);} } while(0)

static int same(mpfr_srcptr a,mpfr_srcptr b){
    if(mpfr_nan_p(a)||mpfr_nan_p(b))return mpfr_nan_p(a)&&mpfr_nan_p(b);
    if(mpfr_zero_p(a)||mpfr_zero_p(b))return mpfr_zero_p(a)&&mpfr_zero_p(b)&&mpfr_signbit(a)==mpfr_signbit(b);
    return mpfr_equal_p(a,b)!=0;
}
static int sgn(int x){return (x>0)-(x<0);}

static void exact_uniform(void){
    const mpfr_prec_t ps[]={2,3,17,53,100,257};
    for(size_t j=0;j<sizeof(ps)/sizeof(ps[0]);j++){
        gmp_randstate_t a,b;ppf_context_t ctx;ppf_t x;mpfr_t m,out;
        gmp_randinit_mt(a);gmp_randinit_mt(b);gmp_randseed_ui(a,12345+j);gmp_randseed_ui(b,12345+j);
        CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&x,ps[j],&ctx)==0);mpfr_inits2(ps[j],m,out,(mpfr_ptr)0);
        for(int i=0;i<40;i++){
            int mt=mpfr_urandomb(m,a);int pt=ppf_urandomb(&x,b);CHECK(mt==pt);CHECK(ppf_get_mpfr(out,&x,MPFR_RNDN)==0);CHECK(same(m,out));
        }
        mpfr_clears(m,out,(mpfr_ptr)0);ppf_clear(&x);ppf_context_clear(&ctx);gmp_randclear(a);gmp_randclear(b);
    }
}

static void exact_continuous_uniform(void){
    const mpfr_prec_t ps[]={2,17,53,100};
    for(size_t j=0;j<sizeof(ps)/sizeof(ps[0]);j++)for(int r=MPFR_RNDN;r<=MPFR_RNDA;r++){
        gmp_randstate_t a,b;ppf_context_t ctx;ppf_t x;mpfr_t m,out;
        gmp_randinit_mt(a);gmp_randinit_mt(b);gmp_randseed_ui(a,9000+j*17+r);gmp_randseed_ui(b,9000+j*17+r);
        CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&x,ps[j],&ctx)==0);mpfr_inits2(ps[j],m,out,(mpfr_ptr)0);
        for(int i=0;i<30;i++){
            int mt=mpfr_urandom(m,a,(mpfr_rnd_t)r);int pt=ppf_urandom(&x,b,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            CHECK(pt!=PPF_TERNARY_ERROR);CHECK(sgn(mt)==sgn(pt));CHECK(ppf_get_mpfr(out,&x,MPFR_RNDN)==0);
            if(!same(m,out)){mpfr_printf("urandom mismatch p=%Pu r=%d\nmpfr=%Ra\nppf =%Ra\n",ps[j],r,m,out);exit(1);}
        }
        mpfr_clears(m,out,(mpfr_ptr)0);ppf_clear(&x);ppf_context_clear(&ctx);gmp_randclear(a);gmp_randclear(b);
    }
}

static void distribution_smoke(void){
    gmp_randstate_t rs;ppf_context_t ctx;ppf_t x,y;double sum=0.0,sum2=0.0,esum=0.0;const int n=80;
    gmp_randinit_mt(rs);gmp_randseed_ui(rs,0x55aa77UL);CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&x,40,&ctx)==0);CHECK(ppf_init2_ctx(&y,40,&ctx)==0);
    for(int i=0;i<n;i++){
        int t=ppf_nrandom(&x,rs,PPF_RNDN,&ctx);CHECK(t!=PPF_TERNARY_ERROR);double v=ppf_get_d(&x,PPF_RNDN,&ctx);CHECK(isfinite(v));sum+=v;sum2+=v*v;
        t=ppf_erandom(&x,rs,PPF_RNDN,&ctx);CHECK(t!=PPF_TERNARY_ERROR);v=ppf_get_d(&x,PPF_RNDN,&ctx);CHECK(v>=0.0&&isfinite(v));esum+=v;
    }
    CHECK(fabs(sum/n)<0.45);CHECK(sum2/n>0.45&&sum2/n<1.8);CHECK(esum/n>0.45&&esum/n<1.8);
    {int code=ppf_grandom(&x,&y,rs,PPF_RNDN,&ctx);CHECK(code!=PPF_TERNARY_ERROR);CHECK(isfinite(ppf_get_d(&x,PPF_RNDN,&ctx)));CHECK(isfinite(ppf_get_d(&y,PPF_RNDN,&ctx)));}
    ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&ctx);gmp_randclear(rs);
}

int main(void){exact_uniform();exact_continuous_uniform();distribution_smoke();puts("random runtime tests passed");return 0;}
