/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); exit(1);} } while(0)

typedef int (*pu_fn)(ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);
typedef int (*mu_fn)(mpfr_ptr,mpfr_srcptr,mpfr_rnd_t);
typedef int (*pb_fn)(ppf_t*,const ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);
typedef int (*mb_fn)(mpfr_ptr,mpfr_srcptr,mpfr_srcptr,mpfr_rnd_t);
static const ppf_rnd_t pr[5]={PPF_RNDN,PPF_RNDZ,PPF_RNDU,PPF_RNDD,PPF_RNDA};
static const mpfr_rnd_t mr[5]={MPFR_RNDN,MPFR_RNDZ,MPFR_RNDU,MPFR_RNDD,MPFR_RNDA};
static int sgn(int x){return (x>0)-(x<0);}
static int same(const ppf_t*p,mpfr_srcptr m){mpfr_t g;int ok;mpfr_init2(g,mpfr_get_prec(m));CHECK(ppf_get_mpfr(g,p,MPFR_RNDN)!=PPF_TERNARY_ERROR);if(mpfr_nan_p(m))ok=mpfr_nan_p(g);else if(mpfr_inf_p(m))ok=mpfr_inf_p(g)&&mpfr_signbit(m)==mpfr_signbit(g);else if(mpfr_zero_p(m))ok=mpfr_zero_p(g)&&mpfr_signbit(m)==mpfr_signbit(g);else ok=mpfr_equal_p(g,m);if(!ok)mpfr_printf("got=%.*Rg ref=%.*Rg\n",50,g,50,m);mpfr_clear(g);return ok;}
static void setv(ppf_t*p,mpfr_ptr m,const char*s,ppf_context_t*c){CHECK(mpfr_set_str(m,s,0,MPFR_RNDN)==0);CHECK(ppf_set_mpfr(p,m,PPF_RNDN,c)!=PPF_TERNARY_ERROR);}
static void unary(const char*n,pu_fn pf,mu_fn mf,const char*v,unsigned p,int all){ppf_context_t c;ppf_t x,y;mpfr_t mx,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,p+11,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(mx,p+11);mpfr_init2(my,p);setv(&x,mx,v,&c);int count=all?5:1;for(int i=0;i<count;i++){ppf_context_flags_clear(&c,PPF_FLAGS_ALL);mpfr_clear_flags();int a=pf(&y,&x,pr[i],&c),b=mf(my,mx,mr[i]);if(a==PPF_TERNARY_ERROR||!same(&y,my)||sgn(a)!=sgn(b)){fprintf(stderr,"%s(%s) p=%u rnd=%d a=%d b=%d\n",n,v,p,i,a,b);exit(1);}}ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(mx);mpfr_clear(my);}
static void binary(const char*n,pb_fn pf,mb_fn mf,const char*av,const char*bv,unsigned p,int all){ppf_context_t c;ppf_t a,b,y;mpfr_t ma,mb,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&a,p+9,&c)==0);CHECK(ppf_init2_ctx(&b,p+7,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(ma,p+9);mpfr_init2(mb,p+7);mpfr_init2(my,p);setv(&a,ma,av,&c);setv(&b,mb,bv,&c);int count=all?5:1;for(int i=0;i<count;i++){int x=pf(&y,&a,&b,pr[i],&c),z=mf(my,ma,mb,mr[i]);if(x==PPF_TERNARY_ERROR||!same(&y,my)||sgn(x)!=sgn(z)){fprintf(stderr,"%s(%s,%s) p=%u rnd=%d x=%d z=%d\n",n,av,bv,p,i,x,z);exit(1);}}ppf_clear(&a);ppf_clear(&b);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(ma);mpfr_clear(mb);mpfr_clear(my);}

static void gamma_inc_case(const char*a,const char*x,unsigned p){ppf_context_t c;ppf_t pa,px,y;mpfr_t ma,mx,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&pa,p+7,&c)==0);CHECK(ppf_init2_ctx(&px,p+9,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(ma,p+7);mpfr_init2(mx,p+9);mpfr_init2(my,p);setv(&pa,ma,a,&c);setv(&px,mx,x,&c);for(int i=0;i<5;i++){int u=ppf_gamma_inc(&y,&pa,&px,pr[i],&c),v=mpfr_gamma_inc(my,ma,mx,mr[i]);if(u==PPF_TERNARY_ERROR||!same(&y,my)||sgn(u)!=sgn(v)){fprintf(stderr,"gamma_inc(%s,%s) p=%u rnd=%d\n",a,x,p,i);exit(1);}}ppf_clear(&pa);ppf_clear(&px);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(ma);mpfr_clear(mx);mpfr_clear(my);}
static void lgamma_case(const char*x,unsigned p){ppf_context_t c;ppf_t px,y;mpfr_t mx,my;int ps,ms;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&px,p+7,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(mx,p+7);mpfr_init2(my,p);setv(&px,mx,x,&c);for(int i=0;i<5;i++){int u=ppf_lgamma(&y,&ps,&px,pr[i],&c),v=mpfr_lgamma(my,&ms,mx,mr[i]);if(u==PPF_TERNARY_ERROR||!same(&y,my)||sgn(u)!=sgn(v)||ps!=ms){fprintf(stderr,"lgamma(%s) p=%u rnd=%d signs=%d,%d\n",x,p,i,ps,ms);exit(1);}}ppf_clear(&px);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(mx);mpfr_clear(my);}
static void order_case(const char*n,long k,const char*x,unsigned p,int ykind){ppf_context_t c;ppf_t px,r;mpfr_t mx,mr0;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&px,p+7,&c)==0);CHECK(ppf_init2_ctx(&r,p,&c)==0);mpfr_init2(mx,p+7);mpfr_init2(mr0,p);setv(&px,mx,x,&c);for(int i=0;i<5;i++){int a=ykind?ppf_yn(&r,k,&px,pr[i],&c):ppf_jn(&r,k,&px,pr[i],&c);int b=ykind?mpfr_yn(mr0,k,mx,mr[i]):mpfr_jn(mr0,k,mx,mr[i]);if(a==PPF_TERNARY_ERROR||!same(&r,mr0)||sgn(a)!=sgn(b)){fprintf(stderr,"%s(%ld,%s) p=%u rnd=%d\n",n,k,x,p,i);exit(1);}}ppf_clear(&px);ppf_clear(&r);ppf_context_clear(&c);mpfr_clear(mx);mpfr_clear(mr0);}
static void constant_case(int id,unsigned p){ppf_context_t c;ppf_t y;mpfr_t m;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(m,p);for(int i=0;i<5;i++){int a,b;if(id==PPF_CONSTANT_EULER){a=ppf_const_euler(&y,pr[i],&c);b=mpfr_const_euler(m,mr[i]);}else{a=ppf_const_catalan(&y,pr[i],&c);b=mpfr_const_catalan(m,mr[i]);}if(a==PPF_TERNARY_ERROR||!same(&y,m)||sgn(a)!=sgn(b)){fprintf(stderr,"constant id=%d p=%u rnd=%d\n",id,p,i);exit(1);}}ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(m);}

