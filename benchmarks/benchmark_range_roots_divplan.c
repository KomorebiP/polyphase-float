/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ppf_openmp_compat.h"

static double now_s(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(double)t.tv_sec+1e-9*(double)t.tv_nsec;}
static int cmpd(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}static double med(double*x,int n){qsort(x,(size_t)n,sizeof(*x),cmpd);return n&1?x[n/2]:(x[n/2-1]+x[n/2])/2;}
static int same(mpfr_srcptr a,mpfr_srcptr b){if(mpfr_nan_p(a)||mpfr_nan_p(b))return mpfr_nan_p(a)&&mpfr_nan_p(b);if(mpfr_inf_p(a)||mpfr_inf_p(b))return mpfr_inf_p(a)&&mpfr_inf_p(b)&&mpfr_signbit(a)==mpfr_signbit(b);if(!mpfr_equal_p(a,b))return 0;return !mpfr_zero_p(a)||mpfr_signbit(a)==mpfr_signbit(b);}
static void make_value(gmp_randstate_t rs,mpz_t z,mpfr_ptr m,ppf_t*p,ppf_context_t*c,unsigned long bits,unsigned tag){mpz_urandomb(z,rs,bits);mpz_setbit(z,bits-1);mpfr_set_z_2exp(m,z,-(long)(bits/2+tag),MPFR_RNDN);if(ppf_import_mpfr_exact(p,m,c)!=0){fprintf(stderr,"import failed\n");exit(2);}}
#define BENCH(NAME,EXPR) do{for(int w=0;w<2;++w)(void)(EXPR);for(int k=0;k<reps;++k){double t=now_s();(void)(EXPR);times[k]=now_s()-t;}printf("%lu,%s,%.9f\n",bits,NAME,med(times,reps));}while(0)

