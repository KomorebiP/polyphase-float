/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); \
} } while (0)

typedef int (*ppf_unary_fn)(ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_unary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
typedef int (*ppf_binary_fn)(ppf_t *, const ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*mpfr_binary_fn)(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);

static int same_mpfr(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_inf_p(a) || mpfr_inf_p(b))
        return mpfr_inf_p(a) && mpfr_inf_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    if (!mpfr_equal_p(a, b)) return 0;
    return !mpfr_zero_p(a) || mpfr_signbit(a) == mpfr_signbit(b);
}

static void set_input(ppf_t *p, mpfr_ptr m, const char *text,
                      ppf_context_t *ctx) {
    CHECK(mpfr_set_str(m, text, 0, MPFR_RNDN) == 0);
    CHECK(ppf_set_mpfr(p, m, PPF_RNDN, ctx) != PPF_TERNARY_ERROR);
}

static void check_faithful_result(const char *name, const ppf_t *got,
                                  mpfr_srcptr down, mpfr_srcptr up) {
    mpfr_t output;
    mpfr_init2(output, mpfr_get_prec(down));
    CHECK(ppf_get_mpfr(output, got, MPFR_RNDN) != PPF_TERNARY_ERROR);
    if (!same_mpfr(output, down) && !same_mpfr(output, up)) {
        fprintf(stderr, "faithful mismatch: %s\n", name);
        mpfr_printf("got=%Ra\ndown=%Ra\nup=%Ra\n", output, down, up);
        exit(1);
    }
    mpfr_clear(output);
}

static void unary_case(const char *name, ppf_unary_fn pf, mpfr_unary_fn mf,
                       const char *text) {
    ppf_context_t ctx;
    ppf_t x, result;
    mpfr_t mx, down, up;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 103, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 80, &ctx) == 0);
    mpfr_init2(mx, 103);
    mpfr_inits2(80, down, up, (mpfr_ptr)0);
    set_input(&x, mx, text, &ctx);
    CHECK(pf(&result, &x, PPF_RNDF, &ctx) != PPF_TERNARY_ERROR);
    (void)mf(down, mx, MPFR_RNDD);
    (void)mf(up, mx, MPFR_RNDU);
    check_faithful_result(name, &result, down, up);
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clear(mx); mpfr_clears(down, up, (mpfr_ptr)0);
}

static void binary_case(const char *name, ppf_binary_fn pf, mpfr_binary_fn mf,
                        const char *at, const char *bt) {
    ppf_context_t ctx;
    ppf_t a, b, result;
    mpfr_t ma, mb, down, up;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 101, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 107, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 80, &ctx) == 0);
    mpfr_init2(ma, 101); mpfr_init2(mb, 107);
    mpfr_inits2(80, down, up, (mpfr_ptr)0);
    set_input(&a, ma, at, &ctx); set_input(&b, mb, bt, &ctx);
    CHECK(pf(&result, &a, &b, PPF_RNDF, &ctx) != PPF_TERNARY_ERROR);
    (void)mf(down, ma, mb, MPFR_RNDD);
    (void)mf(up, ma, mb, MPFR_RNDU);
    check_faithful_result(name, &result, down, up);
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clear(ma); mpfr_clear(mb); mpfr_clears(down, up, (mpfr_ptr)0);
}

static void gamma_inc_case(void) {
    ppf_context_t ctx; ppf_t a, x, result; mpfr_t ma, mx, down, up;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 101, &ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 107, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 80, &ctx) == 0);
    mpfr_init2(ma, 101); mpfr_init2(mx, 107); mpfr_inits2(80, down, up, (mpfr_ptr)0);
    set_input(&a, ma, "2.5", &ctx); set_input(&x, mx, "1.2", &ctx);
    CHECK(ppf_gamma_inc(&result, &a, &x, PPF_RNDF, &ctx) != PPF_TERNARY_ERROR);
    (void)mpfr_gamma_inc(down, ma, mx, MPFR_RNDD);
    (void)mpfr_gamma_inc(up, ma, mx, MPFR_RNDU);
    check_faithful_result("gamma_inc", &result, down, up);
    ppf_clear(&a); ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clear(ma); mpfr_clear(mx); mpfr_clears(down, up, (mpfr_ptr)0);
}

