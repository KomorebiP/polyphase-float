/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float_mpfr.h"
#include "ppf_openmp_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}
static void die(const char *s) { fprintf(stderr,"matched recsqrt failure: %s\n",s); exit(2); }
static int same_ternary(int a,int b){return (a>0)==(b>0)&&(a<0)==(b<0);}

int main(int argc,char **argv){
    const char *method=argc>1?argv[1]:"ppf_outer";
    unsigned long bits=argc>2?strtoul(argv[2],0,10):1000000UL;
    int workers=argc>3?atoi(argv[3]):3;
    int jobs=argc>4?atoi(argv[4]):3;
    int inner=argc>5?atoi(argv[5]):1;
    ppf_context_t *ctx; ppf_t *px,*py; mpfr_t *mx,*my,*ref,*exported;
    mpz_t z; int *pt,*mt; double started,elapsed;
    if (bits < 64) bits = 64;
    if (workers < 1) workers = 1;
    if (jobs < 1) jobs = 1;
    if (inner < 1) inner = 1;
    if(!mpfr_buildopt_tls_p() && workers>1) die("MPFR build lacks TLS");
    ctx=calloc((size_t)jobs,sizeof(*ctx)); px=calloc((size_t)jobs,sizeof(*px));py=calloc((size_t)jobs,sizeof(*py));
    mx=calloc((size_t)jobs,sizeof(*mx));my=calloc((size_t)jobs,sizeof(*my));ref=calloc((size_t)jobs,sizeof(*ref));exported=calloc((size_t)jobs,sizeof(*exported));
    pt=calloc((size_t)jobs,sizeof(*pt));mt=calloc((size_t)jobs,sizeof(*mt));
    if(!ctx||!px||!py||!mx||!my||!ref||!exported||!pt||!mt)die("allocation");
    mpz_init(z);
    for(int j=0;j<jobs;j++){
        if(ppf_context_init(&ctx[j])||ppf_init2_ctx(&px[j],bits,&ctx[j])||ppf_init2_ctx(&py[j],bits,&ctx[j]))die("ppf init");
        mpfr_init2(mx[j],bits);mpfr_init2(my[j],bits);mpfr_init2(ref[j],bits);mpfr_init2(exported[j],bits);
        mpz_set_ui(z,1);mpz_mul_2exp(z,z,bits-1);
        for(unsigned k=0;k<47;k++){unsigned long pos=(unsigned long)(((uint64_t)(k+3)*(bits/(unsigned long)(53+j)+1)+(unsigned long)(17*j+11))%(bits-1));mpz_setbit(z,pos);}
        ppf_set_z(&px[j],z,PPF_RNDN,&ctx[j]);mpfr_set_z(mx[j],z,MPFR_RNDN);
        if((j&1)!=0){ppf_mul_2si(&px[j],&px[j],(long)(j%17)-8,PPF_RNDN,&ctx[j]);mpfr_mul_2si(mx[j],mx[j],(long)(j%17)-8,MPFR_RNDN);}
        ppf_get_mpfr(exported[j], &px[j], MPFR_RNDN);
        if (!mpfr_equal_p(exported[j], mx[j])) die("input mismatch");
        mt[j]=mpfr_rec_sqrt(ref[j],mx[j],MPFR_RNDN);
    }
    /* warmup */
    for(int j=0;j<jobs;j++) ppf_rec_sqrt(&py[j],&px[j],PPF_RNDN,&ctx[j]);
    for(int j=0;j<jobs;j++) mpfr_rec_sqrt(my[j],mx[j],MPFR_RNDN);
    started=now_seconds();
    for(int r=0;r<inner;r++){
        if(!strcmp(method,"ppf_outer")){
PPF_OMP_PRAGMA(omp parallel for num_threads(workers) schedule(static))
            for(int j=0;j<jobs;j++) pt[j]=ppf_rec_sqrt(&py[j],&px[j],PPF_RNDN,&ctx[j]);
        }else if(!strcmp(method,"mpfr_outer")){
PPF_OMP_PRAGMA(omp parallel for num_threads(workers) schedule(static))
            for(int j=0;j<jobs;j++) mt[j]=mpfr_rec_sqrt(my[j],mx[j],MPFR_RNDN);
        } else if (!strcmp(method, "ppf_serial")) {
            for (int j = 0; j < jobs; j++)
                pt[j] = ppf_rec_sqrt(&py[j], &px[j], PPF_RNDN, &ctx[j]);
        } else if(!strcmp(method,"mpfr_serial")){
            for(int j=0;j<jobs;j++)mt[j]=mpfr_rec_sqrt(my[j],mx[j],MPFR_RNDN);
        }else die("unknown method");
    }
    elapsed=(now_seconds()-started)/(double)inner;
    if(!strncmp(method,"ppf_",4)){
        for(int j=0;j<jobs;j++){
            ppf_get_mpfr(exported[j],&py[j],MPFR_RNDN);
            if(!mpfr_equal_p(exported[j],ref[j])||!same_ternary(pt[j],mt[j]))die("ppf correctness");
        }
    }else{
        for(int j=0;j<jobs;j++)if(!mpfr_equal_p(my[j],ref[j]))die("mpfr correctness");
    }
    printf("{\"method\":\"%s\",\"bits\":%lu,\"workers\":%d,\"jobs\":%d,\"seconds_per_batch\":%.12f}\n",method,bits,workers,jobs,elapsed);
PPF_OMP_PRAGMA(omp parallel num_threads(workers))
    { mpfr_mp_memory_cleanup(); }
    for(int j=0;j<jobs;j++){ppf_clear(&px[j]);ppf_clear(&py[j]);ppf_context_clear(&ctx[j]);mpfr_clears(mx[j],my[j],ref[j],exported[j],(mpfr_ptr)0);}
    mpz_clear(z);free(ctx);free(px);free(py);free(mx);free(my);free(ref);free(exported);free(pt);free(mt);return 0;
}
