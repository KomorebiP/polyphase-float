/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

/* Reuse the independently rounded MPFR comparison helpers and their complete
   value, ternary-direction, sign and exception-flag checks. */
#define main inherited_edge_audit_main
#include "test_special_edge_audit.c"
#undef main
#include "special_internal.h"

static int ppf_j2(ppf_t *r, const ppf_t *x, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_jn(r, 2, x, rnd, ctx);
}
static int oracle_j2(mpfr_ptr r, mpfr_srcptr x, mpfr_rnd_t rnd) {
    return mpfr_jn(r, 2, x, rnd);
}

typedef struct { unsigned calls, live; int armed; } admission_allocator_t;
static void *admission_malloc(void *opaque, size_t size) {
    admission_allocator_t *state = opaque;
    if (state->armed && ++state->calls > 1) return NULL;
    void *ptr = malloc(size);
    CHECK(ptr != NULL);
    ++state->live;
    return ptr;
}
static void *admission_realloc(void *opaque, void *ptr,
                                size_t old_size, size_t new_size) {
    admission_allocator_t *state = opaque;
    (void)old_size;
    if (state->armed && ++state->calls > 1) return NULL;
    int was_null = ptr == NULL;
    void *result = realloc(ptr, new_size);
    CHECK(result != NULL);
    if (was_null) ++state->live;
    return result;
}
static void admission_free(void *opaque, void *ptr, size_t size) {
    admission_allocator_t *state = opaque;
    (void)size;
    if (ptr) { CHECK(state->live > 0); --state->live; free(ptr); }
}

/* A finite input just outside the exact |x| <= 500 admission boundary must
   not be rounded back inside by an intermediate machine-format conversion. */
static void airy_exact_admission(void) {
    admission_allocator_t state = {0};
    ppf_allocator_t allocator = {admission_malloc, admission_realloc, admission_free, &state};
    ppf_context_t ctx;
    ppf_t x, result;
    mpfr_t mx, delta;
    CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
    CHECK(ppf_init2_ctx(&x, 512, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 53, &ctx) == 0);
    mpfr_inits2(512, mx, delta, (mpfr_ptr)0);
    (void)mpfr_set_ui(delta, 1, MPFR_RNDN);
    CHECK(mpfr_mul_2si(delta, delta, -400, MPFR_RNDN) == 0);
    for (int sign = -1; sign <= 1; sign += 2) {
        CHECK(mpfr_set_ui(mx, 500, MPFR_RNDN) == 0);
        CHECK(mpfr_add(mx, mx, delta, MPFR_RNDN) == 0);
        if (sign < 0) CHECK(mpfr_neg(mx, mx, MPFR_RNDN) == 0);
        CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_cmpabs_ui(&x, 500, &ctx) > 0);
        for (unsigned mode = 0; mode < 5; ++mode) {
            CHECK(ppf_set_ui(&result, 17, PPF_RNDN, &ctx) == 0);
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            state.calls = 0; state.armed = 1;
            CHECK(ppf_ai(&result, &x, pr[mode], &ctx) == PPF_TERNARY_ERROR);
            state.armed = 0;
            CHECK(state.calls == 1);
            CHECK(ctx.flags == PPF_FLAG_ERANGE);
            CHECK(ppf_nan_p(&result));
            ++cases;
        }
    }
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    CHECK(state.live == 0);
    mpfr_clears(mx, delta, (mpfr_ptr)0);
}

static void narrow_exponent_case(const char *name, ppf_unary_fn pf,
                                  mpfr_unary_fn mf, const char *text) {
    ppf_context_t ctx;
    ppf_t x, result;
    mpfr_t mx, expected, actual;
    mpfr_exp_t saved_emin = mpfr_get_emin(), saved_emax = mpfr_get_emax();
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 128, &ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 17, &ctx) == 0);
    mpfr_init2(mx, 128); mpfr_inits2(17, expected, actual, (mpfr_ptr)0);
    CHECK(mpfr_set_str(mx, text, 0, MPFR_RNDN) == 0);
    CHECK(ppf_set_mpfr(&x, mx, PPF_RNDN, &ctx) == 0);
    ctx.emin = -4; ctx.emax = 4;
    for (unsigned mode = 0; mode < 5; ++mode) {
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        CHECK(mpfr_set_emin(-4) == 0); CHECK(mpfr_set_emax(4) == 0);
        mpfr_clear_flags();
        int a = pf(&result, &x, pr[mode], &ctx);
        int b = mf(expected, mx, mr[mode]);
        ppf_flags_t flags = oracle_flags();
        CHECK(mpfr_set_emin(saved_emin) == 0);
        CHECK(mpfr_set_emax(saved_emax) == 0);
        CHECK(ppf_get_mpfr(actual, &result, MPFR_RNDN) != PPF_TERNARY_ERROR);
        if (a == PPF_TERNARY_ERROR || signum(a) != signum(b) ||
            !same(actual, expected) || ctx.flags != flags) {
            fprintf(stderr, "narrow %s(%s) mode=%u: ternary %d/%d flags %#x/%#x\n",
                    name, text, mode, a, b, (unsigned)ctx.flags, (unsigned)flags);
            mpfr_printf("actual=%Ra expected=%Ra\n", actual, expected);
            exit(1);
        }
        ++cases;
    }
    ppf_clear(&x); ppf_clear(&result); ppf_context_clear(&ctx);
    mpfr_clears(mx, expected, actual, (mpfr_ptr)0);
}

