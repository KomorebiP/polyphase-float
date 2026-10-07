/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float_mpfr.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    METHOD_FULL,
    METHOD_RANK3,
    METHOD_RANK5,
    METHOD_WINDOW,
    METHOD_AUTO,
    METHOD_MPFR,
    METHOD_COUNT
} method_t;

static const char *const method_names[METHOD_COUNT] = {
    "ppf_full_1t",
    "ppf_rank3",
    "ppf_rank5",
    "ppf_window",
    "ppf_auto",
    "mpfr_1t"
};

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static void die(const char *message) {
    fprintf(stderr, "product benchmark failure: %s\n", message);
    exit(2);
}

static int same_ternary_direction(int a, int b) {
    return (a > 0) == (b > 0) && (a < 0) == (b < 0);
}

static void shuffle(int *order, unsigned count, unsigned *state) {
    unsigned i;
    for (i = 0; i < count; ++i) order[i] = (int)i;
    for (i = count; i > 1; --i) {
        unsigned j;
        int temporary;
        *state = *state * 1664525u + 1013904223u;
        j = *state % i;
        temporary = order[i - 1];
        order[i - 1] = order[j];
        order[j] = temporary;
    }
}

static int run_ppf_method(method_t method, int square,
                          ppf_t *out, const ppf_t *a, const ppf_t *b,
                          unsigned long workers, ppf_context_t *context) {
    static const ppf_product_algorithm_t algorithms[] = {
        PPF_PRODUCT_FULL,
        PPF_PRODUCT_KARATSUBA3,
        PPF_PRODUCT_TOOM3,
        PPF_PRODUCT_WINDOW,
        PPF_PRODUCT_AUTO
    };
    if (method >= METHOD_MPFR) return PPF_TERNARY_ERROR;
    if (square) {
        return ppf_sqr_algorithm_threads(out, a, PPF_RNDN, workers,
                                         algorithms[method], context);
    }
    return ppf_mul_algorithm_threads(out, a, b, PPF_RNDN, workers,
                                     algorithms[method], context);
}

static int run_ppf_fma_method(method_t method,
                              ppf_t *out, const ppf_t *a,
                              const ppf_t *b, const ppf_t *c,
                              unsigned long workers,
                              ppf_context_t *context) {
    static const ppf_product_algorithm_t algorithms[] = {
        PPF_PRODUCT_FULL,
        PPF_PRODUCT_KARATSUBA3,
        PPF_PRODUCT_TOOM3,
        PPF_PRODUCT_WINDOW,
        PPF_PRODUCT_AUTO
    };
    if (method >= METHOD_MPFR || method == METHOD_WINDOW)
        return PPF_TERNARY_ERROR;
    return ppf_fma_algorithm_threads(out, a, b, c, PPF_RNDN,
                                     workers, algorithms[method], context);
}


