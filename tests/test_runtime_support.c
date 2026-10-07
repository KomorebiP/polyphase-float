/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr,"CHECK FAILED %s:%d: %s\n",__FILE__,__LINE__,#c); \
    exit(1); } } while(0)

static int same_mpfr(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b)) return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_zero_p(a) || mpfr_zero_p(b))
        return mpfr_zero_p(a) && mpfr_zero_p(b) && mpfr_signbit(a)==mpfr_signbit(b);
    return mpfr_equal_p(a,b)!=0;
}

static void test_build_and_memory(void) {
    CHECK(strcmp(ppf_get_version(),PPF_VERSION_STRING)==0);
    CHECK(ppf_buildopt_tls_p()!=0);
    CHECK(ppf_buildopt_gmpinternals_p()==0);
    CHECK(ppf_buildopt_sharedcache_p()!=0);
    CHECK(ppf_buildopt_float128_p()==PPF_HAVE_FLOAT128);
    CHECK(ppf_buildopt_decimal_p()==PPF_HAVE_DECIMAL_FLOAT);
    CHECK(ppf_get_patches()!=NULL);
    CHECK(ppf_buildopt_tune_case()!=NULL);
    ppf_free_pool();
    ppf_free_cache2(PPF_FREE_LOCAL_CACHE);
    ppf_free_cache2(PPF_FREE_GLOBAL_CACHE);
    ppf_free_cache();
    CHECK(ppf_mp_memory_cleanup()==0);
}

static void test_custom_storage(void) {
    ppf_context_t ctx;
    ppf_t source, x, y;
    const ppf_prec_t p=128;
    size_t bytes=ppf_custom_get_size(p);
    mp_limb_t *a=(mp_limb_t*)aligned_alloc(_Alignof(mp_limb_t),bytes);
    mp_limb_t *b=(mp_limb_t*)aligned_alloc(_Alignof(mp_limb_t),bytes);
    CHECK(a&&b);
    CHECK(ppf_context_init(&ctx)==0);
    CHECK(ppf_init2_ctx(&source,p,&ctx)==0);
    CHECK(ppf_set_ui_2exp(&source,3,-1,PPF_RNDN,&ctx)==0);
    ppf_custom_init(a,p); ppf_custom_init(b,p);
    memcpy(a,source.limbs,bytes);
    CHECK(ppf_custom_init_set(&x,PPF_REGULAR_KIND,source.exponent,p,a)==0);
    CHECK(ppf_custom_get_kind(&x)==PPF_REGULAR_KIND);
    CHECK(ppf_custom_get_significand(&x)==a);
    CHECK(ppf_custom_get_exp(&x)==source.exponent);
    CHECK(ppf_equal_p(&x,&source));
    CHECK(ppf_custom_init_set(&y,PPF_ZERO_KIND,0,p,b)==0);
    CHECK(ppf_add(&y,&x,&source,PPF_RNDN,&ctx)==0);
    CHECK(ppf_cmp_ui(&y,3,&ctx)==0);
    memcpy(b,a,bytes);
    ppf_custom_move(&x,b);
    CHECK(ppf_custom_get_significand(&x)==b);
    ppf_clear(&x); ppf_clear(&y); /* external buffers must not be freed */
    free(a); free(b);
    ppf_clear(&source); ppf_context_clear(&ctx);
}