static unsigned exhausted_builder_calls;
static int never_decidable(ppf_special_iv_t *out, ppf_prec_t work,
                            void *opaque, ppf_context_t *ctx) {
    (void)work; (void)opaque;
    ++exhausted_builder_calls;
    return ppf_set_ui(&out->lo, 1, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
           ppf_set_ui(&out->hi, 2, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
}

/* Exercise the production retry-exhaustion path with a valid interval whose
   ambiguity deliberately persists at every precision. No finite special-
   function sample is assumed to exhaust a particular precision schedule. */
static void certification_budget(void) {
    ppf_context_t ctx;
    ppf_t result;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&result, 17, &ctx) == 0);
    for (unsigned mode = 0; mode < 5; ++mode) {
        CHECK(ppf_set_ui(&result, 17, PPF_RNDN, &ctx) == 0);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        exhausted_builder_calls = 0;
        CHECK(ppf_special_evaluate(&result, pr[mode], &ctx,
                                    never_decidable, NULL) == PPF_TERNARY_ERROR);
        CHECK(exhausted_builder_calls == 12);
        CHECK(ctx.flags == PPF_FLAG_ERANGE);
        CHECK(ppf_nan_p(&result));
        CHECK(ppf_validate(&result));
        ++cases;
    }
    ppf_clear(&result); ppf_context_clear(&ctx);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "admission") == 0) {
        airy_exact_admission();
        puts("exact Airy resource-admission checks passed");
        return 0;
    }
    airy_exact_admission();
    const unsigned precisions[] = {2, 17, 53, 113};
    const char *reflection[] = {"-0.0009765625", "-0.9990234375",
        "-1.0009765625", "-7.9990234375", "-8.0009765625"};
    const char *reductions[] = {"-1.0009765625", "-0.5009765625",
        "-0.4990234375", "0.4990234375", "0.5009765625",
        "0.9990234375", "1.0009765625", "1.9990234375", "2.0009765625"};
    for (unsigned p = 0; p < sizeof(precisions)/sizeof(precisions[0]); ++p) {
        for (unsigned i = 0; i < sizeof(reflection)/sizeof(reflection[0]); ++i) {
            unary_case("gamma-reflection", ppf_gamma, mpfr_gamma,
                       reflection[i], precisions[p], 0);
            unary_case("digamma-reflection", ppf_digamma, mpfr_digamma,
                       reflection[i], precisions[p], 0);
        }
        for (unsigned i = 0; i < sizeof(reductions)/sizeof(reductions[0]); ++i)
            unary_case("li2-reduction", ppf_li2, mpfr_li2,
                       reductions[i], precisions[p], 0);
        unary_case("airy-negative", ppf_ai, mpfr_ai, "-6.25", precisions[p], 0);
        unary_case("airy-positive", ppf_ai, mpfr_ai, "6.25", precisions[p], 0);
        unary_case("bessel-even-negative", ppf_j2, oracle_j2,
                   "-1.0009765625", precisions[p], 0);
        fprintf(stderr, "branch matrix precision %u passed\n", precisions[p]);
    }
    for (int sign = -1; sign <= 1; sign += 2) {
        high_input_precision_case("gamma-reflection", ppf_gamma, mpfr_gamma, -8, sign);
        high_input_precision_case("digamma-reflection", ppf_digamma, mpfr_digamma, -8, sign);
        high_input_precision_case("li2-reflection", ppf_li2, mpfr_li2, 1, sign);
        high_input_precision_case("bessel-even-negative", ppf_j2, oracle_j2, -1, sign);
    }
    narrow_exponent_case("gamma-overflow", ppf_gamma, mpfr_gamma, "6");
    narrow_exponent_case("gamma-reflection", ppf_gamma, mpfr_gamma, "-0.0009765625");
    narrow_exponent_case("airy-underflow", ppf_ai, mpfr_ai, "6.25");
    narrow_exponent_case("li2", ppf_li2, mpfr_li2, "1.0009765625");
    certification_budget();
    ppf_free_constant_caches(); mpfr_mp_memory_cleanup();
    printf("release branch and boundary review passed: %lu comparisons/checks\n", cases);
    return 0;
}