int main(int argc,char**argv){
    unsigned long bits=argc>1?strtoul(argv[1],0,10):1000000UL;int reps=argc>2?atoi(argv[2]):5;unsigned long workers=argc>3?strtoul(argv[3],0,10):4UL;enum{N=8};
    gmp_randstate_t rs;mpz_t z;ppf_context_t c;ppf_t a,b,r,nums[N],outs[N];const ppf_t*np[N];ppf_t*op[N];int tern[N];
    mpfr_t ma,mb,mr,mo,mnums[N],mouts[N];ppf_divisor_plan_t*plan=NULL;double*times=calloc((size_t)reps,sizeof(*times));if(!times)return 2;
    gmp_randinit_mt(rs);gmp_randseed_ui(rs,0x524f4f54504c414eUL);mpz_init(z);if(ppf_context_init(&c)!=0)return 2;
    if(ppf_init2_ctx(&a,bits,&c)||ppf_init2_ctx(&b,bits,&c)||ppf_init2_ctx(&r,bits,&c)) return 2;
    mpfr_inits2(bits,ma,mb,mr,mo,(mpfr_ptr)0);
    make_value(rs,z,ma,&a,&c,bits,7);make_value(rs,z,mb,&b,&c,bits,13);if(mpfr_sgn(ma)<0){mpfr_neg(ma,ma,MPFR_RNDN);ppf_import_mpfr_exact(&a,ma,&c);}if(mpfr_sgn(mb)<0){mpfr_neg(mb,mb,MPFR_RNDN);ppf_import_mpfr_exact(&b,mb,&c);}
    for(int i=0;i<N;++i){ppf_init2_ctx(&nums[i],bits,&c);ppf_init2_ctx(&outs[i],bits,&c);mpfr_init2(mnums[i],bits);mpfr_init2(mouts[i],bits);make_value(rs,z,mnums[i],&nums[i],&c,bits,(unsigned)(17+i));np[i]=&nums[i];op[i]=&outs[i];}
    double bt=now_s();if(ppf_divisor_plan_create(&plan,&b,bits,&c)!=0)return 3;printf("%lu,ppf_plan_build,%.9f\n",bits,now_s()-bt);
#define VERIFY(P,M) do{ppf_get_mpfr(mo,&r,MPFR_RNDN);if(!same(mo,mr)){fprintf(stderr,"verify failed %s\n",P);return 4;}}while(0)
    ppf_cbrt(&r,&a,PPF_RNDN,&c);mpfr_cbrt(mr,ma,MPFR_RNDN);VERIFY("cbrt",mr);BENCH("ppf_cbrt",ppf_cbrt(&r,&a,PPF_RNDN,&c));BENCH("mpfr_cbrt",mpfr_cbrt(mr,ma,MPFR_RNDN));
    ppf_rootn_ui(&r,&a,7,PPF_RNDN,&c);mpfr_rootn_ui(mr,ma,7,MPFR_RNDN);VERIFY("root7",mr);BENCH("ppf_root7",ppf_rootn_ui(&r,&a,7,PPF_RNDN,&c));BENCH("mpfr_root7",mpfr_rootn_ui(mr,ma,7,MPFR_RNDN));
    ppf_rec_sqrt(&r,&a,PPF_RNDN,&c);mpfr_rec_sqrt(mr,ma,MPFR_RNDN);VERIFY("recsqrt",mr);BENCH("ppf_rec_sqrt",ppf_rec_sqrt(&r,&a,PPF_RNDN,&c));BENCH("mpfr_rec_sqrt",mpfr_rec_sqrt(mr,ma,MPFR_RNDN));
    ppf_hypot(&r,&a,&b,PPF_RNDN,&c);mpfr_hypot(mr,ma,mb,MPFR_RNDN);VERIFY("hypot",mr);BENCH("ppf_hypot",ppf_hypot(&r,&a,&b,PPF_RNDN,&c));BENCH("mpfr_hypot",mpfr_hypot(mr,ma,mb,MPFR_RNDN));
    ppf_div(&r,&a,&b,PPF_RNDN,&c);mpfr_div(mr,ma,mb,MPFR_RNDN);VERIFY("div",mr);BENCH("ppf_div_direct",ppf_div(&r,&a,&b,PPF_RNDN,&c));BENCH("ppf_div_planned",ppf_div_with_plan(&r,&a,plan,PPF_RNDN,&c));BENCH("mpfr_div",mpfr_div(mr,ma,mb,MPFR_RNDN));
    if(ppf_div_with_plan_many(op,np,N,plan,PPF_RNDN,workers,tern,&c)!=0)return 5;
    for(int i=0;i<N;++i){mpfr_div(mouts[i],mnums[i],mb,MPFR_RNDN);ppf_get_mpfr(mo,&outs[i],MPFR_RNDN);if(!same(mo,mouts[i]))return 6;}
    BENCH("ppf_plan_batch_matched",ppf_div_with_plan_many(op,np,N,plan,PPF_RNDN,workers,tern,&c));
    for(int w=0;w<2;++w){
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel for num_threads((int)workers) schedule(static))
#endif
        for(int i=0;i<N;++i)mpfr_div(mouts[i],mnums[i],mb,MPFR_RNDN);
    }
    for(int k=0;k<reps;++k){double t=now_s();
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel for num_threads((int)workers) schedule(static))
#endif
        for(int i=0;i<N;++i)mpfr_div(mouts[i],mnums[i],mb,MPFR_RNDN);times[k]=now_s()-t;}
    printf("%lu,mpfr_batch_matched,%.9f\n",bits,med(times,reps));
#undef VERIFY
#undef BENCH
    ppf_divisor_plan_destroy(plan);for(int i=0;i<N;++i){ppf_clear(&nums[i]);ppf_clear(&outs[i]);mpfr_clear(mnums[i]);mpfr_clear(mouts[i]);}ppf_clear(&a);ppf_clear(&b);ppf_clear(&r);ppf_context_clear(&c);mpfr_clears(ma,mb,mr,mo,(mpfr_ptr)0);mpz_clear(z);gmp_randclear(rs);free(times);return 0;
}
