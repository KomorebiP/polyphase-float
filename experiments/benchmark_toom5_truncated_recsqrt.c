/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float_mpfr.h"
#include <gmp.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ppf_openmp_compat.h"

#define BASE_BITS 40u
#define INPUT_GUARD 12u
#define CORRECTION_GUARD 8u
#define RECURSION_SLACK 10u

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}
static int cmp_double(const void *aa, const void *bb) {
    const double a = *(const double *)aa, b = *(const double *)bb;
    return (a > b) - (a < b);
}
static double median(double *x, int n) {
    qsort(x, (size_t)n, sizeof(*x), cmp_double);
    return x[n / 2];
}

static void round_signed_pow2_nearest_reuse(mpz_t out, mpz_t magnitude,
                                               mpz_srcptr value,
                                               mp_bitcnt_t shift) {
    const int sign = mpz_sgn(value);
    if (shift == 0) { mpz_set(out, value); return; }
    mpz_abs(magnitude, value);
    mpz_fdiv_q_2exp(out, magnitude, shift);
    if (mpz_tstbit(magnitude, shift - 1)) {
        const int sticky = shift > 1 && mpz_scan1(magnitude, 0) < shift - 1;
        if (sticky || mpz_odd_p(out)) mpz_add_ui(out, out, 1);
    }
    if (sign < 0) mpz_neg(out, out);
}

static int extract_shifted_low_window(mpz_t out, mpz_srcptr value,
                                      mp_bitcnt_t shift,
                                      unsigned long width) {
    const mp_srcptr src = mpz_limbs_read(value);
    const size_t srcn = (size_t)mpz_size(value);
    const size_t word = (size_t)(shift / GMP_NUMB_BITS);
    const unsigned bit = (unsigned)(shift % GMP_NUMB_BITS);
    const size_t outn = (width + GMP_NUMB_BITS - 1u) / GMP_NUMB_BITS;
    mp_ptr dst;
    if (outn == 0) { mpz_set_ui(out, 0); return 0; }
    dst = mpz_limbs_write(out, (mp_size_t)outn);
    for (size_t i = 0; i < outn; ++i) {
        const size_t j = word + i;
        mp_limb_t v = j < srcn ? src[j] >> bit : 0;
        if (bit && j + 1 < srcn) v |= src[j + 1] << (GMP_NUMB_BITS - bit);
        dst[i] = v;
    }
    if (width % GMP_NUMB_BITS)
        dst[outn - 1] &= (((mp_limb_t)1 << (width % GMP_NUMB_BITS)) - 1);
    mpz_limbs_finish(out, (mp_size_t)outn);
    return 0;
}

static int make_truncated_input(mpz_t out, mp_bitcnt_t *out_d,
                                mpz_srcptr M, mp_bitcnt_t d,
                                unsigned long target_bits) {
    const size_t mbits = mpz_sizeinbase(M, 2);
    const size_t keep = (size_t)target_bits + INPUT_GUARD;
    if (mbits > keep) {
        const mp_bitcnt_t cut = (mp_bitcnt_t)(mbits - keep);
        if (d < cut) return -1;
        mpz_fdiv_q_2exp(out, M, cut);
        *out_d = d - cut;
    } else {
        mpz_set(out, M);
        *out_d = d;
    }
    return 0;
}

static int exact_div_ui(mpz_t x, unsigned long d) {
    if (!mpz_divisible_ui_p(x, d)) return -1;
    mpz_divexact_ui(x, x, d);
    return 0;
}

