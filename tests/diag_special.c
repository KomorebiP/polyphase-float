/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <string.h>

int mpfr_eint(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_li2(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_gamma(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_gamma_inc(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_lngamma(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_lgamma(mpfr_ptr, int *, mpfr_srcptr, mpfr_rnd_t);
int mpfr_digamma(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_beta(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_zeta(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_zeta_ui(mpfr_ptr, unsigned long, mpfr_rnd_t);
int mpfr_erf(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_erfc(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_j0(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_j1(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_jn(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
int mpfr_y0(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_y1(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_yn(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
int mpfr_agm(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_ai(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_const_euler(mpfr_ptr, mpfr_rnd_t);
int mpfr_const_catalan(mpfr_ptr, mpfr_rnd_t);
int mpfr_printf(const char *, ...);

static int signum(int x){return (x>0)-(x<0);}
static int check(const char *name, ppf_t *p, int tp, mpfr_t m, int tm){
    mpfr_t got; mpfr_init2(got, mpfr_get_prec(m));
    int eg=ppf_get_mpfr(got,p,MPFR_RNDN);
    int ok=eg!=PPF_TERNARY_ERROR && mpfr_equal_p(got,m) && signum(tp)==signum(tm);
    printf("%-12s ok=%d ptern=%d mtern=%d ",name,ok,tp,tm);
    if(!ok){mpfr_printf("got=%.20Rg ref=%.20Rg\n",got,m);}else puts("");
    mpfr_clear(got); return ok?0:1;
}
int main(void){
    int bad=0,sp=0,sm=0; ppf_context_t c; ppf_context_init(&c);
    ppf_t x,y,r; ppf_init2(&x,128); ppf_init2(&y,128); ppf_init2(&r,128);
    mpfr_t mx,my,mr; mpfr_init2(mx,128);mpfr_init2(my,128);mpfr_init2(mr,128);
#define SETX(v) do{ppf_set_d(&x,(v),PPF_RNDN,&c);mpfr_set_d(mx,(v),MPFR_RNDN);}while(0)
#define SETY(v) do{ppf_set_d(&y,(v),PPF_RNDN,&c);mpfr_set_d(my,(v),MPFR_RNDN);}while(0)
#define U1(name,pfn,mfn,val) do{SETX(val);int tp=pfn(&r,&x,PPF_RNDN,&c);int tm=mfn(mr,mx,MPFR_RNDN);bad+=check(name,&r,tp,mr,tm);}while(0)
    U1("eint",ppf_eint,mpfr_eint,0.5);
    U1("li2",ppf_li2,mpfr_li2,0.5);
    U1("gamma",ppf_gamma,mpfr_gamma,2.5);
    SETX(2.5);SETY(1.2);bad+=check("gamma_inc",&r,ppf_gamma_inc(&r,&x,&y,PPF_RNDN,&c),mr,mpfr_gamma_inc(mr,mx,my,MPFR_RNDN));
    U1("lngamma",ppf_lngamma,mpfr_lngamma,2.5);
    SETX(-0.5); {int tp=ppf_lgamma(&r,&sp,&x,PPF_RNDN,&c);int tm=mpfr_lgamma(mr,&sm,mx,MPFR_RNDN);bad+=check("lgamma",&r,tp,mr,tm);printf(" signs %d %d\n",sp,sm);if(sp!=sm)bad++;}
    U1("digamma",ppf_digamma,mpfr_digamma,2.5);
    SETX(2.5);SETY(1.5);bad+=check("beta",&r,ppf_beta(&r,&x,&y,PPF_RNDN,&c),mr,mpfr_beta(mr,mx,my,MPFR_RNDN));
    U1("zeta",ppf_zeta,mpfr_zeta,2.5);
    bad+=check("zeta_ui",&r,ppf_zeta_ui(&r,3,PPF_RNDN,&c),mr,mpfr_zeta_ui(mr,3,MPFR_RNDN));
    U1("erf",ppf_erf,mpfr_erf,0.5); U1("erfc",ppf_erfc,mpfr_erfc,0.5);
    U1("j0",ppf_j0,mpfr_j0,2.0); U1("j1",ppf_j1,mpfr_j1,2.0);
    SETX(2.0);bad+=check("jn3",&r,ppf_jn(&r,3,&x,PPF_RNDN,&c),mr,mpfr_jn(mr,3,mx,MPFR_RNDN));
    U1("y0",ppf_y0,mpfr_y0,2.0); U1("y1",ppf_y1,mpfr_y1,2.0);
    SETX(2.0);bad+=check("yn3",&r,ppf_yn(&r,3,&x,PPF_RNDN,&c),mr,mpfr_yn(mr,3,mx,MPFR_RNDN));
    SETX(1.0);SETY(0.5);bad+=check("agm",&r,ppf_agm(&r,&x,&y,PPF_RNDN,&c),mr,mpfr_agm(mr,mx,my,MPFR_RNDN));
    U1("airy",ppf_ai,mpfr_ai,0.5);
    bad+=check("euler",&r,ppf_const_euler(&r,PPF_RNDN,&c),mr,mpfr_const_euler(mr,MPFR_RNDN));
    bad+=check("catalan",&r,ppf_const_catalan(&r,PPF_RNDN,&c),mr,mpfr_const_catalan(mr,MPFR_RNDN));
    printf("bad=%d\n",bad);
    mpfr_clears(mx,my,mr,(mpfr_ptr)0);ppf_clear(&x);ppf_clear(&y);ppf_clear(&r);ppf_context_clear(&c);return bad?1:0;
}
