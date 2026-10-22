/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "polyphase_float.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

/* This bounds GMP work for these fixed-precision conversions. It does not
   impose a recoverable allocation-failure contract on GMP. */
static size_t largest_request;
static void check_request(size_t bytes) {
    if (bytes > largest_request) largest_request = bytes;
    CHECK(bytes <= 65536u);
}
static void *bounded_malloc(size_t bytes) {
    void *ptr; check_request(bytes); ptr = malloc(bytes); CHECK(ptr); return ptr;
}
static void *bounded_realloc(void *ptr, size_t old, size_t bytes) {
    void *next; (void)old; check_request(bytes);
    next = realloc(ptr, bytes); CHECK(next); return next;
}
static void bounded_free(void *ptr, size_t bytes) { (void)bytes; free(ptr); }

static long double convert(int type, const ppf_t *x, ppf_rnd_t rnd,
                            ppf_context_t *ctx) {
    if (type == 0) return (long double)ppf_get_flt(x, rnd, ctx);
    if (type == 1) return (long double)ppf_get_d(x, rnd, ctx);
    return ppf_get_ld(x, rnd, ctx);
}

static void check_value(long double actual, long double expected, int sign) {
    CHECK(actual == expected);
    if (actual == 0.0L) CHECK((signbit(actual) != 0) == (sign < 0));
}

int main(void) {
    const long double minimum[] = {
        (long double)nextafterf(0.0f, 1.0f),
        (long double)nextafter(0.0, 1.0), nextafterl(0.0L, 1.0L)
    };
    const int unit[] = {FLT_MIN_EXP - FLT_MANT_DIG,
                       DBL_MIN_EXP - DBL_MANT_DIG,
                       LDBL_MIN_EXP - LDBL_MANT_DIG};
    const ppf_exp_t tiny[] = {-1000000000, PPF_EMIN_MIN};
    ppf_context_t ctx; ppf_t x;
    mp_set_memory_functions(bounded_malloc, bounded_realloc, bounded_free);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_context_set_range(&ctx, PPF_EMIN_MIN, PPF_EMAX_MAX) == 0);
    CHECK(ppf_init2_ctx(&x, 128, &ctx) == 0);
    for (int sign = -1; sign <= 1; sign += 2) {
        for (size_t i = 0; i < sizeof(tiny) / sizeof(tiny[0]); ++i) {
            CHECK(ppf_set_si(&x, sign, PPF_RNDN, &ctx) == 0);
            CHECK(ppf_set_exp(&x, tiny[i], &ctx) == 0);
            for (int r = PPF_RNDN; r <= PPF_RNDF; ++r) {
                int away = r == PPF_RNDA || (r == PPF_RNDU && sign > 0) ||
                           (r == PPF_RNDD && sign < 0);
                for (int type = 0; type < 3; ++type)
                    check_value(convert(type, &x, (ppf_rnd_t)r, &ctx),
                                away ? sign * minimum[type] : 0.0L, sign);
            }
        }
        /* Halfway subnormal values exercise ties-to-even as well as both
           directed signs, without using the conversion under test as oracle. */
        for (int type = 0; type < 3; ++type) {
            for (unsigned long numerator = 1; numerator <= 5; numerator += 2) {
                CHECK(ppf_set_si_2exp(&x, sign * (long)numerator,
                                     unit[type] - 1, PPF_RNDN, &ctx) == 0);
                for (int r = PPF_RNDN; r <= PPF_RNDF; ++r) {
                    unsigned long q = numerator / 2;
                    int away = r == PPF_RNDA || (r == PPF_RNDU && sign > 0) ||
                               (r == PPF_RNDD && sign < 0);
                    if (r == PPF_RNDN) q += q & 1u;
                    else if (away) ++q;
                    check_value(convert(type, &x, (ppf_rnd_t)r, &ctx),
                                sign * (long double)q * minimum[type], sign);
                }
            }
        }
    }
    ppf_clear(&x); ppf_context_clear(&ctx); ppf_thread_cleanup();
    printf("Fixed-width conversion rounding passed; largest GMP request: %zu bytes\n",
           largest_request);
    return 0;
}
