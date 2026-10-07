/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(double)t.tv_sec+1e-9*(double)t.tv_nsec;}
static int same(const ppf_t*p,mpfr_srcptr m){mpfr_t g;mpfr_init2(g,mpfr_get_prec(m));ppf_get_mpfr(g,p,MPFR_RNDN);int o=mpfr_equal_p(g,m);mpfr_clear(g);return o;}
int main(int argc,char**argv){unsigned long p=argc>1?strtoul(argv[1],0,10):1000;unsigned long reps=argc>2?strtoul(argv[2],0,10):3;ppf_context_t c;ppf_t x,y,z;mpfr_t mx,my,mz;ppf_context_init(&c);ppf_init2_ctx(&x,p,&c);ppf_init2_ctx(&y,p,&c);ppf_init2_ctx(&z,p,&c);mpfr_init2(mx,p);mpfr_init2(my,p);mpfr_init2(mz,p);mpfr_set_d(mx,1.25,MPFR_RNDN);ppf_set_mpfr(&x,mx,PPF_RNDN,&c);ppf_const_log2(&y,PPF_RNDN,&c);mpfr_const_log2(my,MPFR_RNDN);const char*names[]={"log","exp","pow"};for(int op=0;op<3;op++){double tp=0,tm=0;for(unsigned long i=0;i<reps;i++){double a=now();if(op==0)ppf_log(&y,&x,PPF_RNDN,&c);else if(op==1)ppf_exp(&y,&x,PPF_RNDN,&c);else{ppf_set_d(&z,3.25,PPF_RNDN,&c);ppf_pow(&y,&x,&z,PPF_RNDN,&c);}tp+=now()-a;a=now();if(op==0)mpfr_log(my,mx,MPFR_RNDN);else if(op==1)mpfr_exp(my,mx,MPFR_RNDN);else{mpfr_set_d(mz,3.25,MPFR_RNDN);mpfr_pow(my,mx,mz,MPFR_RNDN);}tm+=now()-a;}if(!same(&y,my)){fprintf(stderr,"mismatch %s\n",names[op]);return 2;}printf("%lu,%s,%.9f,%.9f,%.6f\n",p,names[op],tp/reps,tm/reps,(tm/tp));}return 0;}
