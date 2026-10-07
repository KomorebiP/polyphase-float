/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#ifndef PPF_MPFR_HAVE_FLOAT128_API
#define PPF_MPFR_HAVE_FLOAT128_API PPF_HAVE_FLOAT128
#endif
#ifndef PPF_MPFR_HAVE_DECIMAL_API
#define PPF_MPFR_HAVE_DECIMAL_API PPF_HAVE_DECIMAL_FLOAT
#endif

#if PPF_HAVE_FLOAT128 && PPF_MPFR_HAVE_FLOAT128_API
# define MPFR_USE_EXTENSION 1
# define MPFR_WANT_FLOAT128 1
# ifndef mpfr_float128
#  define mpfr_float128 ppf_float128_t
# endif
#endif
#if PPF_HAVE_DECIMAL_FLOAT && PPF_MPFR_HAVE_DECIMAL_API
# define MPFR_USE_EXTENSION 1
# define MPFR_WANT_DECIMAL_FLOATS 1
#endif

#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if(!(c)){fprintf(stderr,"CHECK FAILED %s:%d: %s\n",__FILE__,__LINE__,#c);exit(1);} } while(0)

#if (PPF_HAVE_FLOAT128 && PPF_MPFR_HAVE_FLOAT128_API) || \
    (PPF_HAVE_DECIMAL_FLOAT && PPF_MPFR_HAVE_DECIMAL_API)
static int same(mpfr_srcptr a,mpfr_srcptr b){
    if(mpfr_nan_p(a)||mpfr_nan_p(b))return mpfr_nan_p(a)&&mpfr_nan_p(b);
    if(mpfr_zero_p(a)||mpfr_zero_p(b))return mpfr_zero_p(a)&&mpfr_zero_p(b)&&mpfr_signbit(a)==mpfr_signbit(b);
    return mpfr_equal_p(a,b)!=0;
}
static int sgn(int x){return (x>0)-(x<0);}

static void make_random(gmp_randstate_t rs,mpfr_ptr m,mpfr_prec_t p){
    mpz_t z;unsigned long bits=1+gmp_urandomm_ui(rs,p+80);long e=(long)gmp_urandomm_ui(rs,40001)-20000;
    mpz_init(z);mpz_urandomb(z,rs,bits);if(mpz_sgn(z)==0)mpz_set_ui(z,1);if(gmp_urandomb_ui(rs,1))mpz_neg(z,z);
    mpfr_set_prec(m,p);mpfr_set_z_2exp(m,z,e,MPFR_RNDN);mpz_clear(z);
}
#endif

#if PPF_HAVE_FLOAT128 && PPF_MPFR_HAVE_FLOAT128_API
static void test_float128(void){
    gmp_randstate_t rs;ppf_context_t ctx;ppf_t p;mpfr_t source,expected,out;
    gmp_randinit_mt(rs);gmp_randseed_ui(rs,0xf128aUL);CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&p,53,&ctx)==0);mpfr_inits2(200,source,expected,out,(mpfr_ptr)0);
    for(int i=0;i<300;i++){
        mpfr_prec_t dp=2+gmp_urandomm_ui(rs,220);make_random(rs,source,180);
        ppf_float128_t f=(ppf_float128_t)mpfr_get_float128(source,MPFR_RNDN);
        CHECK(ppf_set_prec(&p,dp)==0);mpfr_set_prec(expected,dp);mpfr_set_prec(out,dp);
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;r++){
            int mt=mpfr_set_float128(expected,f,(mpfr_rnd_t)r);int pt=ppf_set_float128(&p,(ppf_float128_t)f,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            CHECK(pt!=PPF_TERNARY_ERROR);CHECK(sgn(mt)==sgn(pt));CHECK(ppf_get_mpfr(out,&p,MPFR_RNDN)==0);CHECK(same(expected,out));
        }
        CHECK(ppf_import_mpfr_exact(&p,source,&ctx)==0);
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;r++){
            ppf_float128_t a=(ppf_float128_t)mpfr_get_float128(source,(mpfr_rnd_t)r);ppf_float128_t b=ppf_get_float128(&p,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            mpfr_set_float128(expected,a,MPFR_RNDN);mpfr_set_float128(out,b,MPFR_RNDN);if(!same(expected,out)){ fprintf(stderr,"float128 get mismatch i=%d r=%d dp=%lu\n",i,r,(unsigned long)dp); mpfr_printf("source=%Ra\nexpected=%Ra\nout=%Ra\n",source,expected,out);exit(1);}
        }
    }
    ppf_clear(&p);ppf_context_clear(&ctx);mpfr_clears(source,expected,out,(mpfr_ptr)0);gmp_randclear(rs);
}
#endif

