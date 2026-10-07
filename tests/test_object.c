/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)


typedef struct {
    size_t live_bytes;
    size_t allocations;
    size_t frees;
} alloc_state_t;

static void *tracked_malloc(void *opaque, size_t size) {
    alloc_state_t *state = (alloc_state_t *)opaque;
    void *p = malloc(size);
    if (p) { state->live_bytes += size; state->allocations++; }
    return p;
}

static void *tracked_realloc(void *opaque, void *ptr,
                             size_t old_size, size_t new_size) {
    alloc_state_t *state = (alloc_state_t *)opaque;
    void *p = realloc(ptr, new_size);
    if (p) {
        state->live_bytes -= old_size;
        state->live_bytes += new_size;
    }
    return p;
}

static void tracked_free(void *opaque, void *ptr, size_t size) {
    alloc_state_t *state = (alloc_state_t *)opaque;
    if (ptr) { state->live_bytes -= size; state->frees++; }
    free(ptr);
}

static void test_custom_allocator(void) {
    alloc_state_t state = {0,0,0};
    ppf_allocator_t allocator = {
        tracked_malloc, tracked_realloc, tracked_free, &state
    };
    ppf_context_t ctx;
    ppf_t x;
    CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    CHECK(ppf_reserve(&x, 100000) == 0);
    CHECK(ppf_arena_alloc(&ctx, 200000, 64) != NULL);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    CHECK(state.live_bytes == 0);
    CHECK(state.allocations >= 2);
    CHECK(state.frees >= 2);
}

static void expect_exact(const ppf_t *x, long value) {
    mpz_t z;
    ppf_exp_t e;
    mpz_init(z);
    e = ppf_get_z_2exp(z, x);
    if (e >= 0) mpz_mul_2exp(z, z, (mp_bitcnt_t)e);
    else {
        CHECK(mpz_divisible_2exp_p(z, (mp_bitcnt_t)(-e)));
        mpz_fdiv_q_2exp(z, z, (mp_bitcnt_t)(-e));
    }
    CHECK(mpz_cmp_si(z, value) == 0);
    mpz_clear(z);
}

static void test_context(void) {
    ppf_context_t ctx, clone;
    void *a, *b;
    ppf_arena_mark_t mark;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ctx.default_precision == 53);
    CHECK(ctx.emin < 0 && ctx.emax > 0);
    CHECK(ppf_context_set_default_precision(&ctx, 113) == 0);
    CHECK(ppf_context_set_range(&ctx, -1000, 1000) == 0);
    CHECK(ppf_context_set_rounding(&ctx, PPF_RNDU) == 0);
    ppf_context_flags_set(&ctx, PPF_FLAG_INEXACT | PPF_FLAG_ERANGE);
    CHECK(ppf_context_flags(&ctx) == (PPF_FLAG_INEXACT | PPF_FLAG_ERANGE));
    ppf_context_flags_clear(&ctx, PPF_FLAG_ERANGE);
    CHECK(ppf_context_flags(&ctx) == PPF_FLAG_INEXACT);
    CHECK(ppf_context_clone(&clone, &ctx) == 0);
    CHECK(clone.default_precision == 113 && clone.emin == -1000 &&
          clone.default_rounding == PPF_RNDU);

    a = ppf_arena_alloc(&ctx, 100, 64);
    CHECK(a != NULL && ((uintptr_t)a % 64) == 0);
    mark = ppf_arena_mark(&ctx);
    b = ppf_arena_alloc(&ctx, 100000, 32);
    CHECK(b != NULL && ((uintptr_t)b % 32) == 0);
    ppf_arena_release(&ctx, mark);
    b = ppf_arena_alloc(&ctx, 1024, 16);
    CHECK(b != NULL);
    ppf_arena_reset(&ctx);
    CHECK(ppf_arena_alloc(&ctx, 512, 16) != NULL);

    ppf_context_clear(&clone);
    ppf_context_clear(&ctx);
}


static void test_raw_significand_access(void) {
    ppf_context_t ctx;
    ppf_t x;
    mp_limb_t *limbs;
    size_t n;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 65, &ctx) == 0);
    n = ppf_limb_count(&x);
    CHECK(n == 2);
    limbs = ppf_limbs_write(&x);
    CHECK(limbs != NULL);
    limbs[0] = 3;
    limbs[1] = 1; /* precision bit 64 */
    CHECK(ppf_commit_regular(&x, -1, 17, &ctx) == 0);
    CHECK(ppf_regular_p(&x) && ppf_signbit(&x));
    CHECK(ppf_limbs_read(&x) == limbs);
    CHECK(ppf_validate(&x));
    limbs = ppf_limbs_write(&x);
    limbs[0] = 1;
    limbs[1] = 0; /* not normalized */
    CHECK(ppf_commit_regular(&x, 1, 0, &ctx) != 0);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
}

