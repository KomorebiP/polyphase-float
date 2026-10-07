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

typedef enum {
    OP_EINT, OP_LI2, OP_GAMMA, OP_GAMMA_INC, OP_DIGAMMA, OP_BETA,
    OP_ZETA, OP_ERF, OP_ERFC, OP_J0, OP_Y0, OP_AGM, OP_AI,
    OP_EULER_CACHED, OP_CATALAN_CACHED, OP_COUNT
} op_t;

static const char *op_name[OP_COUNT] = {
    "eint", "li2", "gamma", "gamma_inc", "digamma", "beta",
    "zeta", "erf", "erfc", "j0", "y0", "agm", "ai",
    "const_euler_cached", "const_catalan_cached"
};

typedef struct {
    ppf_context_t ctx;
    ppf_t a, b, out;
    mpfr_t ma, mb, mout;
} fixture_t;

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double median(double *values, unsigned n) {
    qsort(values, n, sizeof(*values), compare_double);
    return n & 1U ? values[n / 2U]
                  : 0.5 * (values[n / 2U - 1U] + values[n / 2U]);
}

static int sign_of(int x) { return (x > 0) - (x < 0); }

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b)) return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_inf_p(a) || mpfr_inf_p(b))
        return mpfr_inf_p(a) && mpfr_inf_p(b) && mpfr_signbit(a) == mpfr_signbit(b);
    if (!mpfr_equal_p(a, b)) return 0;
    return !mpfr_zero_p(a) || mpfr_signbit(a) == mpfr_signbit(b);
}