#if PPF_HAVE_DECIMAL_FLOAT && PPF_MPFR_HAVE_DECIMAL_API
static void test_decimal(void){
    gmp_randstate_t rs;ppf_context_t ctx;ppf_t p;mpfr_t source,expected,out,da,db;
    gmp_randinit_mt(rs);gmp_randseed_ui(rs,0xdec64128UL);CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&p,53,&ctx)==0);mpfr_inits2(260,source,expected,out,da,db,(mpfr_ptr)0);
    for(int i=0;i<260;i++){
        mpfr_prec_t dp=2+gmp_urandomm_ui(rs,230);make_random(rs,source,220);
        ppf_decimal64_t d64=(ppf_decimal64_t)mpfr_get_decimal64(source,MPFR_RNDN);ppf_decimal128_t d128=(ppf_decimal128_t)mpfr_get_decimal128(source,MPFR_RNDN);
        CHECK(ppf_set_prec(&p,dp)==0);mpfr_set_prec(expected,dp);mpfr_set_prec(out,dp);
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;r++){
            int mt=mpfr_set_decimal64(expected,d64,(mpfr_rnd_t)r);int pt=ppf_set_decimal64(&p,(ppf_decimal64_t)d64,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            CHECK(pt!=PPF_TERNARY_ERROR);CHECK(sgn(mt)==sgn(pt));CHECK(ppf_get_mpfr(out,&p,MPFR_RNDN)==0);if(!same(expected,out)){mpfr_printf("decimal64 set mismatch\n%Ra\n%Ra\n",expected,out);exit(1);}
            mt=mpfr_set_decimal128(expected,d128,(mpfr_rnd_t)r);pt=ppf_set_decimal128(&p,(ppf_decimal128_t)d128,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            CHECK(pt!=PPF_TERNARY_ERROR);CHECK(sgn(mt)==sgn(pt));CHECK(ppf_get_mpfr(out,&p,MPFR_RNDN)==0);if(!same(expected,out)){mpfr_printf("decimal128 set mismatch\n%Ra\n%Ra\n",expected,out);exit(1);}
        }
        CHECK(ppf_import_mpfr_exact(&p,source,&ctx)==0);
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;r++){
            ppf_decimal64_t a64=(ppf_decimal64_t)mpfr_get_decimal64(source,(mpfr_rnd_t)r);ppf_decimal64_t b64=ppf_get_decimal64(&p,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            mpfr_set_decimal64(da,a64,MPFR_RNDN);mpfr_set_decimal64(db,b64,MPFR_RNDN);if(!same(da,db)){mpfr_printf("decimal64 get mismatch\n%Ra\n%Ra\n",da,db);exit(1);}
            ppf_decimal128_t a128=(ppf_decimal128_t)mpfr_get_decimal128(source,(mpfr_rnd_t)r);ppf_decimal128_t b128=ppf_get_decimal128(&p,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            mpfr_set_decimal128(da,a128,MPFR_RNDN);mpfr_set_decimal128(db,b128,MPFR_RNDN);if(!same(da,db)){mpfr_printf("decimal128 get mismatch\n%Ra\n%Ra\n",da,db);exit(1);}
        }
    }
    ppf_clear(&p);ppf_context_clear(&ctx);mpfr_clears(source,expected,out,da,db,(mpfr_ptr)0);gmp_randclear(rs);
}
#endif

int main(void){
#if PPF_HAVE_FLOAT128 && PPF_MPFR_HAVE_FLOAT128_API
    CHECK(mpfr_buildopt_float128_p()!=0);
    test_float128();
#endif
#if PPF_HAVE_DECIMAL_FLOAT && PPF_MPFR_HAVE_DECIMAL_API
    CHECK(mpfr_buildopt_decimal_p()!=0);
    test_decimal();
#endif
    puts("conditional format tests passed");return 0;
}
