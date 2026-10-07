/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>
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

int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000000UL;
    int reps = argc > 2 ? atoi(argv[2]) : 21;
    ppf_context_t ctx;
    ppf_t a, b;
    mpfr_t ma, mb;
    mpz_t z;
    double *ppf_copy, *mpfr_copy, *roundtrip;

    if (reps < 3) reps = 3;
    ppf_copy = calloc((size_t)reps, sizeof(double));
    mpfr_copy = calloc((size_t)reps, sizeof(double));
    roundtrip = calloc((size_t)reps, sizeof(double));
    if (!ppf_copy || !mpfr_copy || !roundtrip) return 2;

    ppf_context_init(&ctx);
    ppf_init2_ctx(&a, bits, &ctx);
    ppf_init2_ctx(&b, bits, &ctx);
    mpfr_init2(ma, bits);
    mpfr_init2(mb, bits);
    mpz_init(z);
    mpz_set_ui(z, 1);
    mpz_mul_2exp(z, z, bits - 1);
    mpz_add_ui(z, z, 1234567);
    ppf_set_z(&a, z, PPF_RNDN, &ctx);
    mpfr_set_z_2exp(ma, z, 0, MPFR_RNDN);

    for (int r = 0; r < reps; ++r) {
        double t0 = now_seconds();
        (void)ppf_set(&b, &a, PPF_RNDN, &ctx);
        ppf_copy[r] = now_seconds() - t0;

        t0 = now_seconds();
        (void)mpfr_set(mb, ma, MPFR_RNDN);
        mpfr_copy[r] = now_seconds() - t0;

        t0 = now_seconds();
        (void)ppf_get_mpfr(mb, &a, MPFR_RNDN);
        (void)ppf_set_mpfr(&b, mb, PPF_RNDN, &ctx);
        roundtrip[r] = now_seconds() - t0;
    }

    printf("{\"bits\":%lu,\"repetitions\":%d,"
           "\"ppf_copy_seconds\":%.9f,"
           "\"mpfr_copy_seconds\":%.9f,"
           "\"mpfr_boundary_roundtrip_seconds\":%.9f}\n",
           bits, reps, median(ppf_copy, reps), median(mpfr_copy, reps),
           median(roundtrip, reps));

    mpz_clear(z);
    mpfr_clear(ma); mpfr_clear(mb);
    ppf_clear(&a); ppf_clear(&b);
    ppf_context_clear(&ctx);
    free(ppf_copy); free(mpfr_copy); free(roundtrip);
    return 0;
}
