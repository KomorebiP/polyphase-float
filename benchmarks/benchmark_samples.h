/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef PPF_BENCHMARK_SAMPLES_H
#define PPF_BENCHMARK_SAMPLES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int ppf_benchmark_compare_double(const void *ap, const void *bp) {
    const double a = *(const double *)ap;
    const double b = *(const double *)bp;
    return (a > b) - (a < b);
}

/* Preserve acquisition order in values; return nonzero on invalid input/OOM. */
static inline int ppf_benchmark_median(const double *values, size_t count,
                                double *result) {
    double *copy;
    if (!values || count == 0u || !result) return -1;
    copy = malloc(count * sizeof(*copy));
    if (!copy) return -1;
    memcpy(copy, values, count * sizeof(*copy));
    qsort(copy, count, sizeof(*copy), ppf_benchmark_compare_double);
    if (count & 1u)
        *result = copy[count / 2u];
    else
        *result = 0.5 * (copy[count / 2u - 1u] + copy[count / 2u]);
    free(copy);
    return 0;
}

static inline double ppf_benchmark_mean(const double *values, size_t count) {
    double total = 0.0;
    if (!values || count == 0u) return 0.0;
    for (size_t i = 0u; i < count; ++i) total += values[i];
    return total / (double)count;
}

static inline void ppf_benchmark_print_samples_json(const double *values,
                                             size_t count) {
    putchar('[');
    for (size_t i = 0u; i < count; ++i) {
        if (i) putchar(',');
        printf("%.12f", values[i]);
    }
    putchar(']');
}

#endif