static void test_compatibility(void) {
    ppf_context_t ctx;
    ppf_t a,b,r;
    mpfr_t ma,mb,mr,out;
    CHECK(ppf_context_init(&ctx)==0);
    CHECK(ppf_init2_ctx(&a,160,&ctx)==0);
    CHECK(ppf_init2_ctx(&b,160,&ctx)==0);
    CHECK(ppf_init2_ctx(&r,96,&ctx)==0);
    CHECK(ppf_set_ui_2exp(&a,3,-1,PPF_RNDN,&ctx)==0);
    CHECK(ppf_set_ui_2exp(&b,3,-1,PPF_RNDN,&ctx)==0);
    CHECK(ppf_eq(&a,&b,160));
    b.limbs[0]^=1u;
    CHECK(ppf_eq(&a,&b,100));
    CHECK(!ppf_eq(&a,&b,160));
    b.limbs[0]^=1u;
    ppf_set_prec_raw(&a,96);
    CHECK(a.precision==96 && ppf_cmp_ui_2exp(&a,3,-1,&ctx)==0);

    mpfr_inits2(96,ma,mb,mr,out,(mpfr_ptr)0);
    mpfr_set_d(ma,1.25,MPFR_RNDN);mpfr_set_d(mb,1.0,MPFR_RNDN);
    CHECK(ppf_set_d(&a,1.25,PPF_RNDN,&ctx)==0);
    CHECK(ppf_set_d(&b,1.0,PPF_RNDN,&ctx)==0);
    mpfr_reldiff(mr,ma,mb,MPFR_RNDN);
    ppf_reldiff(&r,&a,&b,PPF_RNDN,&ctx);
    CHECK(ppf_get_mpfr(out,&r,MPFR_RNDN)==0);
    CHECK(same_mpfr(mr,out));
    CHECK(ppf_mul_2exp(&r,&a,7,PPF_RNDN,&ctx)==
          ppf_mul_2ui(&b,&a,7,PPF_RNDN,&ctx));
    CHECK(ppf_equal_p(&r,&b));
    CHECK(ppf_div_2exp(&r,&a,7,PPF_RNDN,&ctx)==
          ppf_div_2ui(&b,&a,7,PPF_RNDN,&ctx));
    CHECK(ppf_equal_p(&r,&b));
    mpfr_clears(ma,mb,mr,out,(mpfr_ptr)0);
    ppf_clear(&a);ppf_clear(&b);ppf_clear(&r);ppf_context_clear(&ctx);
}

static void test_can_round(void) {
    gmp_randstate_t rs;
    ppf_context_t ctx;
    ppf_t p;
    mpfr_t m;
    gmp_randinit_mt(rs);gmp_randseed_ui(rs,0x92a71UL);
    CHECK(ppf_context_init(&ctx)==0);
    CHECK(ppf_init2_ctx(&p,53,&ctx)==0);mpfr_init2(m,53);
    for(int i=0;i<400;i++){
        mpz_t z;unsigned long bits=2+gmp_urandomm_ui(rs,120);long e=(long)gmp_urandomm_ui(rs,81)-40;
        mpz_init(z);mpz_urandomb(z,rs,bits);if(mpz_sgn(z)==0)mpz_set_ui(z,1);if(gmp_urandomb_ui(rs,1))mpz_neg(z,z);
        mpfr_set_prec(m,53);mpfr_set_z_2exp(m,z,e,MPFR_RNDN);CHECK(ppf_import_mpfr_exact(&p,m,&ctx)==0);
        for(int r1=MPFR_RNDN;r1<=MPFR_RNDF;r1++)for(int r2=MPFR_RNDN;r2<=MPFR_RNDF;r2++){
            mpfr_exp_t err=5+(mpfr_exp_t)gmp_urandomm_ui(rs,90);mpfr_prec_t target=2+gmp_urandomm_ui(rs,80);
            int a=mpfr_can_round(m,err,(mpfr_rnd_t)r1,(mpfr_rnd_t)r2,target)!=0;
            int b=ppf_can_round(&p,(ppf_exp_t)err,ppf_rounding_from_mpfr((mpfr_rnd_t)r1),ppf_rounding_from_mpfr((mpfr_rnd_t)r2),(ppf_prec_t)target)!=0;
            if(a!=b){fprintf(stderr,"can_round mismatch i=%d r1=%d r2=%d err=%ld target=%lu mpfr=%d ppf=%d\n",i,r1,r2,(long)err,(unsigned long)target,a,b);exit(1);} }
        mpz_clear(z);
    }
    ppf_clear(&p);ppf_context_clear(&ctx);mpfr_clear(m);gmp_randclear(rs);
}

static void test_nearest_away(void) {
    ppf_context_t ctx;ppf_t x;
    CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&x,2,&ctx)==0);
    CHECK(ppf_round_nearest_away_ctx(ppf_set_ui,&x,&ctx,5UL)>0);
    CHECK(ppf_cmp_ui(&x,6,&ctx)==0);
    CHECK(ppf_round_nearest_away_ctx(ppf_set_si,&x,&ctx,-5L)<0);
    CHECK(ppf_cmp_si(&x,-6,&ctx)==0);
    CHECK(strcmp(ppf_print_rnd_mode(PPF_RNDN),"PPF_RNDN")==0);
    CHECK(ppf_print_rnd_mode((ppf_rnd_t)99)==NULL);
    ppf_clear(&x);ppf_context_clear(&ctx);
}

int main(void){
    test_build_and_memory();
    test_custom_storage();
    test_compatibility();
    test_can_round();
    test_nearest_away();
    puts("runtime support tests passed");
    return 0;
}
