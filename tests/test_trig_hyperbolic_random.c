/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

typedef int (*ppf_unary_fn)(ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_unary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
typedef int (*ppf_binary_fn)(ppf_t *, const ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_binary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
typedef int (*ppf_unary_u_fn)(ppf_t *, const ppf_t *, unsigned long, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_unary_u_fn)(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);

static uint64_t rng_state = UINT64_C(0x9e3779b97f4a7c15);
static uint64_t rng64(void) {
    uint64_t x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * UINT64_C(2685821657736338717);
}
static int signum(int x) { return (x > 0) - (x < 0); }

static int same_value(const ppf_t *p, mpfr_srcptr m) {
    mpfr_t got;
    int ok;
    mpfr_init2(got, mpfr_get_prec(m));
    CHECK(ppf_get_mpfr(got, p, MPFR_RNDN) != PPF_TERNARY_ERROR);
    if (mpfr_nan_p(m)) ok = mpfr_nan_p(got);
    else if (mpfr_inf_p(m)) ok = mpfr_inf_p(got) && mpfr_signbit(got) == mpfr_signbit(m);
    else if (mpfr_zero_p(m)) ok = mpfr_zero_p(got) && mpfr_signbit(got) == mpfr_signbit(m);
    else ok = mpfr_equal_p(got, m) != 0;
    if (!ok) mpfr_printf("got=%.*Rg ref=%.*Rg\n", 100, got, 100, m);
    mpfr_clear(got);
    return ok;
}

static void compare_unary(const char *name, ppf_unary_fn pf, mpfr_unary_fn mf,
                          const ppf_t *x, mpfr_srcptr mx, ppf_t *y, mpfr_ptr my,
                          ppf_rnd_t pr, mpfr_rnd_t mr, ppf_context_t *ctx) {
    int a, b;
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
    mpfr_clear_flags();
    a = pf(y, x, pr, ctx);
    b = mf(my, mx, mr);
    if (a == PPF_TERNARY_ERROR || !same_value(y, my) || signum(a) != signum(b)) {
        fprintf(stderr, "%s random mismatch p=%" PRIu64 " a=%d b=%d\n",
                name, (uint64_t)y->precision, a, b);
        exit(2);
    }
}

static void compare_unary_u(const char *name, ppf_unary_u_fn pf, mpfr_unary_u_fn mf,
                            const ppf_t *x, mpfr_srcptr mx, unsigned long u,
                            ppf_t *y, mpfr_ptr my, ppf_rnd_t pr, mpfr_rnd_t mr,
                            ppf_context_t *ctx) {
    int a, b;
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
    mpfr_clear_flags();
    a = pf(y, x, u, pr, ctx);
    b = mf(my, mx, u, mr);
    if (a == PPF_TERNARY_ERROR || !same_value(y, my) || signum(a) != signum(b)) {
        fprintf(stderr, "%s random mismatch u=%lu p=%" PRIu64 " a=%d b=%d\n",
                name, u, (uint64_t)y->precision, a, b);
        exit(3);
    }
}

static void compare_binary(const char *name, ppf_binary_fn pf, mpfr_binary_fn mf,
                           const ppf_t *a, const ppf_t *b,
                           mpfr_srcptr ma, mpfr_srcptr mb,
                           ppf_t *y, mpfr_ptr my,
                           ppf_rnd_t pr, mpfr_rnd_t mr, ppf_context_t *ctx) {
    int x, z;
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
    mpfr_clear_flags();
    x = pf(y, a, b, pr, ctx);
    z = mf(my, ma, mb, mr);
    if (x == PPF_TERNARY_ERROR || !same_value(y, my) || signum(x) != signum(z)) {
        fprintf(stderr, "%s random mismatch p=%" PRIu64 " a=%d b=%d\n",
                name, (uint64_t)y->precision, x, z);
        exit(4);
    }
}

int main(void) {
    static const ppf_rnd_t prnd[] = { PPF_RNDN, PPF_RNDU, PPF_RNDD, PPF_RNDZ, PPF_RNDA };
    static const mpfr_rnd_t mrnd[] = { MPFR_RNDN, MPFR_RNDU, MPFR_RNDD, MPFR_RNDZ, MPFR_RNDA };
    for (unsigned trial = 0; trial < 24; ++trial) {
        unsigned long p = 8UL + (unsigned long)(rng64() % 121UL);
        unsigned long ip = p + 17UL;
        long n = (long)(rng64() % 2000001UL) - 1000000L;
        long e = (long)(rng64() % 25UL) - 12L;
        unsigned ri = (unsigned)(rng64() % 5UL);
        unsigned long u = 1UL + (unsigned long)(rng64() % 97UL);
        ppf_context_t ctx;
        ppf_t x, z, y;
        mpfr_t mx, mz, my;
        CHECK(ppf_context_init(&ctx) == 0);
        CHECK(ppf_init2_ctx(&x, ip, &ctx) == 0);
        CHECK(ppf_init2_ctx(&z, ip, &ctx) == 0);
        CHECK(ppf_init2_ctx(&y, p, &ctx) == 0);
        mpfr_init2(mx, ip); mpfr_init2(mz, ip); mpfr_init2(my, p);

        (void)mpfr_set_si_2exp(mx, n, e, MPFR_RNDN);
        CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        compare_unary("sin", ppf_sin, mpfr_sin, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("cos", ppf_cos, mpfr_cos, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("tan", ppf_tan, mpfr_tan, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("atan", ppf_atan, mpfr_atan, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("sinh", ppf_sinh, mpfr_sinh, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("cosh", ppf_cosh, mpfr_cosh, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("tanh", ppf_tanh, mpfr_tanh, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("asinh", ppf_asinh, mpfr_asinh, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary_u("sinu", ppf_sinu, mpfr_sinu, &x, mx, u, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary_u("cosu", ppf_cosu, mpfr_cosu, &x, mx, u, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary_u("tanu", ppf_tanu, mpfr_tanu, &x, mx, u, &y, my, prnd[ri], mrnd[ri], &ctx);

        /* Domain-limited inverse inputs are exact dyadics in (-1,1). */
        (void)mpfr_set_si_2exp(mx, (long)(rng64() % 2000001UL) - 1000000L,
                               -20L, MPFR_RNDN);
        CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        compare_unary("asin", ppf_asin, mpfr_asin, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("acos", ppf_acos, mpfr_acos, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("atanh", ppf_atanh, mpfr_atanh, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("asinpi", ppf_asinpi, mpfr_asinpi, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary("acospi", ppf_acospi, mpfr_acospi, &x, mx, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary_u("asinu", ppf_asinu, mpfr_asinu, &x, mx, u, &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_unary_u("acosu", ppf_acosu, mpfr_acosu, &x, mx, u, &y, my, prnd[ri], mrnd[ri], &ctx);

        /* atan2 with two nonzero exact dyadic inputs. */
        (void)mpfr_set_si_2exp(mx, n == 0 ? 1L : n, -10L, MPFR_RNDN);
        (void)mpfr_set_si_2exp(mz, (long)(rng64() % 2000001UL) - 1000000L,
                               -10L, MPFR_RNDN);
        if (mpfr_zero_p(mz)) (void)mpfr_set_ui(mz, 1UL, MPFR_RNDN);
        CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        CHECK(ppf_set_mpfr(&z, mz, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        compare_binary("atan2", ppf_atan2, mpfr_atan2, &x, &z, mx, mz,
                       &y, my, prnd[ri], mrnd[ri], &ctx);
        compare_binary("atan2pi", ppf_atan2pi, mpfr_atan2pi, &x, &z, mx, mz,
                       &y, my, prnd[ri], mrnd[ri], &ctx);

        ppf_clear(&x); ppf_clear(&z); ppf_clear(&y); ppf_context_clear(&ctx);
        mpfr_clear(mx); mpfr_clear(mz); mpfr_clear(my);
    }
    puts("TRIG/HYPERBOLIC RANDOM TESTS PASSED");
    return 0;
}