static void toom3_mpz_mul(mpz_t out, mpz_srcptr a, mpz_srcptr b) {
    size_t an=(size_t)mpz_size(a), bn=(size_t)mpz_size(b);
    size_t n=an>bn?an:bn, k=(n+2u)/3u;
    mp_srcptr ap=mpz_limbs_read(a), bp=mpz_limbs_read(b);
    size_t a0n=an<k?an:k, a1n=an>k?(an-k<k?an-k:k):0, a2n=an>2*k?an-2*k:0;
    size_t b0n=bn<k?bn:k, b1n=bn>k?(bn-k<k?bn-k:k):0, b2n=bn>2*k?bn-2*k:0;
    mpz_t a0,a1,a2,b0,b1,b2,A1,Am1,A2,B1,Bm1,B2;
    mpz_t v0,v1,vm1,v2,vinf,c1,c2,c3,d,t,tmp;
    int sign=mpz_sgn(a)*mpz_sgn(b);
    mpz_roinit_n(a0,ap,(mp_size_t)a0n); mpz_roinit_n(a1,ap+k,(mp_size_t)a1n); mpz_roinit_n(a2,ap+2*k,(mp_size_t)a2n);
    mpz_roinit_n(b0,bp,(mp_size_t)b0n); mpz_roinit_n(b1,bp+k,(mp_size_t)b1n); mpz_roinit_n(b2,bp+2*k,(mp_size_t)b2n);
    mpz_inits(A1,Am1,A2,B1,Bm1,B2,v0,v1,vm1,v2,vinf,c1,c2,c3,d,t,tmp,NULL);
    mpz_add(A1,a0,a1);mpz_add(A1,A1,a2);
    mpz_sub(Am1,a0,a1);mpz_add(Am1,Am1,a2);
    mpz_mul_2exp(A2,a2,2);mpz_addmul_ui(A2,a1,2);mpz_add(A2,A2,a0);
    mpz_add(B1,b0,b1);mpz_add(B1,B1,b2);
    mpz_sub(Bm1,b0,b1);mpz_add(Bm1,Bm1,b2);
    mpz_mul_2exp(B2,b2,2);mpz_addmul_ui(B2,b1,2);mpz_add(B2,B2,b0);
PPF_OMP_PRAGMA(omp parallel sections num_threads(5))
    {
PPF_OMP_PRAGMA(omp section)
      {mpz_mul(v0,a0,b0);}
PPF_OMP_PRAGMA(omp section)
      {mpz_mul(v1,A1,B1);}
PPF_OMP_PRAGMA(omp section)
      {mpz_mul(vm1,Am1,Bm1);}
PPF_OMP_PRAGMA(omp section)
      {mpz_mul(v2,A2,B2);}
PPF_OMP_PRAGMA(omp section)
      {mpz_mul(vinf,a2,b2);}
    }
    mpz_add(c2,v1,vm1); exact_div_ui(c2,2); mpz_sub(c2,c2,v0); mpz_sub(c2,c2,vinf);
    mpz_sub(d,v1,vm1); exact_div_ui(d,2);
    mpz_set(t,v2);mpz_sub(t,t,v0);mpz_submul_ui(t,c2,4);mpz_submul_ui(t,vinf,16);exact_div_ui(t,2);
    mpz_sub(c3,t,d); exact_div_ui(c3,3);
    mpz_sub(c1,d,c3);
    mpz_mul_2exp(out,vinf,(mp_bitcnt_t)(4*k*GMP_NUMB_BITS));
    mpz_mul_2exp(tmp,c3,(mp_bitcnt_t)(3*k*GMP_NUMB_BITS));mpz_add(out,out,tmp);
    mpz_mul_2exp(tmp,c2,(mp_bitcnt_t)(2*k*GMP_NUMB_BITS));mpz_add(out,out,tmp);
    mpz_mul_2exp(tmp,c1,(mp_bitcnt_t)(k*GMP_NUMB_BITS));mpz_add(out,out,tmp);mpz_add(out,out,v0);
    if(sign<0)mpz_neg(out,out);
    mpz_clears(A1,Am1,A2,B1,Bm1,B2,v0,v1,vm1,v2,vinf,c1,c2,c3,d,t,tmp,NULL);
}

