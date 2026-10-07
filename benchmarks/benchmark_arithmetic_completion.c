/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float.h"
#include "polyphase_float_mpfr.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    OP_ADD_UI = 0,
    OP_ADD_Q,
    OP_MUL_Z,
    OP_DIV_Q,
    OP_FMMA,
    OP_FAC_UI,
    OP_COUNT
} operation_t;

typedef struct {
    unsigned long bits;
    unsigned long inner;
    unsigned long factorial_n;
    ppf_context_t ctx;
    ppf_t a, b, c, d, ppf_result;
    mpfr_t ma, mb, mc, md, mpfr_result, exported;
    mpz_t z;
    mpq_t q;
} bench_t;

static const char *operation_name(operation_t op) {
    static const char *names[OP_COUNT] = {
        "add_ui", "add_q", "mul_z", "div_q", "fmma", "fac_ui"
    };
    return names[(int)op];
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int compare_double(const void *a, const void *b) {
    const double x = *(const double *)a;
    const double y = *(const double *)b;
    return (x > y) - (x < y);
}

static double median(double *values, int count) {
    qsort(values, (size_t)count, sizeof(*values), compare_double);
    return (count & 1) ? values[count / 2]
                       : 0.5 * (values[count / 2 - 1] + values[count / 2]);
}

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_zero_p(a) || mpfr_zero_p(b))
        return mpfr_zero_p(a) && mpfr_zero_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    return mpfr_equal_p(a, b) != 0;
}

static void set_pattern(mpz_ptr value, unsigned long bits,
                        unsigned long stride, unsigned long offset) {
    mpz_set_ui(value, 0);
    if (bits == 0) return;
    mpz_setbit(value, bits - 1);
    for (unsigned long k = offset; k + 1 < bits; k += stride)
        mpz_setbit(value, k);
    mpz_add_ui(value, value, 17u + offset);
}

static unsigned long choose_inner(unsigned long bits) {
    if (bits <= 1000ul) return 200ul;
    if (bits <= 10000ul) return 30ul;
    if (bits <= 100000ul) return 5ul;
    return 1ul;
}

static unsigned long choose_factorial_n(unsigned long bits) {
    double b = bits < 64ul ? 64.0 : (double)bits;
    unsigned long n = (unsigned long)(1.35 * b / log2(b));
    return n < 20ul ? 20ul : n;
}

static int bench_init(bench_t *b, unsigned long bits) {
    mpz_t za, zb, zc, zd;
    memset(b, 0, sizeof(*b));
    b->bits = bits;
    b->inner = choose_inner(bits);
    b->factorial_n = choose_factorial_n(bits);
    if (ppf_context_init(&b->ctx) != 0) return -1;
    if (ppf_init2_ctx(&b->a, bits, &b->ctx) != 0 ||
        ppf_init2_ctx(&b->b, bits, &b->ctx) != 0 ||
        ppf_init2_ctx(&b->c, bits, &b->ctx) != 0 ||
        ppf_init2_ctx(&b->d, bits, &b->ctx) != 0 ||
        ppf_init2_ctx(&b->ppf_result, bits, &b->ctx) != 0) return -1;
    mpfr_inits2(bits, b->ma, b->mb, b->mc, b->md,
                b->mpfr_result, b->exported, (mpfr_ptr)0);
    mpz_init(b->z);
    mpq_init(b->q);
    mpz_inits(za, zb, zc, zd, NULL);
    set_pattern(za, bits, 97ul, 3ul);
    set_pattern(zb, bits, 89ul, 5ul);
    set_pattern(zc, bits, 83ul, 7ul);
    set_pattern(zd, bits, 79ul, 11ul);
    mpz_neg(zb, zb);
    mpz_neg(zd, zd);
    (void)mpfr_set_z_2exp(b->ma, za, -(mpfr_exp_t)(bits / 3ul), MPFR_RNDN);
    (void)mpfr_set_z_2exp(b->mb, zb, -(mpfr_exp_t)(bits / 4ul), MPFR_RNDN);
    (void)mpfr_set_z_2exp(b->mc, zc, -(mpfr_exp_t)(bits / 5ul), MPFR_RNDN);
    (void)mpfr_set_z_2exp(b->md, zd, -(mpfr_exp_t)(bits / 6ul), MPFR_RNDN);
    if (ppf_import_mpfr_exact(&b->a, b->ma, &b->ctx) != 0 ||
        ppf_import_mpfr_exact(&b->b, b->mb, &b->ctx) != 0 ||
        ppf_import_mpfr_exact(&b->c, b->mc, &b->ctx) != 0 ||
        ppf_import_mpfr_exact(&b->d, b->md, &b->ctx) != 0) return -1;
    set_pattern(b->z, bits, 73ul, 13ul);
    set_pattern(mpq_numref(b->q), bits / 2ul + 3ul, 61ul, 2ul);
    set_pattern(mpq_denref(b->q), bits / 3ul + 5ul, 59ul, 1ul);
    mpz_setbit(mpq_denref(b->q), 0);
    mpq_canonicalize(b->q);
    mpz_clears(za, zb, zc, zd, NULL);
    return 0;
}

static void bench_clear(bench_t *b) {
    mpq_clear(b->q); mpz_clear(b->z);
    mpfr_clears(b->ma, b->mb, b->mc, b->md,
                b->mpfr_result, b->exported, (mpfr_ptr)0);
    ppf_clear(&b->a); ppf_clear(&b->b); ppf_clear(&b->c); ppf_clear(&b->d);
    ppf_clear(&b->ppf_result); ppf_context_clear(&b->ctx);
}

