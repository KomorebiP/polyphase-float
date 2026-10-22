/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "polyphase_float.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

typedef struct {
    size_t calls, fail_at, failed, live;
} fault_state_t;

typedef struct {
    ppf_t object;
    mp_limb_t limbs[80];
} snapshot_t;

static int fail_now(fault_state_t *s) {
    ++s->calls;
    if (s->fail_at && s->calls == s->fail_at) {
        ++s->failed;
        return 1;
    }
    return 0;
}

static void *fault_malloc(void *opaque, size_t bytes) {
    fault_state_t *s = opaque;
    void *p;
    if (fail_now(s)) return NULL;
    p = malloc(bytes);
    if (p) ++s->live;
    return p;
}

static void *fault_realloc(void *opaque, void *ptr,
                           size_t old_bytes, size_t new_bytes) {
    fault_state_t *s = opaque;
    void *next;
    if (fail_now(s)) return NULL;
    next = malloc(new_bytes);
    if (!next) return NULL;
    if (ptr) {
        memcpy(next, ptr, old_bytes < new_bytes ? old_bytes : new_bytes);
        free(ptr);
    } else ++s->live;
    return next;
}

static void fault_free(void *opaque, void *ptr, size_t bytes) {
    fault_state_t *s = opaque;
    (void)bytes;
    if (ptr) {
        CHECK(s->live > 0);
        --s->live;
        free(ptr);
    }
}

static void context_setup(ppf_context_t *ctx, fault_state_t *s) {
    ppf_allocator_t allocator = {
        fault_malloc, fault_realloc, fault_free, s
    };
    CHECK(ppf_context_init_custom(ctx, &allocator) == 0);
}

static void arm(fault_state_t *s, ppf_context_t *ctx, size_t fail_at) {
    s->calls = s->failed = 0;
    s->fail_at = fail_at;
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
}

static void snapshot_take(snapshot_t *save, const ppf_t *x) {
    CHECK(x->allocated_limbs <= 80);
    memcpy(&save->object, x, sizeof(*x));
    memcpy(save->limbs, x->limbs,
           x->allocated_limbs * sizeof(*x->limbs));
}

static void unchanged(const snapshot_t *save, const ppf_t *x) {
    CHECK(memcmp(&save->object, x, sizeof(*x)) == 0);
    CHECK(memcmp(save->limbs, x->limbs,
                 x->allocated_limbs * sizeof(*x->limbs)) == 0);
}

