/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "include/polyphase_float.h"
#include "include/polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static ppf_exp_t ceil_div_exp(ppf_exp_t x, unsigned long n) {
    ppf_exp_t q = x / (ppf_exp_t)n;
    ppf_exp_t r = x % (ppf_exp_t)n;
    if (r > 0) ++q;
    return q;
}

static int set_prec_scratch(ppf_t *x, ppf_prec_t p) {
    return ppf_set_prec(x, p);
}

static int pow_bounds(ppf_t *lo, ppf_t *hi, const ppf_t *x,
                      unsigned long exponent, ppf_context_t *ctx,
                      ppf_t *blo, ppf_t *bhi, ppf_t *tlo, ppf_t *thi) {
    if (ppf_set_ui(lo, 1, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(hi, 1, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(blo, x, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(bhi, x, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    while (exponent) {
        if (exponent & 1UL) {
            if (ppf_mul(tlo, lo, blo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
                ppf_mul(thi, hi, bhi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
                return -1;
            ppf_swap(lo, tlo);
            ppf_swap(hi, thi);
        }
        exponent >>= 1;
        if (!exponent) break;
        if (ppf_mul(tlo, blo, blo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_mul(thi, bhi, bhi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
        ppf_swap(blo, tlo);
        ppf_swap(bhi, thi);
    }
    return 0;
}

static int interval_to_result(ppf_t *rop, const ppf_t *lo, const ppf_t *hi,
                              int sign, int reciprocal, ppf_rnd_t rnd,
                              ppf_context_t *ctx, ppf_context_t *work,
                              ppf_t *rlo, ppf_t *rhi, ppf_t *one) {
    const ppf_t *a = lo, *b = hi;
    mpz_t zl, zu;
    ppf_exp_t el, eu, common;
    int ternary = 0;
    ppf_interval_status_t status;
    if (reciprocal) {
        if (ppf_set_ui(one, 1, PPF_RNDN, work) == PPF_TERNARY_ERROR ||
            ppf_div(rlo, one, hi, PPF_RNDD, work) == PPF_TERNARY_ERROR ||
            ppf_div(rhi, one, lo, PPF_RNDU, work) == PPF_TERNARY_ERROR)
            return PPF_TERNARY_ERROR;
        a = rlo;
        b = rhi;
    }
    mpz_inits(zl, zu, NULL);
    el = ppf_get_z_2exp(zl, a);
    eu = ppf_get_z_2exp(zu, b);
    common = el < eu ? el : eu;
    if (el > common) mpz_mul_2exp(zl, zl, (mp_bitcnt_t)(el - common));
    if (eu > common) mpz_mul_2exp(zu, zu, (mp_bitcnt_t)(eu - common));
    if (sign < 0) {
        mpz_t t;
        mpz_init_set(t, zl);
        mpz_neg(zl, zu);
        mpz_neg(zu, t);
        mpz_clear(t);
    }
    status = ppf_set_dyadic_interval(rop, zl, zu, common, rnd, &ternary, ctx);
    mpz_clears(zl, zu, NULL);
    return status == PPF_INTERVAL_DECIDED ? ternary : PPF_TERNARY_ERROR;
}

static int newton_root(ppf_t *rop, const ppf_t *input, unsigned long degree,
                       int reciprocal, int result_sign, ppf_rnd_t rnd,
                       ppf_context_t *ctx) {
    ppf_context_t work;
    ppf_t mag, xlo, xhi, u, unext, l, powlo, powhi, qlo, qhi;
    ppf_t coeff, deg, term, sum, blo, bhi, tlo, thi, rlo, rhi, one;
    ppf_prec_t target, wp, next;
    int rc = PPF_TERNARY_ERROR;
    unsigned init_iters;

    if (ppf_context_clone(&work, ctx) != 0) return PPF_TERNARY_ERROR;
    (void)ppf_context_set_range(&work, PPF_EMIN_MIN, PPF_EMAX_MAX);
    ppf_context_flags_clear(&work, PPF_FLAGS_ALL);
    target = rop->precision + 24;
    if (target < rop->precision) goto done_ctx;
    wp = target < 96 ? target : 96;

#define INIT(name) if (ppf_init2_ctx(&(name), wp, &work) != 0) goto fail_init
    INIT(mag); INIT(xlo); INIT(xhi); INIT(u); INIT(unext); INIT(l);
    INIT(powlo); INIT(powhi); INIT(qlo); INIT(qhi); INIT(coeff); INIT(deg);
    INIT(term); INIT(sum); INIT(blo); INIT(bhi); INIT(tlo); INIT(thi);
    INIT(rlo); INIT(rhi); INIT(one);
#undef INIT

    if (ppf_abs(&mag, input, PPF_RNDU, &work) == PPF_TERNARY_ERROR ||
        ppf_set(&xlo, &mag, PPF_RNDD, &work) == PPF_TERNARY_ERROR ||
        ppf_set(&xhi, &mag, PPF_RNDU, &work) == PPF_TERNARY_ERROR ||
        ppf_set_ui_2exp(&u, 1UL, ceil_div_exp(mag.exponent, degree),
                        PPF_RNDN, &work) == PPF_TERNARY_ERROR)
        goto clear_all;

    init_iters = degree <= 2 ? 8u : 12u;
    for (;;) {
        if (ppf_set_ui(&coeff, degree - 1UL, PPF_RNDN, &work) == PPF_TERNARY_ERROR ||
            ppf_set_ui(&deg, degree, PPF_RNDN, &work) == PPF_TERNARY_ERROR)
            goto clear_all;
        for (unsigned it = 0; it < init_iters; ++it) {
            if (pow_bounds(&powlo, &powhi, &u, degree - 1UL, &work,
                           &blo, &bhi, &tlo, &thi) != 0)
                goto clear_all;
            if (ppf_div(&qlo, &xlo, &powhi, PPF_RNDD, &work) == PPF_TERNARY_ERROR ||
                ppf_div(&qhi, &xhi, &powlo, PPF_RNDU, &work) == PPF_TERNARY_ERROR ||
                ppf_mul(&term, &u, &coeff, PPF_RNDU, &work) == PPF_TERNARY_ERROR ||
                ppf_add(&sum, &term, &qhi, PPF_RNDU, &work) == PPF_TERNARY_ERROR ||
                ppf_div(&unext, &sum, &deg, PPF_RNDU, &work) == PPF_TERNARY_ERROR ||
                ppf_set(&u, &unext, PPF_RNDU, &work) == PPF_TERNARY_ERROR ||
                ppf_set(&l, &qlo, PPF_RNDD, &work) == PPF_TERNARY_ERROR)
                goto clear_all;
        }
        if (wp >= target) {
            rc = interval_to_result(rop, &l, &u, result_sign, reciprocal,
                                    rnd, ctx, &work, &rlo, &rhi, &one);
            if (rc != PPF_TERNARY_ERROR) break;
            if (target > UINT64_MAX - 64) break;
            target += 64;
        }
        next = wp >= target / 2 ? target : wp * 2;
        if (next <= wp) next = target;
#define RESIZE_SCRATCH(name) do { if (set_prec_scratch(&(name), next) != 0) goto clear_all; } while (0)
        if (ppf_prec_round(&u, next, PPF_RNDU, &work) == PPF_TERNARY_ERROR)
            goto clear_all;
        RESIZE_SCRATCH(mag); RESIZE_SCRATCH(xlo); RESIZE_SCRATCH(xhi);
        RESIZE_SCRATCH(unext); RESIZE_SCRATCH(l); RESIZE_SCRATCH(powlo);
        RESIZE_SCRATCH(powhi); RESIZE_SCRATCH(qlo); RESIZE_SCRATCH(qhi);
        RESIZE_SCRATCH(coeff); RESIZE_SCRATCH(deg); RESIZE_SCRATCH(term);
        RESIZE_SCRATCH(sum); RESIZE_SCRATCH(blo); RESIZE_SCRATCH(bhi);
        RESIZE_SCRATCH(tlo); RESIZE_SCRATCH(thi); RESIZE_SCRATCH(rlo);
        RESIZE_SCRATCH(rhi); RESIZE_SCRATCH(one);
#undef RESIZE_SCRATCH
        if (ppf_abs(&mag, input, PPF_RNDU, &work) == PPF_TERNARY_ERROR ||
            ppf_set(&xlo, &mag, PPF_RNDD, &work) == PPF_TERNARY_ERROR ||
            ppf_set(&xhi, &mag, PPF_RNDU, &work) == PPF_TERNARY_ERROR)
            goto clear_all;
        wp = next;
        init_iters = 2u;
    }

clear_all:
    ppf_clear(&mag); ppf_clear(&xlo); ppf_clear(&xhi); ppf_clear(&u);
    ppf_clear(&unext); ppf_clear(&l); ppf_clear(&powlo); ppf_clear(&powhi);
    ppf_clear(&qlo); ppf_clear(&qhi); ppf_clear(&coeff); ppf_clear(&deg);
    ppf_clear(&term); ppf_clear(&sum); ppf_clear(&blo); ppf_clear(&bhi);
    ppf_clear(&tlo); ppf_clear(&thi); ppf_clear(&rlo); ppf_clear(&rhi);
    ppf_clear(&one);
    goto done_ctx;
fail_init:
    rc = PPF_TERNARY_ERROR;
done_ctx:
    ppf_context_clear(&work);
    return rc;
}

static int test_one(ppf_prec_t p, unsigned long degree, int reciprocal,
                    unsigned reps) {
    ppf_context_t ctx;
    ppf_t x, oldr, newr;
    mpfr_t mx, mr;
    gmp_randstate_t rs;
    double oldt=0,newt=0,mpfrt=0,t0;
    if (ppf_context_init(&ctx) != 0) return 1;
    ppf_context_set_range(&ctx, PPF_EMIN_MIN, PPF_EMAX_MAX);
    if (ppf_init2_ctx(&x,p,&ctx) || ppf_init2_ctx(&oldr,p,&ctx) ||
        ppf_init2_ctx(&newr,p,&ctx)) return 1;
    mpfr_init2(mx,(mpfr_prec_t)p); mpfr_init2(mr,(mpfr_prec_t)p);
    gmp_randinit_mt(rs); gmp_randseed_ui(rs, 1234567 + degree + reciprocal);
    mpfr_urandomb(mx,rs); mpfr_add_ui(mx,mx,1,MPFR_RNDN);
    ppf_import_mpfr_exact(&x,mx,&ctx);
    for (unsigned i=0;i<reps+1;i++) {
        t0=now_s();
        if (reciprocal) ppf_rec_sqrt(&oldr,&x,PPF_RNDN,&ctx);
        else if (degree==2) ppf_sqrt(&oldr,&x,PPF_RNDN,&ctx);
        else ppf_rootn_ui(&oldr,&x,degree,PPF_RNDN,&ctx);
        if(i) oldt+=now_s()-t0;
        t0=now_s();
        newton_root(&newr,&x,degree,reciprocal,1,PPF_RNDN,&ctx);
        if(i) newt+=now_s()-t0;
        t0=now_s();
        if (reciprocal) mpfr_rec_sqrt(mr,mx,MPFR_RNDN);
        else if (degree==2) mpfr_sqrt(mr,mx,MPFR_RNDN);
        else mpfr_rootn_ui(mr,mx,degree,MPFR_RNDN);
        if(i) mpfrt+=now_s()-t0;
    }
    ppf_t oracle; ppf_init2_ctx(&oracle,p,&ctx); ppf_import_mpfr_exact(&oracle,mr,&ctx);
    int equal = ppf_cmp(&newr,&oracle,&ctx)==0;
    printf("bits=%llu degree=%lu recip=%d old=%.9f newton=%.9f mpfr=%.9f equal=%d\n",
           (unsigned long long)p,degree,reciprocal,oldt/reps,newt/reps,mpfrt/reps,equal);
    ppf_clear(&oracle); gmp_randclear(rs); mpfr_clears(mx,mr,(mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&oldr); ppf_clear(&newr); ppf_context_clear(&ctx);
    return equal?0:1;
}

int main(int argc,char **argv){
    ppf_prec_t p=argc>1?strtoull(argv[1],0,10):100000;
    unsigned reps=argc>2?(unsigned)strtoul(argv[2],0,10):3;
    int rc=0;
    rc|=test_one(p,2,0,reps);
    rc|=test_one(p,3,0,reps);
    rc|=test_one(p,7,0,reps);
    rc|=test_one(p,2,1,reps);
    return rc;
}