static int run_ppf(bench_t *b, operation_t op) {
    int t = 0;
    for (unsigned long i = 0; i < b->inner; ++i) {
        switch (op) {
        case OP_ADD_UI:
            t = ppf_add_ui(&b->ppf_result, &b->a, 123456789ul,
                           PPF_RNDN, &b->ctx); break;
        case OP_ADD_Q:
            t = ppf_add_q(&b->ppf_result, &b->a, b->q,
                          PPF_RNDN, &b->ctx); break;
        case OP_MUL_Z:
            t = ppf_mul_z(&b->ppf_result, &b->a, b->z,
                          PPF_RNDN, &b->ctx); break;
        case OP_DIV_Q:
            t = ppf_div_q(&b->ppf_result, &b->a, b->q,
                          PPF_RNDN, &b->ctx); break;
        case OP_FMMA:
            t = ppf_fmma(&b->ppf_result, &b->a, &b->b, &b->c, &b->d,
                         PPF_RNDN, &b->ctx); break;
        case OP_FAC_UI:
            t = ppf_fac_ui(&b->ppf_result, b->factorial_n,
                           PPF_RNDN, &b->ctx); break;
        default: return PPF_TERNARY_ERROR;
        }
        if (t == PPF_TERNARY_ERROR) return t;
    }
    return t;
}

static int run_mpfr(bench_t *b, operation_t op) {
    int t = 0;
    for (unsigned long i = 0; i < b->inner; ++i) {
        switch (op) {
        case OP_ADD_UI:
            t = mpfr_add_ui(b->mpfr_result, b->ma, 123456789ul, MPFR_RNDN); break;
        case OP_ADD_Q:
            t = mpfr_add_q(b->mpfr_result, b->ma, b->q, MPFR_RNDN); break;
        case OP_MUL_Z:
            t = mpfr_mul_z(b->mpfr_result, b->ma, b->z, MPFR_RNDN); break;
        case OP_DIV_Q:
            t = mpfr_div_q(b->mpfr_result, b->ma, b->q, MPFR_RNDN); break;
        case OP_FMMA:
            t = mpfr_fmma(b->mpfr_result, b->ma, b->mb, b->mc, b->md,
                          MPFR_RNDN); break;
        case OP_FAC_UI:
            t = mpfr_fac_ui(b->mpfr_result, b->factorial_n, MPFR_RNDN); break;
        default: return 0;
        }
    }
    return t;
}

static int correctness_gate(bench_t *b, operation_t op) {
    int pt, mt;
    ppf_context_flags_clear(&b->ctx, PPF_FLAGS_ALL);
    mpfr_clear_flags();
    pt = run_ppf(b, op);
    mt = run_mpfr(b, op);
    if (pt == PPF_TERNARY_ERROR || ((pt > 0) - (pt < 0)) != ((mt > 0) - (mt < 0)))
        return -1;
    (void)ppf_get_mpfr(b->exported, &b->ppf_result, MPFR_RNDN);
    return same_value(b->exported, b->mpfr_result) ? 0 : -1;
}

int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 10000ul;
    int repetitions = argc > 2 ? atoi(argv[2]) : 9;
    bench_t bench;
    double *ppf_times, *mpfr_times;
    if (bits < 64ul || repetitions < 1 || bench_init(&bench, bits) != 0) return 2;
    ppf_times = calloc((size_t)repetitions, sizeof(*ppf_times));
    mpfr_times = calloc((size_t)repetitions, sizeof(*mpfr_times));
    if (!ppf_times || !mpfr_times) return 2;
    printf("bits,operation,inner,ppf_median_s,mpfr_median_s,speedup_mpfr_over_ppf\n");
    for (int op = 0; op < OP_COUNT; ++op) {
        if (correctness_gate(&bench, (operation_t)op) != 0) {
            fprintf(stderr, "correctness gate failed: %s\n", operation_name((operation_t)op));
            return 3;
        }
        for (int rep = 0; rep < repetitions; ++rep) {
            double start;
            if ((rep & 1) == 0) {
                ppf_context_flags_clear(&bench.ctx, PPF_FLAGS_ALL);
                start = now_seconds(); (void)run_ppf(&bench, (operation_t)op);
                ppf_times[rep] = (now_seconds() - start) / (double)bench.inner;
                mpfr_clear_flags();
                start = now_seconds(); (void)run_mpfr(&bench, (operation_t)op);
                mpfr_times[rep] = (now_seconds() - start) / (double)bench.inner;
            } else {
                mpfr_clear_flags();
                start = now_seconds(); (void)run_mpfr(&bench, (operation_t)op);
                mpfr_times[rep] = (now_seconds() - start) / (double)bench.inner;
                ppf_context_flags_clear(&bench.ctx, PPF_FLAGS_ALL);
                start = now_seconds(); (void)run_ppf(&bench, (operation_t)op);
                ppf_times[rep] = (now_seconds() - start) / (double)bench.inner;
            }
        }
        double pt = median(ppf_times, repetitions);
        double mt = median(mpfr_times, repetitions);
        printf("%lu,%s,%lu,%.9f,%.9f,%.6f\n",
               bits, operation_name((operation_t)op), bench.inner,
               pt, mt, pt > 0.0 ? mt / pt : 0.0);
    }
    free(ppf_times); free(mpfr_times);
    bench_clear(&bench);
    ppf_free_constant_caches();
    mpfr_mp_memory_cleanup();
    return 0;
}
