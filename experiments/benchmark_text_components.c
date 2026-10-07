/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+1e-9*t.tv_nsec;}
static int cmp(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}static double med(double*x,int n){qsort(x,n,sizeof(*x),cmp);return x[n/2];}
int main(int argc,char**argv){unsigned long bits=argc>1?strtoul(argv[1],0,10):1000000;int reps=argc>2?atoi(argv[2]):31;ppf_context_t c;ppf_t big,small;mpz_t z;double *a=calloc(reps,sizeof(double)),*b=calloc(reps,sizeof(double));ppf_exp_t e;char*s;ppf_context_init(&c);ppf_init2_ctx(&big,bits,&c);ppf_init2_ctx(&small,640,&c);mpz_init(z);mpz_set_ui(z,1);mpz_mul_2exp(z,z,bits-1);mpz_setbit(z,bits/3);mpz_setbit(z,17);ppf_set_z(&big,z,PPF_RNDN,&c);ppf_set(&small,&big,PPF_RNDN,&c);for(int i=0;i<3;i++){s=ppf_get_str(0,&e,10,128,&big,PPF_RNDN,&c);ppf_free_str(s);s=ppf_get_str(0,&e,10,128,&small,PPF_RNDN,&c);ppf_free_str(s);}for(int i=0;i<reps;i++){double t;if(i&1){t=now();s=ppf_get_str(0,&e,10,128,&small,PPF_RNDN,&c);b[i]=now()-t;ppf_free_str(s);t=now();s=ppf_get_str(0,&e,10,128,&big,PPF_RNDN,&c);a[i]=now()-t;ppf_free_str(s);}else{t=now();s=ppf_get_str(0,&e,10,128,&big,PPF_RNDN,&c);a[i]=now()-t;ppf_free_str(s);t=now();s=ppf_get_str(0,&e,10,128,&small,PPF_RNDN,&c);b[i]=now()-t;ppf_free_str(s);}}printf("bits=%lu fast_two=%.9g exact_small=%.9g ratio=%.3f\n",bits,med(a,reps),med(b,reps),med(a,reps)/med(b,reps));}
