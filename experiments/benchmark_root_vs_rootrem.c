/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+1e-9*t.tv_nsec;}
static int cmpd(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}static double med(double*x,int n){qsort(x,n,sizeof(*x),cmpd);return x[n/2];}
int main(int argc,char**argv){unsigned long p=argc>1?strtoul(argv[1],0,10):1000000;unsigned n=argc>2?atoi(argv[2]):3;int reps=argc>3?atoi(argv[3]):7;gmp_randstate_t rs;gmp_randinit_mt(rs);gmp_randseed_ui(rs,123);mpz_t a,r,rem;mpz_inits(a,r,rem,NULL);mpz_urandomb(a,rs,p*n);mpz_setbit(a,p*n-1);double *x=calloc(reps,sizeof(double)),*y=calloc(reps,sizeof(double));for(int i=0;i<2;i++){mpz_rootrem(r,rem,a,n);mpz_root(r,a,n);}for(int i=0;i<reps;i++){double t=now();mpz_rootrem(r,rem,a,n);x[i]=now()-t;t=now();mpz_root(r,a,n);y[i]=now()-t;}printf("p=%lu n=%u rootrem=%.9f root=%.9f speed=%.3f\n",p,n,med(x,reps),med(y,reps),med(x,reps)/med(y,reps));}
