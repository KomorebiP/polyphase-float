/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#c); exit(1); } } while(0)

typedef int (*pu_fn)(ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);
typedef int (*mu_fn)(mpfr_ptr,mpfr_srcptr,mpfr_rnd_t);
typedef int (*pb_fn)(ppf_t*,const ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);
typedef int (*mb_fn)(mpfr_ptr,mpfr_srcptr,mpfr_srcptr,mpfr_rnd_t);

static int sgn(int x){return(x>0)-(x<0);}
static int same(ppf_t *p, mpfr_srcptr m){mpfr_t g;mpfr_init2(g,mpfr_get_prec(m));CHECK(ppf_get_mpfr(g,p,MPFR_RNDN)!=PPF_TERNARY_ERROR);int ok;if(mpfr_nan_p(m))ok=mpfr_nan_p(g);else if(mpfr_inf_p(m))ok=mpfr_inf_p(g)&&mpfr_signbit(m)==mpfr_signbit(g);else if(mpfr_zero_p(m))ok=mpfr_zero_p(g)&&mpfr_signbit(m)==mpfr_signbit(g);else ok=mpfr_equal_p(g,m);mpfr_clear(g);return ok;}
static void setv(ppf_t*p,mpfr_ptr m,const char*s,ppf_context_t*c){CHECK(mpfr_set_str(m,s,0,MPFR_RNDN)==0);CHECK(ppf_set_mpfr(p,m,PPF_RNDN,c)!=PPF_TERNARY_ERROR);}

static void unary_alias(const char*n,pu_fn pf,mu_fn mf,const char*v){ppf_context_t c;ppf_t x;mpfr_t mx;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,80,&c)==0);mpfr_init2(mx,80);setv(&x,mx,v,&c);int a=pf(&x,&x,PPF_RNDN,&c),b=mf(mx,mx,MPFR_RNDN);if(a==PPF_TERNARY_ERROR||!same(&x,mx)||sgn(a)!=sgn(b)){fprintf(stderr,"alias %s(%s)\n",n,v);exit(1);}ppf_clear(&x);ppf_context_clear(&c);mpfr_clear(mx);}

static void binary_alias(const char*n,pb_fn pf,mb_fn mf,const char*av,const char*bv,int which){(void)n;ppf_context_t c;ppf_t a,b;mpfr_t ma,mb;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&a,80,&c)==0);CHECK(ppf_init2_ctx(&b,80,&c)==0);mpfr_init2(ma,80);mpfr_init2(mb,80);setv(&a,ma,av,&c);setv(&b,mb,bv,&c);int x,z;if(which==0){x=pf(&a,&a,&b,PPF_RNDN,&c);z=mf(ma,ma,mb,MPFR_RNDN);CHECK(x!=PPF_TERNARY_ERROR&&same(&a,ma)&&sgn(x)==sgn(z));}else{x=pf(&b,&a,&b,PPF_RNDN,&c);z=mf(mb,ma,mb,MPFR_RNDN);CHECK(x!=PPF_TERNARY_ERROR&&same(&b,mb)&&sgn(x)==sgn(z));}ppf_clear(&a);ppf_clear(&b);ppf_context_clear(&c);mpfr_clear(ma);mpfr_clear(mb);}

static void gamma_inc_alias(int which){ppf_context_t c;ppf_t a,x;mpfr_t ma,mx;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&a,80,&c)==0);CHECK(ppf_init2_ctx(&x,80,&c)==0);mpfr_init2(ma,80);mpfr_init2(mx,80);setv(&a,ma,"2.5",&c);setv(&x,mx,"1.2",&c);int p,m;if(which==0){p=ppf_gamma_inc(&a,&a,&x,PPF_RNDN,&c);m=mpfr_gamma_inc(ma,ma,mx,MPFR_RNDN);CHECK(p!=PPF_TERNARY_ERROR&&same(&a,ma)&&sgn(p)==sgn(m));}else{p=ppf_gamma_inc(&x,&a,&x,PPF_RNDN,&c);m=mpfr_gamma_inc(mx,ma,mx,MPFR_RNDN);CHECK(p!=PPF_TERNARY_ERROR&&same(&x,mx)&&sgn(p)==sgn(m));}ppf_clear(&a);ppf_clear(&x);ppf_context_clear(&c);mpfr_clear(ma);mpfr_clear(mx);}

static void order_alias(long n,int ykind){ppf_context_t c;ppf_t x;mpfr_t mx;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,80,&c)==0);mpfr_init2(mx,80);setv(&x,mx,"2",&c);int p=ykind?ppf_yn(&x,n,&x,PPF_RNDN,&c):ppf_jn(&x,n,&x,PPF_RNDN,&c);int m=ykind?mpfr_yn(mx,n,mx,MPFR_RNDN):mpfr_jn(mx,n,mx,MPFR_RNDN);CHECK(p!=PPF_TERNARY_ERROR&&same(&x,mx)&&sgn(p)==sgn(m));ppf_clear(&x);ppf_context_clear(&c);mpfr_clear(mx);}

static void lgamma_alias(void){ppf_context_t c;ppf_t x;mpfr_t mx;int ps=0,ms=0;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,80,&c)==0);mpfr_init2(mx,80);setv(&x,mx,"-2.5",&c);int p=ppf_lgamma(&x,&ps,&x,PPF_RNDN,&c);int m=mpfr_lgamma(mx,&ms,mx,MPFR_RNDN);CHECK(p!=PPF_TERNARY_ERROR&&same(&x,mx)&&sgn(p)==sgn(m)&&ps==ms);ppf_clear(&x);ppf_context_clear(&c);mpfr_clear(mx);}

int main(void){
 unary_alias("eint",ppf_eint,mpfr_eint,"0.5");unary_alias("li2",ppf_li2,mpfr_li2,"0.5");unary_alias("gamma",ppf_gamma,mpfr_gamma,"2.5");unary_alias("lngamma",ppf_lngamma,mpfr_lngamma,"2.5");lgamma_alias();unary_alias("digamma",ppf_digamma,mpfr_digamma,"2.5");unary_alias("zeta",ppf_zeta,mpfr_zeta,"2.5");unary_alias("erf",ppf_erf,mpfr_erf,"0.5");unary_alias("erfc",ppf_erfc,mpfr_erfc,"0.5");unary_alias("j0",ppf_j0,mpfr_j0,"2");unary_alias("j1",ppf_j1,mpfr_j1,"2");unary_alias("y0",ppf_y0,mpfr_y0,"2");unary_alias("y1",ppf_y1,mpfr_y1,"2");unary_alias("ai",ppf_ai,mpfr_ai,"0.5");order_alias(3,0);order_alias(3,1);
 binary_alias("beta",ppf_beta,mpfr_beta,"2.5","1.5",0);binary_alias("beta",ppf_beta,mpfr_beta,"2.5","1.5",1);binary_alias("agm",ppf_agm,mpfr_agm,"1","0.5",0);binary_alias("agm",ppf_agm,mpfr_agm,"1","0.5",1);gamma_inc_alias(0);gamma_inc_alias(1);
 puts("special aliasing passed");return 0;
}
