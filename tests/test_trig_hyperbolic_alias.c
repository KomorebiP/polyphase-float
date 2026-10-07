/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#c); exit(1); } } while (0)
typedef int (*pfu)(ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mfu)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
static int signum(int x){return (x>0)-(x<0);}
static int same(const ppf_t *p, mpfr_srcptr m){mpfr_t g;int ok;mpfr_init2(g,mpfr_get_prec(m));CHECK(ppf_get_mpfr(g,p,MPFR_RNDN)!=PPF_TERNARY_ERROR);if(mpfr_nan_p(m))ok=mpfr_nan_p(g);else if(mpfr_inf_p(m))ok=mpfr_inf_p(g)&&mpfr_signbit(m)==mpfr_signbit(g);else if(mpfr_zero_p(m))ok=mpfr_zero_p(g)&&mpfr_signbit(m)==mpfr_signbit(g);else ok=mpfr_equal_p(g,m);mpfr_clear(g);return ok;}
static void unary_alias(const char *name, pfu pf, mfu mf, const char *value){ppf_context_t c;ppf_t x;mpfr_t m,ref;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,80,&c)==0);mpfr_init2(m,80);mpfr_init2(ref,80);CHECK(mpfr_set_str(m,value,0,MPFR_RNDN)==0);CHECK(ppf_set_mpfr(&x,m,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);int a=pf(&x,&x,PPF_RNDN,&c);int b=mf(ref,m,MPFR_RNDN);if(a==PPF_TERNARY_ERROR||!same(&x,ref)||signum(a)!=signum(b)){fprintf(stderr,"alias %s failed\n",name);exit(2);}ppf_clear(&x);ppf_context_clear(&c);mpfr_clear(m);mpfr_clear(ref);}
int main(void){
    unary_alias("sin",ppf_sin,mpfr_sin,"0.3"); unary_alias("cos",ppf_cos,mpfr_cos,"0.3"); unary_alias("tan",ppf_tan,mpfr_tan,"0.3");
    unary_alias("asin",ppf_asin,mpfr_asin,"0.3"); unary_alias("acos",ppf_acos,mpfr_acos,"0.3"); unary_alias("atan",ppf_atan,mpfr_atan,"0.3");
    unary_alias("sinh",ppf_sinh,mpfr_sinh,"0.3"); unary_alias("cosh",ppf_cosh,mpfr_cosh,"0.3"); unary_alias("tanh",ppf_tanh,mpfr_tanh,"0.3");
    unary_alias("asinh",ppf_asinh,mpfr_asinh,"0.3"); unary_alias("acosh",ppf_acosh,mpfr_acosh,"2"); unary_alias("atanh",ppf_atanh,mpfr_atanh,"0.3");
    {
        ppf_context_t c; ppf_t y,x; mpfr_t my,mx,ref;
        CHECK(ppf_context_init(&c)==0); CHECK(ppf_init2_ctx(&y,80,&c)==0); CHECK(ppf_init2_ctx(&x,80,&c)==0);
        mpfr_init2(my,80);mpfr_init2(mx,80);mpfr_init2(ref,80);
        CHECK(mpfr_set_str(my,"0.75",0,MPFR_RNDN)==0);CHECK(mpfr_set_str(mx,"-0.25",0,MPFR_RNDN)==0);
        CHECK(ppf_set_mpfr(&y,my,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);CHECK(ppf_set_mpfr(&x,mx,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);
        int a=ppf_atan2(&y,&y,&x,PPF_RNDN,&c);int b=mpfr_atan2(ref,my,mx,MPFR_RNDN);CHECK(a!=PPF_TERNARY_ERROR&&same(&y,ref)&&signum(a)==signum(b));
        CHECK(ppf_set_mpfr(&y,my,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);CHECK(ppf_set_mpfr(&x,mx,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);
        a=ppf_atan2(&x,&y,&x,PPF_RNDN,&c);b=mpfr_atan2(ref,my,mx,MPFR_RNDN);CHECK(a!=PPF_TERNARY_ERROR&&same(&x,ref)&&signum(a)==signum(b));
        ppf_clear(&y);ppf_clear(&x);ppf_context_clear(&c);mpfr_clear(my);mpfr_clear(mx);mpfr_clear(ref);
    }
    {
        ppf_context_t c; ppf_t x,cval; mpfr_t mx,ms,mc;
        CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,80,&c)==0);CHECK(ppf_init2_ctx(&cval,80,&c)==0);
        mpfr_init2(mx,80);mpfr_init2(ms,80);mpfr_init2(mc,80);CHECK(mpfr_set_str(mx,"0.3",0,MPFR_RNDN)==0);CHECK(ppf_set_mpfr(&x,mx,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);
        int a=ppf_sin_cos(&x,&cval,&x,PPF_RNDN,&c);int b=mpfr_sin_cos(ms,mc,mx,MPFR_RNDN);CHECK(a==b&&same(&x,ms)&&same(&cval,mc));
        CHECK(ppf_set_mpfr(&x,mx,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);a=ppf_sinh_cosh(&x,&cval,&x,PPF_RNDN,&c);b=mpfr_sinh_cosh(ms,mc,mx,MPFR_RNDN);CHECK(a==b&&same(&x,ms)&&same(&cval,mc));
        ppf_clear(&x);ppf_clear(&cval);ppf_context_clear(&c);mpfr_clear(mx);mpfr_clear(ms);mpfr_clear(mc);
    }
    puts("TRIG/HYPERBOLIC ALIAS TESTS PASSED");
    return 0;
}
