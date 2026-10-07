/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float_mpfr.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef enum {
    METHOD_RANK3,
    METHOD_RANK5,
    METHOD_AUTO,
    METHOD_MPFR,
    METHOD_COUNT
} method_t;

static const char *const method_names[METHOD_COUNT] = {
    "ppf_rank3", "ppf_rank5", "ppf_auto", "mpfr_1t"
};

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static void die(const char *message) {
    fprintf(stderr, "reciprocal-root planner failure: %s\n", message);
    exit(2);
}

static int same_ternary(int a, int b) {
    return (a > 0) == (b > 0) && (a < 0) == (b < 0);
}

static void shuffle(int *order, unsigned count, unsigned *state) {
    for (unsigned i = 0; i < count; ++i) order[i] = (int)i;
    for (unsigned i = count; i > 1; --i) {
        *state = *state * 1664525u + 1013904223u;
        unsigned j = *state % i;
        int temporary = order[i - 1];
        order[i - 1] = order[j];
        order[j] = temporary;
    }
}

static int run_ppf(method_t method, ppf_t *out, const ppf_t *input,
                   unsigned long workers, ppf_context_t *context) {
    ppf_product_algorithm_t algorithm = PPF_PRODUCT_AUTO;
    if (method == METHOD_RANK3) algorithm = PPF_PRODUCT_KARATSUBA3;
    if (method == METHOD_RANK5) algorithm = PPF_PRODUCT_TOOM3;
    return ppf_rec_sqrt_algorithm_threads(
        out, input, PPF_RNDN, workers, algorithm, context);
}

int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000000UL;
    int repetitions = argc > 2 ? atoi(argv[2]) : 9;
    unsigned long workers = argc > 3 ? strtoul(argv[3], NULL, 10) : 5UL;
    int inner = argc > 4 ? atoi(argv[4]) : 1;
    ppf_context_t context;
    ppf_t input, outputs[METHOD_COUNT - 1];
    mpfr_t source, output, reference, exported;
    mpz_t z;
    int ternaries[METHOD_COUNT];
    int reference_ternary;
    unsigned random_state = 0x7215A11Du;

    if (bits < 128) bits = 128;
    if (repetitions < 1) repetitions = 1;
    if (workers < 1) workers = 1;
    if (inner < 1) inner = 1;
    if (ppf_context_init(&context) != 0 ||
        ppf_init2_ctx(&input, bits, &context) != 0)
        die("initialization");
    for (int index = 0; index < METHOD_COUNT - 1; ++index) {
        if (ppf_init2_ctx(&outputs[index], bits, &context) != 0)
            die("output initialization");
    }
    mpfr_init2(source, bits);
    mpfr_init2(output, bits);
    mpfr_init2(reference, bits);
    mpfr_init2(exported, bits);
    mpz_init(z);
    mpz_set_ui(z, 1);
    mpz_mul_2exp(z, z, bits - 1);
    for (unsigned k = 0; k < 83; ++k) {
        unsigned long position = (unsigned long)(
            ((uint64_t)(k + 19) * UINT64_C(0x9e3779b97f4a7c15)) %
            (bits - 1));
        mpz_setbit(z, position);
    }
    if (ppf_set_z(&input, z, PPF_RNDN, &context) == PPF_TERNARY_ERROR)
        die("input assignment");
    mpfr_set_z(source, z, MPFR_RNDN);
    reference_ternary = mpfr_rec_sqrt(reference, source, MPFR_RNDN);

    for (method_t method = METHOD_RANK3; method < METHOD_MPFR; ++method) {
        ternaries[method] = run_ppf(method, &outputs[method], &input,
                                    workers, &context);
        if (ternaries[method] == PPF_TERNARY_ERROR ||
            ppf_get_mpfr(exported, &outputs[method], MPFR_RNDN) ==
                PPF_TERNARY_ERROR ||
            !mpfr_equal_p(exported, reference) ||
            !same_ternary(ternaries[method], reference_ternary))
            die("warm-up correctness");
    }
    mpfr_set(output, reference, MPFR_RNDN);

    puts("bits,operation,method,method_workers,repeat,order,seconds");
    for (int repeat = 1; repeat <= repetitions; ++repeat) {
        int order[METHOD_COUNT];
        shuffle(order, METHOD_COUNT, &random_state);
        for (int position = 0; position < METHOD_COUNT; ++position) {
            method_t method = (method_t)order[position];
            double start = now_seconds();
            for (int q = 0; q < inner; ++q) {
                if (method == METHOD_MPFR)
                    ternaries[method] = mpfr_rec_sqrt(
                        output, source, MPFR_RNDN);
                else
                    ternaries[method] = run_ppf(
                        method, &outputs[method], &input,
                        workers, &context);
            }
            double seconds = (now_seconds() - start) / (double)inner;
            if (method == METHOD_MPFR) {
                if (!mpfr_equal_p(output, reference) ||
                    !same_ternary(ternaries[method], reference_ternary))
                    die("MPFR timed correctness");
            } else {
                if (ppf_get_mpfr(exported, &outputs[method], MPFR_RNDN) ==
                        PPF_TERNARY_ERROR ||
                    !mpfr_equal_p(exported, reference) ||
                    !same_ternary(ternaries[method], reference_ternary))
                    die("Polyphase timed correctness");
            }
            printf("%lu,rec_sqrt,%s,%lu,%d,%d,%.12f\n",
                   bits, method_names[method],
                   method == METHOD_MPFR ? 1UL : workers,
                   repeat, position + 1, seconds);
            fflush(stdout);
        }
    }

    mpz_clear(z);
    mpfr_clears(source, output, reference, exported, (mpfr_ptr)0);
    for (int index = 0; index < METHOD_COUNT - 1; ++index)
        ppf_clear(&outputs[index]);
    ppf_clear(&input);
    ppf_context_clear(&context);
    return 0;
}
