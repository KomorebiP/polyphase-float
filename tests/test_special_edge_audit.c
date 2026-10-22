/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); \
} } while (0)

typedef int (*ppf_unary_fn)(ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_unary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);

static unsigned long cases;
static const ppf_rnd_t pr[] = {PPF_RNDN, PPF_RNDZ, PPF_RNDU, PPF_RNDD, PPF_RNDA};
static const mpfr_rnd_t mr[] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static int signum(int x) { return (x > 0) - (x < 0); }

static int same(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b)) return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_inf_p(a) || mpfr_inf_p(b))
        return mpfr_inf_p(a) && mpfr_inf_p(b) && mpfr_signbit(a) == mpfr_signbit(b);
    if (!mpfr_equal_p(a, b)) return 0;
    return !mpfr_zero_p(a) || mpfr_signbit(a) == mpfr_signbit(b);
}

static ppf_flags_t oracle_flags(void) {
    ppf_flags_t flags = 0;
    if (mpfr_underflow_p()) flags |= PPF_FLAG_UNDERFLOW;
    if (mpfr_overflow_p()) flags |= PPF_FLAG_OVERFLOW;
    if (mpfr_divby0_p()) flags |= PPF_FLAG_DIVBY0;
    if (mpfr_nanflag_p()) flags |= PPF_FLAG_NAN;
    if (mpfr_inexflag_p()) flags |= PPF_FLAG_INEXACT;
    if (mpfr_erangeflag_p()) flags |= PPF_FLAG_ERANGE;
    return flags;
}

static void unary_case(const char *name, ppf_unary_fn pf, mpfr_unary_fn mf,
                       const char *text, unsigned precision, int alias) {
    ppf_context_t ctx;
    ppf_t x, result;
    mpfr_t mx, expected, actual;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, alias ? precision : precision + 19, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, precision, &ctx) == 0);
    mpfr_init2(mx, alias ? precision : precision + 19);
    mpfr_inits2(precision, expected, actual, (mpfr_ptr)0);
    for (unsigned mode = 0; mode < 5; ++mode) {
        CHECK(mpfr_set_str(mx, text, 0, MPFR_RNDN) == 0);
        CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        mpfr_clear_flags();
        int a = pf(alias ? &x : &result, &x, pr[mode], &ctx);
        int b = mf(expected, mx, mr[mode]);
        ppf_flags_t flags = oracle_flags();
        CHECK(ppf_get_mpfr(actual, alias ? &x : &result, MPFR_RNDN) != PPF_TERNARY_ERROR);
        if (a == PPF_TERNARY_ERROR || signum(a) != signum(b) ||
            !same(actual, expected) || ctx.flags != flags) {
            fprintf(stderr, "%s(%s) p=%u mode=%u alias=%d: ternary %d/%d flags %#x/%#x\n",
                    name, text, precision, mode, alias, a, b,
                    (unsigned)ctx.flags, (unsigned)flags);
            mpfr_printf("actual=%Ra expected=%Ra\n", actual, expected);
            exit(1);
        }
        ++cases;
    }
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clear(mx); mpfr_clears(expected, actual, (mpfr_ptr)0);
}

static void lgamma_case(const char *text, unsigned precision, int alias) {
    ppf_context_t ctx; ppf_t x, result; mpfr_t mx, expected, actual;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, alias ? precision : precision + 19, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, precision, &ctx) == 0);
    mpfr_init2(mx, alias ? precision : precision + 19);
    mpfr_inits2(precision, expected, actual, (mpfr_ptr)0);
    for (unsigned mode = 0; mode < 5; ++mode) {
        int signp = 99, signs = 99;
        CHECK(mpfr_set_str(mx, text, 0, MPFR_RNDN) == 0);
        CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); mpfr_clear_flags();
        int a = ppf_lgamma(alias ? &x : &result, &signp, &x, pr[mode], &ctx);
        int b = mpfr_lgamma(expected, &signs, mx, mr[mode]);
        ppf_flags_t flags = oracle_flags();
        CHECK(ppf_get_mpfr(actual, alias ? &x : &result, MPFR_RNDN) != PPF_TERNARY_ERROR);
        if (a == PPF_TERNARY_ERROR || signum(a) != signum(b) || !same(actual, expected) ||
            ctx.flags != flags || signp != signs) {
            fprintf(stderr, "lgamma(%s) p=%u mode=%u alias=%d: ternary %d/%d sign %d/%d flags %#x/%#x\n",
                    text, precision, mode, alias, a, b, signp, signs,
                    (unsigned)ctx.flags, (unsigned)flags);
            exit(1);
        }
        ++cases;
    }
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clear(mx); mpfr_clears(expected, actual, (mpfr_ptr)0);
}

