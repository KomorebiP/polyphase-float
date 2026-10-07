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

static int sign_of(int v){return v>0?1:(v<0?-1:0);}
static int same_value(mpfr_srcptr a,mpfr_srcptr b){
    if(mpfr_nan_p(a)||mpfr_nan_p(b))return mpfr_nan_p(a)&&mpfr_nan_p(b);
    if(mpfr_inf_p(a)||mpfr_inf_p(b))return mpfr_inf_p(a)&&mpfr_inf_p(b)&&mpfr_signbit(a)==mpfr_signbit(b);
    if(!mpfr_equal_p(a,b))return 0;
    return !mpfr_zero_p(a)||mpfr_signbit(a)==mpfr_signbit(b);
}
static void compare_flags(ppf_context_t *ctx){ppf_flags_t p=ppf_context_flags(ctx);
    CHECK(((p&PPF_FLAG_UNDERFLOW)!=0)==(mpfr_underflow_p()!=0));
    CHECK(((p&PPF_FLAG_OVERFLOW)!=0)==(mpfr_overflow_p()!=0));
    CHECK(((p&PPF_FLAG_DIVBY0)!=0)==(mpfr_divby0_p()!=0));
    CHECK(((p&PPF_FLAG_NAN)!=0)==(mpfr_nanflag_p()!=0));
    CHECK(((p&PPF_FLAG_INEXACT)!=0)==(mpfr_inexflag_p()!=0));
    CHECK(((p&PPF_FLAG_ERANGE)!=0)==(mpfr_erangeflag_p()!=0));}

static void random_regular(gmp_randstate_t rs,mpfr_ptr x){mpfr_urandomb(x,rs);if(mpfr_zero_p(x))mpfr_set_ui(x,1,MPFR_RNDN);
    mpfr_mul_2si(x,x,(long)gmp_urandomm_ui(rs,601)-300,MPFR_RNDN);if(gmp_urandomm_ui(rs,2))mpfr_neg(x,x,MPFR_RNDN);}

static ppf_prec_t ppf_prec_from_mpfr(mpfr_prec_t value){
    CHECK(value>0);
    ppf_prec_t converted=(ppf_prec_t)value;
    CHECK((mpfr_prec_t)converted==value);
    return converted;
}

static void randomized_plans(void){
    gmp_randstate_t rs;ppf_context_t ctx;ppf_t den,num,got;mpfr_t md,mn,mr,out;
    gmp_randinit_mt(rs);gmp_randseed_ui(rs,0x504c414eUL);CHECK(ppf_context_init(&ctx)==0);
    CHECK(ppf_init2_ctx(&den,53,&ctx)==0);CHECK(ppf_init2_ctx(&num,53,&ctx)==0);CHECK(ppf_init2_ctx(&got,53,&ctx)==0);
    mpfr_inits2(53,md,mn,mr,out,(mpfr_ptr)0);
    for(int plan_index=0;plan_index<36;++plan_index){
        mpfr_prec_t pd=40+gmp_urandomm_ui(rs,230), pr=20+gmp_urandomm_ui(rs,220);
        ppf_prec_t ppf_pd=ppf_prec_from_mpfr(pd),ppf_pr=ppf_prec_from_mpfr(pr);
        mpfr_set_prec(md,pd);mpfr_set_prec(mr,pr);mpfr_set_prec(out,pr);CHECK(ppf_set_prec(&den,ppf_pd)==0);CHECK(ppf_set_prec(&got,ppf_pr)==0);
        random_regular(rs,md);if(mpfr_zero_p(md))mpfr_set_ui(md,3,MPFR_RNDN);CHECK(ppf_import_mpfr_exact(&den,md,&ctx)==0);
        ppf_divisor_plan_t *plan=NULL;CHECK(ppf_divisor_plan_create_ex(&plan,&den,ppf_pr,8,&ctx)==0);CHECK(plan);CHECK(ppf_divisor_plan_precision(plan)==ppf_pr);
        unsigned long start=ppf_divisor_plan_applications(plan);
        for(int iter=0;iter<22;++iter){
            mpfr_prec_t pn=20+gmp_urandomm_ui(rs,240);ppf_prec_t ppf_pn=ppf_prec_from_mpfr(pn);mpfr_set_prec(mn,pn);CHECK(ppf_set_prec(&num,ppf_pn)==0);random_regular(rs,mn);CHECK(ppf_import_mpfr_exact(&num,mn,&ctx)==0);
            for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){int mt,pt;mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
                mt=mpfr_div(mr,mn,md,(mpfr_rnd_t)r);pt=ppf_div_with_plan(&got,&num,plan,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
                CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&got,MPFR_RNDN);
                if(!same_value(mr,out)){fprintf(stderr,"plan mismatch pr=%lu rnd=%d\n",(unsigned long)pr,r);mpfr_printf("n=%Ra d=%Ra mpfr=%Ra ppf=%Ra\n",mn,md,mr,out);exit(1);}CHECK(sign_of(mt)==sign_of(pt));compare_flags(&ctx);
            }
        }
        CHECK(ppf_divisor_plan_applications(plan)==start+22UL*5UL);
        ppf_divisor_plan_destroy(plan);
    }
    mpfr_clears(md,mn,mr,out,(mpfr_ptr)0);ppf_clear(&den);ppf_clear(&num);ppf_clear(&got);ppf_context_clear(&ctx);gmp_randclear(rs);
}

