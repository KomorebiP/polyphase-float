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

static double now_seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

static int same_value(const ppf_t *value, mpfr_srcptr reference) {
    mpfr_t got;
    int ok;
    mpfr_init2(got, mpfr_get_prec(reference));
    if (ppf_get_mpfr(got, value, MPFR_RNDN) == PPF_TERNARY_ERROR) {
        mpfr_clear(got);
        return 0;
    }
    if (mpfr_nan_p(reference))
        ok = mpfr_nan_p(got) != 0;
    else if (mpfr_inf_p(reference) || mpfr_zero_p(reference))
        ok = ((mpfr_inf_p(reference) && mpfr_inf_p(got)) ||
              (mpfr_zero_p(reference) && mpfr_zero_p(got))) &&
             mpfr_signbit(reference) == mpfr_signbit(got);
    else
        ok = mpfr_equal_p(got, reference) != 0;
    mpfr_clear(got);
    return ok;
}

static int signum(int x) { return (x > 0) - (x < 0); }

int main(int argc, char **argv) {
    const char *method;
    unsigned long bits, loops;
    ppf_context_t ctx;
    ppf_t x, exponent, result;
    mpfr_t mx, mexponent, mresult;
    double start, elapsed;
    int ternary = 0;
    int reference_ternary = 0;
    int is_ppf;

    if (argc != 4) {
        fprintf(stderr, "usage: %s METHOD BITS LOOPS\n", argv[0]);
        return 2;
    }
    method = argv[1];
    bits = strtoul(argv[2], NULL, 10);
    loops = strtoul(argv[3], NULL, 10);
    if (bits < 2 || loops < 1) return 2;
    is_ppf = strncmp(method, "ppf_", 4) == 0;

    if (ppf_context_init(&ctx) != 0 ||
        ppf_init2_ctx(&x, bits, &ctx) != 0 ||
        ppf_init2_ctx(&exponent, bits, &ctx) != 0 ||
        ppf_init2_ctx(&result, bits, &ctx) != 0)
        return 3;
    mpfr_init2(mx, bits);
    mpfr_init2(mexponent, bits);
    mpfr_init2(mresult, bits);

    if (strstr(method, "log1p") || strstr(method, "log2p1") ||
        strstr(method, "log10p1") || strstr(method, "expm1") ||
        strstr(method, "exp2m1") || strstr(method, "exp10m1"))
        mpfr_set_d(mx, 0.125, MPFR_RNDN);
    else if (strstr(method, "exp"))
        mpfr_set_d(mx, 0.125, MPFR_RNDN);
    else
        mpfr_set_d(mx, 1.25, MPFR_RNDN);
    mpfr_set_d(mexponent, 3.25, MPFR_RNDN);
    if (ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_mpfr(&exponent, mexponent, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        return 3;

#define RUN_PPF_BODY() do { \
    if (strcmp(method, "ppf_log") == 0) ternary = ppf_log(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_log1p") == 0) ternary = ppf_log1p(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_log2") == 0) ternary = ppf_log2(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_log10") == 0) ternary = ppf_log10(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_log2p1") == 0) ternary = ppf_log2p1(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_log10p1") == 0) ternary = ppf_log10p1(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_exp") == 0) ternary = ppf_exp(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_exp2") == 0) ternary = ppf_exp2(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_exp10") == 0) ternary = ppf_exp10(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_expm1") == 0) ternary = ppf_expm1(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_exp2m1") == 0) ternary = ppf_exp2m1(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_exp10m1") == 0) ternary = ppf_exp10m1(&result, &x, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_pow") == 0) ternary = ppf_pow(&result, &x, &exponent, PPF_RNDN, &ctx); \
    else if (strcmp(method, "ppf_pow_ui") == 0) ternary = ppf_pow_ui(&result, &x, 17UL, PPF_RNDN, &ctx); \
    else { fprintf(stderr, "unknown method %s\n", method); return 2; } \
} while (0)

