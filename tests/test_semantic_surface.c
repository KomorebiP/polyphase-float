/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "polyphase_float.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void expect_exact_si(ppf_t *x, long value, ppf_context_t *ctx)
{
    assert(ppf_set_si(x, value, PPF_RNDN, ctx) == 0);
    assert(ppf_get_sj(x, PPF_RNDN, ctx) == (intmax_t)value);
}

static void expect_value_si(const ppf_t *x, long value, ppf_context_t *ctx)
{
    assert(ppf_get_sj(x, PPF_RNDN, ctx) == (intmax_t)value);
}

static void test_context_and_metadata(ppf_context_t *ctx)
{
    ppf_allocator_t allocator = ppf_default_allocator();
    ppf_flags_t saved;
    ppf_rnd_t old_rounding;

    assert(allocator.malloc_fn != NULL);
    assert(allocator.realloc_fn != NULL);
    assert(allocator.free_fn != NULL);

    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
    ppf_context_flags_set(ctx, PPF_FLAG_INEXACT | PPF_FLAG_ERANGE);
    saved = ppf_context_flags_save(ctx);
    assert((saved & (PPF_FLAG_INEXACT | PPF_FLAG_ERANGE)) ==
           (PPF_FLAG_INEXACT | PPF_FLAG_ERANGE));
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
    ppf_context_flags_restore(ctx, saved, PPF_FLAG_INEXACT);
    assert(ppf_context_flags(ctx) == PPF_FLAG_INEXACT);
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);

    assert(ppf_get_emin_min() == PPF_EMIN_MIN);
    assert(ppf_get_emin_max() == PPF_EMAX_MAX);
    assert(ppf_get_emax_min() == PPF_EMIN_MIN);
    assert(ppf_get_emax_max() == PPF_EMAX_MAX);

    old_rounding = ppf_get_default_rounding_mode();
    assert(ppf_set_default_rounding_mode(PPF_RNDU) == 0);
    assert(ppf_get_default_rounding_mode() == PPF_RNDU);
    assert(ppf_set_default_rounding_mode(old_rounding) == 0);

    assert(strcmp(ppf_rounding_name(PPF_RNDN), "PPF_RNDN") == 0);
    assert(strcmp(ppf_kind_name(PPF_KIND_REGULAR), "regular") == 0);
}

static void test_assignment_shortcuts(ppf_context_t *ctx)
{
    ppf_t source, x;
    mpz_t z;
    mpq_t q;
    mpf_t f;
    long exponent = 0;
    long double mantissa;

    assert(ppf_init2_ctx(&source, 128, ctx) == 0);
    expect_exact_si(&source, 17, ctx);

    assert(ppf_init_set(&x, &source, PPF_RNDN, ctx) == 0);
    expect_value_si(&x, 17, ctx);
    ppf_clear(&x);

    assert(ppf_init_set_sj(&x, INTMAX_C(-19), PPF_RNDN, ctx) == 0);
    expect_value_si(&x, -19, ctx);
    ppf_clear(&x);

    assert(ppf_init_set_d(&x, 1.5, PPF_RNDN, ctx) == 0);
    assert(ppf_get_d(&x, PPF_RNDN, ctx) == 1.5);
    ppf_clear(&x);

    assert(ppf_init_set_ld(&x, 2.25L, PPF_RNDN, ctx) == 0);
    assert(ppf_get_ld(&x, PPF_RNDN, ctx) == 2.25L);
    ppf_clear(&x);

    mpz_init_set_si(z, -23);
    assert(ppf_init_set_z(&x, z, PPF_RNDN, ctx) == 0);
    expect_value_si(&x, -23, ctx);
    ppf_clear(&x);

    mpq_init(q);
    mpq_set_si(q, 3, 2);
    mpq_canonicalize(q);
    assert(ppf_init_set_q(&x, q, PPF_RNDN, ctx) == 0);
    assert(ppf_get_d(&x, PPF_RNDN, ctx) == 1.5);
    ppf_clear(&x);

    mpf_init2(f, 128);
    mpf_set_ui(f, 29);
    assert(ppf_init_set_f(&x, f, PPF_RNDN, ctx) == 0);
    expect_value_si(&x, 29, ctx);
    ppf_clear(&x);

    assert(ppf_init_set_str(&x, "31", 10, PPF_RNDN, ctx) == 0);
    expect_value_si(&x, 31, ctx);
    ppf_clear(&x);

    assert(ppf_init2_ctx(&x, 128, ctx) == 0);
    assert(ppf_set_flt(&x, 1.25f, PPF_RNDN, ctx) == 0);
    assert(ppf_get_flt(&x, PPF_RNDN, ctx) == 1.25f);
    assert(ppf_set_si_2exp(&x, -3, 2, PPF_RNDN, ctx) == 0);
    expect_value_si(&x, -12, ctx);
    assert(ppf_set_uj_2exp(&x, UINTMAX_C(5), 3, PPF_RNDN, ctx) == 0);
    assert(ppf_get_uj(&x, PPF_RNDN, ctx) == UINTMAX_C(40));
    mantissa = ppf_get_ld_2exp(&exponent, &x, PPF_RNDN, ctx);
    assert(ldexpl(mantissa, (int)exponent) == 40.0L);
    assert(ppf_fits_uint_p(&x, PPF_RNDN));
    assert(ppf_fits_sint_p(&x, PPF_RNDN));
    assert(ppf_fits_ushort_p(&x, PPF_RNDN));
    assert(ppf_fits_sshort_p(&x, PPF_RNDN));
    ppf_clear(&x);

    mpf_clear(f);
    mpq_clear(q);
    mpz_clear(z);
    ppf_clear(&source);
}