static void lgamma_case(void) {
    ppf_context_t ctx; ppf_t x, result; mpfr_t mx, down, up; int signp = 0, sd = 0, su = 0;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 103, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 80, &ctx) == 0);
    mpfr_init2(mx, 103); mpfr_inits2(80, down, up, (mpfr_ptr)0);
    set_input(&x, mx, "-2.5", &ctx);
    CHECK(ppf_lgamma(&result, &signp, &x, PPF_RNDF, &ctx) != PPF_TERNARY_ERROR);
    (void)mpfr_lgamma(down, &sd, mx, MPFR_RNDD);
    (void)mpfr_lgamma(up, &su, mx, MPFR_RNDU);
    CHECK(signp == sd && signp == su);
    check_faithful_result("lgamma", &result, down, up);
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clear(mx); mpfr_clears(down, up, (mpfr_ptr)0);
}

static void order_case(const char *name, long n, const char *text, int second_kind) {
    ppf_context_t ctx; ppf_t x, result; mpfr_t mx, down, up;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 103, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 80, &ctx) == 0);
    mpfr_init2(mx, 103); mpfr_inits2(80, down, up, (mpfr_ptr)0);
    set_input(&x, mx, text, &ctx);
    CHECK((second_kind ? ppf_yn(&result, n, &x, PPF_RNDF, &ctx)
                       : ppf_jn(&result, n, &x, PPF_RNDF, &ctx)) != PPF_TERNARY_ERROR);
    if (second_kind) {
        (void)mpfr_yn(down, n, mx, MPFR_RNDD); (void)mpfr_yn(up, n, mx, MPFR_RNDU);
    } else {
        (void)mpfr_jn(down, n, mx, MPFR_RNDD); (void)mpfr_jn(up, n, mx, MPFR_RNDU);
    }
    check_faithful_result(name, &result, down, up);
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clear(mx); mpfr_clears(down, up, (mpfr_ptr)0);
}

static void constant_case(int id, const char *name) {
    ppf_context_t ctx; ppf_t result; mpfr_t down, up;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 80, &ctx) == 0);
    mpfr_inits2(80, down, up, (mpfr_ptr)0);
    if (id == PPF_CONSTANT_EULER) {
        CHECK(ppf_const_euler(&result, PPF_RNDF, &ctx) != PPF_TERNARY_ERROR);
        (void)mpfr_const_euler(down, MPFR_RNDD); (void)mpfr_const_euler(up, MPFR_RNDU);
    } else {
        CHECK(ppf_const_catalan(&result, PPF_RNDF, &ctx) != PPF_TERNARY_ERROR);
        (void)mpfr_const_catalan(down, MPFR_RNDD); (void)mpfr_const_catalan(up, MPFR_RNDU);
    }
    check_faithful_result(name, &result, down, up);
    ppf_clear(&result); ppf_context_clear(&ctx); mpfr_clears(down, up, (mpfr_ptr)0);
}

int main(void) {
    unary_case("eint", ppf_eint, mpfr_eint, "0.5");
    unary_case("li2", ppf_li2, mpfr_li2, "0.5");
    unary_case("gamma", ppf_gamma, mpfr_gamma, "2.5");
    gamma_inc_case();
    unary_case("lngamma", ppf_lngamma, mpfr_lngamma, "2.5");
    lgamma_case();
    unary_case("digamma", ppf_digamma, mpfr_digamma, "2.5");
    binary_case("beta", ppf_beta, mpfr_beta, "2.5", "1.5");
    unary_case("zeta", ppf_zeta, mpfr_zeta, "2.5");
    unary_case("erf", ppf_erf, mpfr_erf, "0.5");
    unary_case("erfc", ppf_erfc, mpfr_erfc, "0.5");
    unary_case("j0", ppf_j0, mpfr_j0, "2");
    unary_case("j1", ppf_j1, mpfr_j1, "2");
    order_case("jn", 3, "2", 0);
    unary_case("y0", ppf_y0, mpfr_y0, "2");
    unary_case("y1", ppf_y1, mpfr_y1, "2");
    order_case("yn", 3, "2", 1);
    binary_case("agm", ppf_agm, mpfr_agm, "1", "0.5");
    unary_case("ai", ppf_ai, mpfr_ai, "0.5");
    constant_case(PPF_CONSTANT_EULER, "euler");
    constant_case(PPF_CONSTANT_CATALAN, "catalan");
    puts("special faithful rounding passed");
    return 0;
}