static void beta_case(const char *at, const char *bt, unsigned precision, int alias) {
    ppf_context_t ctx; ppf_t a, b, result; mpfr_t ma, mb, expected, actual;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, alias ? precision : precision + 19, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, precision + 19, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, precision, &ctx) == 0);
    mpfr_init2(ma, alias ? precision : precision + 19); mpfr_init2(mb, precision + 19);
    mpfr_inits2(precision, expected, actual, (mpfr_ptr)0);
    for (unsigned mode = 0; mode < 5; ++mode) {
        CHECK(mpfr_set_str(ma, at, 0, MPFR_RNDN) == 0);
        CHECK(mpfr_set_str(mb, bt, 0, MPFR_RNDN) == 0);
        CHECK(ppf_set_mpfr(&a, ma, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        CHECK(ppf_set_mpfr(&b, mb, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); mpfr_clear_flags();
        int p = ppf_beta(alias ? &a : &result, &a, &b, pr[mode], &ctx);
        int m = mpfr_beta(expected, ma, mb, mr[mode]);
        ppf_flags_t flags = oracle_flags();
        CHECK(ppf_get_mpfr(actual, alias ? &a : &result, MPFR_RNDN) != PPF_TERNARY_ERROR);
        if (p == PPF_TERNARY_ERROR || signum(p) != signum(m) || !same(actual, expected) ||
            ctx.flags != flags) {
            fprintf(stderr, "beta(%s,%s) p=%u mode=%u alias=%d: ternary %d/%d flags %#x/%#x\n",
                    at, bt, precision, mode, alias, p, m, (unsigned)ctx.flags, (unsigned)flags);
            mpfr_printf("actual=%Ra expected=%Ra\n", actual, expected); exit(1);
        }
        ++cases;
    }
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clear(ma); mpfr_clear(mb); mpfr_clears(expected, actual, (mpfr_ptr)0);
}

static void high_input_precision_case(const char *name, ppf_unary_fn pf,
                                      mpfr_unary_fn mf, long base, int delta_sign) {
    ppf_context_t ctx; ppf_t x, result; mpfr_t mx, delta, expected, actual;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 512, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 53, &ctx) == 0);
    mpfr_inits2(512, mx, delta, (mpfr_ptr)0);
    mpfr_inits2(53, expected, actual, (mpfr_ptr)0);
    (void)mpfr_set_si(mx, base, MPFR_RNDN);
    (void)mpfr_set_si(delta, delta_sign, MPFR_RNDN);
    (void)mpfr_mul_2si(delta, delta, -400, MPFR_RNDN);
    CHECK(mpfr_add(mx, mx, delta, MPFR_RNDN) == 0);
    CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) == 0);
    for (unsigned mode = 0; mode < 5; ++mode) {
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); mpfr_clear_flags();
        int p = pf(&result, &x, pr[mode], &ctx);
        int m = mf(expected, mx, mr[mode]);
        ppf_flags_t flags = oracle_flags();
        CHECK(ppf_get_mpfr(actual, &result, MPFR_RNDN) != PPF_TERNARY_ERROR);
        if (p == PPF_TERNARY_ERROR || signum(p) != signum(m) || !same(actual, expected) ||
            ctx.flags != flags) {
            fprintf(stderr, "%s(%ld%+d*2^-400) mode=%u: ternary %d/%d flags %#x/%#x\n",
                    name, base, delta_sign, mode, p, m, (unsigned)ctx.flags, (unsigned)flags);
            mpfr_printf("actual=%Ra expected=%Ra\n", actual, expected); exit(1);
        }
        ++cases;
    }
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clears(mx, delta, expected, actual, (mpfr_ptr)0);
}

static void order_case(long n, const char *text, int second_kind) {
    ppf_context_t ctx; ppf_t x, result; mpfr_t mx, expected, actual;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 80, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 53, &ctx) == 0);
    mpfr_init2(mx, 80); mpfr_inits2(53, expected, actual, (mpfr_ptr)0);
    for (unsigned mode = 0; mode < 5; ++mode) {
        CHECK(mpfr_set_str(mx, text, 0, MPFR_RNDN) == 0);
        CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL); mpfr_clear_flags();
        int a = second_kind ? ppf_yn(&result, n, &x, pr[mode], &ctx)
                            : ppf_jn(&result, n, &x, pr[mode], &ctx);
        int b = second_kind ? mpfr_yn(expected, n, mx, mr[mode])
                            : mpfr_jn(expected, n, mx, mr[mode]);
        ppf_flags_t flags = oracle_flags();
        CHECK(ppf_get_mpfr(actual, &result, MPFR_RNDN) != PPF_TERNARY_ERROR);
        if (a == PPF_TERNARY_ERROR || signum(a) != signum(b) || !same(actual, expected) ||
            ctx.flags != flags) {
            fprintf(stderr, "%sn(%ld,%s) mode=%u: ternary %d/%d flags %#x/%#x\n",
                    second_kind ? "y" : "j", n, text, mode, a, b,
                    (unsigned)ctx.flags, (unsigned)flags);
            exit(1);
        }
        ++cases;
    }
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clear(mx); mpfr_clears(expected, actual, (mpfr_ptr)0);
}

