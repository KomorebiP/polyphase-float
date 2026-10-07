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

typedef int (*ppf_unary_fn)(ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_unary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
typedef int (*ppf_binary_fn)(ppf_t *, const ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_binary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
typedef int (*ppf_pair_fn)(ppf_t *, ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_pair_fn)(mpfr_ptr, mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);

typedef struct {
    const char *name;
    int binary;
    double x;
    double y;
    ppf_unary_fn ppf_u;
    mpfr_unary_fn mpfr_u;
    ppf_binary_fn ppf_b;
    mpfr_binary_fn mpfr_b;
} bench_case_t;

static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a;
    double y = *(const double *)b;
    return (x > y) - (x < y);
}

static double median(double *values, size_t n) {
    qsort(values, n, sizeof(*values), cmp_double);
    if ((n & 1U) != 0U) return values[n / 2U];
    return 0.5 * (values[n / 2U - 1U] + values[n / 2U]);
}

static int signum_int(int x) {
    return (x > 0) - (x < 0);
}

static int ppf_matches_mpfr(const ppf_t *x, mpfr_srcptr y) {
    mpfr_t converted;
    int ok;
    mpfr_init2(converted, mpfr_get_prec(y));
    if (ppf_get_mpfr(converted, x, MPFR_RNDN) == PPF_TERNARY_ERROR) {
        mpfr_clear(converted);
        return 0;
    }
    if (mpfr_nan_p(y)) {
        ok = mpfr_nan_p(converted) != 0;
    } else if (mpfr_inf_p(y)) {
        ok = mpfr_inf_p(converted) != 0 && mpfr_signbit(y) == mpfr_signbit(converted);
    } else if (mpfr_zero_p(y)) {
        ok = mpfr_zero_p(converted) != 0 && mpfr_signbit(y) == mpfr_signbit(converted);
    } else {
        ok = mpfr_equal_p(converted, y) != 0;
    }
    mpfr_clear(converted);
    return ok;
}

static unsigned long choose_inner_loops(unsigned long bits) {
    if (bits <= 128UL) return 20UL;
    if (bits <= 512UL) return 8UL;
    if (bits <= 2000UL) return 3UL;
    return 1UL;
}

static int run_case(const bench_case_t *bc, unsigned long bits,
                    unsigned long repetitions, unsigned long loops) {
    ppf_context_t ctx;
    ppf_t px, py, pout;
    mpfr_t mx, my, mout;
    double *ppf_times = NULL;
    double *mpfr_times = NULL;
    int rc = 1;

    if (ppf_context_init(&ctx) != 0 ||
        ppf_init2_ctx(&px, bits + 32UL, &ctx) != 0 ||
        ppf_init2_ctx(&py, bits + 32UL, &ctx) != 0 ||
        ppf_init2_ctx(&pout, bits, &ctx) != 0) {
        return 1;
    }
    mpfr_init2(mx, bits + 32UL);
    mpfr_init2(my, bits + 32UL);
    mpfr_init2(mout, bits);

    if (ppf_set_d(&px, bc->x, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_d(&py, bc->y, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR) {
        goto cleanup;
    }
    mpfr_set_d(mx, bc->x, MPFR_RNDN);
    mpfr_set_d(my, bc->y, MPFR_RNDN);

    int ppc_inex;
    int mpfr_inex;
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    mpfr_clear_flags();
    if (bc->binary) {
        ppc_inex = bc->ppf_b(&pout, &px, &py, PPF_RNDN, &ctx);
        mpfr_inex = bc->mpfr_b(mout, mx, my, MPFR_RNDN);
    } else {
        ppc_inex = bc->ppf_u(&pout, &px, PPF_RNDN, &ctx);
        mpfr_inex = bc->mpfr_u(mout, mx, MPFR_RNDN);
    }
    if (ppc_inex == PPF_TERNARY_ERROR ||
        signum_int(ppc_inex) != signum_int(mpfr_inex) ||
        !ppf_matches_mpfr(&pout, mout)) {
        fprintf(stderr, "correctness failure for %s at %lu bits\n", bc->name, bits);
        goto cleanup;
    }

    /* Warm both paths before paired timing. */
    for (unsigned long i = 0; i < 2UL; ++i) {
        if (bc->binary) {
            (void)bc->ppf_b(&pout, &px, &py, PPF_RNDN, &ctx);
            (void)bc->mpfr_b(mout, mx, my, MPFR_RNDN);
        } else {
            (void)bc->ppf_u(&pout, &px, PPF_RNDN, &ctx);
            (void)bc->mpfr_u(mout, mx, MPFR_RNDN);
        }
    }

    ppf_times = (double *)calloc(repetitions, sizeof(*ppf_times));
    mpfr_times = (double *)calloc(repetitions, sizeof(*mpfr_times));
    if (!ppf_times || !mpfr_times) goto cleanup;

    for (unsigned long rep = 0; rep < repetitions; ++rep) {
        double t0, t1;
        if ((rep & 1UL) == 0UL) {
            t0 = now_seconds();
            for (unsigned long j = 0; j < loops; ++j) {
                if (bc->binary) (void)bc->ppf_b(&pout, &px, &py, PPF_RNDN, &ctx);
                else (void)bc->ppf_u(&pout, &px, PPF_RNDN, &ctx);
            }
            t1 = now_seconds();
            ppf_times[rep] = (t1 - t0) / (double)loops;

            t0 = now_seconds();
            for (unsigned long j = 0; j < loops; ++j) {
                if (bc->binary) (void)bc->mpfr_b(mout, mx, my, MPFR_RNDN);
                else (void)bc->mpfr_u(mout, mx, MPFR_RNDN);
            }
            t1 = now_seconds();
            mpfr_times[rep] = (t1 - t0) / (double)loops;
        } else {
            t0 = now_seconds();
            for (unsigned long j = 0; j < loops; ++j) {
                if (bc->binary) (void)bc->mpfr_b(mout, mx, my, MPFR_RNDN);
                else (void)bc->mpfr_u(mout, mx, MPFR_RNDN);
            }
            t1 = now_seconds();
            mpfr_times[rep] = (t1 - t0) / (double)loops;

            t0 = now_seconds();
            for (unsigned long j = 0; j < loops; ++j) {
                if (bc->binary) (void)bc->ppf_b(&pout, &px, &py, PPF_RNDN, &ctx);
                else (void)bc->ppf_u(&pout, &px, PPF_RNDN, &ctx);
            }
            t1 = now_seconds();
            ppf_times[rep] = (t1 - t0) / (double)loops;
        }
    }

    double ppf_med = median(ppf_times, repetitions);
    double mpfr_med = median(mpfr_times, repetitions);
    printf("%lu,%lu,%lu,%s,%.9f,%.9f,%.6f\n",
           bits, repetitions, loops, bc->name, ppf_med, mpfr_med,
           ppf_med > 0.0 ? mpfr_med / ppf_med : 0.0);
    rc = 0;

cleanup:
    free(ppf_times);
    free(mpfr_times);
    ppf_clear(&px);
    ppf_clear(&py);
    ppf_clear(&pout);
    ppf_context_clear(&ctx);
    mpfr_clear(mx);
    mpfr_clear(my);
    mpfr_clear(mout);
    return rc;
}

static int run_pair_case(const char *name, ppf_pair_fn ppf_fn,
                         mpfr_pair_fn mpfr_fn, double value,
                         unsigned long bits, unsigned long repetitions,
                         unsigned long loops) {
    ppf_context_t ctx;
    ppf_t x, a, b;
    mpfr_t mx, ma, mb;
    double *pt = NULL, *mt = NULL;
    int rc = 1, pa, pm;
    if (ppf_context_init(&ctx) != 0 ||
        ppf_init2_ctx(&x, bits + 32UL, &ctx) != 0 ||
        ppf_init2_ctx(&a, bits, &ctx) != 0 ||
        ppf_init2_ctx(&b, bits, &ctx) != 0)
        return 1;
    mpfr_init2(mx, bits + 32UL); mpfr_init2(ma, bits); mpfr_init2(mb, bits);
    if (ppf_set_d(&x, value, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        goto done;
    mpfr_set_d(mx, value, MPFR_RNDN);
    pa = ppf_fn(&a, &b, &x, PPF_RNDN, &ctx);
    pm = mpfr_fn(ma, mb, mx, MPFR_RNDN);
    if (pa == PPF_TERNARY_ERROR || pa != pm ||
        !ppf_matches_mpfr(&a, ma) || !ppf_matches_mpfr(&b, mb)) {
        fprintf(stderr, "correctness failure for %s at %lu bits\n", name, bits);
        goto done;
    }
    for (unsigned i = 0; i < 2u; ++i) {
        (void)ppf_fn(&a, &b, &x, PPF_RNDN, &ctx);
        (void)mpfr_fn(ma, mb, mx, MPFR_RNDN);
    }
    pt = (double *)calloc(repetitions, sizeof(*pt));
    mt = (double *)calloc(repetitions, sizeof(*mt));
    if (!pt || !mt) goto done;
    for (unsigned long rep = 0; rep < repetitions; ++rep) {
        double t0, t1;
        if ((rep & 1UL) == 0UL) {
            t0 = now_seconds();
            for (unsigned long j = 0; j < loops; ++j)
                (void)ppf_fn(&a, &b, &x, PPF_RNDN, &ctx);
            t1 = now_seconds(); pt[rep] = (t1 - t0) / (double)loops;
            t0 = now_seconds();
            for (unsigned long j = 0; j < loops; ++j)
                (void)mpfr_fn(ma, mb, mx, MPFR_RNDN);
            t1 = now_seconds(); mt[rep] = (t1 - t0) / (double)loops;
        } else {
            t0 = now_seconds();
            for (unsigned long j = 0; j < loops; ++j)
                (void)mpfr_fn(ma, mb, mx, MPFR_RNDN);
            t1 = now_seconds(); mt[rep] = (t1 - t0) / (double)loops;
            t0 = now_seconds();
            for (unsigned long j = 0; j < loops; ++j)
                (void)ppf_fn(&a, &b, &x, PPF_RNDN, &ctx);
            t1 = now_seconds(); pt[rep] = (t1 - t0) / (double)loops;
        }
    }
    {
        double pmed = median(pt, repetitions), mmed = median(mt, repetitions);
        printf("%lu,%lu,%lu,%s,%.9f,%.9f,%.6f\n",
               bits, repetitions, loops, name, pmed, mmed,
               pmed > 0.0 ? mmed / pmed : 0.0);
    }
    rc = 0;
done:
    free(pt); free(mt);
    ppf_clear(&x); ppf_clear(&a); ppf_clear(&b); ppf_context_clear(&ctx);
    mpfr_clear(mx); mpfr_clear(ma); mpfr_clear(mb);
    return rc;
}

int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000UL;
    unsigned long repetitions = argc > 2 ? strtoul(argv[2], NULL, 10) : 5UL;
    unsigned long loops = argc > 3 ? strtoul(argv[3], NULL, 10) : choose_inner_loops(bits);
    if (bits < 2UL || repetitions == 0UL || loops == 0UL) {
        fprintf(stderr, "usage: %s [bits>=2] [repetitions>=1] [inner_loops>=1]\n", argv[0]);
        return 2;
    }

    const bench_case_t cases[] = {
        {"sin_0.3", 0, 0.3, 0.0, ppf_sin, mpfr_sin, NULL, NULL},
        {"cos_0.3", 0, 0.3, 0.0, ppf_cos, mpfr_cos, NULL, NULL},
        {"tan_0.3", 0, 0.3, 0.0, ppf_tan, mpfr_tan, NULL, NULL},
        {"sinpi_0.3", 0, 0.3, 0.0, ppf_sinpi, mpfr_sinpi, NULL, NULL},
        {"atan_0.3", 0, 0.3, 0.0, ppf_atan, mpfr_atan, NULL, NULL},
        {"sinh_0.3", 0, 0.3, 0.0, ppf_sinh, mpfr_sinh, NULL, NULL},
        {"cosh_0.3", 0, 0.3, 0.0, ppf_cosh, mpfr_cosh, NULL, NULL},
        {"tanh_0.3", 0, 0.3, 0.0, ppf_tanh, mpfr_tanh, NULL, NULL},
        {"asinh_0.3", 0, 0.3, 0.0, ppf_asinh, mpfr_asinh, NULL, NULL},
        {"acosh_2", 0, 2.0, 0.0, ppf_acosh, mpfr_acosh, NULL, NULL},
        {"atanh_0.3", 0, 0.3, 0.0, ppf_atanh, mpfr_atanh, NULL, NULL},
        {"atan2_0.75_-0.25", 1, 0.75, -0.25, NULL, NULL, ppf_atan2, mpfr_atan2}
    };

    puts("bits,repetitions,inner_loops,operation,ppf_median_s,mpfr_median_s,mpfr_over_ppf");
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (run_case(&cases[i], bits, repetitions, loops) != 0) return 1;
    }
    if (run_pair_case("sin_cos_0.3", ppf_sin_cos, mpfr_sin_cos, 0.3,
                      bits, repetitions, loops) != 0) return 1;
    if (run_pair_case("sinh_cosh_0.3", ppf_sinh_cosh, mpfr_sinh_cosh, 0.3,
                      bits, repetitions, loops) != 0) return 1;
    return 0;
}
