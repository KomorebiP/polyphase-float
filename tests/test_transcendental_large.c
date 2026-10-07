/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(C) do { if (!(C)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #C); \
    exit(1); \
} } while (0)

typedef int (*ppf_unary_fn)(ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_unary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
typedef int (*ppf_binary_fn)(ppf_t *, const ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_binary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);

typedef struct {
    int underflow, overflow, divby0, nan, inexact, erange;
} flags_t;

static const ppf_rnd_t ppf_roundings[] = {
    PPF_RNDN, PPF_RNDZ, PPF_RNDU, PPF_RNDD, PPF_RNDA
};
static const mpfr_rnd_t mpfr_roundings[] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA
};

static int signum(int x) { return (x > 0) - (x < 0); }

static flags_t read_mpfr_flags(void) {
    flags_t f;
    f.underflow = mpfr_underflow_p() != 0;
    f.overflow = mpfr_overflow_p() != 0;
    f.divby0 = mpfr_divby0_p() != 0;
    f.nan = mpfr_nanflag_p() != 0;
    f.inexact = mpfr_inexflag_p() != 0;
    f.erange = mpfr_erangeflag_p() != 0;
    return f;
}

static int same_flags(const ppf_context_t *ctx, flags_t f) {
    return f.underflow == !!(ctx->flags & PPF_FLAG_UNDERFLOW) &&
           f.overflow == !!(ctx->flags & PPF_FLAG_OVERFLOW) &&
           f.divby0 == !!(ctx->flags & PPF_FLAG_DIVBY0) &&
           f.nan == !!(ctx->flags & PPF_FLAG_NAN) &&
           f.inexact == !!(ctx->flags & PPF_FLAG_INEXACT) &&
           f.erange == !!(ctx->flags & PPF_FLAG_ERANGE);
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
    if (!ok)
        mpfr_printf("mismatch got=%.*Rg ref=%.*Rg\n", 80, got, 80, reference);
    mpfr_clear(got);
    return ok;
}

static void unary_case(const char *name, ppf_unary_fn pfun, mpfr_unary_fn mfun,
                       const char *input, unsigned long bits) {
    ppf_context_t ctx;
    ppf_t x, result;
    mpfr_t mx, reference;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, bits + 19u, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, bits, &ctx) == 0);
    mpfr_init2(mx, bits + 19u);
    mpfr_init2(reference, bits);
    CHECK(mpfr_set_str(mx, input, 0, MPFR_RNDN) == 0);
    CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);

    for (size_t i = 0; i < sizeof(ppf_roundings) / sizeof(ppf_roundings[0]); ++i) {
        int pt, mt;
        flags_t mf;
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        mpfr_clear_flags();
        pt = pfun(&result, &x, ppf_roundings[i], &ctx);
        mt = mfun(reference, mx, mpfr_roundings[i]);
        mf = read_mpfr_flags();
        if (pt == PPF_TERNARY_ERROR || !same_value(&result, reference) ||
            signum(pt) != signum(mt) || !same_flags(&ctx, mf)) {
            fprintf(stderr, "%s input=%s bits=%lu rnd=%zu pt=%d mt=%d\n",
                    name, input, bits, i, pt, mt);
            exit(1);
        }
    }

    /* A large same-precision alias case exercises the fixed-point fast path
       without allowing a separate source object to hide an overlap defect. */
    ppf_clear(&x);
    mpfr_set_prec(mx, bits);
    CHECK(mpfr_set_str(mx, input, 0, MPFR_RNDN) == 0);
    CHECK(ppf_init2_ctx(&x, bits, &ctx) == 0);
    CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
    CHECK(pfun(&x, &x, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
    (void)mfun(reference, mx, MPFR_RNDN);
    CHECK(same_value(&x, reference));

    ppf_clear(&x);
    ppf_clear(&result);
    ppf_context_clear(&ctx);
    mpfr_clear(mx);
    mpfr_clear(reference);
}

static void binary_case(const char *name, ppf_binary_fn pfun, mpfr_binary_fn mfun,
                        const char *left, const char *right, unsigned long bits) {
    ppf_context_t ctx;
    ppf_t x, y, result;
    mpfr_t mx, my, reference;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, bits + 19u, &ctx) == 0);
    CHECK(ppf_init2_ctx(&y, bits + 23u, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, bits, &ctx) == 0);
    mpfr_init2(mx, bits + 19u);
    mpfr_init2(my, bits + 23u);
    mpfr_init2(reference, bits);
    CHECK(mpfr_set_str(mx, left, 0, MPFR_RNDN) == 0);
    CHECK(mpfr_set_str(my, right, 0, MPFR_RNDN) == 0);
    CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
    CHECK(ppf_set_mpfr(&y, my, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);

    for (size_t i = 0; i < sizeof(ppf_roundings) / sizeof(ppf_roundings[0]); ++i) {
        int pt, mt;
        flags_t mf;
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        mpfr_clear_flags();
        pt = pfun(&result, &x, &y, ppf_roundings[i], &ctx);
        mt = mfun(reference, mx, my, mpfr_roundings[i]);
        mf = read_mpfr_flags();
        if (pt == PPF_TERNARY_ERROR || !same_value(&result, reference) ||
            signum(pt) != signum(mt) || !same_flags(&ctx, mf)) {
            fprintf(stderr, "%s left=%s right=%s bits=%lu rnd=%zu pt=%d mt=%d\n",
                    name, left, right, bits, i, pt, mt);
            exit(1);
        }
    }

    ppf_clear(&x);
    ppf_clear(&y);
    ppf_clear(&result);
    ppf_context_clear(&ctx);
    mpfr_clear(mx);
    mpfr_clear(my);
    mpfr_clear(reference);
}

int main(void) {
    const unsigned long precisions[] = {1000UL, 5000UL};
    for (size_t i = 0; i < sizeof(precisions) / sizeof(precisions[0]); ++i) {
        unsigned long p = precisions[i];
        unary_case("log", ppf_log, mpfr_log, "1.25", p);
        unary_case("log2", ppf_log2, mpfr_log2, "1.25", p);
        unary_case("log10", ppf_log10, mpfr_log10, "1.25", p);
        unary_case("log1p", ppf_log1p, mpfr_log1p, "0.125", p);
        unary_case("log2p1", ppf_log2p1, mpfr_log2p1, "0.125", p);
        unary_case("log10p1", ppf_log10p1, mpfr_log10p1, "0.125", p);
        unary_case("exp", ppf_exp, mpfr_exp, "0.125", p);
        unary_case("expm1", ppf_expm1, mpfr_expm1, "0.125", p);
        unary_case("exp2", ppf_exp2, mpfr_exp2, "0.125", p);
        unary_case("exp10", ppf_exp10, mpfr_exp10, "0.125", p);
        unary_case("exp2m1", ppf_exp2m1, mpfr_exp2m1, "0.125", p);
        unary_case("exp10m1", ppf_exp10m1, mpfr_exp10m1, "0.125", p);
        binary_case("pow", ppf_pow, mpfr_pow, "1.25", "3.25", p);
        binary_case("powr", ppf_powr, mpfr_powr, "1.25", "3.25", p);
    }
    puts("large-precision transcendental differential passed");
    return 0;
}
