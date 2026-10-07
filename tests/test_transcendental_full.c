/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); return 1; } } while(0)

typedef int (*pf_un)(ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);
typedef int (*mf_un)(mpfr_ptr,mpfr_srcptr,mpfr_rnd_t);
typedef int (*pf_bin)(ppf_t*,const ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);
typedef int (*mf_bin)(mpfr_ptr,mpfr_srcptr,mpfr_srcptr,mpfr_rnd_t);

static int sgn_i(int x){return (x>0)-(x<0);}
static ppf_rnd_t pr[] = {PPF_RNDN,PPF_RNDZ,PPF_RNDU,PPF_RNDD,PPF_RNDA};
static mpfr_rnd_t mr[] = {MPFR_RNDN,MPFR_RNDZ,MPFR_RNDU,MPFR_RNDD,MPFR_RNDA};

static int same_value(const ppf_t *p, mpfr_srcptr m){
  mpfr_t got; mpfr_init2(got,mpfr_get_prec(m)); ppf_get_mpfr(got,p,MPFR_RNDN);
  int ok=(mpfr_nan_p(got)&&mpfr_nan_p(m))||(mpfr_inf_p(got)&&mpfr_inf_p(m)&&mpfr_signbit(got)==mpfr_signbit(m))||mpfr_equal_p(got,m);
  if(!ok){mpfr_printf("got %.100Rg\nref %.100Rg\n",got,m);} mpfr_clear(got);return ok;
}

static int run_un(const char *name,pf_un pf,mf_un mf,const char *v,unsigned long p){
  ppf_context_t c;ppf_t x,y;mpfr_t mx,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,p,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(mx,p);mpfr_init2(my,p);CHECK(mpfr_set_str(mx,v,10,MPFR_RNDN)==0);CHECK(ppf_set_mpfr(&x,mx,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);
  for(int i=0;i<5;i++){ppf_context_flags_clear(&c,PPF_FLAGS_ALL);mpfr_clear_flags();int a=pf(&y,&x,pr[i],&c);int b=mf(my,mx,mr[i]);if(a==PPF_TERNARY_ERROR||!same_value(&y,my)||sgn_i(a)!=sgn_i(b)){fprintf(stderr,"%s(%s) p%lu rnd%d a%d b%d flags%u\n",name,v,p,i,a,b,c.flags);return 1;}}
  ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(mx);mpfr_clear(my);return 0;
}

static int run_bin(const char *name,pf_bin pf,mf_bin mf,const char *a0,const char *b0,unsigned long p){
  ppf_context_t c;ppf_t a,b,y;mpfr_t ma,mb,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&a,p,&c)==0);CHECK(ppf_init2_ctx(&b,p,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(ma,p);mpfr_init2(mb,p);mpfr_init2(my,p);CHECK(mpfr_set_str(ma,a0,10,MPFR_RNDN)==0);CHECK(mpfr_set_str(mb,b0,10,MPFR_RNDN)==0);ppf_set_mpfr(&a,ma,PPF_RNDN,&c);ppf_set_mpfr(&b,mb,PPF_RNDN,&c);
  for(int i=0;i<5;i++){ppf_context_flags_clear(&c,PPF_FLAGS_ALL);mpfr_clear_flags();int x=pf(&y,&a,&b,pr[i],&c);int z=mf(my,ma,mb,mr[i]);if(x==PPF_TERNARY_ERROR||!same_value(&y,my)||sgn_i(x)!=sgn_i(z)){fprintf(stderr,"%s(%s,%s) p%lu rnd%d x%d z%d flags%u\n",name,a0,b0,p,i,x,z,c.flags);return 1;}}
  ppf_clear(&a);ppf_clear(&b);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(ma);mpfr_clear(mb);mpfr_clear(my);return 0;
}

int main(void){
 const char *lp[]={"0.0009765625","0.125","0.5","0.999999999","1.000000001","1.1","2","3","10","12345.75"};
 const char *ep[]={"-20","-3","-1","-0.000001","0.000001","0.1","1","3","8"};
 unsigned long ps[]={12,24,53,80,128,256};
 for(size_t j=0;j<sizeof(ps)/sizeof(ps[0]);j++){
  for(size_t i=0;i<sizeof(lp)/sizeof(lp[0]);i++){CHECK(!run_un("log",ppf_log,mpfr_log,lp[i],ps[j]));CHECK(!run_un("log2",ppf_log2,mpfr_log2,lp[i],ps[j]));CHECK(!run_un("log10",ppf_log10,mpfr_log10,lp[i],ps[j]));}
  for(size_t i=0;i<sizeof(ep)/sizeof(ep[0]);i++){CHECK(!run_un("exp",ppf_exp,mpfr_exp,ep[i],ps[j]));CHECK(!run_un("exp2",ppf_exp2,mpfr_exp2,ep[i],ps[j]));CHECK(!run_un("exp10",ppf_exp10,mpfr_exp10,ep[i],ps[j]));CHECK(!run_un("expm1",ppf_expm1,mpfr_expm1,ep[i],ps[j]));CHECK(!run_un("exp2m1",ppf_exp2m1,mpfr_exp2m1,ep[i],ps[j]));CHECK(!run_un("exp10m1",ppf_exp10m1,mpfr_exp10m1,ep[i],ps[j]));}
  CHECK(!run_bin("pow",ppf_pow,mpfr_pow,"2.5","3.25",ps[j]));
  CHECK(!run_bin("pow",ppf_pow,mpfr_pow,"0.75","-2.5",ps[j]));
  CHECK(!run_bin("pow",ppf_pow,mpfr_pow,"-2","7",ps[j]));
 }
 puts("full transcendental differential passed");return 0;
}