static void parallel_mpz_mul(mpz_t out, mpz_srcptr a, mpz_srcptr b,
                             unsigned long workers) {
    const size_t an = (size_t)mpz_size(a), bn = (size_t)mpz_size(b);
    const size_t n = an > bn ? an : bn;
    const size_t split = (n + 1u) / 2u;
    if (workers >= 5 && n >= 8192u && an > 2u*((n+2u)/3u) && bn > 2u*((n+2u)/3u)) {
        toom3_mpz_mul(out,a,b);
        return;
    }
    if (workers < 3 || n < 4096u || an < split || bn < split) {
        mpz_mul(out, a, b);
        return;
    }
    {
        mpz_t al, ah, bl, bh;
        mpz_t z0, z1, z2, sa, sb, temp;
        const mp_srcptr ap = mpz_limbs_read(a), bp = mpz_limbs_read(b);
        const size_t al_n = an < split ? an : split;
        const size_t bl_n = bn < split ? bn : split;
        const size_t ah_n = an > split ? an - split : 0;
        const size_t bh_n = bn > split ? bn - split : 0;
        mpz_roinit_n(al, ap, (mp_size_t)al_n);
        mpz_roinit_n(bl, bp, (mp_size_t)bl_n);
        if (ah_n) mpz_roinit_n(ah, ap + split, (mp_size_t)ah_n); else mpz_init_set_ui(ah,0);
        if (bh_n) mpz_roinit_n(bh, bp + split, (mp_size_t)bh_n); else mpz_init_set_ui(bh,0);
        mpz_inits(z0,z1,z2,sa,sb,temp,NULL);
PPF_OMP_PRAGMA(omp parallel sections num_threads(3))
        {
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(z0, al, bl); }
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(z2, ah, bh); }
PPF_OMP_PRAGMA(omp section)
            {
                mpz_add(sa, al, ah);
                mpz_add(sb, bl, bh);
                mpz_mul(z1, sa, sb);
            }
        }
        mpz_sub(z1,z1,z0); mpz_sub(z1,z1,z2);
        mpz_mul_2exp(out,z2,(mp_bitcnt_t)(2u*split*GMP_NUMB_BITS));
        mpz_mul_2exp(temp,z1,(mp_bitcnt_t)(split*GMP_NUMB_BITS));
        mpz_add(out,out,temp); mpz_add(out,out,z0);
        mpz_clears(z0,z1,z2,sa,sb,temp,NULL);
        if (!ah_n) mpz_clear(ah);
        if (!bh_n) mpz_clear(bh);
    }
}

/* Compute X with |X*2^-p - 1/sqrt(M*2^-d)| < 2*2^-p.
   Each Newton step retains only the residual bits that can affect the target
   precision.  The omitted correction is < 2^-CORRECTION_GUARD ulp. */
static int invsqrt_fixed_truncated(mpz_t result, mpz_srcptr M,
                                   mp_bitcnt_t d, unsigned long p,
                                   unsigned long workers) {
    unsigned long levels[64], cur = p;
    size_t count = 0;
    mpz_t mt, x, x0, sq, prod, qlow, residual_hi;
    mpz_t corr_num, delta, abs_corr;
    mp_bitcnt_t dt;

    while (cur > BASE_BITS) {
        if (count >= sizeof(levels) / sizeof(levels[0])) return -1;
        levels[count++] = cur;
        cur = (cur + RECURSION_SLACK + 1u) / 2u;
    }

    mpz_inits(mt, x, x0, sq, prod, qlow, residual_hi,
              corr_num, delta, abs_corr, NULL);
    if (make_truncated_input(mt, &dt, M, d, cur) != 0) goto fail;
    {
        signed long e2 = 0;
        const double frac = mpz_get_d_2exp(&e2, mt);
        const long de = e2 - (long)dt;
        const long double A = ldexpl((long double)frac, (int)de);
        const long double y = 1.0L / sqrtl(A);
        const long double scaled = ldexpl(y, (int)cur);
        mpz_set_ui(x, (unsigned long long)floorl(scaled + 0.5L));
    }

    while (count != 0) {
        const unsigned long target = levels[--count];
        const unsigned long h = cur;
        mp_bitcnt_t full_shift, cut, kept_shift;
        int low_nonzero;

        if (make_truncated_input(mt, &dt, M, d, target) != 0) goto fail;
        mpz_mul_2exp(x0, x, (mp_bitcnt_t)(target - h));
        parallel_mpz_mul(sq, x, x, workers);
        parallel_mpz_mul(prod, mt, sq, workers);

        /* Exact correction denominator is 2^(dt+3h+1-target).  Retaining
           only floor(R/2^cut), with cut=full_shift-h-g, changes the scaled
           correction by less than 2^-g. */
        full_shift = dt + (mp_bitcnt_t)(3u * h + 1u - target);
        cut = full_shift > (mp_bitcnt_t)(h + CORRECTION_GUARD)
            ? full_shift - (mp_bitcnt_t)(h + CORRECTION_GUARD) : 0;
        kept_shift = full_shift - cut;

        {
            const mp_bitcnt_t threshold_pos = dt + (mp_bitcnt_t)(2u * h);
            const mp_bitcnt_t q_threshold = threshold_pos - cut;
            const unsigned long residual_window = h + CORRECTION_GUARD + 8u;
            const size_t product_bits = mpz_sizeinbase(prod, 2);
            int relation;
            low_nonzero = cut != 0 && !mpz_divisible_2exp_p(prod, cut);
            extract_shifted_low_window(qlow, prod, cut, residual_window);

            if (product_bits - 1u > (size_t)threshold_pos) relation = 1;
            else if (!mpz_tstbit(prod, threshold_pos)) relation = -1;
            else if (mpz_sgn(qlow) != 0 || low_nonzero) relation = 1;
            else relation = 0;

            if (relation < 0) {
                /* q = 2^q_threshold - D, with D fitting in the retained
                   residual window.  Recover D from the low window. */
                mpz_set_ui(residual_hi, 0);
                mpz_setbit(residual_hi, residual_window);
                mpz_sub(residual_hi, residual_hi, qlow);
                if (low_nonzero) mpz_sub_ui(residual_hi, residual_hi, 1);
            } else if (relation > 0) {
                mpz_neg(residual_hi, qlow);
            } else {
                mpz_set_ui(residual_hi, 0);
            }
            (void)q_threshold;
        }

        parallel_mpz_mul(corr_num, x, residual_hi, workers);
        round_signed_pow2_nearest_reuse(delta, abs_corr, corr_num, kept_shift);
        mpz_add(x, x0, delta);
        cur = target;
    }

    mpz_set(result, x);
    mpz_clears(mt, x, x0, sq, prod, qlow, residual_hi,
               corr_num, delta, abs_corr, NULL);
    return 0;
fail:
    mpz_clears(mt, x, x0, sq, prod, qlow, residual_hi,
               corr_num, delta, abs_corr, NULL);
    return -1;
}

