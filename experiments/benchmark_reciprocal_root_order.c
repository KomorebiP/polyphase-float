/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static unsigned long euclidean_mod2(ppf_exp_t e) {
    long r = (long)(e % 2);
    return (unsigned long)(r < 0 ? r + 2 : r);
}

static int candidate_rec_sqrt(ppf_t *rop, const ppf_t *x,
                              ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t m, rad, a, numerator, q, rem, lower, upper, d, den1, correction;
    ppf_exp_t e;
    int ternary = 0;
    ppf_interval_status_t status;
    if (!ppf_regular_p(x) || x->sign < 0) return PPF_TERNARY_ERROR;
    mpz_inits(m, rad, a, numerator, q, rem, lower, upper, d, den1,
              correction, NULL);
    e = ppf_get_z_2exp(m, x);
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        uint64_t desired = rop->precision + 48u + 64u * attempt;
        uint64_t mbits = (uint64_t)mpz_sizeinbase(m, 2);
        uint64_t rshift = 2u * desired > mbits ? 2u * desired - mbits : 0u;
        unsigned long parity = euclidean_mod2(e);
        uint64_t A, nexp;
        __extension__ typedef __int128 i128;
        i128 k128, scale128;
        int exact_root;
        if ((rshift & 1u) != parity) ++rshift;
        if (rshift > ULONG_MAX) goto fail;
        mpz_mul_2exp(rad, m, (mp_bitcnt_t)rshift);
        exact_root = mpz_root(a, rad, 2);
        A = (uint64_t)mpz_sizeinbase(a, 2);
        nexp = A + desired;
        if (nexp > ULONG_MAX) goto fail;
        k128 = ((i128)rshift - (i128)e) / 2;
        scale128 = (i128)nexp - k128;
        if (scale128 < INT64_MIN || scale128 > INT64_MAX) goto fail;
        mpz_set_ui(numerator, 0);
        mpz_setbit(numerator, (mp_bitcnt_t)nexp);
        mpz_fdiv_qr(q, rem, numerator, a);
        mpz_set(lower, q);
        mpz_set(upper, q);
        if (mpz_sgn(rem) != 0) mpz_add_ui(upper, upper, 1);
        if (!exact_root) {
            mpz_add_ui(den1, a, 1);
            if (mpz_cmp(rem, q) < 0) {
                mpz_sub(d, q, rem);
                mpz_cdiv_q(correction, d, den1);
                mpz_sub(lower, lower, correction);
            }
        }
        status = ppf_set_dyadic_interval(rop, lower, upper,
                                         (ppf_exp_t)(-scale128), rnd,
                                         &ternary, ctx);
        if (status == PPF_INTERVAL_DECIDED) {
            mpz_clears(m, rad, a, numerator, q, rem, lower, upper, d,
                       den1, correction, NULL);
            return ternary;
        }
    }
fail:
    mpz_clears(m, rad, a, numerator, q, rem, lower, upper, d,
               den1, correction, NULL);
    return PPF_TERNARY_ERROR;
}

static double median(double *x, int n) {
    for (int i=1;i<n;i++){ double v=x[i]; int j=i-1; while(j>=0&&x[j]>v){x[j+1]=x[j];j--;}x[j+1]=v; }
    return x[n/2];
}

int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1],0,10) : 1000000UL;
    int reps = argc > 2 ? atoi(argv[2]) : 7;
    ppf_context_t ctx;
    ppf_t x, a, b;
    mpfr_t mx, ma, mb;
    mpz_t z;
    double *old, *neo, *ref;
    if (reps < 3) reps = 3;
    old=calloc(reps,sizeof(double)); neo=calloc(reps,sizeof(double)); ref=calloc(reps,sizeof(double));
    if(!old||!neo||!ref) return 2;
    ppf_context_init(&ctx); ppf_init2_ctx(&x,bits,&ctx); ppf_init2_ctx(&a,bits,&ctx); ppf_init2_ctx(&b,bits,&ctx);
    mpfr_init2(mx,bits); mpfr_init2(ma,bits); mpfr_init2(mb,bits); mpz_init(z);
    mpz_set_ui(z,1); mpz_mul_2exp(z,z,bits-1); mpz_setbit(z,bits/3); mpz_setbit(z,17);
    ppf_set_z(&x,z,PPF_RNDN,&ctx); mpfr_set_z(mx,z,MPFR_RNDN);
    if(candidate_rec_sqrt(&b,&x,PPF_RNDN,&ctx)==PPF_TERNARY_ERROR){fprintf(stderr,"candidate failed\n");return 3;}
    ppf_get_mpfr(mb,&b,MPFR_RNDN); mpfr_rec_sqrt(ma,mx,MPFR_RNDN);
    if(!mpfr_equal_p(ma,mb)){fprintf(stderr,"candidate mismatch\n");return 4;}
    for(int r=0;r<reps;r++){
        double t;
        if(r%3==0){
            t=now_seconds(); ppf_rec_sqrt(&a,&x,PPF_RNDN,&ctx); old[r]=now_seconds()-t;
            t=now_seconds(); candidate_rec_sqrt(&b,&x,PPF_RNDN,&ctx); neo[r]=now_seconds()-t;
            t=now_seconds(); mpfr_rec_sqrt(ma,mx,MPFR_RNDN); ref[r]=now_seconds()-t;
        } else if(r%3==1){
            t=now_seconds(); mpfr_rec_sqrt(ma,mx,MPFR_RNDN); ref[r]=now_seconds()-t;
            t=now_seconds(); candidate_rec_sqrt(&b,&x,PPF_RNDN,&ctx); neo[r]=now_seconds()-t;
            t=now_seconds(); ppf_rec_sqrt(&a,&x,PPF_RNDN,&ctx); old[r]=now_seconds()-t;
        } else {
            t=now_seconds(); candidate_rec_sqrt(&b,&x,PPF_RNDN,&ctx); neo[r]=now_seconds()-t;
            t=now_seconds(); ppf_rec_sqrt(&a,&x,PPF_RNDN,&ctx); old[r]=now_seconds()-t;
            t=now_seconds(); mpfr_rec_sqrt(ma,mx,MPFR_RNDN); ref[r]=now_seconds()-t;
        }
    }
    printf("bits=%lu old=%.9f root_first=%.9f mpfr=%.9f old/root_first=%.3f mpfr/root_first=%.3f\n",
           bits,median(old,reps),median(neo,reps),median(ref,reps),median(old,reps)/median(neo,reps),median(ref,reps)/median(neo,reps));
    mpz_clear(z); mpfr_clears(mx,ma,mb,(mpfr_ptr)0); ppf_clear(&x);ppf_clear(&a);ppf_clear(&b);ppf_context_clear(&ctx);free(old);free(neo);free(ref);return 0;
}