static void test_lifetime_and_specials(void) {
    ppf_t x, y;
    CHECK(ppf_init2(&x, 53) == 0);
    CHECK(ppf_init2(&y, 113) == 0);
    CHECK(ppf_nan_p(&x));
    ppf_set_zero(&x, -1);
    CHECK(ppf_zero_p(&x) && ppf_signbit(&x));
    ppf_set_inf(&x, 1);
    CHECK(ppf_inf_p(&x) && !ppf_signbit(&x));
    ppf_set_nan(&x, -1);
    CHECK(ppf_nan_p(&x) && ppf_signbit(&x));
    CHECK(ppf_set_prec(&x, 257) == 0);
    CHECK(ppf_get_prec(&x) == 257 && ppf_nan_p(&x));
    ppf_swap(&x, &y);
    CHECK(ppf_get_prec(&x) == 113 && ppf_get_prec(&y) == 257);
    CHECK(ppf_validate(&x));
    CHECK(ppf_validate(&y));
    ppf_clear(&x);
    ppf_clear(&y);
}

static void test_rounding(void) {
    ppf_context_t ctx;
    ppf_t x;
    mpz_t z;
    int t;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 4, &ctx) == 0);
    mpz_init(z);

    mpz_set_ui(z, 31);
    CHECK(ppf_context_set_rounding(&ctx, PPF_RNDU) == 0);
    t = ppf_set_z(&x, z, PPF_RND_DEFAULT, &ctx);
    CHECK(t > 0); expect_exact(&x, 32);
    CHECK(ppf_context_set_rounding(&ctx, PPF_RNDN) == 0);
    t = ppf_set_z(&x, z, PPF_RNDZ, &ctx);
    CHECK(t < 0); expect_exact(&x, 30); CHECK(ppf_validate(&x));
    t = ppf_set_z(&x, z, PPF_RNDU, &ctx);
    CHECK(t > 0); expect_exact(&x, 32);

    mpz_set_si(z, -31);
    t = ppf_set_z(&x, z, PPF_RNDZ, &ctx);
    CHECK(t > 0); expect_exact(&x, -30);
    t = ppf_set_z(&x, z, PPF_RNDD, &ctx);
    CHECK(t < 0); expect_exact(&x, -32);

    mpz_set_ui(z, 17); /* 8.5 ulps at four bits: tie to even 8 -> 16 */
    t = ppf_set_z(&x, z, PPF_RNDN, &ctx);
    CHECK(t < 0); expect_exact(&x, 16);
    mpz_set_ui(z, 19); /* tie to odd 9 -> 10 -> 20 */
    t = ppf_set_z(&x, z, PPF_RNDN, &ctx);
    CHECK(t > 0); expect_exact(&x, 20);

    mpz_set_ui(z, 15);
    t = ppf_set_z(&x, z, PPF_RNDN, &ctx);
    CHECK(t == 0); expect_exact(&x, 15);
    CHECK(ppf_context_flags(&ctx) & PPF_FLAG_INEXACT);

    mpz_clear(z);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
}

static void test_range(void) {
    ppf_context_t ctx;
    ppf_t x;
    mpz_t z;
    int t;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_context_set_range(&ctx, -2, 2) == 0);
    CHECK(ppf_init2_ctx(&x, 4, &ctx) == 0);
    mpz_init(z);

    mpz_set_ui(z, 4);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    t = ppf_set_z(&x, z, PPF_RNDN, &ctx);
    CHECK(ppf_inf_p(&x) && t > 0);
    CHECK((ppf_context_flags(&ctx) &
           (PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT)) ==
          (PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT));

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    t = ppf_set_z(&x, z, PPF_RNDZ, &ctx);
    CHECK(ppf_regular_p(&x) && t < 0 && x.exponent == 2);

    mpz_set_ui(z, 1);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    t = ppf_set_z_2exp(&x, z, -5, PPF_RNDN, &ctx); /* 0.03125 */
    CHECK(ppf_zero_p(&x) && t < 0);
    CHECK(ppf_context_flags(&ctx) & PPF_FLAG_UNDERFLOW);

    mpz_set_ui(z, 3);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    t = ppf_set_z_2exp(&x, z, -5, PPF_RNDN, &ctx); /* 0.09375 */
    CHECK(ppf_regular_p(&x) && x.exponent == -2 && t > 0);

    mpz_set_ui(z, 1);
    t = ppf_set_z_2exp(&x, z, -4, PPF_RNDN, &ctx); /* halfway 0.0625 */
    CHECK(ppf_zero_p(&x) && t < 0);

    mpz_set_si(z, -3);
    t = ppf_set_z_2exp(&x, z, -5, PPF_RNDN, &ctx);
    CHECK(ppf_regular_p(&x) && ppf_signbit(&x) && t < 0);

    mpz_clear(z);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
}