static int candidate_rec_sqrt(ppf_t *rop, const ppf_t *a,
                              ppf_rnd_t rnd, unsigned long workers,
                              ppf_context_t *ctx) {
    mpz_t Mview, approx, lower, upper;
    ppf_exp_t result_exp, interval_exp;
    int s, ternary = 0;
    unsigned long work;
    ppf_interval_status_t status;

    if (!ppf_regular_p(a) || a->sign < 0) return PPF_TERNARY_ERROR;
    mpz_roinit_n(Mview, a->limbs, (mp_size_t)a->used_limbs);
    mpz_inits(approx, lower, upper, NULL);
    s = 1 - ((uint64_t)a->exponent & 1u);
    result_exp = -(a->exponent - 1 - s) / 2;

    for (unsigned attempt = 0; attempt < 6; ++attempt) {
        work = (unsigned long)rop->precision + 12u + 32u * attempt;
        if (invsqrt_fixed_truncated(
                approx, Mview,
                (mp_bitcnt_t)(a->precision - 1u - (ppf_prec_t)s), work,
                workers) != 0)
            goto fail;
        mpz_sub_ui(lower, approx, 2);
        mpz_add_ui(upper, approx, 2);
        interval_exp = result_exp - (ppf_exp_t)work;
        status = ppf_set_dyadic_interval(rop, lower, upper, interval_exp,
                                         rnd, &ternary, ctx);
        if (status == PPF_INTERVAL_DECIDED) {
            mpz_clears(approx, lower, upper, NULL);
            return ternary;
        }
    }
fail:
    mpz_clears(approx, lower, upper, NULL);
    return PPF_TERNARY_ERROR;
}

static int check_case(ppf_t *x, ppf_t *got, mpfr_t mx, mpfr_t mgot,
                      mpfr_t mref, ppf_rnd_t prnd, mpfr_rnd_t mrnd,
                      ppf_context_t *ctx) {
    int tg = candidate_rec_sqrt(got, x, prnd, 1, ctx);
    int tr = mpfr_rec_sqrt(mref, mx, mrnd);
    if (tg == PPF_TERNARY_ERROR) return -1;
    ppf_get_mpfr(mgot, got, mrnd);
    if (!mpfr_equal_p(mgot, mref)) return -2;
    if ((tg > 0) != (tr > 0) || (tg < 0) != (tr < 0)) return -3;
    return 0;
}

