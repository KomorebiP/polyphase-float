/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#c);exit(1);}}while(0)
typedef int(*pu)(ppf_t*,const ppf_t*,ppf_rnd_t,ppf_context_t*);typedef int(*mu)(mpfr_ptr,mpfr_srcptr,mpfr_rnd_t);
static int sg(int x){return(x>0)-(x<0);}static int same(ppf_t*p,mpfr_t m){mpfr_t g;mpfr_init2(g,53);CHECK(ppf_get_mpfr(g,p,MPFR_RNDN)!=PPF_TERNARY_ERROR);int ok=mpfr_nan_p(m)?mpfr_nan_p(g):mpfr_equal_p(g,m);if(!ok)mpfr_printf("got=%.20Rg ref=%.20Rg\n",g,m);mpfr_clear(g);return ok;}
static void one(const char*n,pu pf,mu mf,double d){ppf_context_t c;ppf_t x,y;mpfr_t mx,my;CHECK(ppf_context_init(&c)==0);CHECK(ppf_init2_ctx(&x,53,&c)==0);CHECK(ppf_init2_ctx(&y,53,&c)==0);mpfr_init2(mx,53);mpfr_init2(my,53);CHECK(ppf_set_d(&x,d,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);mpfr_set_d(mx,d,MPFR_RNDN);int a=pf(&y,&x,PPF_RNDN,&c),b=mf(my,mx,MPFR_RNDN);if(a==PPF_TERNARY_ERROR||!same(&y,my)||sg(a)!=sg(b)){fprintf(stderr,"%s %.17g a=%d b=%d\n",n,d,a,b);exit(1);}ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&c);mpfr_clear(mx);mpfr_clear(my);}
static unsigned long st=0x12345678UL;static double rnd(double a,double b){st=st*1664525UL+1013904223UL;double u=(double)(st&0xffffffUL)/16777216.0;return a+(b-a)*u;}
int main(void){for(int i=0;i<24;i++){double x=rnd(-5,5);if(fabs(x)<0.03)x=.125;one("eint",ppf_eint,mpfr_eint,x);one("li2",ppf_li2,mpfr_li2,x);double g=rnd(-4.8,9);double k=round(g);if(fabs(g-k)<.04)g+=.17;one("gamma",ppf_gamma,mpfr_gamma,g);one("digamma",ppf_digamma,mpfr_digamma,g);double z=rnd(-4.5,6);if(fabs(z-1)<.05)z+=.2;one("zeta",ppf_zeta,mpfr_zeta,z);one("erf",ppf_erf,mpfr_erf,rnd(-3,3));one("erfc",ppf_erfc,mpfr_erfc,rnd(-3,3));double q=rnd(.1,6);one("j0",ppf_j0,mpfr_j0,q);one("j1",ppf_j1,mpfr_j1,q);one("y0",ppf_y0,mpfr_y0,q);one("y1",ppf_y1,mpfr_y1,q);one("ai",ppf_ai,mpfr_ai,rnd(-5,5));fprintf(stderr,".%d",i);fflush(stderr);}puts("\nrandom special passed");ppf_free_constant_caches();mpfr_mp_memory_cleanup();return 0;}
