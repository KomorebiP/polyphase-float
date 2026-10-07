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

static void die(const char *message) {
    fprintf(stderr, "hotpath benchmark failure: %s\n", message);
    exit(2);
}

static int same_ternary(int a, int b) {
    return (a > 0) == (b > 0) && (a < 0) == (b < 0);
}

static void verify_value(const ppf_t *x, mpfr_srcptr expected,
                         mpfr_t scratch, const char *name) {
    if (ppf_get_mpfr(scratch, x, MPFR_RNDN) == PPF_TERNARY_ERROR ||
        !mpfr_equal_p(scratch, expected))
        die(name);
}

int main(int argc, char **argv) {
    const char *method = argc > 1 ? argv[1] : "ppf_set_z";
    unsigned long bits = argc > 2 ? strtoul(argv[2], NULL, 10) : 1000000UL;
    int inner = argc > 3 ? atoi(argv[3]) : 3;
    ppf_context_t ctx;
    ppf_t x, y, qx, out;
    mpfr_t mx, my, mqx, mout, reference, exported;
    mpz_t zx, zy, zout, mzout, den;
    mpq_t rational;
    char *ps = NULL, *ms = NULL;
    ppf_exp_t pe = 0;
    mpfr_exp_t me = 0;
    int pt = 0, mt = 0;
    int loops = inner;
    double started, elapsed;

    if (bits < 64) bits = 64;
    if (inner < 1) inner = 1;
    if (ppf_context_init(&ctx) != 0 ||
        ppf_init2_ctx(&x, bits, &ctx) != 0 ||
        ppf_init2_ctx(&y, bits, &ctx) != 0 ||
        ppf_init2_ctx(&qx, bits, &ctx) != 0 ||
        ppf_init2_ctx(&out, bits, &ctx) != 0)
        die("ppf initialization");
    mpfr_init2(mx, bits); mpfr_init2(my, bits); mpfr_init2(mqx, bits);
    mpfr_init2(mout, bits); mpfr_init2(reference, bits); mpfr_init2(exported, bits);
    mpz_inits(zx, zy, zout, mzout, den, NULL); mpq_init(rational);

    mpz_set_ui(zx, 1); mpz_mul_2exp(zx, zx, bits - 1);
    for (unsigned i = 0; i < 73 && i + 3 < bits; ++i)
        if ((i * 19u + 7u) % 13u < 6u)
            mpz_setbit(zx, (mp_bitcnt_t)(i * (bits / 79u + 1u)));
    mpz_set_ui(zy, 1); mpz_mul_2exp(zy, zy, bits - 2);
    for (unsigned i = 0; i < 61 && i + 5 < bits; ++i)
        if ((i * 11u + 3u) % 9u < 4u)
            mpz_setbit(zy, (mp_bitcnt_t)(i * (bits / 67u + 1u)));
    mpz_ui_pow_ui(den, 3, 257);
    mpq_set_num(rational, zx); mpq_set_den(rational, den); mpq_canonicalize(rational);

    (void)ppf_set_z(&x, zx, PPF_RNDN, &ctx);
    (void)ppf_set_z(&y, zy, PPF_RNDN, &ctx);
    (void)ppf_set_q(&qx, rational, PPF_RNDN, &ctx);
    (void)mpfr_set_z(mx, zx, MPFR_RNDN);
    (void)mpfr_set_z(my, zy, MPFR_RNDN);
    (void)mpfr_set_q(mqx, rational, MPFR_RNDN);

    /* Correctness gate for the selected operation.  The benchmark process
       executes both implementations once before timing either one. */
    if (strstr(method, "set_z")) {
        pt = ppf_set_z(&out, zx, PPF_RNDN, &ctx);
        mt = mpfr_set_z(reference, zx, MPFR_RNDN);
        verify_value(&out, reference, exported, "set_z value");
        if (!same_ternary(pt, mt)) die("set_z ternary");
    } else if (strstr(method, "get_z")) {
        pt = ppf_get_z(zout, &x, PPF_RNDN, &ctx);
        mt = mpfr_get_z(mzout, mx, MPFR_RNDN);
        if (mpz_cmp(zout, mzout) != 0 || !same_ternary(pt, mt))
            die("get_z");
    } else if (strstr(method, "get_str")) {
        ps = ppf_get_str(NULL, &pe, 10, 128, &qx, PPF_RNDN, &ctx);
        ms = mpfr_get_str(NULL, &me, 10, 128, mqx, MPFR_RNDN);
        if (!ps || !ms || pe != (ppf_exp_t)me || strcmp(ps, ms) != 0)
            die("get_str value");
        ppf_free_str(ps); mpfr_free_str(ms); ps = ms = NULL;
    } else if (strstr(method, "fmod")) {
        pt = ppf_fmod(&out, &x, &y, PPF_RNDN, &ctx);
        mt = mpfr_fmod(reference, mx, my, MPFR_RNDN);
        verify_value(&out, reference, exported, "fmod value");
        if (!same_ternary(pt, mt)) die("fmod ternary");
    } else if (strstr(method, "remainder")) {
        pt = ppf_remainder(&out, &x, &y, PPF_RNDN, &ctx);
        mt = mpfr_remainder(reference, mx, my, MPFR_RNDN);
        verify_value(&out, reference, exported, "remainder value");
        if (!same_ternary(pt, mt)) die("remainder ternary");
    } else if (strstr(method, "cbrt")) {
        pt = ppf_cbrt(&out, &x, PPF_RNDN, &ctx);
        mt = mpfr_cbrt(reference, mx, MPFR_RNDN);
        verify_value(&out, reference, exported, "cbrt value");
        if (!same_ternary(pt, mt)) die("cbrt ternary");
    } else if (strstr(method, "root7")) {
        pt = ppf_rootn_ui(&out, &x, 7UL, PPF_RNDN, &ctx);
        mt = mpfr_rootn_ui(reference, mx, 7UL, MPFR_RNDN);
        verify_value(&out, reference, exported, "root7 value");
        if (!same_ternary(pt, mt)) die("root7 ternary");
    } else if (strstr(method, "rec_sqrt")) {
        pt = ppf_rec_sqrt(&out, &x, PPF_RNDN, &ctx);
        mt = mpfr_rec_sqrt(reference, mx, MPFR_RNDN);
        verify_value(&out, reference, exported, "rec_sqrt value");
        if (!same_ternary(pt, mt)) die("rec_sqrt ternary");
    } else if (strstr(method, "hypot")) {
        pt = ppf_hypot(&out, &x, &y, PPF_RNDN, &ctx);
        mt = mpfr_hypot(reference, mx, my, MPFR_RNDN);
        verify_value(&out, reference, exported, "hypot value");
        if (!same_ternary(pt, mt)) die("hypot ternary");
    } else if (strstr(method, "div")) {
        pt = ppf_div(&out, &x, &y, PPF_RNDN, &ctx);
        mt = mpfr_div(reference, mx, my, MPFR_RNDN);
        verify_value(&out, reference, exported, "division value");
        if (!same_ternary(pt, mt)) die("division ternary");
    } else {
        die("unknown method during correctness gate");
    }

    if (!strcmp(method, "ppf_set_z") || !strcmp(method, "mpfr_set_z") ||
        !strcmp(method, "ppf_get_z") || !strcmp(method, "mpfr_get_z") ||
        !strcmp(method, "ppf_get_str") || !strcmp(method, "mpfr_get_str"))
        loops *= 16;
    else if (!strcmp(method, "ppf_fmod") || !strcmp(method, "mpfr_fmod") ||
             !strcmp(method, "ppf_remainder") || !strcmp(method, "mpfr_remainder") ||
             !strcmp(method, "ppf_div") || !strcmp(method, "mpfr_div"))
        loops *= 3;

    started = now_seconds();
    for (int i = 0; i < loops; ++i) {
        if (!strcmp(method, "ppf_set_z")) pt = ppf_set_z(&out, zx, PPF_RNDN, &ctx);
        else if (!strcmp(method, "mpfr_set_z")) mt = mpfr_set_z(mout, zx, MPFR_RNDN);
        else if (!strcmp(method, "ppf_get_z")) pt = ppf_get_z(zout, &x, PPF_RNDN, &ctx);
        else if (!strcmp(method, "mpfr_get_z")) mt = mpfr_get_z(mzout, mx, MPFR_RNDN);
        else if (!strcmp(method, "ppf_get_str")) {
            ps = ppf_get_str(NULL, &pe, 10, 128, &qx, PPF_RNDN, &ctx);
            if (!ps) die("timed ppf_get_str");
            ppf_free_str(ps); ps = NULL;
        } else if (!strcmp(method, "mpfr_get_str")) {
            ms = mpfr_get_str(NULL, &me, 10, 128, mqx, MPFR_RNDN);
            if (!ms) die("timed mpfr_get_str");
            mpfr_free_str(ms); ms = NULL;
        } else if (!strcmp(method, "ppf_fmod")) pt = ppf_fmod(&out, &x, &y, PPF_RNDN, &ctx);
        else if (!strcmp(method, "mpfr_fmod")) mt = mpfr_fmod(mout, mx, my, MPFR_RNDN);
        else if (!strcmp(method, "ppf_remainder")) pt = ppf_remainder(&out, &x, &y, PPF_RNDN, &ctx);
        else if (!strcmp(method, "mpfr_remainder")) mt = mpfr_remainder(mout, mx, my, MPFR_RNDN);
        else if (!strcmp(method, "ppf_cbrt")) pt = ppf_cbrt(&out, &x, PPF_RNDN, &ctx);
        else if (!strcmp(method, "mpfr_cbrt")) mt = mpfr_cbrt(mout, mx, MPFR_RNDN);
        else if (!strcmp(method, "ppf_root7")) pt = ppf_rootn_ui(&out, &x, 7UL, PPF_RNDN, &ctx);
        else if (!strcmp(method, "mpfr_root7")) mt = mpfr_rootn_ui(mout, mx, 7UL, MPFR_RNDN);
        else if (!strcmp(method, "ppf_rec_sqrt")) pt = ppf_rec_sqrt(&out, &x, PPF_RNDN, &ctx);
        else if (!strcmp(method, "ppf_rec_sqrt_3t")) pt = ppf_rec_sqrt_threads(&out, &x, PPF_RNDN, 3, &ctx);
        else if (!strcmp(method, "mpfr_rec_sqrt")) mt = mpfr_rec_sqrt(mout, mx, MPFR_RNDN);
        else if (!strcmp(method, "ppf_hypot")) pt = ppf_hypot(&out, &x, &y, PPF_RNDN, &ctx);
        else if (!strcmp(method, "mpfr_hypot")) mt = mpfr_hypot(mout, mx, my, MPFR_RNDN);
        else if (!strcmp(method, "ppf_div")) pt = ppf_div(&out, &x, &y, PPF_RNDN, &ctx);
        else if (!strcmp(method, "mpfr_div")) mt = mpfr_div(mout, mx, my, MPFR_RNDN);
        else die("unknown method");
    }
    elapsed = (now_seconds() - started) / (double)loops;

    /* Post-timing verification of the chosen method. */
    if (!strcmp(method, "ppf_set_z")) {
        verify_value(&out, mx, exported, "post-timing set_z");
    } else if (!strcmp(method, "ppf_get_z")) {
        if (mpz_cmp(zout, zx) != 0) die("post-timing get_z");
    } else if (!strncmp(method, "ppf_", 4) && strcmp(method, "ppf_get_str")) {
        const char *op = method + 4;
        if (!strcmp(op, "fmod")) mpfr_fmod(reference, mx, my, MPFR_RNDN);
        else if (!strcmp(op, "remainder")) mpfr_remainder(reference, mx, my, MPFR_RNDN);
        else if (!strcmp(op, "cbrt")) mpfr_cbrt(reference, mx, MPFR_RNDN);
        else if (!strcmp(op, "root7")) mpfr_rootn_ui(reference, mx, 7UL, MPFR_RNDN);
        else if (!strcmp(op, "rec_sqrt") || !strcmp(op, "rec_sqrt_3t")) mpfr_rec_sqrt(reference, mx, MPFR_RNDN);
        else if (!strcmp(op, "hypot")) mpfr_hypot(reference, mx, my, MPFR_RNDN);
        else if (!strcmp(op, "div")) mpfr_div(reference, mx, my, MPFR_RNDN);
        verify_value(&out, reference, exported, "post-timing ppf value");
    }

    printf("{\"method\":\"%s\",\"bits\":%lu,\"loops\":%d,"
           "\"seconds_per_call\":%.12f,\"ternary\":%d}\n",
           method, bits, loops, elapsed,
           !strncmp(method, "ppf_", 4) ? pt : mt);

    mpq_clear(rational); mpz_clears(zx, zy, zout, mzout, den, NULL);
    mpfr_clears(mx, my, mqx, mout, reference, exported, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&y); ppf_clear(&qx); ppf_clear(&out);
    ppf_context_clear(&ctx);
    return 0;
}
