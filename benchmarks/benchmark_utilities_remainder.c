/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static double median(double *x, int n) {
    for (int i = 1; i < n; ++i) {
        double v = x[i];
        int j = i - 1;
        while (j >= 0 && x[j] > v) {
            x[j + 1] = x[j];
            --j;
        }
        x[j + 1] = v;
    }
    return x[n / 2];
}

static void fail(const char *what) {
    fprintf(stderr, "benchmark correctness failure: %s\n", what);
    exit(2);
}

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        return mpfr_nan_p(a) && mpfr_nan_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    if (mpfr_zero_p(a) || mpfr_zero_p(b))
        return mpfr_zero_p(a) && mpfr_zero_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    return mpfr_equal_p(a, b) != 0;
}

static void check_ppf(mpfr_ptr scratch, const ppf_t *value,
                      mpfr_srcptr expected, const char *label) {
    mpfr_set_prec(scratch, mpfr_get_prec(expected));
    (void)ppf_get_mpfr(scratch, value, MPFR_RNDN);
    if (!same_value(scratch, expected)) fail(label);
}

static void make_operand(mpz_ptr z, unsigned long bits, unsigned seed) {
    mpz_set_ui(z, 0);
    mpz_setbit(z, bits - 1);
    if (bits > 3) mpz_setbit(z, bits - 3);
    for (unsigned i = 0; i < 97; ++i) {
        unsigned long pos = ((unsigned long)(i + 1) *
                             (unsigned long)(seed * 131u + 977u)) % bits;
        mpz_setbit(z, pos);
    }
}

int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000000UL;
    int reps = argc > 2 ? atoi(argv[2]) : 11;
    ppf_context_t ctx;
    ppf_t x, y, pr, pi, pf;
    mpfr_t mx, my, mr, mi, mf, exported;
    mpz_t zx, zy;
    ppf_exp_t scale;
    volatile int sink = 0;

    double *p_round, *m_round, *p_frac, *m_frac;
    double *p_modf, *m_modf, *p_fmod, *m_fmod;
    double *p_rem, *m_rem, *p_cmp, *m_cmp;

    if (bits < 64) bits = 64;
    if (reps < 3) reps = 3;
#define ALLOC(name) do { name = calloc((size_t)reps, sizeof(double)); if (!name) return 2; } while (0)
    ALLOC(p_round); ALLOC(m_round); ALLOC(p_frac); ALLOC(m_frac);
    ALLOC(p_modf); ALLOC(m_modf); ALLOC(p_fmod); ALLOC(m_fmod);
    ALLOC(p_rem); ALLOC(m_rem); ALLOC(p_cmp); ALLOC(m_cmp);
#undef ALLOC

    if (ppf_context_init(&ctx) != 0 ||
        ppf_init2_ctx(&x, bits, &ctx) != 0 ||
        ppf_init2_ctx(&y, bits, &ctx) != 0 ||
        ppf_init2_ctx(&pr, bits, &ctx) != 0 ||
        ppf_init2_ctx(&pi, bits, &ctx) != 0 ||
        ppf_init2_ctx(&pf, bits, &ctx) != 0)
        return 2;
    mpfr_inits2(bits, mx, my, mr, mi, mf, exported, (mpfr_ptr)0);
    mpz_inits(zx, zy, NULL);

    make_operand(zx, bits, 17u);
    make_operand(zy, bits, 29u);
    if (mpz_cmp(zx, zy) <= 0) mpz_add(zx, zx, zy);
    scale = -(ppf_exp_t)(bits / 3u + 17u);
    (void)ppf_set_z_2exp(&x, zx, scale, PPF_RNDN, &ctx);
    (void)ppf_set_z_2exp(&y, zy, scale, PPF_RNDN, &ctx);
    (void)mpfr_set_z_2exp(mx, zx, (mpfr_exp_t)scale, MPFR_RNDN);
    (void)mpfr_set_z_2exp(my, zy, (mpfr_exp_t)scale, MPFR_RNDN);

    /* Correctness preflight. */
    (void)ppf_roundeven(&pr, &x, &ctx);
    (void)mpfr_roundeven(mr, mx);
    check_ppf(exported, &pr, mr, "roundeven");
    (void)ppf_frac(&pr, &x, PPF_RNDN, &ctx);
    (void)mpfr_frac(mr, mx, MPFR_RNDN);
    check_ppf(exported, &pr, mr, "frac");
    (void)ppf_modf(&pi, &pf, &x, PPF_RNDN, &ctx);
    (void)mpfr_modf(mi, mf, mx, MPFR_RNDN);
    check_ppf(exported, &pi, mi, "modf integer");
    check_ppf(exported, &pf, mf, "modf fraction");
    (void)ppf_fmod(&pr, &x, &y, PPF_RNDN, &ctx);
    (void)mpfr_fmod(mr, mx, my, MPFR_RNDN);
    check_ppf(exported, &pr, mr, "fmod");
    (void)ppf_remainder(&pr, &x, &y, PPF_RNDN, &ctx);
    (void)mpfr_remainder(mr, mx, my, MPFR_RNDN);
    check_ppf(exported, &pr, mr, "remainder");
    if ((ppf_cmp(&x, &y, &ctx) > 0) != (mpfr_cmp(mx, my) > 0)) fail("cmp");

    /* One untimed warmup per operation. */
    (void)ppf_roundeven(&pr, &x, &ctx); (void)mpfr_roundeven(mr, mx);
    (void)ppf_frac(&pr, &x, PPF_RNDN, &ctx); (void)mpfr_frac(mr, mx, MPFR_RNDN);
    (void)ppf_modf(&pi, &pf, &x, PPF_RNDN, &ctx); (void)mpfr_modf(mi, mf, mx, MPFR_RNDN);
    (void)ppf_fmod(&pr, &x, &y, PPF_RNDN, &ctx); (void)mpfr_fmod(mr, mx, my, MPFR_RNDN);
    (void)ppf_remainder(&pr, &x, &y, PPF_RNDN, &ctx); (void)mpfr_remainder(mr, mx, my, MPFR_RNDN);