static void test_precision_failure(void) {
    for (size_t fail_at = 1; fail_at <= 2; ++fail_at) {
        fault_state_t s = {0};
        ppf_context_t ctx;
        ppf_t x;
        snapshot_t save;
        context_setup(&ctx, &s);
        CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
        CHECK(ppf_set_d(&x, -1.375, PPF_RNDN, &ctx) == 0);
        snapshot_take(&save, &x);
        arm(&s, &ctx, fail_at);
        CHECK(ppf_prec_round(&x, 257, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR);
        CHECK(s.failed == 1);
        CHECK(ppf_context_flags(&ctx) == PPF_FLAG_ALLOC);
        unchanged(&save, &x);
        arm(&s, &ctx, 0);
        CHECK(ppf_prec_round(&x, 257, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_get_prec(&x) == 257 && ppf_validate(&x));
        CHECK(ppf_get_d(&x, PPF_RNDN, &ctx) == -1.375);
        CHECK(ppf_context_flags(&ctx) == 0);
        ppf_clear(&x);
        ppf_context_clear(&ctx);
        CHECK(s.live == 0);
    }

    for (int kind = 0; kind < 4; ++kind) {
        fault_state_t s = {0};
        ppf_context_t ctx;
        ppf_t x;
        snapshot_t save;
        context_setup(&ctx, &s);
        CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
        if (kind == 0) CHECK(ppf_set_d(&x, -1.375, PPF_RNDN, &ctx) == 0);
        else if (kind == 1) ppf_set_zero(&x, -1);
        else if (kind == 2) ppf_set_inf(&x, -1);
        else ppf_set_nan(&x, -1);
        snapshot_take(&save, &x);
        arm(&s, &ctx, 1);
        CHECK(ppf_prec_round(&x, 2, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR);
        CHECK(s.failed == 1 && ppf_context_flags(&ctx) == PPF_FLAG_ALLOC);
        unchanged(&save, &x);
        arm(&s, &ctx, 0);
        CHECK(ppf_prec_round(&x, 2, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        CHECK(ppf_validate(&x) && ppf_signbit(&x));
        if (kind) {
            CHECK(x.kind == save.object.kind);
            CHECK(ppf_context_flags(&ctx) == 0);
        }
        ppf_clear(&x);
        ppf_context_clear(&ctx);
        CHECK(s.live == 0);
    }
}

static void test_invalid_rounding(void) {
    fault_state_t s = {0};
    ppf_context_t ctx;
    ppf_t x, y;
    snapshot_t save;
    mpz_t z;
    context_setup(&ctx, &s);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&y, 53, &ctx) == 0);
    CHECK(ppf_set_d(&x, -1.375, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_d(&y, 2.25, PPF_RNDN, &ctx) == 0);
    snapshot_take(&save, &x);
    arm(&s, &ctx, 0);
    CHECK(ppf_prec_round(&x, 257, (ppf_rnd_t)999, &ctx) == PPF_TERNARY_ERROR);
    CHECK(ppf_context_flags(&ctx) == PPF_FLAG_ERANGE && s.calls == 0);
    unchanged(&save, &x);
    CHECK(ppf_prec_round(&x, 0, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR);
    unchanged(&save, &x);
    CHECK(ppf_set(&x, &y, (ppf_rnd_t)999, &ctx) == PPF_TERNARY_ERROR);
    unchanged(&save, &x);
    CHECK(ppf_neg(&x, &y, (ppf_rnd_t)999, &ctx) == PPF_TERNARY_ERROR);
    unchanged(&save, &x);
    CHECK(ppf_neg(&x, &x, (ppf_rnd_t)999, &ctx) == PPF_TERNARY_ERROR);
    unchanged(&save, &x);
    CHECK(ppf_abs(&x, &y, (ppf_rnd_t)999, &ctx) == PPF_TERNARY_ERROR);
    unchanged(&save, &x);
    mpz_init_set_ui(z, 11);
    CHECK(ppf_set_z_2exp(&x, z, -3, (ppf_rnd_t)999, &ctx) == PPF_TERNARY_ERROR);
    unchanged(&save, &x);
    mpz_clear(z);
    ppf_clear(&x); ppf_clear(&y);
    ppf_context_clear(&ctx);
    CHECK(s.live == 0);
}

static void test_sign_rounding(void) {
    ppf_context_t ctx;
    ppf_t source, got, expected;
    mpq_t exact;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&source, 53, &ctx) == 0);
    mpq_init(exact);
    for (ppf_prec_t p = 2; p <= 5; ++p) {
        CHECK(ppf_init2_ctx(&got, p, &ctx) == 0);
        CHECK(ppf_init2_ctx(&expected, p, &ctx) == 0);
        for (int sign = -1; sign <= 1; sign += 2) {
            CHECK(ppf_set_d(&source, sign * 1.375, PPF_RNDN, &ctx) == 0);
            for (int mode = PPF_RNDN; mode <= PPF_RNDF; ++mode) {
                int gt, et;
                mpq_set_si(exact, -sign * 11, 8);
                et = ppf_set_q(&expected, exact, (ppf_rnd_t)mode, &ctx);
                gt = ppf_neg(&got, &source, (ppf_rnd_t)mode, &ctx);
                CHECK(gt == et && ppf_equal_p(&got, &expected));
                mpq_set_ui(exact, 11, 8);
                et = ppf_set_q(&expected, exact, (ppf_rnd_t)mode, &ctx);
                gt = ppf_abs(&got, &source, (ppf_rnd_t)mode, &ctx);
                CHECK(gt == et && ppf_equal_p(&got, &expected));
            }
        }
        ppf_clear(&got); ppf_clear(&expected);
    }
    mpq_clear(exact);
    ppf_clear(&source);
    ppf_context_clear(&ctx);
}

static void test_fmod_ui_failure(ppf_prec_t precision, int overlap,
                                 int external) {
    size_t expected_allocations = 0;
    for (size_t fail_at = 0;
         fail_at == 0 || fail_at <= expected_allocations; ++fail_at) {
        fault_state_t s = {0};
        ppf_context_t ctx;
        ppf_t source, destination;
        ppf_t *out;
        mp_limb_t storage[80] = {0};
        snapshot_t save;
        context_setup(&ctx, &s);
        CHECK(ppf_init2_ctx(&source, precision, &ctx) == 0);
        CHECK(ppf_set_d(&source, 7.25, PPF_RNDN, &ctx) == 0);
        if (external) {
            CHECK(ppf_custom_init_set(&destination, PPF_ZERO_KIND, 0,
                                      precision, storage) == 0);
        } else CHECK(ppf_init2_ctx(&destination, precision, &ctx) == 0);
        CHECK(ppf_set_d(&destination, -2.5, PPF_RNDN, &ctx) == 0);
        out = overlap ? &source : &destination;
        snapshot_take(&save, out);
        arm(&s, &ctx, fail_at);
        if (fail_at == 0) {
            CHECK(ppf_fmod_ui(out, &source, 3, PPF_RNDN, &ctx) == 0);
            expected_allocations = s.calls;
            CHECK(expected_allocations > 0);
        } else {
            CHECK(ppf_fmod_ui(out, &source, 3, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR);
            CHECK(s.failed == 1 && ppf_context_flags(&ctx) == PPF_FLAG_ALLOC);
            unchanged(&save, out);
            arm(&s, &ctx, 0);
            CHECK(ppf_fmod_ui(out, &source, 3, PPF_RNDN, &ctx) == 0);
        }
        CHECK(s.calls == expected_allocations);
        CHECK(ppf_validate(out) && ppf_get_d(out, PPF_RNDN, &ctx) == 1.25);
        CHECK(out->limbs == save.object.limbs);
        CHECK(out->storage_flags == save.object.storage_flags);
        CHECK(ppf_context_flags(&ctx) == 0);
        ppf_clear(&destination); ppf_clear(&source);
        ppf_context_clear(&ctx);
        CHECK(s.live == 0);
    }
}

static void test_interval_failure(int external) {
    for (size_t fail_at = 1; fail_at <= 2; ++fail_at) {
        fault_state_t s = {0};
        ppf_context_t ctx;
        ppf_t destination;
        mp_limb_t storage[80] = {0};
        mpz_t lower, upper;
        snapshot_t save;
        int ternary = 777;
        context_setup(&ctx, &s);
        if (external) {
            CHECK(ppf_custom_init_set(&destination, PPF_ZERO_KIND, 0,
                                      128, storage) == 0);
        } else CHECK(ppf_init2_ctx(&destination, 128, &ctx) == 0);
        CHECK(ppf_set_d(&destination, -2.5, PPF_RNDN, &ctx) == 0);
        mpz_init_set_ui(lower, 11); mpz_init_set_ui(upper, 11);
        snapshot_take(&save, &destination);
        arm(&s, &ctx, fail_at);
        ppf_context_flags_set(&ctx, PPF_FLAG_OVERFLOW);
        CHECK(ppf_set_dyadic_interval(&destination, lower, upper, -3,
                                      PPF_RNDN, &ternary, &ctx) == PPF_INTERVAL_ERROR);
        CHECK(s.failed == 1);
        CHECK(ppf_context_flags(&ctx) == (PPF_FLAG_OVERFLOW | PPF_FLAG_ALLOC));
        CHECK(ternary == 777);
        unchanged(&save, &destination);
        arm(&s, &ctx, 0);
        CHECK(ppf_set_dyadic_interval(&destination, lower, upper, -3,
                                      PPF_RNDN, &ternary, &ctx) == PPF_INTERVAL_DECIDED);
        CHECK(s.calls == 2 && ternary == 0);
        CHECK(ppf_get_d(&destination, PPF_RNDN, &ctx) == 1.375);
        CHECK(destination.limbs == save.object.limbs);
        CHECK(destination.storage_flags == save.object.storage_flags);
        CHECK(ppf_context_flags(&ctx) == 0);

        snapshot_take(&save, &destination);
        ternary = 777;
        mpz_set_ui(upper, 13);
        CHECK(ppf_set_dyadic_interval(&destination, lower, upper, -3,
                                      PPF_RNDN, &ternary, &ctx) == PPF_INTERVAL_UNDECIDED);
        CHECK(ternary == 777);
        unchanged(&save, &destination);
        CHECK(ppf_set_dyadic_interval(&destination, lower, upper, -3,
                                      (ppf_rnd_t)999, &ternary, &ctx) == PPF_INTERVAL_ERROR);
        CHECK(ternary == 777);
        CHECK(ppf_context_flags(&ctx) == PPF_FLAG_ERANGE);
        unchanged(&save, &destination);
        mpz_set_ui(lower, 15);
        CHECK(ppf_set_dyadic_interval(&destination, lower, upper, -3,
                                      PPF_RNDN, &ternary, &ctx) == PPF_INTERVAL_ERROR);
        CHECK(ternary == 777);
        unchanged(&save, &destination);

        mpz_clear(lower); mpz_clear(upper);
        ppf_clear(&destination);
        ppf_context_clear(&ctx);
        CHECK(s.live == 0);
    }
}

static void test_interval_ternary(void) {
    ppf_context_t ctx;
    ppf_t destination;
    mpz_t lower, upper;
    snapshot_t save;
    int ternary = 777;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&destination, 2, &ctx) == 0);
    mpz_init_set_ui(lower, 11); mpz_init_set_ui(upper, 11);
    CHECK(ppf_set_dyadic_interval(&destination, lower, upper, -3,
                                  PPF_RNDN, &ternary, &ctx) == PPF_INTERVAL_DECIDED);
    CHECK(ternary == 1 && ppf_get_d(&destination, PPF_RNDN, &ctx) == 1.5);
    snapshot_take(&save, &destination);
    ternary = 777;
    mpz_set_ui(upper, 13);
    CHECK(ppf_set_dyadic_interval(&destination, lower, upper, -3,
                                  PPF_RNDN, &ternary, &ctx) == PPF_INTERVAL_UNDECIDED);
    CHECK(ternary == 777);
    unchanged(&save, &destination);
    mpz_clear(lower); mpz_clear(upper);
    ppf_clear(&destination);
    ppf_context_clear(&ctx);
}

int main(void) {
    test_precision_failure();
    test_invalid_rounding();
    test_sign_rounding();
    test_fmod_ui_failure(128, 0, 0);
    test_fmod_ui_failure(128, 1, 0);
    test_fmod_ui_failure(128, 0, 1);
    test_fmod_ui_failure(2049, 0, 0);
    test_fmod_ui_failure(2049, 1, 0);
    test_fmod_ui_failure(2049, 0, 1);
    test_interval_failure(0);
    test_interval_failure(1);
    test_interval_ternary();
    ppf_thread_cleanup();
    puts("precision, sign rounding, and remainder failure regressions: PASS");
    return 0;
}
