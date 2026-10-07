/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"CHECK FAILED %s:%d: %s\n",__FILE__,__LINE__,#c); exit(1);} } while(0)
static int sign_of(int x){return (x>0)-(x<0);}
static int same(mpfr_srcptr a, mpfr_srcptr b){
    if(mpfr_nan_p(a)||mpfr_nan_p(b)) return mpfr_nan_p(a)&&mpfr_nan_p(b);
    if(mpfr_zero_p(a)||mpfr_zero_p(b)) return mpfr_zero_p(a)&&mpfr_zero_p(b)&&mpfr_signbit(a)==mpfr_signbit(b);
    return mpfr_equal_p(a,b)!=0;
}
static void flags(ppf_context_t *c){ppf_flags_t f=ppf_context_flags(c);CHECK((mpfr_inexflag_p()!=0)==((f&PPF_FLAG_INEXACT)!=0));CHECK((mpfr_underflow_p()!=0)==((f&PPF_FLAG_UNDERFLOW)!=0));CHECK((mpfr_overflow_p()!=0)==((f&PPF_FLAG_OVERFLOW)!=0));CHECK((mpfr_divby0_p()!=0)==((f&PPF_FLAG_DIVBY0)!=0));CHECK((mpfr_nanflag_p()!=0)==((f&PPF_FLAG_NAN)!=0));}
static void set_case(mpfr_ptr m,int id){switch(id){case 0:mpfr_set_zero(m,1);break;case 1:mpfr_set_zero(m,-1);break;case 2:mpfr_set_ui(m,1,MPFR_RNDN);break;case 3:mpfr_set_si(m,-1,MPFR_RNDN);break;case 4:mpfr_set_inf(m,1);break;case 5:mpfr_set_inf(m,-1);break;default:mpfr_set_nan(m);break;}}
int main(void){
    ppf_context_t ctx;ppf_t a,b,c,g;mpfr_t ma,mb,mc,mr,out;
    CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&a,17,&ctx)==0);CHECK(ppf_init2_ctx(&b,17,&ctx)==0);CHECK(ppf_init2_ctx(&c,17,&ctx)==0);CHECK(ppf_init2_ctx(&g,17,&ctx)==0);
    mpfr_inits2(17,ma,mb,mc,mr,out,(mpfr_ptr)0);
    for(int ia=0;ia<7;++ia)for(int ib=0;ib<7;++ib){set_case(ma,ia);set_case(mb,ib);CHECK(ppf_import_mpfr_exact(&a,ma,&ctx)==0);CHECK(ppf_import_mpfr_exact(&b,mb,&ctx)==0);
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){int mt,pt;mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);mt=mpfr_add(mr,ma,mb,(mpfr_rnd_t)r);pt=ppf_add(&g,&a,&b,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&g,MPFR_RNDN);CHECK(same(mr,out));CHECK(sign_of(mt)==sign_of(pt));flags(&ctx);
            mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);mt=mpfr_sub(mr,ma,mb,(mpfr_rnd_t)r);pt=ppf_sub(&g,&a,&b,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&g,MPFR_RNDN);CHECK(same(mr,out));CHECK(sign_of(mt)==sign_of(pt));flags(&ctx);
            mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);mt=mpfr_mul(mr,ma,mb,(mpfr_rnd_t)r);pt=ppf_mul(&g,&a,&b,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&g,MPFR_RNDN);CHECK(same(mr,out));CHECK(sign_of(mt)==sign_of(pt));flags(&ctx);
            mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);mt=mpfr_div(mr,ma,mb,(mpfr_rnd_t)r);pt=ppf_div(&g,&a,&b,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&g,MPFR_RNDN);CHECK(same(mr,out));CHECK(sign_of(mt)==sign_of(pt));flags(&ctx);
        }}
    for(int ia=0;ia<7;++ia){set_case(ma,ia);CHECK(ppf_import_mpfr_exact(&a,ma,&ctx)==0);for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){int mt,pt;mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);mt=mpfr_sqrt(mr,ma,(mpfr_rnd_t)r);pt=ppf_sqrt(&g,&a,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&g,MPFR_RNDN);CHECK(same(mr,out));CHECK(sign_of(mt)==sign_of(pt));flags(&ctx);}}
    for(int ia=0;ia<7;++ia)for(int ib=0;ib<7;++ib)for(int ic=0;ic<7;++ic){set_case(ma,ia);set_case(mb,ib);set_case(mc,ic);CHECK(ppf_import_mpfr_exact(&a,ma,&ctx)==0);CHECK(ppf_import_mpfr_exact(&b,mb,&ctx)==0);CHECK(ppf_import_mpfr_exact(&c,mc,&ctx)==0);for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){int mt,pt;mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);mt=mpfr_fma(mr,ma,mb,mc,(mpfr_rnd_t)r);pt=ppf_fma(&g,&a,&b,&c,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&g,MPFR_RNDN);CHECK(same(mr,out));CHECK(sign_of(mt)==sign_of(pt));flags(&ctx);}}
    mpfr_clears(ma,mb,mc,mr,out,(mpfr_ptr)0);ppf_clear(&a);ppf_clear(&b);ppf_clear(&c);ppf_clear(&g);ppf_context_clear(&ctx);puts("SPECIAL-VALUE ARITHMETIC TESTS PASSED");return 0;
}
