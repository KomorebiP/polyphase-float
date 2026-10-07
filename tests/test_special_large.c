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
static const ppf_rnd_t pr[5]={PPF_RNDN,PPF_RNDZ,PPF_RNDU,PPF_RNDD,PPF_RNDA};
static const mpfr_rnd_t mr[5]={MPFR_RNDN,MPFR_RNDZ,MPFR_RNDU,MPFR_RNDD,MPFR_RNDA};
static int sg(int x){return(x>0)-(x<0);}static int same(ppf_t*p,mpfr_t m){mpfr_t g;mpfr_init2(g,mpfr_get_prec(m));CHECK(ppf_get_mpfr(g,p,MPFR_RNDN)!=PPF_TERNARY_ERROR);int ok;if(mpfr_nan_p(m))ok=mpfr_nan_p(g);else if(mpfr_inf_p(m))ok=mpfr_inf_p(g)&&mpfr_signbit(m)==mpfr_signbit(g);else if(mpfr_zero_p(m))ok=mpfr_zero_p(g)&&mpfr_signbit(m)==mpfr_signbit(g);else ok=mpfr_equal_p(g,m);if(!ok)mpfr_printf("got=%Ra ref=%Ra\n",g,m);mpfr_clear(g);return ok;}static void setv(ppf_t*p,mpfr_ptr m,const char*s,ppf_context_t*c){CHECK(mpfr_set_str(m,s,0,MPFR_RNDN)==0);CHECK(ppf_set_mpfr(p,m,PPF_RNDN,c)!=PPF_TERNARY_ERROR);}
static void unary(const char*n,pu_fn pf,mu_fn mf,const char*v,unsigned p,int all){ppf_context_t c;ppf_t x,y;mpfr_t mx,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,p+23,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(mx,p+23);mpfr_init2(my,p);setv(&x,mx,v,&c);for(int i=0;i<(all?5:1);i++){int a=pf(&y,&x,pr[i],&c),b=mf(my,mx,mr[i]);if(a==PPF_TERNARY_ERROR||!same(&y,my)||sg(a)!=sg(b)){fprintf(stderr,"%s p=%u rnd=%d\n",n,p,i);exit(1);}}ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(mx);mpfr_clear(my);}
static void binary(const char*n,pb_fn pf,mb_fn mf,const char*av,const char*bv,unsigned p,int all){ppf_context_t c;ppf_t a,b,y;mpfr_t ma,mb,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&a,p+17,&c)==0);CHECK(ppf_init2_ctx(&b,p+19,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(ma,p+17);mpfr_init2(mb,p+19);mpfr_init2(my,p);setv(&a,ma,av,&c);setv(&b,mb,bv,&c);for(int i=0;i<(all?5:1);i++){int x=pf(&y,&a,&b,pr[i],&c),z=mf(my,ma,mb,mr[i]);if(x==PPF_TERNARY_ERROR||!same(&y,my)||sg(x)!=sg(z)){fprintf(stderr,"%s p=%u rnd=%d\n",n,p,i);exit(1);}}ppf_clear(&a);ppf_clear(&b);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(ma);mpfr_clear(mb);mpfr_clear(my);}
static void gamma_inc(unsigned p,int all){ppf_context_t c;ppf_t a,x,y;mpfr_t ma,mx,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&a,p+17,&c)==0);CHECK(ppf_init2_ctx(&x,p+19,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(ma,p+17);mpfr_init2(mx,p+19);mpfr_init2(my,p);setv(&a,ma,"2.5",&c);setv(&x,mx,"1.2",&c);for(int i=0;i<(all?5:1);i++){int q=ppf_gamma_inc(&y,&a,&x,pr[i],&c),r=mpfr_gamma_inc(my,ma,mx,mr[i]);CHECK(q!=PPF_TERNARY_ERROR&&same(&y,my)&&sg(q)==sg(r));}ppf_clear(&a);ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(ma);mpfr_clear(mx);mpfr_clear(my);}
static void order(long n,int second,unsigned p){ppf_context_t c;ppf_t x,y;mpfr_t mx,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,p+19,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(mx,p+19);mpfr_init2(my,p);setv(&x,mx,"2",&c);int a=second?ppf_yn(&y,n,&x,PPF_RNDN,&c):ppf_jn(&y,n,&x,PPF_RNDN,&c);int b=second?mpfr_yn(my,n,mx,MPFR_RNDN):mpfr_jn(my,n,mx,MPFR_RNDN);CHECK(a!=PPF_TERNARY_ERROR&&same(&y,my)&&sg(a)==sg(b));ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(mx);mpfr_clear(my);}
static void constants(unsigned p,int all){ppf_context_t c;ppf_t y;mpfr_t m;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(m,p);for(int i=0;i<(all?5:1);i++){int a=ppf_const_euler(&y,pr[i],&c),b=mpfr_const_euler(m,mr[i]);CHECK(a!=PPF_TERNARY_ERROR&&same(&y,m)&&sg(a)==sg(b));a=ppf_const_catalan(&y,pr[i],&c);b=mpfr_const_catalan(m,mr[i]);CHECK(a!=PPF_TERNARY_ERROR&&same(&y,m)&&sg(a)==sg(b));}ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(m);}
static void run(unsigned p,int all){unary("eint",ppf_eint,mpfr_eint,"0.5",p,all);unary("li2",ppf_li2,mpfr_li2,"0.5",p,all);unary("gamma",ppf_gamma,mpfr_gamma,"2.5",p,all);gamma_inc(p,all);unary("lngamma",ppf_lngamma,mpfr_lngamma,"2.5",p,all);unary("digamma",ppf_digamma,mpfr_digamma,"2.5",p,all);binary("beta",ppf_beta,mpfr_beta,"2.5","1.5",p,all);unary("zeta",ppf_zeta,mpfr_zeta,"2.5",p,all);unary("erf",ppf_erf,mpfr_erf,"0.5",p,all);unary("erfc",ppf_erfc,mpfr_erfc,"0.5",p,all);unary("j0",ppf_j0,mpfr_j0,"2",p,all);unary("j1",ppf_j1,mpfr_j1,"2",p,all);unary("y0",ppf_y0,mpfr_y0,"2",p,all);unary("y1",ppf_y1,mpfr_y1,"2",p,all);order(3,0,p);order(3,1,p);binary("agm",ppf_agm,mpfr_agm,"1","0.5",p,all);unary("ai",ppf_ai,mpfr_ai,"0.5",p,all);constants(p,all);}
int main(void){run(256,1);run(512,0);puts("large special-function tests passed");return 0;}