int main(void){
  const unsigned ps[]={24,53,96};
  const char *eiv[]={"-5","-1","-0.125","0.125","1","5"};
  const char *liv[]={"-5","-1","-0.5","0.5","1","2","5"};
  const char *gv[]={"-3.5","-2.5","-0.5","0.5","1.5","2.5","8.25"};
  const char *dv[]={"-3.5","-2.5","-0.5","0.5","1","2.5","10"};
  const char *zv[]={"-5","-3","-2","-1","0","0.5","2","2.5","3","10"};
  const char *ev[]={"-3","-1","-0.5","0.5","1","3"};
  const char *bv[]={"0.125","1","2","5"};
  for(size_t ip=0;ip<sizeof(ps)/sizeof(ps[0]);ip++){unsigned p=ps[ip];
    for(size_t i=0;i<sizeof(eiv)/sizeof(eiv[0]);i++)unary("eint",ppf_eint,mpfr_eint,eiv[i],p,i==2);
    for(size_t i=0;i<sizeof(liv)/sizeof(liv[0]);i++)unary("li2",ppf_li2,mpfr_li2,liv[i],p,i==3);
    for(size_t i=0;i<sizeof(gv)/sizeof(gv[0]);i++){unary("gamma",ppf_gamma,mpfr_gamma,gv[i],p,i==4);lgamma_case(gv[i],p);}
    unary("lngamma",ppf_lngamma,mpfr_lngamma,"2.5",p,1);
    for(size_t i=0;i<sizeof(dv)/sizeof(dv[0]);i++)unary("digamma",ppf_digamma,mpfr_digamma,dv[i],p,i==5);
    binary("beta",ppf_beta,mpfr_beta,"2.5","1.5",p,1);binary("beta",ppf_beta,mpfr_beta,"-0.5","1.25",p,0);
    gamma_inc_case("0.5","0.125",p);gamma_inc_case("2.5","1.2",p);gamma_inc_case("5","8",p);gamma_inc_case("0","0.5",p);gamma_inc_case("-0.5","0.5",p);gamma_inc_case("-1","0.5",p);gamma_inc_case("-2.5","2",p);
    for(size_t i=0;i<sizeof(zv)/sizeof(zv[0]);i++)unary("zeta",ppf_zeta,mpfr_zeta,zv[i],p,i==7);
    for(size_t i=0;i<sizeof(ev)/sizeof(ev[0]);i++){unary("erf",ppf_erf,mpfr_erf,ev[i],p,i==3);unary("erfc",ppf_erfc,mpfr_erfc,ev[i],p,i==3);}
    for(size_t i=0;i<sizeof(bv)/sizeof(bv[0]);i++){unary("j0",ppf_j0,mpfr_j0,bv[i],p,0);unary("j1",ppf_j1,mpfr_j1,bv[i],p,0);unary("y0",ppf_y0,mpfr_y0,bv[i],p,0);unary("y1",ppf_y1,mpfr_y1,bv[i],p,0);}
    order_case("jn",3,"2",p,0);order_case("jn",-4,"1.25",p,0);order_case("yn",3,"2",p,1);order_case("yn",-4,"1.25",p,1);
    binary("agm",ppf_agm,mpfr_agm,"1","0.5",p,1);binary("agm",ppf_agm,mpfr_agm,"20","3",p,0);
    unary("ai",ppf_ai,mpfr_ai,"-3",p,0);unary("ai",ppf_ai,mpfr_ai,"0.5",p,1);unary("ai",ppf_ai,mpfr_ai,"5",p,0);
    constant_case(PPF_CONSTANT_EULER,p);constant_case(PPF_CONSTANT_CATALAN,p);
  }
  puts("special function differential passed");return 0;
}
