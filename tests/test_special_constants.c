/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); \
} } while (0)

static const mpfr_rnd_t mrnd[] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA, MPFR_RNDF
};

static int sign_of(int x) { return (x > 0) - (x < 0); }

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b)) return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_inf_p(a) || mpfr_inf_p(b))
        return mpfr_inf_p(a) && mpfr_inf_p(b) && mpfr_signbit(a) == mpfr_signbit(b);
    if (!mpfr_equal_p(a, b)) return 0;
    return !mpfr_zero_p(a) || mpfr_signbit(a) == mpfr_signbit(b);
}

static int call_ppf(int id, ppf_t *x, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return id == PPF_CONSTANT_EULER ? ppf_const_euler(x, rnd, ctx)
                                    : ppf_const_catalan(x, rnd, ctx);
}

static int call_mpfr(int id, mpfr_ptr x, mpfr_rnd_t rnd) {
    return id == PPF_CONSTANT_EULER ? mpfr_const_euler(x, rnd)
                                    : mpfr_const_catalan(x, rnd);
}

static void compare_constant(int id, mpfr_prec_t precision, mpfr_rnd_t rnd) {
    ppf_context_t ctx; ppf_t got; mpfr_t ref, out, down, up;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&got, (ppf_prec_t)precision, &ctx) == 0);
    mpfr_inits2(precision, ref, out, down, up, (mpfr_ptr)0);
    ppf_rnd_t prnd = ppf_rounding_from_mpfr(rnd);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    mpfr_clear_flags();
    int pt = call_ppf(id, &got, prnd, &ctx);
    int mt = call_mpfr(id, ref, rnd);
    CHECK(pt != PPF_TERNARY_ERROR);
    CHECK(ppf_get_mpfr(out, &got, MPFR_RNDN) != PPF_TERNARY_ERROR);
    if (rnd == MPFR_RNDF) {
        (void)call_mpfr(id, down, MPFR_RNDD);
        (void)call_mpfr(id, up, MPFR_RNDU);
        CHECK(same_value(out, down) || same_value(out, up));
    } else {
        if (!same_value(out, ref)) {
            fprintf(stderr, "constant mismatch id=%d p=%lu rnd=%d\n",
                    id, (unsigned long)precision, (int)rnd);
            mpfr_printf("got=%Ra\nref=%Ra\n", out, ref);
            exit(1);
        }
        CHECK(sign_of(pt) == sign_of(mt));
        CHECK((ppf_context_flags(&ctx) & PPF_FLAG_INEXACT) != 0);
    }
    ppf_clear(&got); ppf_context_clear(&ctx);
    mpfr_clears(ref, out, down, up, (mpfr_ptr)0);
}

static int compare_dyadic(mpz_srcptr a, ppf_exp_t ae,
                          mpz_srcptr b, ppf_exp_t be) {
    mpz_t x, y; int result;
    mpz_inits(x, y, NULL); mpz_set(x, a); mpz_set(y, b);
    if (ae > be) mpz_mul_2exp(x, x, (mp_bitcnt_t)(ae - be));
    else if (be > ae) mpz_mul_2exp(y, y, (mp_bitcnt_t)(be - ae));
    result = mpz_cmp(x, y); mpz_clears(x, y, NULL); return result;
}

static void interval_case(int id) {
    mpz_t lower, upper, exact; ppf_exp_t ie; mpfr_exp_t ee; mpfr_t ref;
    mpz_inits(lower, upper, exact, NULL); mpfr_init2(ref, 900);
    ppf_clear_constant_cache((ppf_constant_id_t)id);
    CHECK(ppf_get_constant_interval((ppf_constant_id_t)id,
                                    lower, upper, &ie, 640, 1) == 0);
    (void)call_mpfr(id, ref, MPFR_RNDN);
    ee = mpfr_get_z_2exp(exact, ref);
    CHECK(compare_dyadic(lower, ie, exact, (ppf_exp_t)ee) <= 0);
    CHECK(compare_dyadic(exact, (ppf_exp_t)ee, upper, ie) <= 0);
    mpfr_clear(ref); mpz_clears(lower, upper, exact, NULL);
}