static void set_text(ppf_t *p, mpfr_ptr m, const char *text,
                     ppf_context_t *ctx) {
    if (mpfr_set_str(m, text, 0, MPFR_RNDN) != 0 ||
        ppf_set_mpfr(p, m, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) {
        fprintf(stderr, "input setup failed: %s\n", text);
        exit(2);
    }
}

static void fixture_init(fixture_t *f, unsigned long bits) {
    memset(f, 0, sizeof(*f));
    if (ppf_context_init(&f->ctx) != 0 ||
        ppf_init2_ctx(&f->a, bits + 31UL, &f->ctx) != 0 ||
        ppf_init2_ctx(&f->b, bits + 29UL, &f->ctx) != 0 ||
        ppf_init2_ctx(&f->out, bits, &f->ctx) != 0) {
        fprintf(stderr, "fixture initialization failed\n");
        exit(2);
    }
    mpfr_init2(f->ma, bits + 31UL);
    mpfr_init2(f->mb, bits + 29UL);
    mpfr_init2(f->mout, bits);
}

static void fixture_clear(fixture_t *f) {
    ppf_clear(&f->a); ppf_clear(&f->b); ppf_clear(&f->out);
    ppf_context_clear(&f->ctx);
    mpfr_clear(f->ma); mpfr_clear(f->mb); mpfr_clear(f->mout);
}

static void prepare(op_t op, fixture_t *f) {
    switch (op) {
        case OP_EINT: case OP_LI2: case OP_ERF: case OP_ERFC: case OP_AI:
            set_text(&f->a, f->ma, "0.5", &f->ctx); break;
        case OP_GAMMA: case OP_DIGAMMA: case OP_ZETA:
            set_text(&f->a, f->ma, "2.5", &f->ctx); break;
        case OP_GAMMA_INC:
            set_text(&f->a, f->ma, "2.5", &f->ctx);
            set_text(&f->b, f->mb, "1.2", &f->ctx); break;
        case OP_BETA:
            set_text(&f->a, f->ma, "2.5", &f->ctx);
            set_text(&f->b, f->mb, "1.5", &f->ctx); break;
        case OP_J0: case OP_Y0:
            set_text(&f->a, f->ma, "2", &f->ctx); break;
        case OP_AGM:
            set_text(&f->a, f->ma, "1", &f->ctx);
            set_text(&f->b, f->mb, "0.5", &f->ctx); break;
        case OP_EULER_CACHED: case OP_CATALAN_CACHED: case OP_COUNT: break;
    }
}

static int call_ppf(op_t op, fixture_t *f) {
    switch (op) {
        case OP_EINT: return ppf_eint(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_LI2: return ppf_li2(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_GAMMA: return ppf_gamma(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_GAMMA_INC: return ppf_gamma_inc(&f->out, &f->a, &f->b, PPF_RNDN, &f->ctx);
        case OP_DIGAMMA: return ppf_digamma(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_BETA: return ppf_beta(&f->out, &f->a, &f->b, PPF_RNDN, &f->ctx);
        case OP_ZETA: return ppf_zeta(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_ERF: return ppf_erf(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_ERFC: return ppf_erfc(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_J0: return ppf_j0(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_Y0: return ppf_y0(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_AGM: return ppf_agm(&f->out, &f->a, &f->b, PPF_RNDN, &f->ctx);
        case OP_AI: return ppf_ai(&f->out, &f->a, PPF_RNDN, &f->ctx);
        case OP_EULER_CACHED: return ppf_const_euler(&f->out, PPF_RNDN, &f->ctx);
        case OP_CATALAN_CACHED: return ppf_const_catalan(&f->out, PPF_RNDN, &f->ctx);
        case OP_COUNT: break;
    }
    return PPF_TERNARY_ERROR;
}

static int call_mpfr(op_t op, fixture_t *f) {
    switch (op) {
        case OP_EINT: return mpfr_eint(f->mout, f->ma, MPFR_RNDN);
        case OP_LI2: return mpfr_li2(f->mout, f->ma, MPFR_RNDN);
        case OP_GAMMA: return mpfr_gamma(f->mout, f->ma, MPFR_RNDN);
        case OP_GAMMA_INC: return mpfr_gamma_inc(f->mout, f->ma, f->mb, MPFR_RNDN);
        case OP_DIGAMMA: return mpfr_digamma(f->mout, f->ma, MPFR_RNDN);
        case OP_BETA: return mpfr_beta(f->mout, f->ma, f->mb, MPFR_RNDN);
        case OP_ZETA: return mpfr_zeta(f->mout, f->ma, MPFR_RNDN);
        case OP_ERF: return mpfr_erf(f->mout, f->ma, MPFR_RNDN);
        case OP_ERFC: return mpfr_erfc(f->mout, f->ma, MPFR_RNDN);
        case OP_J0: return mpfr_j0(f->mout, f->ma, MPFR_RNDN);
        case OP_Y0: return mpfr_y0(f->mout, f->ma, MPFR_RNDN);
        case OP_AGM: return mpfr_agm(f->mout, f->ma, f->mb, MPFR_RNDN);
        case OP_AI: return mpfr_ai(f->mout, f->ma, MPFR_RNDN);
        case OP_EULER_CACHED: return mpfr_const_euler(f->mout, MPFR_RNDN);
        case OP_CATALAN_CACHED: return mpfr_const_catalan(f->mout, MPFR_RNDN);
        case OP_COUNT: break;
    }
    return 0;
}

static void verify(op_t op, fixture_t *f, int pternary, int mternary) {
    mpfr_t converted;
    mpfr_init2(converted, mpfr_get_prec(f->mout));
    if (pternary == PPF_TERNARY_ERROR ||
        ppf_get_mpfr(converted, &f->out, MPFR_RNDN) == PPF_TERNARY_ERROR ||
        !same_value(converted, f->mout) || sign_of(pternary) != sign_of(mternary)) {
        fprintf(stderr, "verification failed: %s\n", op_name[op]);
        mpfr_printf("ppf=%Ra\nmpfr=%Ra\n", converted, f->mout);
        exit(3);
    }
    mpfr_clear(converted);
}

int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 256UL;
    unsigned repetitions = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 5U;
    if (bits < 24UL || repetitions == 0U) return 2;
    printf("bits,operation,ppf_median_s,mpfr_median_s,speedup_mpfr_over_ppf\n");
    for (op_t op = 0; op < OP_COUNT; op = (op_t)(op + 1)) {
        fixture_t f; double *pt = calloc(repetitions, sizeof(*pt));
        double *mt = calloc(repetitions, sizeof(*mt));
        if (!pt || !mt) return 2;
        fixture_init(&f, bits); prepare(op, &f);
        if (op == OP_EULER_CACHED) {
            if (ppf_precompute_euler(bits, 1) != 0) return 2;
            (void)mpfr_const_euler(f.mout, MPFR_RNDN);
        } else if (op == OP_CATALAN_CACHED) {
            if (ppf_precompute_catalan(bits, 1) != 0) return 2;
            (void)mpfr_const_catalan(f.mout, MPFR_RNDN);
        }
        for (unsigned r = 0; r < repetitions; ++r) {
            int pternary, mternary; double t0, t1;
            if ((r & 1U) == 0U) {
                t0 = now_seconds(); pternary = call_ppf(op, &f); t1 = now_seconds(); pt[r] = t1 - t0;
                t0 = now_seconds(); mternary = call_mpfr(op, &f); t1 = now_seconds(); mt[r] = t1 - t0;
            } else {
                t0 = now_seconds(); mternary = call_mpfr(op, &f); t1 = now_seconds(); mt[r] = t1 - t0;
                t0 = now_seconds(); pternary = call_ppf(op, &f); t1 = now_seconds(); pt[r] = t1 - t0;
            }
            verify(op, &f, pternary, mternary);
        }
        double pm = median(pt, repetitions), mm = median(mt, repetitions);
        printf("%lu,%s,%.9g,%.9g,%.6f\n", bits, op_name[op], pm, mm, mm / pm);
        fixture_clear(&f); free(pt); free(mt);
    }
    return 0;
}