#define RUN_MPFR_BODY() do { \
    if (strcmp(method, "mpfr_log") == 0) ternary = mpfr_log(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_log1p") == 0) ternary = mpfr_log1p(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_log2") == 0) ternary = mpfr_log2(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_log10") == 0) ternary = mpfr_log10(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_log2p1") == 0) ternary = mpfr_log2p1(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_log10p1") == 0) ternary = mpfr_log10p1(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_exp") == 0) ternary = mpfr_exp(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_exp2") == 0) ternary = mpfr_exp2(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_exp10") == 0) ternary = mpfr_exp10(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_expm1") == 0) ternary = mpfr_expm1(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_exp2m1") == 0) ternary = mpfr_exp2m1(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_exp10m1") == 0) ternary = mpfr_exp10m1(mresult, mx, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_pow") == 0) ternary = mpfr_pow(mresult, mx, mexponent, MPFR_RNDN); \
    else if (strcmp(method, "mpfr_pow_ui") == 0) ternary = mpfr_pow_ui(mresult, mx, 17UL, MPFR_RNDN); \
    else { fprintf(stderr, "unknown method %s\n", method); return 2; } \
} while (0)

    /* Untimed correctness gate and warm-up. */
    if (is_ppf) {
        RUN_PPF_BODY();
        if (ternary == PPF_TERNARY_ERROR) return 4;
        if (strcmp(method, "ppf_log") == 0) reference_ternary = mpfr_log(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_log1p") == 0) reference_ternary = mpfr_log1p(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_log2") == 0) reference_ternary = mpfr_log2(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_log10") == 0) reference_ternary = mpfr_log10(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_log2p1") == 0) reference_ternary = mpfr_log2p1(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_log10p1") == 0) reference_ternary = mpfr_log10p1(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_exp") == 0) reference_ternary = mpfr_exp(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_exp2") == 0) reference_ternary = mpfr_exp2(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_exp10") == 0) reference_ternary = mpfr_exp10(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_expm1") == 0) reference_ternary = mpfr_expm1(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_exp2m1") == 0) reference_ternary = mpfr_exp2m1(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_exp10m1") == 0) reference_ternary = mpfr_exp10m1(mresult, mx, MPFR_RNDN);
        else if (strcmp(method, "ppf_pow") == 0) reference_ternary = mpfr_pow(mresult, mx, mexponent, MPFR_RNDN);
        else reference_ternary = mpfr_pow_ui(mresult, mx, 17UL, MPFR_RNDN);
        if (!same_value(&result, mresult) || signum(ternary) != signum(reference_ternary))
            return 5;
    } else {
        RUN_MPFR_BODY();
        if (strcmp(method, "mpfr_log") == 0) reference_ternary = ppf_log(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_log1p") == 0) reference_ternary = ppf_log1p(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_log2") == 0) reference_ternary = ppf_log2(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_log10") == 0) reference_ternary = ppf_log10(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_log2p1") == 0) reference_ternary = ppf_log2p1(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_log10p1") == 0) reference_ternary = ppf_log10p1(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_exp") == 0) reference_ternary = ppf_exp(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_exp2") == 0) reference_ternary = ppf_exp2(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_exp10") == 0) reference_ternary = ppf_exp10(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_expm1") == 0) reference_ternary = ppf_expm1(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_exp2m1") == 0) reference_ternary = ppf_exp2m1(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_exp10m1") == 0) reference_ternary = ppf_exp10m1(&result, &x, PPF_RNDN, &ctx);
        else if (strcmp(method, "mpfr_pow") == 0) reference_ternary = ppf_pow(&result, &x, &exponent, PPF_RNDN, &ctx);
        else reference_ternary = ppf_pow_ui(&result, &x, 17UL, PPF_RNDN, &ctx);
        if (reference_ternary == PPF_TERNARY_ERROR || !same_value(&result, mresult) ||
            signum(reference_ternary) != signum(ternary))
            return 5;
    }

    start = now_seconds();
    if (is_ppf) {
        for (unsigned long i = 0; i < loops; ++i) RUN_PPF_BODY();
    } else {
        for (unsigned long i = 0; i < loops; ++i) RUN_MPFR_BODY();
    }
    elapsed = now_seconds() - start;

    printf("{\"method\":\"%s\",\"bits\":%lu,\"loops\":%lu,"
           "\"seconds_per_call\":%.12f,\"ternary\":%d}\n",
           method, bits, loops, elapsed / (double)loops, ternary);

    ppf_clear(&x);
    ppf_clear(&exponent);
    ppf_clear(&result);
    ppf_context_clear(&ctx);
    mpfr_clear(mx);
    mpfr_clear(mexponent);
    mpfr_clear(mresult);
    return 0;
}
