/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); return 1; } } while(0)

typedef int (*ppf_unary_fn)(ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);
typedef int (*mpfr_unary_fn)(mpfr_ptr,mpfr_srcptr,mpfr_rnd_t);

static int same(ppf_t *p, mpfr_t m) {
    mpfr_t got;
    mpfr_init2(got, mpfr_get_prec(m));
    ppf_get_mpfr(got,p,MPFR_RNDN);
    int ok=mpfr_equal_p(got,m) || (mpfr_nan_p(got)&&mpfr_nan_p(m));
    if(!ok){fprintf(stderr,"mismatch\n");mpfr_printf("got %.80Rg\nref %.80Rg\n",got,m);}
    mpfr_clear(got);return ok;
}

static int test_unary(const char *name, ppf_unary_fn pf, mpfr_unary_fn mf,
                      const char *value, unsigned long prec) {
    ppf_context_t ctx; ppf_t x,y; mpfr_t mx,my;
    CHECK(ppf_context_init(&ctx)==0); CHECK(ppf_init2_ctx(&x,prec,&ctx)==0); CHECK(ppf_init2_ctx(&y,prec,&ctx)==0);
    mpfr_init2(mx,prec);mpfr_init2(my,prec);
    CHECK(mpfr_set_str(mx,value,10,MPFR_RNDN)==0); CHECK(ppf_set_mpfr(&x,mx,PPF_RNDN,&ctx)!=PPF_TERNARY_ERROR);
    int pt=pf(&y,&x,PPF_RNDN,&ctx); int mt=mf(my,mx,MPFR_RNDN);
    if(pt==PPF_TERNARY_ERROR || !same(&y,my) || ((pt>0)-(pt<0))!=((mt>0)-(mt<0))){fprintf(stderr,"unary %s value %s p=%lu pt=%d mt=%d\n",name,value,prec,pt,mt);return 1;}
    ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&ctx);mpfr_clear(mx);mpfr_clear(my);return 0;
}

int main(void){
    const char *vals_log[]={"0.125","0.5","0.9","1.1","2","3.1415926535","10","123.75"};
    const char *vals_exp[]={"-10","-1","-0.1","0.1","1","2","5"};
    unsigned long ps[]={24,53,80,128,256};
    for(size_t j=0;j<sizeof(ps)/sizeof(ps[0]);++j){
      for(size_t i=0;i<sizeof(vals_log)/sizeof(vals_log[0]);++i){
        CHECK(test_unary("log",ppf_log,mpfr_log,vals_log[i],ps[j])==0);
        CHECK(test_unary("log2",ppf_log2,mpfr_log2,vals_log[i],ps[j])==0);
        CHECK(test_unary("log10",ppf_log10,mpfr_log10,vals_log[i],ps[j])==0);
      }
      for(size_t i=0;i<sizeof(vals_exp)/sizeof(vals_exp[0]);++i){
        CHECK(test_unary("exp",ppf_exp,mpfr_exp,vals_exp[i],ps[j])==0);
        CHECK(test_unary("exp2",ppf_exp2,mpfr_exp2,vals_exp[i],ps[j])==0);
        CHECK(test_unary("exp10",ppf_exp10,mpfr_exp10,vals_exp[i],ps[j])==0);
      }
    }
    puts("transcendental smoke passed");return 0;
}
