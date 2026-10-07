/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float.h"
#include "polyphase_float_mpfr.h"
#include "benchmark_samples.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
}

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b)) return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_inf_p(a) || mpfr_inf_p(b))
        return mpfr_inf_p(a) && mpfr_inf_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    if (!mpfr_equal_p(a, b)) return 0;
    if (mpfr_zero_p(a)) return mpfr_signbit(a) == mpfr_signbit(b);
    return 1;
}

static int method_is_pi(const char *method) {
    return strstr(method, "pi") != NULL;
}

int main(int argc, char **argv) {
    const char *method;
    unsigned long bits, workers, repetitions;
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t ref, out;
    double total = 0.0;
    double *samples = NULL;
    int pi;
    int status = 0;
    int context_initialized = 0;
    int value_initialized = 0;
    int mpfr_initialized = 0;

    if (argc != 5) {
        fprintf(stderr, "usage: %s METHOD BITS WORKERS REPETITIONS\n", argv[0]);
        return 2;
    }
    method = argv[1];
    bits = strtoul(argv[2], NULL, 10);
    workers = strtoul(argv[3], NULL, 10);
    repetitions = strtoul(argv[4], NULL, 10);
    if (bits < 2UL || workers < 1UL || repetitions < 1UL) return 2;

    samples = calloc((size_t)repetitions, sizeof(*samples));
    if (!samples) return 3;
    pi = method_is_pi(method);
    if (ppf_context_init(&ctx) != 0) {
        status = 3;
        goto cleanup;
    }
    context_initialized = 1;
    if (ppf_init2_ctx(&x, bits, &ctx) != 0) {
        status = 3;
        goto cleanup;
    }
    value_initialized = 1;
    mpfr_inits2(bits, ref, out, (mpfr_ptr)0);
    mpfr_initialized = 1;

    if (strstr(method, "cached")) {
        int warm;
        if (pi) {
            if (ppf_precompute_pi(bits, workers) != 0) {
                status = 4;
                goto cleanup;
            }
            warm = ppf_const_pi_threads(&x, PPF_RNDN, workers, &ctx);
            (void)mpfr_const_pi(ref, MPFR_RNDN);
        } else {
            if (ppf_precompute_log2(bits, workers) != 0) {
                status = 4;
                goto cleanup;
            }
            warm = ppf_const_log2_threads(&x, PPF_RNDN, workers, &ctx);
            (void)mpfr_const_log2(ref, MPFR_RNDN);
        }
        if (warm == PPF_TERNARY_ERROR) {
            status = 4;
            goto cleanup;
        }
    }

    for (unsigned long i = 0UL; i < repetitions; ++i) {
        int ternary;
        double started;
        if (strcmp(method, "ppf_pi_cold") == 0 ||
            strcmp(method, "ppf_log2_cold") == 0)
            ppf_clear_constant_cache(pi ? PPF_CONSTANT_PI : PPF_CONSTANT_LOG2);
        if (strcmp(method, "mpfr_pi_cold") == 0 ||
            strcmp(method, "mpfr_log2_cold") == 0)
            mpfr_free_cache();
        started = now_seconds();
        if (strncmp(method, "ppf_", 4) == 0) {
            ternary = pi ? ppf_const_pi_threads(&x, PPF_RNDN, workers, &ctx)
                         : ppf_const_log2_threads(&x, PPF_RNDN, workers, &ctx);
        } else {
            ternary = pi ? mpfr_const_pi(ref, MPFR_RNDN)
                         : mpfr_const_log2(ref, MPFR_RNDN);
        }
        samples[i] = now_seconds() - started;
        total += samples[i];
        if (ternary == PPF_TERNARY_ERROR) {
            status = 5;
            goto cleanup;
        }
    }

    if (pi) (void)mpfr_const_pi(ref, MPFR_RNDN);
    else (void)mpfr_const_log2(ref, MPFR_RNDN);
    if (strncmp(method, "ppf_", 4) == 0) {
        (void)ppf_get_mpfr(out, &x, MPFR_RNDN);
        if (!same_value(ref, out)) {
            status = 6;
            goto cleanup;
        }
    }
    {
        double median;
        const double mean = ppf_benchmark_mean(samples, (size_t)repetitions);
        if (ppf_benchmark_median(samples, (size_t)repetitions, &median) != 0) {
            fprintf(stderr, "unable to compute benchmark median\n");
            status = 7;
            goto cleanup;
        }
        printf("{\"schema_version\":2,\"method\":\"%s\",\"bits\":%lu,"
               "\"workers\":%lu,\"jobs\":1,\"warmups\":0,"
               "\"repetitions\":%lu,\"sample_statistic\":\"median\","
               "\"seconds\":%.12f,\"mean_seconds\":%.12f,"
               "\"total_seconds\":%.12f,\"seconds_per_call\":%.12f,"
               "\"sample_count\":%lu,\"samples_seconds\":",
               method, bits, workers, repetitions, median, mean, total, mean,
               repetitions);
        ppf_benchmark_print_samples_json(samples, (size_t)repetitions);
        printf(",\"verified\":true}\n");
    }

cleanup:
    if (mpfr_initialized) mpfr_clears(ref, out, (mpfr_ptr)0);
    if (value_initialized) ppf_clear(&x);
    if (context_initialized) ppf_context_clear(&ctx);
    free(samples);
    return status;
}
