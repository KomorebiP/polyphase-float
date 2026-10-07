/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"
#include "polyphase_float_mpfr.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)


typedef struct {
    int constant_id;
    mpfr_prec_t precision;
    mpfr_rnd_t rounding;
    ppf_t value;
    int ternary;
    int initialized;
} constant_thread_case_t;

static void *constant_thread_main(void *opaque) {
    constant_thread_case_t *test = (constant_thread_case_t *)opaque;
    ppf_context_t ctx;
    ppf_rnd_t rnd = ppf_rounding_from_mpfr(test->rounding);
    if (ppf_context_init(&ctx) != 0) return opaque;
    if (ppf_init2_ctx(&test->value, (ppf_prec_t)test->precision, &ctx) != 0) {
        ppf_context_clear(&ctx);
        return opaque;
    }
    test->initialized = 1;
    if (test->constant_id == PPF_CONSTANT_PI)
        test->ternary = ppf_const_pi_threads(&test->value, rnd, 2UL, &ctx);
    else
        test->ternary = ppf_const_log2_threads(&test->value, rnd, 2UL, &ctx);
    ppf_context_clear(&ctx);
    return test->ternary == PPF_TERNARY_ERROR ? (void *)1 : NULL;
}

static int sign_of(int x) { return (x > 0) - (x < 0); }

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        return mpfr_nan_p(a) && mpfr_nan_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    if (mpfr_inf_p(a) || mpfr_inf_p(b))
        return mpfr_inf_p(a) && mpfr_inf_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    if (!mpfr_equal_p(a, b)) return 0;
    if (mpfr_zero_p(a)) return mpfr_signbit(a) == mpfr_signbit(b);
    return 1;
}

static int compare_exact_dyadics(mpz_srcptr a, ppf_exp_t ae,
                                 mpz_srcptr b, ppf_exp_t be) {
    mpz_t x, y;
    int cmp;
    mpz_inits(x, y, NULL);
    mpz_set(x, a);
    mpz_set(y, b);
    if (ae > be) {
        uint64_t d = (uint64_t)ae - (uint64_t)be;
        CHECK(d <= (uint64_t)ULONG_MAX);
        mpz_mul_2exp(x, x, (mp_bitcnt_t)d);
    } else if (be > ae) {
        uint64_t d = (uint64_t)be - (uint64_t)ae;
        CHECK(d <= (uint64_t)ULONG_MAX);
        mpz_mul_2exp(y, y, (mp_bitcnt_t)d);
    }
    cmp = mpz_cmp(x, y);
    mpz_clears(x, y, NULL);
    return cmp;
}

static void compare_constant(int which, mpfr_prec_t precision,
                             mpfr_rnd_t mrnd) {
    ppf_context_t ctx;
    ppf_t got;
    mpfr_t ref, out, down, up;
    int pt, mt;
    ppf_rnd_t prnd = ppf_rounding_from_mpfr(mrnd);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&got, (ppf_prec_t)precision, &ctx) == 0);
    mpfr_inits2(precision, ref, out, down, up, (mpfr_ptr)0);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    mpfr_clear_flags();
    if (which == PPF_CONSTANT_PI) {
        pt = ppf_const_pi_threads(&got, prnd, 2UL, &ctx);
        mt = mpfr_const_pi(ref, mrnd);
    } else {
        pt = ppf_const_log2_threads(&got, prnd, 2UL, &ctx);
        mt = mpfr_const_log2(ref, mrnd);
    }
    if (pt == PPF_TERNARY_ERROR) {
        fprintf(stderr, "constant error id=%d precision=%lu rnd=%d\n",
                which, (unsigned long)precision, (int)mrnd);
        exit(1);
    }
    CHECK(ppf_get_mpfr(out, &got, MPFR_RNDN) == 0);
    if (mrnd == MPFR_RNDF) {
        if (which == PPF_CONSTANT_PI) {
            (void)mpfr_const_pi(down, MPFR_RNDD);
            (void)mpfr_const_pi(up, MPFR_RNDU);
        } else {
            (void)mpfr_const_log2(down, MPFR_RNDD);
            (void)mpfr_const_log2(up, MPFR_RNDU);
        }
        CHECK(same_value(out, down) || same_value(out, up));
    } else {
        if (!same_value(ref, out)) {
            fprintf(stderr, "constant mismatch id=%d precision=%lu rnd=%d\n",
                    which, (unsigned long)precision, (int)mrnd);
            mpfr_printf("reference=%Ra\npolyphase=%Ra\n", ref, out);
            exit(1);
        }
        CHECK(sign_of(pt) == sign_of(mt));
        CHECK((ppf_context_flags(&ctx) & PPF_FLAG_INEXACT) != 0);
    }
    mpfr_clears(ref, out, down, up, (mpfr_ptr)0);
    ppf_clear(&got);
    ppf_context_clear(&ctx);
}

