/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+1e-9*t.tv_nsec;}static int cmpd(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}static double med(double*x,int n){qsort(x,n,sizeof(*x),cmpd);return x[n/2];}
static void qonly(mpz_t root,mpz_srcptr M,long e,unsigned long p,mpz_t num,mpz_t q){long bits=mpz_sizeinbase(M,2);long total=e+bits;long scale=(long)p+48+(total>=0?(total+1)/2:total/2);long power=2*scale-e;mpz_set_ui(num,0);mpz_setbit(num,power);mpz_fdiv_q(q,num,M);mpz_root(root,q,2);}
static int cmp_scaled(mpz_srcptr A,long ea, mpz_srcptr B,long eb,mpz_t t){if(ea==eb)return mpz_cmp(A,B);if(ea>eb){mpz_mul_2exp(t,A,ea-eb);return mpz_cmp(t,B);}mpz_mul_2exp(t,B,eb-ea);return mpz_cmp(A,t);}
static void direct(mpz_t k,mpz_srcptr M,long e,unsigned long p,mpf_t xf,mpf_t sf,mpf_t yf,mpf_t tf,mpz_t A,mpz_t delta,mpz_t target,mpz_t tmp){long ey;long S;mpf_set_z(xf,M);if(e>=0)mpf_mul_2exp(xf,xf,e);else mpf_div_2exp(xf,xf,-e);mpf_sqrt(sf,xf);mpf_ui_div(yf,1,sf);(void)mpf_get_d_2exp(&ey,yf);S=(long)p+64-ey;mpf_set(tf,yf);if(S>=0)mpf_mul_2exp(tf,tf,S);else mpf_div_2exp(tf,tf,-S);mpz_set_f(k,tf);mpz_mul(A,k,k);mpz_mul(A,A,M);mpz_set_ui(target,1); // compare A*2^e to 2^(2S)
while(cmp_scaled(A,e,target,2*S,tmp)>0){mpz_mul_2exp(delta,k,1);mpz_sub_ui(delta,delta,1);mpz_mul(delta,delta,M);mpz_sub(A,A,delta);mpz_sub_ui(k,k,1);}for(;;){mpz_mul_2exp(delta,k,1);mpz_add_ui(delta,delta,1);mpz_mul(delta,delta,M);mpz_add(A,A,delta);if(cmp_scaled(A,e,target,2*S,tmp)>0){mpz_sub(A,A,delta);break;}mpz_add_ui(k,k,1);} }
int main(int argc,char**argv){unsigned long p=argc>1?strtoul(argv[1],0,10):100000;int reps=argc>2?atoi(argv[2]):5;gmp_randstate_t rs;gmp_randinit_mt(rs);gmp_randseed_ui(rs,123);mpz_t M,r,k,num,q,A,delta,target,tmp;mpz_inits(M,r,k,num,q,A,delta,target,tmp,NULL);mpz_urandomb(M,rs,p);mpz_setbit(M,p-1);long e=-(long)p/2;mpf_t xf,sf,yf,tf;mpf_init2(xf,p+128);mpf_init2(sf,p+128);mpf_init2(yf,p+128);mpf_init2(tf,p+128);double*x=calloc(reps,sizeof(double)),*y=calloc(reps,sizeof(double));for(int i=0;i<2;i++){qonly(r,M,e,p,num,q);direct(k,M,e,p,xf,sf,yf,tf,A,delta,target,tmp);}for(int i=0;i<reps;i++){double t=now();qonly(r,M,e,p,num,q);x[i]=now()-t;t=now();direct(k,M,e,p,xf,sf,yf,tf,A,delta,target,tmp);y[i]=now()-t;}printf("p=%lu qonly=%.9f direct=%.9f speed=%.3f\n",p,med(x,reps),med(y,reps),med(x,reps)/med(y,reps));}