static void test_precision_copy_compare(void) {
    ppf_context_t ctx;
    ppf_t a, b, c;
    mpz_t z;
    int t;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 200, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&c, 200, &ctx) == 0);
    mpz_init(z);
    mpz_ui_pow_ui(z, 3, 100);
    CHECK(ppf_set_z(&a, z, PPF_RNDN, &ctx) == 0);
    t = ppf_set(&b, &a, PPF_RNDN, &ctx);
    CHECK(t != 0 && ppf_validate(&b));
    CHECK(ppf_set(&c, &b, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_cmp(&a, &b, &ctx) != 0);
    CHECK(ppf_equal_p(&b, &c));
    CHECK(ppf_prec_round(&a, 53, PPF_RNDN, &ctx) != 0);
    CHECK(ppf_equal_p(&a, &b));
    CHECK(ppf_prec_round(&a, 257, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_equal_p(&a, &b));

    CHECK(ppf_neg(&c, &a, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_cmp(&c, &a, &ctx) < 0);
    CHECK(ppf_abs(&c, &c, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_equal_p(&c, &a));

    mpz_clear(z);
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&c);
    ppf_context_clear(&ctx);
}

static void test_interval(void) {
    ppf_context_t ctx;
    ppf_t x;
    mpz_t lo, hi;
    int ternary = 99;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 3, &ctx) == 0);
    mpz_inits(lo, hi, NULL);

    mpz_set_ui(lo, 25);
    mpz_set_ui(hi, 26);
    CHECK(ppf_set_dyadic_interval(&x, lo, hi, -3, PPF_RNDN,
                                  &ternary, &ctx) == PPF_INTERVAL_DECIDED);
    CHECK(ternary < 0); /* rounded value 3 is below the full interval */

    mpz_set_ui(lo, 24);
    mpz_set_ui(hi, 25);
    CHECK(ppf_set_dyadic_interval(&x, lo, hi, -3, PPF_RNDN,
                                  &ternary, &ctx) == PPF_INTERVAL_UNDECIDED);

    mpz_clears(lo, hi, NULL);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
}

typedef struct { int id; int ok; } thread_arg_t;

static void *thread_main(void *opaque) {
    thread_arg_t *arg = (thread_arg_t *)opaque;
    ppf_context_t *ctx = ppf_default_context();
    ppf_t x;
    mpz_t z;
    if (!ctx) return NULL;
    ctx->default_precision = (ppf_prec_t)(100 + arg->id);
    ctx->flags = 0;
    if (ppf_init(&x) != 0) return NULL;
    mpz_init_set_ui(z, 31);
    (void)ppf_set_z(&x, z, PPF_RNDN, ctx);
    arg->ok = x.precision == (ppf_prec_t)(100 + arg->id) &&
              (ctx->flags & PPF_FLAG_INEXACT) == 0; /* 31 is exact */
    mpz_clear(z);
    ppf_clear(&x);
    ppf_default_context_cleanup();
    return NULL;
}

static void test_thread_local_context(void) {
    pthread_t threads[4];
    thread_arg_t args[4];
    for (int i = 0; i < 4; ++i) {
        args[i].id = i; args[i].ok = 0;
        CHECK(pthread_create(&threads[i], NULL, thread_main, &args[i]) == 0);
    }
    for (int i = 0; i < 4; ++i) {
        CHECK(pthread_join(threads[i], NULL) == 0);
        CHECK(args[i].ok);
    }
}

int main(void) {
    test_context();
    test_custom_allocator();
    test_raw_significand_access();
    test_lifetime_and_specials();
    test_rounding();
    test_range();
    test_precision_copy_compare();
    test_interval();
    test_thread_local_context();
    puts("POLYPHASE FLOAT OBJECT TESTS PASSED");
    return 0;
}