static void batch_and_alias(void){
    enum{N=12};ppf_context_t ctx;ppf_t den,nums[N],outs[N];const ppf_t *np[N];ppf_t *op[N];int tern[N];
    mpfr_t md,mn[N],mr[N],check;ppf_divisor_plan_t *plan=NULL;
    CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&den,512,&ctx)==0);ppf_set_ui(&den,37,PPF_RNDN,&ctx);mpfr_init2(md,512);mpfr_set_ui(md,37,MPFR_RNDN);
    CHECK(ppf_divisor_plan_create(&plan,&den,512,&ctx)==0);mpfr_init2(check,512);
    for(int i=0;i<N;++i){CHECK(ppf_init2_ctx(&nums[i],512,&ctx)==0);CHECK(ppf_init2_ctx(&outs[i],512,&ctx)==0);ppf_set_ui(&nums[i],(unsigned long)(i+1)*1234567UL,PPF_RNDN,&ctx);np[i]=&nums[i];op[i]=&outs[i];mpfr_init2(mn[i],512);mpfr_init2(mr[i],512);mpfr_set_ui(mn[i],(unsigned long)(i+1)*1234567UL,MPFR_RNDN);}
    CHECK(ppf_div_with_plan_many(op,np,N,plan,PPF_RNDN,4,tern,&ctx)==0);
    for(int i=0;i<N;++i){int mt=mpfr_div(mr[i],mn[i],md,MPFR_RNDN);ppf_get_mpfr(check,&outs[i],MPFR_RNDN);CHECK(same_value(mr[i],check));CHECK(sign_of(mt)==sign_of(tern[i]));}
    /* Same-index alias is supported. */
    CHECK(ppf_div_with_plan(&nums[0],&nums[0],plan,PPF_RNDN,&ctx)!=PPF_TERNARY_ERROR);
    mpfr_div(mr[0],mn[0],md,MPFR_RNDN);ppf_get_mpfr(check,&nums[0],MPFR_RNDN);CHECK(same_value(mr[0],check));
    CHECK(ppf_divisor_plan_applications(plan)>=N+1UL);
    ppf_divisor_plan_destroy(plan);ppf_clear(&den);mpfr_clear(md);mpfr_clear(check);
    for(int i=0;i<N;++i){ppf_clear(&nums[i]);ppf_clear(&outs[i]);mpfr_clear(mn[i]);mpfr_clear(mr[i]);}
    ppf_context_clear(&ctx);
}

static void large_smoke(void){
    ppf_context_t ctx;ppf_t d,n,q;ppf_divisor_plan_t *plan=NULL;mpfr_t md,mn,mq,out;
    const ppf_prec_t p=100000;CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&d,p,&ctx)==0);CHECK(ppf_init2_ctx(&n,p,&ctx)==0);CHECK(ppf_init2_ctx(&q,p,&ctx)==0);
    mpfr_inits2(p,md,mn,mq,out,(mpfr_ptr)0);mpfr_set_ui(md,3,MPFR_RNDN);mpfr_const_pi(mn,MPFR_RNDN);ppf_set_ui(&d,3,PPF_RNDN,&ctx);CHECK(ppf_import_mpfr_exact(&n,mn,&ctx)==0);
    CHECK(ppf_divisor_plan_create(&plan,&d,p,&ctx)==0);int pt=ppf_div_with_plan(&q,&n,plan,PPF_RNDN,&ctx);int mt=mpfr_div(mq,mn,md,MPFR_RNDN);CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&q,MPFR_RNDN);CHECK(same_value(mq,out));CHECK(sign_of(mt)==sign_of(pt));
    ppf_divisor_plan_destroy(plan);ppf_clear(&d);ppf_clear(&n);ppf_clear(&q);ppf_context_clear(&ctx);mpfr_clears(md,mn,mq,out,(mpfr_ptr)0);
}

int main(void){randomized_plans();batch_and_alias();large_smoke();puts("REUSABLE-DENOMINATOR DIVISION TESTS PASSED");return 0;}
