/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+1e-9*t.tv_nsec;}static int cmpd(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}static double med(double*x,int n){qsort(x,n,sizeof(*x),cmpd);return x[n/2];}
static void cand(mpz_t k, mpz_srcptr q, unsigned long bits, mpf_t qf, mpf_t yf, mpz_t sq, mpz_t delta){mpf_set_z(qf,q);mpf_sqrt(yf,qf);mpz_set_f(k,yf);mpz_mul(sq,k,k);while(mpz_cmp(sq,q)>0){mpz_mul_2exp(delta,k,1);mpz_sub_ui(delta,delta,1);mpz_sub(sq,sq,delta);mpz_sub_ui(k,k,1);}for(;;){mpz_mul_2exp(delta,k,1);mpz_add_ui(delta,delta,1);mpz_add(sq,sq,delta);if(mpz_cmp(sq,q)>0){mpz_sub(sq,sq,delta);break;}mpz_add_ui(k,k,1);} }
int main(int argc,char**argv){unsigned long p=argc>1?strtoul(argv[1],0,10):1000000;int reps=argc>2?atoi(argv[2]):7;gmp_randstate_t rs;gmp_randinit_mt(rs);gmp_randseed_ui(rs,123);mpz_t q,r,k,sq,delta;mpz_inits(q,r,k,sq,delta,NULL);mpz_urandomb(q,rs,p*2);mpz_setbit(q,p*2-1);mpf_t qf,yf;mpf_init2(qf,p+128);mpf_init2(yf,p+128);double*x=calloc(reps,sizeof(double)),*y=calloc(reps,sizeof(double));for(int i=0;i<2;i++){mpz_root(r,q,2);cand(k,q,p,qf,yf,sq,delta);}for(int i=0;i<reps;i++){double t=now();mpz_root(r,q,2);x[i]=now()-t;t=now();cand(k,q,p,qf,yf,sq,delta);y[i]=now()-t;if(mpz_cmp(r,k)!=0){puts("bad");return 2;}}printf("p=%lu root=%.9f cand=%.9f speed=%.3f\n",p,med(x,reps),med(y,reps),med(x,reps)/med(y,reps));}
