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
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static int same_mpfr(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_zero_p(a) || mpfr_zero_p(b))
        return mpfr_zero_p(a) && mpfr_zero_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    return mpfr_equal_p(a, b) != 0;
}

static int check_uniform(const char *kind, unsigned long bits) {
    gmp_randstate_t a, b;
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t y, out;
    int ok = 0;
    gmp_randinit_mt(a); gmp_randinit_mt(b);
    gmp_randseed_ui(a, 0x7123UL); gmp_randseed_ui(b, 0x7123UL);
    if (ppf_context_init(&ctx) != 0 || ppf_init2_ctx(&x, bits, &ctx) != 0)
        goto done_states;
    mpfr_inits2(bits, y, out, (mpfr_ptr)0);
    if (strcmp(kind, "urandomb") == 0) {
        if (ppf_urandomb(&x, b) != mpfr_urandomb(y, a)) goto done_values;
    } else {
        int mt = mpfr_urandom(y, a, MPFR_RNDN);
        int pt = ppf_urandom(&x, b, PPF_RNDN, &ctx);
        if ((mt > 0) - (mt < 0) != (pt > 0) - (pt < 0)) goto done_values;
    }
    if (ppf_get_mpfr(out, &x, MPFR_RNDN) != 0 || !same_mpfr(y, out))
        goto done_values;
    ok = 1;
done_values:
    mpfr_clears(y, out, (mpfr_ptr)0);
    ppf_clear(&x); ppf_context_clear(&ctx);
done_states:
    gmp_randclear(a); gmp_randclear(b);
    return ok;
}

static int run_uniform(const char *method, unsigned long bits,
                       unsigned long loops, double *elapsed) {
    gmp_randstate_t state;
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t y;
    double t0;
    int ppf_method = strncmp(method, "ppf_", 4) == 0;
    int continuous = strstr(method, "urandom") != NULL &&
                     strstr(method, "urandomb") == NULL;
    if (!check_uniform(continuous ? "urandom" : "urandomb", bits))
        return -1;
    gmp_randinit_mt(state); gmp_randseed_ui(state, 0x9d53UL);
    if (ppf_method) {
        if (ppf_context_init(&ctx) != 0 || ppf_init2_ctx(&x, bits, &ctx) != 0)
            return -1;
        t0 = now_seconds();
        for (unsigned long i = 0; i < loops; ++i) {
            int r = continuous ? ppf_urandom(&x, state, PPF_RNDN, &ctx)
                               : ppf_urandomb(&x, state);
            if (r == PPF_TERNARY_ERROR) return -1;
        }
        *elapsed = now_seconds() - t0;
        ppf_clear(&x); ppf_context_clear(&ctx);
    } else {
        mpfr_init2(y, bits);
        t0 = now_seconds();
        for (unsigned long i = 0; i < loops; ++i) {
            if (continuous) (void)mpfr_urandom(y, state, MPFR_RNDN);
            else (void)mpfr_urandomb(y, state);
        }
        *elapsed = now_seconds() - t0;
        mpfr_clear(y);
    }
    gmp_randclear(state);
    return 0;
}

static int run_can_round(const char *method, unsigned long bits,
                         unsigned long loops, double *elapsed) {
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t y;
    mpz_t z;
    double t0;
    int ppf_method = strncmp(method, "ppf_", 4) == 0;
    if (ppf_context_init(&ctx) != 0 || ppf_init2_ctx(&x, bits, &ctx) != 0)
        return -1;
    mpfr_init2(y, bits); mpz_init(z);
    mpz_set_ui(z, 1234567UL); mpz_mul_2exp(z, z, bits > 40 ? bits - 40 : 0);
    (void)mpfr_set_z_2exp(y, z, -20, MPFR_RNDN);
    if (ppf_import_mpfr_exact(&x, y, &ctx) != 0) return -1;
    if ((mpfr_can_round(y, (mpfr_exp_t)bits + 20, MPFR_RNDN,
                        MPFR_RNDN, bits / 2 + 2) != 0) !=
        (ppf_can_round(&x, (ppf_exp_t)bits + 20, PPF_RNDN,
                       PPF_RNDN, bits / 2 + 2) != 0)) return -1;
    t0 = now_seconds();
    for (unsigned long i = 0; i < loops; ++i) {
        if (ppf_method)
            (void)ppf_can_round(&x, (ppf_exp_t)bits + 20,
                                PPF_RNDN, PPF_RNDN, bits / 2 + 2);
        else
            (void)mpfr_can_round(y, (mpfr_exp_t)bits + 20,
                                 MPFR_RNDN, MPFR_RNDN, bits / 2 + 2);
    }
    *elapsed = now_seconds() - t0;
    mpz_clear(z); mpfr_clear(y); ppf_clear(&x); ppf_context_clear(&ctx);
    return 0;
}

int main(int argc, char **argv) {
    const char *method;
    unsigned long bits, loops;
    double elapsed = 0.0;
    int rc;
    if (argc != 4) {
        fprintf(stderr, "usage: %s METHOD BITS LOOPS\n", argv[0]);
        return 2;
    }
    method = argv[1]; bits = strtoul(argv[2], NULL, 10);
    loops = strtoul(argv[3], NULL, 10);
    if (bits < 2 || loops == 0) return 2;
    if (strstr(method, "urandom") != NULL)
        rc = run_uniform(method, bits, loops, &elapsed);
    else if (strstr(method, "can_round") != NULL)
        rc = run_can_round(method, bits, loops, &elapsed);
    else rc = -1;
    if (rc != 0) return 1;
    printf("{\"method\":\"%s\",\"bits\":%lu,\"loops\":%lu,"
           "\"seconds\":%.12f,\"seconds_per_call\":%.12g}\n",
           method, bits, loops, elapsed, elapsed / (double)loops);
    return 0;
}
