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

typedef struct { const char *name; double seconds; } row_t;
static double now_s(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(double)t.tv_sec+1e-9*(double)t.tv_nsec;}
static int cmp_double(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}static double median(double*x,int n){qsort(x,n,sizeof(*x),cmp_double);return n&1?x[n/2]:(x[n/2-1]+x[n/2])/2;}
static void make_value(gmp_randstate_t rs,mpz_t z,mpfr_ptr m,ppf_t*p,ppf_context_t*c,unsigned long bits,unsigned seedshift){mpz_urandomb(z,rs,bits);mpz_setbit(z,bits-1);if(seedshift&1)mpz_neg(z,z);mpfr_set_z_2exp(m,z,-(long)(bits/3+seedshift),MPFR_RNDN);if(ppf_import_mpfr_exact(p,m,c)!=0){fprintf(stderr,"import failed\n");exit(2);}}
static int same(mpfr_srcptr a,mpfr_srcptr b){if(mpfr_zero_p(a)||mpfr_zero_p(b))return mpfr_zero_p(a)&&mpfr_zero_p(b)&&mpfr_signbit(a)==mpfr_signbit(b);return mpfr_equal_p(a,b)!=0;}
int main(int argc,char**argv){unsigned long bits=argc>1?strtoul(argv[1],0,10):1000000UL;int reps=argc>2?atoi(argv[2]):9;unsigned long workers=argc>3?strtoul(argv[3],0,10):4UL;enum{N=8};gmp_randstate_t rs;mpz_t z;ppf_context_t c;ppf_t a,b,d,r,pa[N],pb[N];const ppf_t*ap[N],*bp[N];mpfr_t ma,mb,md,mr,mo,xa[N],xb[N];mpfr_ptr xap[N],xbp[N];double *times=calloc((size_t)reps,sizeof(double));if(!times)return 2;gmp_randinit_mt(rs);gmp_randseed_ui(rs,0x5050464152495448UL);mpz_init(z);if(ppf_context_init(&c)!=0)return 2;if(ppf_init2_ctx(&a,bits,&c)||ppf_init2_ctx(&b,bits,&c)||ppf_init2_ctx(&d,bits,&c)||ppf_init2_ctx(&r,bits,&c))return 2;mpfr_inits2(bits,ma,mb,md,mr,mo,(mpfr_ptr)0);make_value(rs,z,ma,&a,&c,bits,3);make_value(rs,z,mb,&b,&c,bits,7);make_value(rs,z,md,&d,&c,bits,11);if(mpfr_sgn(ma)<0){mpfr_neg(ma,ma,MPFR_RNDN);ppf_import_mpfr_exact(&a,ma,&c);}for(int i=0;i<N;++i){ppf_init2_ctx(&pa[i],bits,&c);ppf_init2_ctx(&pb[i],bits,&c);mpfr_init2(xa[i],bits);mpfr_init2(xb[i],bits);make_value(rs,z,xa[i],&pa[i],&c,bits,(unsigned)(13+i));make_value(rs,z,xb[i],&pb[i],&c,bits,(unsigned)(29+i));ap[i]=&pa[i];bp[i]=&pb[i];xap[i]=xa[i];xbp[i]=xb[i];}
#define VERIFY(P,M) do{ppf_get_mpfr(mo,&r,MPFR_RNDN);if(!same(mo,mr)){fprintf(stderr,"verify failed %s\n",P);return 3;}}while(0)
#define BENCH(NAME,EXPR) do{for(int w=0;w<2;++w)(void)(EXPR);for(int k=0;k<reps;++k){double t=now_s();(void)(EXPR);times[k]=now_s()-t;}printf("%lu,%s,%.9f\n",bits,NAME,median(times,reps));}while(0)
    ppf_add(&r,&a,&b,PPF_RNDN,&c);mpfr_add(mr,ma,mb,MPFR_RNDN);VERIFY("add",mr);BENCH("ppf_add",ppf_add(&r,&a,&b,PPF_RNDN,&c));BENCH("mpfr_add",mpfr_add(mr,ma,mb,MPFR_RNDN));
    ppf_mul(&r,&a,&b,PPF_RNDN,&c);mpfr_mul(mr,ma,mb,MPFR_RNDN);VERIFY("mul",mr);BENCH("ppf_mul_1t",ppf_mul(&r,&a,&b,PPF_RNDN,&c));BENCH("ppf_mul_parallel",ppf_mul_threads(&r,&a,&b,PPF_RNDN,workers,&c));BENCH("mpfr_mul",mpfr_mul(mr,ma,mb,MPFR_RNDN));
    ppf_div(&r,&a,&b,PPF_RNDN,&c);mpfr_div(mr,ma,mb,MPFR_RNDN);VERIFY("div",mr);BENCH("ppf_div",ppf_div(&r,&a,&b,PPF_RNDN,&c));BENCH("mpfr_div",mpfr_div(mr,ma,mb,MPFR_RNDN));
    ppf_sqrt(&r,&a,PPF_RNDN,&c);mpfr_sqrt(mr,ma,MPFR_RNDN);VERIFY("sqrt",mr);BENCH("ppf_sqrt",ppf_sqrt(&r,&a,PPF_RNDN,&c));BENCH("mpfr_sqrt",mpfr_sqrt(mr,ma,MPFR_RNDN));
    ppf_fma(&r,&a,&b,&d,PPF_RNDN,&c);mpfr_fma(mr,ma,mb,md,MPFR_RNDN);VERIFY("fma",mr);BENCH("ppf_fma",ppf_fma(&r,&a,&b,&d,PPF_RNDN,&c));BENCH("mpfr_fma",mpfr_fma(mr,ma,mb,md,MPFR_RNDN));
    ppf_dot(&r,ap,bp,N,PPF_RNDN,&c);mpfr_dot(mr,xap,xbp,N,MPFR_RNDN);VERIFY("dot",mr);BENCH("ppf_dot_1t",ppf_dot(&r,ap,bp,N,PPF_RNDN,&c));BENCH("ppf_dot_parallel",ppf_dot_threads(&r,ap,bp,N,PPF_RNDN,workers,&c));BENCH("mpfr_dot",mpfr_dot(mr,xap,xbp,N,MPFR_RNDN));
#undef BENCH
    for(int i=0;i<N;++i){ppf_clear(&pa[i]);ppf_clear(&pb[i]);mpfr_clear(xa[i]);mpfr_clear(xb[i]);}ppf_clear(&a);ppf_clear(&b);ppf_clear(&d);ppf_clear(&r);ppf_context_clear(&c);mpfr_clears(ma,mb,md,mr,mo,(mpfr_ptr)0);mpz_clear(z);gmp_randclear(rs);free(times);return 0;}