static void test_round_nearest_away(ppf_context_t *ctx)
{
    ppf_t result, extended;
    assert(ppf_init2_ctx(&result, 53, ctx) == 0);
    assert(ppf_init2_ctx(&extended, 54, ctx) == 0);
    assert(ppf_set_ui(&extended, 3, PPF_RNDN, ctx) == 0);
    assert(ppf_round_nearest_away_finish(&result, &extended, 0, ctx) == 0);
    expect_value_si(&result, 3, ctx);
    ppf_clear(&extended);
    ppf_clear(&result);
}

static void test_threaded_wrappers(ppf_context_t *ctx)
{
    ppf_t a, b, c, out;
    const ppf_t *left[2];
    const ppf_t *right[2];
    unsigned long previous_trig;

    assert(ppf_init2_ctx(&a, 128, ctx) == 0);
    assert(ppf_init2_ctx(&b, 128, ctx) == 0);
    assert(ppf_init2_ctx(&c, 128, ctx) == 0);
    assert(ppf_init2_ctx(&out, 128, ctx) == 0);
    expect_exact_si(&a, 6, ctx);
    expect_exact_si(&b, 7, ctx);
    expect_exact_si(&c, 2, ctx);

    assert(ppf_mul_threads(&out, &a, &b, PPF_RNDN, 2, ctx) == 0);
    expect_value_si(&out, 42, ctx);
    assert(ppf_sqr_threads(&out, &a, PPF_RNDN, 2, ctx) == 0);
    expect_value_si(&out, 36, ctx);
    assert(ppf_fma_threads(&out, &a, &b, &c, PPF_RNDN, 2, ctx) == 0);
    expect_value_si(&out, 44, ctx);
    assert(ppf_fms_threads(&out, &a, &b, &c, PPF_RNDN, 2, ctx) == 0);
    expect_value_si(&out, 40, ctx);
    assert(ppf_fms_algorithm_threads(&out, &a, &b, &c, PPF_RNDN, 2,
                                     PPF_PRODUCT_FULL, ctx) == 0);
    expect_value_si(&out, 40, ctx);

    assert(ppf_mul_2si(&out, &a, 2, PPF_RNDN, ctx) == 0);
    expect_value_si(&out, 24, ctx);
    assert(ppf_div_2si(&out, &a, 1, PPF_RNDN, ctx) == 0);
    expect_value_si(&out, 3, ctx);

    left[0] = &a;
    left[1] = &b;
    right[0] = &c;
    right[1] = &a;
    assert(ppf_dot_threads(&out, left, right, 2, PPF_RNDN, 2, ctx) == 0);
    expect_value_si(&out, 54, ctx); /* 6*2 + 7*6 */

    previous_trig = ppf_get_trig_threads();
    ppf_set_trig_threads(2);
    assert(ppf_get_trig_threads() >= 1);
    ppf_set_trig_threads(previous_trig);

    ppf_clear(&out);
    ppf_clear(&c);
    ppf_clear(&b);
    ppf_clear(&a);
}

static void test_constant_wrappers(ppf_context_t *ctx)
{
    ppf_t value;
    double d;
    unsigned long previous_workers;

    assert(ppf_init2_ctx(&value, 96, ctx) == 0);
    assert(ppf_const_euler_threads(&value, PPF_RNDN, 2, ctx) !=
           PPF_TERNARY_ERROR);
    d = ppf_get_d(&value, PPF_RNDN, ctx);
    assert(d > 0.57 && d < 0.58);
    assert(ppf_const_euler_uncached(&value, PPF_RNDN, 2, ctx) !=
           PPF_TERNARY_ERROR);
    assert(ppf_const_catalan_threads(&value, PPF_RNDN, 2, ctx) !=
           PPF_TERNARY_ERROR);
    d = ppf_get_d(&value, PPF_RNDN, ctx);
    assert(d > 0.91 && d < 0.92);
    assert(ppf_const_catalan_uncached(&value, PPF_RNDN, 2, ctx) !=
           PPF_TERNARY_ERROR);

    assert(ppf_precompute_euler(96, 2) == 0);
    assert(ppf_precompute_constant(PPF_CONSTANT_CATALAN, 96, 2) == 0);
    previous_workers = ppf_get_constant_threads();
    ppf_set_constant_threads(2);
    assert(ppf_get_constant_threads() >= 1);
    ppf_set_constant_threads(previous_workers);

    ppf_clear(&value);
}

static void test_diagnostic_surface(ppf_context_t *ctx)
{
    ppf_t value;
    assert(ppf_init2_ctx(&value, 64, ctx) == 0);
    ppf_set_zero(&value, 1);
    ppf_dump(&value);
    ppf_clear(&value);
}

int main(void)
{
    ppf_context_t ctx;
    assert(ppf_context_init(&ctx) == 0);
    test_context_and_metadata(&ctx);
    test_assignment_shortcuts(&ctx);
    test_round_nearest_away(&ctx);
    test_threaded_wrappers(&ctx);
    test_constant_wrappers(&ctx);
    test_diagnostic_surface(&ctx);
    ppf_context_clear(&ctx);
    puts("semantic surface coverage passed");
    return 0;
}