#define TIME_PAIR(PARR, MARR, PEXPR, MEXPR) do { \
    double t0; \
    if (r & 1) { \
        mpfr_clear_flags(); t0 = now_seconds(); (void)(MEXPR); (MARR)[r] = now_seconds() - t0; \
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); t0 = now_seconds(); (void)(PEXPR); (PARR)[r] = now_seconds() - t0; \
    } else { \
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); t0 = now_seconds(); (void)(PEXPR); (PARR)[r] = now_seconds() - t0; \
        mpfr_clear_flags(); t0 = now_seconds(); (void)(MEXPR); (MARR)[r] = now_seconds() - t0; \
    } \
} while (0)

    for (int r = 0; r < reps; ++r) {
        TIME_PAIR(p_round, m_round,
                  ppf_roundeven(&pr, &x, &ctx),
                  mpfr_roundeven(mr, mx));
        TIME_PAIR(p_frac, m_frac,
                  ppf_frac(&pr, &x, PPF_RNDN, &ctx),
                  mpfr_frac(mr, mx, MPFR_RNDN));
        TIME_PAIR(p_modf, m_modf,
                  ppf_modf(&pi, &pf, &x, PPF_RNDN, &ctx),
                  mpfr_modf(mi, mf, mx, MPFR_RNDN));
        TIME_PAIR(p_fmod, m_fmod,
                  ppf_fmod(&pr, &x, &y, PPF_RNDN, &ctx),
                  mpfr_fmod(mr, mx, my, MPFR_RNDN));
        TIME_PAIR(p_rem, m_rem,
                  ppf_remainder(&pr, &x, &y, PPF_RNDN, &ctx),
                  mpfr_remainder(mr, mx, my, MPFR_RNDN));
        {
            double t0;
            if (r & 1) {
                t0 = now_seconds(); sink += mpfr_cmp(mx, my); m_cmp[r] = now_seconds() - t0;
                t0 = now_seconds(); sink += ppf_cmp(&x, &y, &ctx); p_cmp[r] = now_seconds() - t0;
            } else {
                t0 = now_seconds(); sink += ppf_cmp(&x, &y, &ctx); p_cmp[r] = now_seconds() - t0;
                t0 = now_seconds(); sink += mpfr_cmp(mx, my); m_cmp[r] = now_seconds() - t0;
            }
        }
    }
#undef TIME_PAIR

    printf("{\"bits\":%lu,\"repetitions\":%d,"
           "\"ppf_roundeven_seconds\":%.9f,\"mpfr_roundeven_seconds\":%.9f,"
           "\"ppf_frac_seconds\":%.9f,\"mpfr_frac_seconds\":%.9f,"
           "\"ppf_modf_seconds\":%.9f,\"mpfr_modf_seconds\":%.9f,"
           "\"ppf_fmod_seconds\":%.9f,\"mpfr_fmod_seconds\":%.9f,"
           "\"ppf_remainder_seconds\":%.9f,\"mpfr_remainder_seconds\":%.9f,"
           "\"ppf_cmp_seconds\":%.9f,\"mpfr_cmp_seconds\":%.9f,"
           "\"sink\":%d}\n",
           bits, reps,
           median(p_round, reps), median(m_round, reps),
           median(p_frac, reps), median(m_frac, reps),
           median(p_modf, reps), median(m_modf, reps),
           median(p_fmod, reps), median(m_fmod, reps),
           median(p_rem, reps), median(m_rem, reps),
           median(p_cmp, reps), median(m_cmp, reps), sink);

    mpz_clears(zx, zy, NULL);
    mpfr_clears(mx, my, mr, mi, mf, exported, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&y); ppf_clear(&pr); ppf_clear(&pi); ppf_clear(&pf);
    ppf_context_clear(&ctx);
    free(p_round); free(m_round); free(p_frac); free(m_frac);
    free(p_modf); free(m_modf); free(p_fmod); free(m_fmod);
    free(p_rem); free(m_rem); free(p_cmp); free(m_cmp);
    return 0;
}