static void rounding_matrix(void) {
    const mpfr_prec_t precisions[] = {
        2, 3, 4, 5, 7, 10, 24, 53, 64, 113, 257, 1000, 4096
    };
    const mpfr_rnd_t modes[] = {
        MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU,
        MPFR_RNDD, MPFR_RNDA, MPFR_RNDF
    };
    for (size_t i = 0; i < sizeof(precisions) / sizeof(precisions[0]); ++i)
        for (size_t r = 0; r < sizeof(modes) / sizeof(modes[0]); ++r) {
            compare_constant(PPF_CONSTANT_PI, precisions[i], modes[r]);
            compare_constant(PPF_CONSTANT_LOG2, precisions[i], modes[r]);
        }
}

static void interval_containment(int which) {
    mpz_t lower, upper, exact;
    ppf_exp_t interval_exp;
    mpfr_exp_t exact_exp;
    mpfr_t reference;
    mpz_inits(lower, upper, exact, NULL);
    ppf_clear_constant_cache((ppf_constant_id_t)which);
    mpfr_init2(reference, 1200);
    CHECK(ppf_get_constant_interval((ppf_constant_id_t)which,
                                    lower, upper, &interval_exp,
                                    1000, 2UL) == 0);
    if (which == PPF_CONSTANT_PI)
        (void)mpfr_const_pi(reference, MPFR_RNDN);
    else
        (void)mpfr_const_log2(reference, MPFR_RNDN);
    exact_exp = mpfr_get_z_2exp(exact, reference);
    CHECK(compare_exact_dyadics(lower, interval_exp,
                                exact, (ppf_exp_t)exact_exp) <= 0);
    CHECK(compare_exact_dyadics(exact, (ppf_exp_t)exact_exp,
                                upper, interval_exp) <= 0);
    mpfr_clear(reference);
    mpz_clears(lower, upper, exact, NULL);
}

static void cache_behavior(void) {
    ppf_context_t ctx;
    ppf_t x;
    ppf_constant_stats_t stats;
    ppf_free_constant_caches();
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 384, &ctx) == 0);
    CHECK(ppf_const_pi_threads(&x, PPF_RNDN, 2UL, &ctx) !=
          PPF_TERNARY_ERROR);
    CHECK(ppf_get_last_constant_stats(&stats) == 0);
    CHECK(stats.cache_hit == 0);
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_PI) >= 384);
    CHECK(ppf_const_pi_threads(&x, PPF_RNDU, 2UL, &ctx) !=
          PPF_TERNARY_ERROR);
    CHECK(ppf_get_last_constant_stats(&stats) == 0);
    CHECK(stats.cache_hit == 1);

    CHECK(ppf_precompute_log2(768, 2UL) == 0);
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_LOG2) >= 768);
    CHECK(ppf_set_prec(&x, 512) == 0);
    CHECK(ppf_const_log2(&x, PPF_RNDD, &ctx) != PPF_TERNARY_ERROR);
    CHECK(ppf_get_last_constant_stats(&stats) == 0);
    CHECK(stats.cache_hit == 1);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
}

static void range_cases(void) {
    ppf_context_t ctx;
    ppf_t got;
    mpfr_t ref, out;
    mpfr_exp_t old_emin = mpfr_get_emin();
    mpfr_exp_t old_emax = mpfr_get_emax();
    int pt, mt;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, ref, out, (mpfr_ptr)0);

    CHECK(ppf_context_set_range(&ctx, PPF_EMIN_DEFAULT, 1) == 0);
    CHECK(mpfr_set_emax(1) == 0);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    mpfr_clear_flags();
    pt = ppf_const_pi_uncached(&got, PPF_RNDN, 1UL, &ctx);
    mt = mpfr_const_pi(ref, MPFR_RNDN);
    CHECK(pt != PPF_TERNARY_ERROR);
    CHECK(sign_of(pt) == sign_of(mt));
    CHECK(ppf_get_mpfr(out, &got, MPFR_RNDN) == 0);
    CHECK(same_value(ref, out));
    CHECK(((ppf_context_flags(&ctx) & PPF_FLAG_OVERFLOW) != 0) ==
          (mpfr_overflow_p() != 0));
    CHECK(mpfr_set_emax(old_emax) == 0);

    CHECK(ppf_context_set_range(&ctx, 1, PPF_EMAX_DEFAULT) == 0);
    CHECK(mpfr_set_emin(1) == 0);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    mpfr_clear_flags();
    pt = ppf_const_log2_uncached(&got, PPF_RNDN, 1UL, &ctx);
    mt = mpfr_const_log2(ref, MPFR_RNDN);
    CHECK(pt != PPF_TERNARY_ERROR);
    CHECK(sign_of(pt) == sign_of(mt));
    CHECK(ppf_get_mpfr(out, &got, MPFR_RNDN) == 0);
    CHECK(same_value(ref, out));
    CHECK(((ppf_context_flags(&ctx) & PPF_FLAG_UNDERFLOW) != 0) ==
          (mpfr_underflow_p() != 0));
    CHECK(mpfr_set_emin(old_emin) == 0);

    mpfr_clears(ref, out, (mpfr_ptr)0);
    ppf_clear(&got);
    ppf_context_clear(&ctx);
}