int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000000UL;
    int repetitions = argc > 2 ? atoi(argv[2]) : 11;
    unsigned long workers = argc > 3 ? strtoul(argv[3], NULL, 10) : 5UL;
    int inner = argc > 4 ? atoi(argv[4]) : 1;
    ppf_context_t context;
    ppf_t a, b, c, outputs[METHOD_COUNT - 1];
    mpfr_t ma, mb, mc, mout, reference, exported;
    mpz_t za, zb, zc;
    int ternaries[METHOD_COUNT];
    unsigned random_state = 0xC001D00Du;
    int output_index;
    int square;

    if (bits < 128) bits = 128;
    if (repetitions < 1) repetitions = 1;
    if (workers < 1) workers = 1;
    if (inner < 1) inner = 1;

    if (ppf_context_init(&context) != 0) die("context initialization");
    if (ppf_init2_ctx(&a, bits, &context) != 0 ||
        ppf_init2_ctx(&b, bits, &context) != 0 ||
        ppf_init2_ctx(&c, bits, &context) != 0)
        die("input initialization");
    for (output_index = 0; output_index < METHOD_COUNT - 1; ++output_index) {
        if (ppf_init2_ctx(&outputs[output_index], bits, &context) != 0)
            die("output initialization");
    }

    mpfr_init2(ma, bits);
    mpfr_init2(mb, bits);
    mpfr_init2(mc, bits);
    mpfr_init2(mout, bits);
    mpfr_init2(reference, bits);
    mpfr_init2(exported, bits);
    mpz_inits(za, zb, zc, NULL);

    mpz_set_ui(za, 1);
    mpz_mul_2exp(za, za, bits - 1);
    mpz_set_ui(zb, 1);
    mpz_mul_2exp(zb, zb, bits - 1);
    mpz_set_ui(zc, 1);
    mpz_mul_2exp(zc, zc, bits - 1);
    for (unsigned k = 0; k < 257; ++k) {
        unsigned long pa = (unsigned long)(
            ((uint64_t)(k + 11) * UINT64_C(0x9e3779b97f4a7c15)) %
            (bits - 1));
        unsigned long pb = (unsigned long)(
            ((uint64_t)(k + 37) * UINT64_C(0xd1b54a32d192ed03)) %
            (bits - 1));
        mpz_setbit(za, pa);
        mpz_setbit(zb, pb);
        mpz_setbit(zc, (pa + pb + 19u) % (bits - 1));
    }
    if (ppf_set_z(&a, za, PPF_RNDN, &context) == PPF_TERNARY_ERROR ||
        ppf_set_z(&b, zb, PPF_RNDN, &context) == PPF_TERNARY_ERROR ||
        ppf_set_z(&c, zc, PPF_RNDN, &context) == PPF_TERNARY_ERROR)
        die("input assignment");
    mpfr_set_z(ma, za, MPFR_RNDN);
    mpfr_set_z(mb, zb, MPFR_RNDN);
    mpfr_set_z(mc, zc, MPFR_RNDN);

    puts("bits,operation,method,method_workers,repeat,order,seconds");
    for (square = 0; square <= 1; ++square) {
        int reference_ternary = square
            ? mpfr_sqr(reference, ma, MPFR_RNDN)
            : mpfr_mul(reference, ma, mb, MPFR_RNDN);

        for (method_t method = METHOD_FULL; method < METHOD_MPFR; ++method) {
            ternaries[method] = run_ppf_method(method, square,
                                               &outputs[method], &a, &b,
                                               workers, &context);
            if (ppf_get_mpfr(exported, &outputs[method], MPFR_RNDN) ==
                    PPF_TERNARY_ERROR ||
                !mpfr_equal_p(exported, reference) ||
                !same_ternary_direction(ternaries[method], reference_ternary))
                die("warm-up correctness");
        }
        mpfr_set(mout, reference, MPFR_RNDN);

        for (int repeat = 1; repeat <= repetitions; ++repeat) {
            int order[METHOD_COUNT];
            shuffle(order, METHOD_COUNT, &random_state);
            for (int position = 0; position < METHOD_COUNT; ++position) {
                method_t method = (method_t)order[position];
                double start = now_seconds();
                for (int q = 0; q < inner; ++q) {
                    if (method == METHOD_MPFR) {
                        ternaries[method] = square
                            ? mpfr_sqr(mout, ma, MPFR_RNDN)
                            : mpfr_mul(mout, ma, mb, MPFR_RNDN);
                    } else {
                        ternaries[method] = run_ppf_method(
                            method, square, &outputs[method], &a, &b,
                            workers, &context);
                    }
                }
                double seconds = (now_seconds() - start) / (double)inner;

                if (method == METHOD_MPFR) {
                    if (!mpfr_equal_p(mout, reference) ||
                        !same_ternary_direction(ternaries[method],
                                                reference_ternary))
                        die("MPFR timed correctness");
                } else {
                    if (ppf_get_mpfr(exported, &outputs[method], MPFR_RNDN) ==
                            PPF_TERNARY_ERROR ||
                        !mpfr_equal_p(exported, reference) ||
                        !same_ternary_direction(ternaries[method],
                                                reference_ternary))
                        die("Polyphase timed correctness");
                }
                printf("%lu,%s,%s,%lu,%d,%d,%.12f\n",
                       bits, square ? "square" : "multiply",
                       method_names[method],
                       method == METHOD_FULL || method == METHOD_MPFR
                           ? 1UL : workers,
                       repeat, position + 1, seconds);
                fflush(stdout);
            }
        }
    }



    {
        const method_t methods[] = {
            METHOD_FULL, METHOD_RANK3, METHOD_RANK5,
            METHOD_AUTO, METHOD_MPFR
        };
        const unsigned method_count = sizeof(methods) / sizeof(methods[0]);
        int reference_ternary = mpfr_fma(reference, ma, mb, mc, MPFR_RNDN);
        for (unsigned index = 0; index < method_count - 1; ++index) {
            method_t method = methods[index];
            ternaries[method] = run_ppf_fma_method(
                method, &outputs[method], &a, &b, &c, workers, &context);
            if (ppf_get_mpfr(exported, &outputs[method], MPFR_RNDN) ==
                    PPF_TERNARY_ERROR ||
                !mpfr_equal_p(exported, reference) ||
                !same_ternary_direction(ternaries[method], reference_ternary))
                die("FMA warm-up correctness");
        }
        mpfr_set(mout, reference, MPFR_RNDN);
        for (int repeat = 1; repeat <= repetitions; ++repeat) {
            int order[5];
            shuffle(order, method_count, &random_state);
            for (unsigned position = 0; position < method_count; ++position) {
                method_t method = methods[order[position]];
                double start = now_seconds();
                for (int q = 0; q < inner; ++q) {
                    if (method == METHOD_MPFR)
                        ternaries[method] = mpfr_fma(
                            mout, ma, mb, mc, MPFR_RNDN);
                    else
                        ternaries[method] = run_ppf_fma_method(
                            method, &outputs[method], &a, &b, &c,
                            workers, &context);
                }
                double seconds = (now_seconds() - start) / (double)inner;
                if (method == METHOD_MPFR) {
                    if (!mpfr_equal_p(mout, reference) ||
                        !same_ternary_direction(ternaries[method],
                                                reference_ternary))
                        die("MPFR FMA timed correctness");
                } else {
                    if (ppf_get_mpfr(exported, &outputs[method], MPFR_RNDN) ==
                            PPF_TERNARY_ERROR ||
                        !mpfr_equal_p(exported, reference) ||
                        !same_ternary_direction(ternaries[method],
                                                reference_ternary))
                        die("Polyphase FMA timed correctness");
                }
                printf("%lu,fma,%s,%lu,%d,%u,%.12f\n",
                       bits, method_names[method],
                       method == METHOD_FULL || method == METHOD_MPFR
                           ? 1UL : workers,
                       repeat, position + 1, seconds);
                fflush(stdout);
            }
        }
    }
    mpz_clears(za, zb, zc, NULL);
    mpfr_clears(ma, mb, mc, mout, reference, exported, (mpfr_ptr)0);
    for (output_index = 0; output_index < METHOD_COUNT - 1; ++output_index)
        ppf_clear(&outputs[output_index]);
    ppf_clear(&a);
    ppf_clear(&b);
    ppf_clear(&c);
    ppf_context_clear(&context);
    return 0;
}
