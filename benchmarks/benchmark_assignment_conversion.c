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
        double v = x[i]; int j = i - 1;
        while (j >= 0 && x[j] > v) { x[j+1] = x[j]; --j; }
        x[j+1] = v;
    }
    return x[n/2];
}

static void fail(const char *what) {
    fprintf(stderr, "benchmark correctness failure: %s\n", what);
    exit(2);
}

int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000000UL;
    int reps = argc > 2 ? atoi(argv[2]) : 15;
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t mx, exported;
    mpz_t z, pz, mz, den;
    mpq_t q;
    double *pset, *mset, *pget, *mget, *pqset, *mqset, *pstr, *mstr;
    char *s1, *s2;
    ppf_exp_t pe;
    mpfr_exp_t me;
    const size_t output_digits = 128;

    if (reps < 3) reps = 3;
#define ALLOC(name) do { name = calloc((size_t)reps, sizeof(double)); if (!name) return 2; } while (0)
    ALLOC(pset); ALLOC(mset); ALLOC(pget); ALLOC(mget);
    ALLOC(pqset); ALLOC(mqset); ALLOC(pstr); ALLOC(mstr);
#undef ALLOC

    if (ppf_context_init(&ctx) != 0 || ppf_init2_ctx(&x, bits, &ctx) != 0)
        return 2;
    mpfr_init2(mx, bits); mpfr_init2(exported, bits);
    mpz_inits(z, pz, mz, den, NULL); mpq_init(q);
    mpz_set_ui(z, 1); mpz_mul_2exp(z, z, bits - 1);
    for (unsigned i = 0; i < 64 && i + 2 < bits; ++i)
        if ((i * 17u + 5u) % 11u < 5u) mpz_setbit(z, i * (bits / 65u + 1u));
    mpz_ui_pow_ui(den, 3, 257);
    mpq_set_num(q, z); mpq_set_den(q, den); mpq_canonicalize(q);

    (void)ppf_set_z(&x, z, PPF_RNDN, &ctx);
    (void)mpfr_set_z(mx, z, MPFR_RNDN);
    (void)ppf_get_mpfr(exported, &x, MPFR_RNDN);
    if (!mpfr_equal_p(mx, exported)) fail("initial set_z");

    (void)ppf_get_z(pz, &x, PPF_RNDN, &ctx);
    (void)mpfr_get_z(mz, mx, MPFR_RNDN);
    if (mpz_cmp(pz, mz) != 0) fail("initial get_z");

    (void)ppf_set_q(&x, q, PPF_RNDN, &ctx);
    (void)mpfr_set_q(mx, q, MPFR_RNDN);
    (void)ppf_get_mpfr(exported, &x, MPFR_RNDN);
    if (!mpfr_equal_p(mx, exported)) fail("initial set_q");

    s1 = ppf_get_str(NULL, &pe, 10, output_digits, &x, PPF_RNDN, &ctx);
    s2 = mpfr_get_str(NULL, &me, 10, output_digits, mx, MPFR_RNDN);
    if (!s1 || !s2 || pe != me || strcmp(s1, s2) != 0) fail("initial get_str");
    ppf_free_str(s1); mpfr_free_str(s2);

    for (int r = 0; r < reps; ++r) {
        double t0;
        if (r & 1) {
            t0 = now_seconds(); (void)mpfr_set_z(mx, z, MPFR_RNDN); mset[r] = now_seconds()-t0;
            t0 = now_seconds(); (void)ppf_set_z(&x, z, PPF_RNDN, &ctx); pset[r] = now_seconds()-t0;
        } else {
            t0 = now_seconds(); (void)ppf_set_z(&x, z, PPF_RNDN, &ctx); pset[r] = now_seconds()-t0;
            t0 = now_seconds(); (void)mpfr_set_z(mx, z, MPFR_RNDN); mset[r] = now_seconds()-t0;
        }

        t0 = now_seconds(); (void)ppf_get_z(pz, &x, PPF_RNDN, &ctx); pget[r] = now_seconds()-t0;
        t0 = now_seconds(); (void)mpfr_get_z(mz, mx, MPFR_RNDN); mget[r] = now_seconds()-t0;

        t0 = now_seconds(); (void)ppf_set_q(&x, q, PPF_RNDN, &ctx); pqset[r] = now_seconds()-t0;
        t0 = now_seconds(); (void)mpfr_set_q(mx, q, MPFR_RNDN); mqset[r] = now_seconds()-t0;

        t0 = now_seconds(); s1 = ppf_get_str(NULL, &pe, 10, output_digits, &x, PPF_RNDN, &ctx); pstr[r] = now_seconds()-t0;
        t0 = now_seconds(); s2 = mpfr_get_str(NULL, &me, 10, output_digits, mx, MPFR_RNDN); mstr[r] = now_seconds()-t0;
        if (!s1 || !s2 || pe != me || strcmp(s1,s2) != 0) fail("timed get_str");
        ppf_free_str(s1); mpfr_free_str(s2);
    }

    printf("{\"bits\":%lu,\"repetitions\":%d,"
           "\"ppf_set_z_seconds\":%.9f,\"mpfr_set_z_seconds\":%.9f,"
           "\"ppf_get_z_seconds\":%.9f,\"mpfr_get_z_seconds\":%.9f,"
           "\"ppf_set_q_seconds\":%.9f,\"mpfr_set_q_seconds\":%.9f,"
           "\"ppf_get_str_seconds\":%.9f,\"mpfr_get_str_seconds\":%.9f}\n",
           bits, reps, median(pset,reps), median(mset,reps),
           median(pget,reps), median(mget,reps),
           median(pqset,reps), median(mqset,reps),
           median(pstr,reps), median(mstr,reps));

    mpq_clear(q); mpz_clears(z,pz,mz,den,NULL);
    mpfr_clear(mx); mpfr_clear(exported); ppf_clear(&x); ppf_context_clear(&ctx);
    free(pset); free(mset); free(pget); free(mget); free(pqset); free(mqset); free(pstr); free(mstr);
    return 0;
}