int main(int argc, char **argv) {
    const unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 100000UL;
    int reps = argc > 2 ? atoi(argv[2]) : 7;
    unsigned long workers = argc > 3 ? strtoul(argv[3], NULL, 10) : 3UL;
    ppf_context_t ctx;
    ppf_t x, old, neo;
    mpfr_t mx, mneo, mref;
    mpz_t z;
    double *to, *tn, *tr;
    static const ppf_rnd_t prnds[] = {PPF_RNDN, PPF_RNDZ, PPF_RNDU,
                                      PPF_RNDD, PPF_RNDA};
    static const mpfr_rnd_t mrnds[] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU,
                                       MPFR_RNDD, MPFR_RNDA};

    if (reps < 3) reps = 3;
    to = calloc((size_t)reps, sizeof(*to));
    tn = calloc((size_t)reps, sizeof(*tn));
    tr = calloc((size_t)reps, sizeof(*tr));
    if (!to || !tn || !tr) return 2;
    ppf_context_init(&ctx);
    ppf_init2_ctx(&x, bits, &ctx);
    ppf_init2_ctx(&old, bits, &ctx);
    ppf_init2_ctx(&neo, bits, &ctx);
    mpfr_init2(mx, bits); mpfr_init2(mneo, bits); mpfr_init2(mref, bits);
    mpz_init(z);
    mpz_set_ui(z, 1); mpz_mul_2exp(z, z, bits - 1);
    mpz_setbit(z, bits / 3); mpz_setbit(z, 17);
    ppf_set_z(&x, z, PPF_RNDN, &ctx);
    mpfr_set_z(mx, z, MPFR_RNDN);

    for (size_t i = 0; i < sizeof(prnds)/sizeof(prnds[0]); ++i) {
        int c = check_case(&x, &neo, mx, mneo, mref,
                           prnds[i], mrnds[i], &ctx);
        if (c != 0) {
            fprintf(stderr, "candidate mismatch mode=%zu code=%d\n", i, c);
            return 3;
        }
    }
    for (int i = 0; i < 2; ++i) {
        ppf_rec_sqrt(&old, &x, PPF_RNDN, &ctx);
        candidate_rec_sqrt(&neo, &x, PPF_RNDN, workers, &ctx);
        mpfr_rec_sqrt(mref, mx, MPFR_RNDN);
    }
    for (int r = 0; r < reps; ++r) {
        double t;
        switch (r % 3) {
        case 0:
            t=now_seconds(); ppf_rec_sqrt(&old,&x,PPF_RNDN,&ctx); to[r]=now_seconds()-t;
            t=now_seconds(); candidate_rec_sqrt(&neo,&x,PPF_RNDN,workers,&ctx); tn[r]=now_seconds()-t;
            t=now_seconds(); mpfr_rec_sqrt(mref,mx,MPFR_RNDN); tr[r]=now_seconds()-t;
            break;
        case 1:
            t=now_seconds(); mpfr_rec_sqrt(mref,mx,MPFR_RNDN); tr[r]=now_seconds()-t;
            t=now_seconds(); candidate_rec_sqrt(&neo,&x,PPF_RNDN,workers,&ctx); tn[r]=now_seconds()-t;
            t=now_seconds(); ppf_rec_sqrt(&old,&x,PPF_RNDN,&ctx); to[r]=now_seconds()-t;
            break;
        default:
            t=now_seconds(); candidate_rec_sqrt(&neo,&x,PPF_RNDN,workers,&ctx); tn[r]=now_seconds()-t;
            t=now_seconds(); ppf_rec_sqrt(&old,&x,PPF_RNDN,&ctx); to[r]=now_seconds()-t;
            t=now_seconds(); mpfr_rec_sqrt(mref,mx,MPFR_RNDN); tr[r]=now_seconds()-t;
        }
    }
    printf("bits=%lu workers=%lu retained=%.9f truncated_newton=%.9f mpfr=%.9f retained/new=%.3f mpfr/new=%.3f\n",
           bits, workers, median(to,reps), median(tn,reps), median(tr,reps),
           median(to,reps)/median(tn,reps), median(tr,reps)/median(tn,reps));
    mpz_clear(z); mpfr_clears(mx,mneo,mref,(mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&old); ppf_clear(&neo); ppf_context_clear(&ctx);
    free(to); free(tn); free(tr); return 0;
}