static void bounded_order_case(long order, int second_kind) {
    ppf_context_t ctx; ppf_t x, result;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 53, &ctx) == 0);
    CHECK(ppf_set_ui(&x, 2, PPF_RNDN, &ctx) == 0);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    int rc = second_kind ? ppf_yn(&result, order, &x, PPF_RNDN, &ctx)
                         : ppf_jn(&result, order, &x, PPF_RNDN, &ctx);
    CHECK(rc == PPF_TERNARY_ERROR);
    CHECK(ppf_nan_p(&result));
    CHECK(ctx.flags == PPF_FLAG_ERANGE);
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
}

int main(int argc, char **argv) {
    const unsigned precisions[] = {2, 24, 53, 113};
    const char *gamma_inputs[] = {"1", "2", "3", "4", "10", "32", "80"};
    const char *log_inputs[] = {"nan", "inf", "-inf", "0", "-0", "-1", "-2", "1", "2", "3"};
    const char *beta_inputs[][2] = {
        {"1", "1"}, {"1", "2"}, {"1", "0.5"}, {"2", "1"}, {"2", "2"},
        {"3", "4"}, {"10", "32"}, {"1", "-0.5"}, {"0", "0"}, {"-0", "-0"},
        {"0", "-0"}, {"-0", "0"}, {"0", "2"}, {"-0", "2"}, {"inf", "inf"},
        {"inf", "2"}, {"inf", "-0.5"}, {"inf", "0"}, {"-inf", "2"},
        {"-inf", "3"}, {"-inf", "0.5"}, {"-inf", "inf"}, {"-inf", "0"}
    };
    /* The optional single-family selectors support focused failure replay. */
    int family = argc > 1 ? atoi(argv[1]) : 0;
    for (unsigned p = 0; p < sizeof(precisions) / sizeof(precisions[0]); ++p) {
        if (!family || family == 1)
            for (unsigned j = 0; j < sizeof(gamma_inputs) / sizeof(gamma_inputs[0]); ++j)
                unary_case("gamma", ppf_gamma, mpfr_gamma, gamma_inputs[j], precisions[p], 0);
        if (!family || family == 2)
            for (unsigned j = 0; j < sizeof(log_inputs) / sizeof(log_inputs[0]); ++j) {
                unary_case("lngamma", ppf_lngamma, mpfr_lngamma, log_inputs[j], precisions[p], 0);
                lgamma_case(log_inputs[j], precisions[p], 0);
            }
        if (!family || family == 4)
            for (unsigned j = 0; j < sizeof(beta_inputs) / sizeof(beta_inputs[0]); ++j)
                beta_case(beta_inputs[j][0], beta_inputs[j][1], precisions[p], 0);
    }
    if (!family || family == 1) unary_case("gamma", ppf_gamma, mpfr_gamma, "10", 53, 1);
    if (!family || family == 2) { lgamma_case("1", 53, 1); lgamma_case("-0", 53, 1); }
    if (!family || family == 4) { beta_case("1", "1", 53, 1); beta_case("3", "4", 53, 1); }
    if (!family || family == 5)
        for (int sign = -1; sign <= 1; sign += 2) {
            high_input_precision_case("gamma", ppf_gamma, mpfr_gamma, 1, sign);
            high_input_precision_case("gamma", ppf_gamma, mpfr_gamma, -1, sign);
            high_input_precision_case("lngamma", ppf_lngamma, mpfr_lngamma, 1, sign);
            high_input_precision_case("digamma", ppf_digamma, mpfr_digamma, -1, sign);
            high_input_precision_case("li2", ppf_li2, mpfr_li2, 1, sign);
            high_input_precision_case("zeta", ppf_zeta, mpfr_zeta, -2, sign);
        }
    if (!family || family == 3) {
        const long orders[] = {LONG_MIN, -3, -2, -1, 0, 1, 2, 3, LONG_MAX};
        const char *inputs[] = {"0", "-0", "inf", "-inf"};
        for (unsigned i = 0; i < sizeof(orders) / sizeof(orders[0]); ++i)
            for (unsigned j = 0; j < sizeof(inputs) / sizeof(inputs[0]); ++j) {
                order_case(orders[i], inputs[j], 0); order_case(orders[i], inputs[j], 1);
            }
        order_case(50, "2", 0); order_case(50, "-2", 0); order_case(-51, "-2", 0);
        const long unsupported[] = {LONG_MIN, -100001, 100001, LONG_MAX};
        for (unsigned i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
            bounded_order_case(unsupported[i], 0); bounded_order_case(unsupported[i], 1);
        }
    }
    ppf_free_constant_caches(); mpfr_mp_memory_cleanup();
    printf("special edge audit passed: %lu oracle comparisons\n", cases);
    return 0;
}
