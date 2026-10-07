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

/* Compute X with |X*2^-p - 1/sqrt(M*2^-d)| < 2*2^-p.
   Each Newton step retains only the residual bits that can affect the target
   precision.  The omitted correction is < 2^-CORRECTION_GUARD ulp. */
static int invsqrt_fixed_truncated(mpz_t result, mpz_srcptr M,
                                   mp_bitcnt_t d, unsigned long p) {
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
        mpz_mul(sq, x, x);
        mpz_mul(prod, mt, sq);

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

        mpz_mul(corr_num, x, residual_hi);
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
                              ppf_rnd_t rnd, ppf_context_t *ctx) {
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
                (mp_bitcnt_t)(a->precision - 1u - (ppf_prec_t)s), work) != 0)
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
    int tg = candidate_rec_sqrt(got, x, prnd, ctx);
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
        candidate_rec_sqrt(&neo, &x, PPF_RNDN, &ctx);
        mpfr_rec_sqrt(mref, mx, MPFR_RNDN);
    }
    for (int r = 0; r < reps; ++r) {
        double t;
        switch (r % 3) {
        case 0:
            t=now_seconds(); ppf_rec_sqrt(&old,&x,PPF_RNDN,&ctx); to[r]=now_seconds()-t;
            t=now_seconds(); candidate_rec_sqrt(&neo,&x,PPF_RNDN,&ctx); tn[r]=now_seconds()-t;
            t=now_seconds(); mpfr_rec_sqrt(mref,mx,MPFR_RNDN); tr[r]=now_seconds()-t;
            break;
        case 1:
            t=now_seconds(); mpfr_rec_sqrt(mref,mx,MPFR_RNDN); tr[r]=now_seconds()-t;
            t=now_seconds(); candidate_rec_sqrt(&neo,&x,PPF_RNDN,&ctx); tn[r]=now_seconds()-t;
            t=now_seconds(); ppf_rec_sqrt(&old,&x,PPF_RNDN,&ctx); to[r]=now_seconds()-t;
            break;
        default:
            t=now_seconds(); candidate_rec_sqrt(&neo,&x,PPF_RNDN,&ctx); tn[r]=now_seconds()-t;
            t=now_seconds(); ppf_rec_sqrt(&old,&x,PPF_RNDN,&ctx); to[r]=now_seconds()-t;
            t=now_seconds(); mpfr_rec_sqrt(mref,mx,MPFR_RNDN); tr[r]=now_seconds()-t;
        }
    }
    printf("bits=%lu retained=%.9f truncated_newton=%.9f mpfr=%.9f retained/new=%.3f mpfr/new=%.3f\n",
           bits, median(to,reps), median(tn,reps), median(tr,reps),
           median(to,reps)/median(tn,reps), median(tr,reps)/median(tn,reps));
    mpz_clear(z); mpfr_clears(mx,mneo,mref,(mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&old); ppf_clear(&neo); ppf_context_clear(&ctx);
    free(to); free(tn); free(tr); return 0;
}