static void randomized_precisions(void) {
    static const mpfr_rnd_t modes[] = {
        MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU,
        MPFR_RNDD, MPFR_RNDA, MPFR_RNDF
    };
    uint64_t state = UINT64_C(0x9e3779b97f4a7c15);
    for (unsigned int i = 0U; i < 72U; ++i) {
        state = state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
        mpfr_prec_t precision = (mpfr_prec_t)(2U + (state % 8191U));
        mpfr_rnd_t mode = modes[(state >> 17) % (sizeof(modes) / sizeof(modes[0]))];
        compare_constant((i & 1U) ? PPF_CONSTANT_PI : PPF_CONSTANT_LOG2,
                         precision, mode);
    }
}

static void concurrent_cache_calls(void) {
    constant_thread_case_t cases[] = {
        { PPF_CONSTANT_PI, 521, MPFR_RNDN, {0}, 0, 0 },
        { PPF_CONSTANT_PI, 1024, MPFR_RNDU, {0}, 0, 0 },
        { PPF_CONSTANT_LOG2, 777, MPFR_RNDD, {0}, 0, 0 },
        { PPF_CONSTANT_LOG2, 1536, MPFR_RNDZ, {0}, 0, 0 },
        { PPF_CONSTANT_PI, 2048, MPFR_RNDA, {0}, 0, 0 },
        { PPF_CONSTANT_LOG2, 2048, MPFR_RNDN, {0}, 0, 0 }
    };
    pthread_t threads[sizeof(cases) / sizeof(cases[0])];
    ppf_free_constant_caches();
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        CHECK(pthread_create(&threads[i], NULL, constant_thread_main, &cases[i]) == 0);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        void *result = NULL;
        mpfr_t reference, output;
        CHECK(pthread_join(threads[i], &result) == 0);
        CHECK(result == NULL);
        CHECK(cases[i].initialized);
        mpfr_inits2(cases[i].precision, reference, output, (mpfr_ptr)0);
        if (cases[i].constant_id == PPF_CONSTANT_PI)
            (void)mpfr_const_pi(reference, cases[i].rounding);
        else
            (void)mpfr_const_log2(reference, cases[i].rounding);
        CHECK(ppf_get_mpfr(output, &cases[i].value, MPFR_RNDN) == 0);
        CHECK(same_value(reference, output));
        ppf_clear(&cases[i].value);
        mpfr_clears(reference, output, (mpfr_ptr)0);
    }
}

static void cache_cleanup_and_flags(void) {
    ppf_context_t ctx;
    ppf_t x;
    CHECK(ppf_precompute_pi(100000, 2UL) == 0);
    CHECK(ppf_precompute_log2(100000, 2UL) == 0);
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_PI) >= 100000);
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_LOG2) >= 100000);
    ppf_free_constant_caches();
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_PI) == 0);
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_LOG2) == 0);

    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 100000, &ctx) == 0);
    ppf_context_flags_set(&ctx, PPF_FLAG_ERANGE);
    CHECK(ppf_const_pi_threads(&x, PPF_RNDN, 2UL, &ctx) != PPF_TERNARY_ERROR);
    CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ERANGE) != 0);
    CHECK((ppf_context_flags(&ctx) & PPF_FLAG_INEXACT) != 0);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
}

static void large_precision_smoke(void) {
    compare_constant(PPF_CONSTANT_PI, 100000, MPFR_RNDN);
    compare_constant(PPF_CONSTANT_LOG2, 100000, MPFR_RNDN);
}

int main(void) {
    ppf_set_constant_threads(2UL);
    rounding_matrix();
    randomized_precisions();
    interval_containment(PPF_CONSTANT_PI);
    interval_containment(PPF_CONSTANT_LOG2);
    cache_behavior();
    concurrent_cache_calls();
    cache_cleanup_and_flags();
    range_cases();
    large_precision_smoke();
    puts("independent constants: PASS");
    return 0;
}