static void cache_case(void) {
    ppf_context_t ctx; ppf_t x; ppf_constant_stats_t stats;
    ppf_free_constant_caches();
    CHECK(ppf_context_init(&ctx) == 0); CHECK(ppf_init2_ctx(&x, 384, &ctx) == 0);
    CHECK(ppf_const_euler(&x, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
    CHECK(ppf_get_last_constant_stats(&stats) == 0 && !stats.cache_hit);
    CHECK(ppf_const_euler(&x, PPF_RNDU, &ctx) != PPF_TERNARY_ERROR);
    CHECK(ppf_get_last_constant_stats(&stats) == 0 && stats.cache_hit);
    CHECK(ppf_precompute_catalan(768, 1) == 0);
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_CATALAN) >= 768);
    CHECK(ppf_set_prec(&x, 512) == 0);
    CHECK(ppf_const_catalan(&x, PPF_RNDD, &ctx) != PPF_TERNARY_ERROR);
    CHECK(ppf_get_last_constant_stats(&stats) == 0 && stats.cache_hit);
    ppf_context_flags_set(&ctx, PPF_FLAG_ERANGE);
    CHECK(ppf_const_catalan(&x, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
    CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ERANGE) != 0);
    ppf_clear(&x); ppf_context_clear(&ctx);
    ppf_free_constant_caches();
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_EULER) == 0);
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_CATALAN) == 0);
}

typedef struct { int id; unsigned precision; int ok; } thread_case_t;
static void *thread_main(void *opaque) {
    thread_case_t *t = opaque; ppf_context_t ctx; ppf_t x; mpfr_t ref, out;
    t->ok = 0;
    if (ppf_context_init(&ctx) != 0 || ppf_init2_ctx(&x, t->precision, &ctx) != 0)
        return NULL;
    mpfr_init2(ref, t->precision); mpfr_init2(out, t->precision);
    if (call_ppf(t->id, &x, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR &&
        ppf_get_mpfr(out, &x, MPFR_RNDN) != PPF_TERNARY_ERROR) {
        (void)call_mpfr(t->id, ref, MPFR_RNDN);
        t->ok = same_value(out, ref);
    }
    ppf_clear(&x); ppf_context_clear(&ctx); mpfr_clear(ref); mpfr_clear(out);
    mpfr_mp_memory_cleanup();
    return NULL;
}

static void concurrent_case(void) {
    thread_case_t cases[] = {
        {PPF_CONSTANT_EULER, 257, 0}, {PPF_CONSTANT_CATALAN, 521, 0},
        {PPF_CONSTANT_EULER, 777, 0}, {PPF_CONSTANT_CATALAN, 1024, 0}
    };
    pthread_t threads[4]; ppf_free_constant_caches();
    for (int i = 0; i < 4; ++i) CHECK(pthread_create(&threads[i], NULL, thread_main, &cases[i]) == 0);
    for (int i = 0; i < 4; ++i) { CHECK(pthread_join(threads[i], NULL) == 0); CHECK(cases[i].ok); }
}

int main(void) {
    const mpfr_prec_t precisions[] = {2, 3, 5, 10, 24, 53, 113, 257, 512, 1024};
    for (size_t p = 0; p < sizeof(precisions)/sizeof(precisions[0]); ++p)
        for (size_t r = 0; r < sizeof(mrnd)/sizeof(mrnd[0]); ++r) {
            compare_constant(PPF_CONSTANT_EULER, precisions[p], mrnd[r]);
            compare_constant(PPF_CONSTANT_CATALAN, precisions[p], mrnd[r]);
        }
    interval_case(PPF_CONSTANT_EULER); interval_case(PPF_CONSTANT_CATALAN);
    cache_case(); concurrent_case();
    ppf_free_constant_caches();
    mpfr_mp_memory_cleanup();
    puts("special constants passed");
    return 0;
}
