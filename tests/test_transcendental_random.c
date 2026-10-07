/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#c);exit(1);}}while(0)
static uint64_t st=UINT64_C(0x9e3779b97f4a7c15);
static uint64_t rng(void){st^=st>>12;st^=st<<25;st^=st>>27;return st*UINT64_C(2685821657736338717);}
static int sg(int x){return(x>0)-(x<0);}
static const ppf_rnd_t pr[5]={PPF_RNDN,PPF_RNDZ,PPF_RNDU,PPF_RNDD,PPF_RNDA};
static const mpfr_rnd_t mr[5]={MPFR_RNDN,MPFR_RNDZ,MPFR_RNDU,MPFR_RNDD,MPFR_RNDA};
typedef int(*pu)(ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);typedef int(*mu)(mpfr_ptr,mpfr_srcptr,mpfr_rnd_t);typedef int(*pb)(ppf_t*,const ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);typedef int(*mb)(mpfr_ptr,mpfr_srcptr,mpfr_srcptr,mpfr_rnd_t);
static int same(const ppf_t*p,mpfr_srcptr m){mpfr_t g;mpfr_init2(g,mpfr_get_prec(m));ppf_get_mpfr(g,p,MPFR_RNDN);int o=mpfr_equal_p(g,m)||(mpfr_nan_p(g)&&mpfr_nan_p(m))||(mpfr_inf_p(g)&&mpfr_inf_p(m)&&mpfr_signbit(g)==mpfr_signbit(m))||(mpfr_zero_p(g)&&mpfr_zero_p(m)&&mpfr_signbit(g)==mpfr_signbit(m));if(!o)mpfr_printf("got %.50Rg ref %.50Rg\n",g,m);mpfr_clear(g);return o;}
static int flags(ppf_context_t*c){unsigned p=c->flags;return !!(p&PPF_FLAG_UNDERFLOW)==!!mpfr_underflow_p()&&!!(p&PPF_FLAG_OVERFLOW)==!!mpfr_overflow_p()&&!!(p&PPF_FLAG_DIVBY0)==!!mpfr_divby0_p()&&!!(p&PPF_FLAG_NAN)==!!mpfr_nanflag_p()&&!!(p&PPF_FLAG_INEXACT)==!!mpfr_inexflag_p()&&!!(p&PPF_FLAG_ERANGE)==!!mpfr_erangeflag_p();}
static void unary(const char*n,pu f,mu g,ppf_t*x,mpfr_ptr mx,ppf_t*y,mpfr_ptr my,ppf_context_t*c){for(int r=0;r<5;r++){c->flags=0;mpfr_clear_flags();int a=f(y,x,pr[r],c),b=g(my,mx,mr[r]);if(a==PPF_TERNARY_ERROR||!same(y,my)||sg(a)!=sg(b)||!flags(c)){fprintf(stderr,"random %s rnd%d rc %d/%d flags%x\n",n,r,a,b,c->flags);exit(1);}}}
static void binary(const char*n,pb f,mb g,ppf_t*x,ppf_t*z,mpfr_ptr mx,mpfr_ptr mz,ppf_t*y,mpfr_ptr my,ppf_context_t*c){for(int r=0;r<5;r++){c->flags=0;mpfr_clear_flags();int a=f(y,x,z,pr[r],c),b=g(my,mx,mz,mr[r]);if(a==PPF_TERNARY_ERROR||!same(y,my)||sg(a)!=sg(b)||!flags(c)){fprintf(stderr,"random %s rnd%d rc %d/%d flags%x\n",n,r,a,b,c->flags);exit(1);}}}
int main(void){unsigned ps[]={8,17,53,96};for(unsigned pi=0;pi<4;pi++){unsigned p=ps[pi],ip=p+13;ppf_context_t c;ppf_t x,z,y;mpfr_t mx,mz,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,ip,&c)==0);CHECK(ppf_init2_ctx(&z,ip,&c)==0);CHECK(ppf_init2_ctx(&y,p,&c)==0);mpfr_init2(mx,ip);mpfr_init2(mz,ip);mpfr_init2(my,p);for(int k=0;k<20;k++){
 long m=(long)(1+(rng()%1000000));long e=(long)(rng()%31)-20;mpfr_set_si_2exp(mx,m,e,MPFR_RNDN);ppf_set_mpfr(&x,mx,PPF_RNDN,&c);unary("log",ppf_log,mpfr_log,&x,mx,&y,my,&c);unary("log2",ppf_log2,mpfr_log2,&x,mx,&y,my,&c);unary("log10",ppf_log10,mpfr_log10,&x,mx,&y,my,&c);
 long fm=(long)(rng()%1900001)-900000;mpfr_set_si_2exp(mx,fm,-20,MPFR_RNDN);ppf_set_mpfr(&x,mx,PPF_RNDN,&c);unary("log1p",ppf_log1p,mpfr_log1p,&x,mx,&y,my,&c);unary("log2p1",ppf_log2p1,mpfr_log2p1,&x,mx,&y,my,&c);unary("log10p1",ppf_log10p1,mpfr_log10p1,&x,mx,&y,my,&c);
 long em=(long)(rng()%2000001)-1000000;mpfr_set_si_2exp(mx,em,-17,MPFR_RNDN);ppf_set_mpfr(&x,mx,PPF_RNDN,&c);unary("exp",ppf_exp,mpfr_exp,&x,mx,&y,my,&c);unary("exp2",ppf_exp2,mpfr_exp2,&x,mx,&y,my,&c);unary("exp10",ppf_exp10,mpfr_exp10,&x,mx,&y,my,&c);unary("expm1",ppf_expm1,mpfr_expm1,&x,mx,&y,my,&c);unary("exp2m1",ppf_exp2m1,mpfr_exp2m1,&x,mx,&y,my,&c);unary("exp10m1",ppf_exp10m1,mpfr_exp10m1,&x,mx,&y,my,&c);
 long bm=(long)(1+rng()%1000000);mpfr_set_si_2exp(mx,bm,-18,MPFR_RNDN);mpfr_set_si_2exp(mz,(long)(rng()%49)-24,-3,MPFR_RNDN);ppf_set_mpfr(&x,mx,PPF_RNDN,&c);ppf_set_mpfr(&z,mz,PPF_RNDN,&c);binary("pow",ppf_pow,mpfr_pow,&x,&z,mx,mz,&y,my,&c);binary("powr",ppf_powr,mpfr_powr,&x,&z,mx,mz,&y,my,&c);
 }
 ppf_clear(&x);ppf_clear(&z);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(mx);mpfr_clear(mz);mpfr_clear(my);}puts("random transcendental differential passed");return 0;}
