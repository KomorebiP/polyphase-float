/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

typedef struct {
    size_t calls, fail_at, failures, live_blocks, live_bytes;
} allocation_state;

typedef struct {
    size_t bytes;
    max_align_t alignment;
    unsigned char data[];
} allocation_record;

static allocation_record *record_for(void *ptr) {
    return (allocation_record *)((unsigned char *)ptr -
                                offsetof(allocation_record, data));
}

static int should_fail(allocation_state *s) {
    ++s->calls;
    if (s->fail_at && s->calls == s->fail_at) {
        ++s->failures;
        return 1;
    }
    return 0;
}

static void *tracked_malloc(void *opaque, size_t bytes) {
    allocation_state *s = opaque;
    allocation_record *record;
    if (should_fail(s)) return NULL;
    CHECK(bytes <= SIZE_MAX - offsetof(allocation_record, data));
    record = malloc(offsetof(allocation_record, data) + bytes);
    CHECK(record != NULL);
    record->bytes = bytes;
    ++s->live_blocks;
    s->live_bytes += bytes;
    return record->data;
}

static void *tracked_realloc(void *opaque, void *ptr,
                             size_t old_bytes, size_t new_bytes) {
    allocation_state *s = opaque;
    allocation_record *record = ptr ? record_for(ptr) : NULL;
    if (record) CHECK(record->bytes == old_bytes);
    else CHECK(old_bytes == 0);
    if (should_fail(s)) return NULL;
    CHECK(new_bytes <= SIZE_MAX - offsetof(allocation_record, data));
    record = realloc(record, offsetof(allocation_record, data) + new_bytes);
    CHECK(record != NULL);
    record->bytes = new_bytes;
    if (!ptr) ++s->live_blocks;
    s->live_bytes -= old_bytes;
    s->live_bytes += new_bytes;
    return record->data;
}

static void tracked_free(void *opaque, void *ptr, size_t bytes) {
    allocation_state *s = opaque;
    allocation_record *record;
    if (!ptr) return;
    record = record_for(ptr);
    CHECK(record->bytes == bytes);
    CHECK(s->live_blocks > 0 && s->live_bytes >= bytes);
    --s->live_blocks;
    s->live_bytes -= bytes;
    free(record);
}

enum random_operation { URANDOMB, URANDOM, ERANDOM, NRANDOM, GRANDOM };
static const char *const operation_names[] = {
    "urandomb", "urandom", "erandom", "nrandom", "grandom"
};

static int call_random(enum random_operation op, ppf_t *a, ppf_t *b,
                       gmp_randstate_t state, ppf_rnd_t rnd,
                       ppf_context_t *ctx) {
    switch (op) {
        case URANDOMB: return ppf_urandomb(a, state);
        case URANDOM: return ppf_urandom(a, state, rnd, ctx);
        case ERANDOM: return ppf_erandom(a, state, rnd, ctx);
        case NRANDOM: return ppf_nrandom(a, state, rnd, ctx);
        case GRANDOM: return ppf_grandom(a, b, state, rnd, ctx);
    }
    return PPF_TERNARY_ERROR;
}

static size_t exercise_random(enum random_operation op, ppf_prec_t precision,
                               size_t fail_at, int external,
                               ppf_rnd_t rnd) {
    allocation_state s = {0};
    ppf_allocator_t allocator = {tracked_malloc, tracked_realloc, tracked_free, &s};
    ppf_context_t explicit_ctx;
    ppf_context_t *ctx;
    ppf_t a, b, a_before, b_before;
    mp_limb_t a_storage[32] = {0}, b_storage[32] = {0};
    mp_limb_t a_copy[32], b_copy[32];
    gmp_randstate_t random_state;
    size_t calls;
    int result;

    ppf_free_cache();
    ppf_thread_cleanup();
    if (op == URANDOMB) {
        ctx = ppf_default_context();
        CHECK(ctx != NULL);
        ctx->allocator = allocator;
    } else {
        CHECK(ppf_context_init_custom(&explicit_ctx, &allocator) == 0);
        ctx = &explicit_ctx;
    }
    if (external) {
        CHECK(ppf_custom_get_size(precision) <= sizeof(a_storage));
        CHECK(ppf_custom_init_set(&a, PPF_ZERO_KIND, 0, precision, a_storage) == 0);
        CHECK(ppf_custom_init_set(&b, PPF_ZERO_KIND, 0, precision, b_storage) == 0);
    } else {
        CHECK(ppf_init2_ctx(&a, precision, ctx) == 0);
        CHECK(ppf_init2_ctx(&b, precision, ctx) == 0);
    }
    CHECK(ppf_set_ui(&a, 7, PPF_RNDN, ctx) == 0);
    CHECK(ppf_set_si(&b, -11, PPF_RNDN, ctx) == 0);
    CHECK(a.allocated_limbs <= 32 && b.allocated_limbs <= 32);
    a_before = a; b_before = b;
    memcpy(a_copy, a.limbs, a.allocated_limbs * sizeof(*a_copy));
    memcpy(b_copy, b.limbs, b.allocated_limbs * sizeof(*b_copy));
    ctx->flags = PPF_FLAG_DIVBY0 | PPF_FLAG_ALLOC;
    s.calls = s.failures = 0;
    s.fail_at = fail_at;
    gmp_randinit_mt(random_state);
    gmp_randseed_ui(random_state, 0x51a73UL);
    result = call_random(op, &a, &b, random_state, rnd, ctx);
    calls = s.calls;
    if (s.failures) {
        CHECK(result == (op == URANDOMB ? 1 : PPF_TERNARY_ERROR));
        CHECK((ctx->flags & PPF_FLAG_ALLOC) != 0);
        CHECK(memcmp(&a, &a_before, sizeof(a)) == 0);
        CHECK(memcmp(&b, &b_before, sizeof(b)) == 0);
        CHECK(memcmp(a.limbs, a_copy, a.allocated_limbs * sizeof(*a_copy)) == 0);
        CHECK(memcmp(b.limbs, b_copy, b.allocated_limbs * sizeof(*b_copy)) == 0);
    } else {
        CHECK(fail_at == 0);
        CHECK(result != PPF_TERNARY_ERROR);
        CHECK(op != URANDOMB || result == 0);
        CHECK(ppf_validate(&a) && ppf_validate(&b));
        CHECK(a.limbs == a_before.limbs && a.allocator.opaque == a_before.allocator.opaque);
        CHECK(b.limbs == b_before.limbs && b.allocator.opaque == b_before.allocator.opaque);
        CHECK(a.storage_flags == a_before.storage_flags && b.storage_flags == b_before.storage_flags);
    }
    CHECK((ctx->flags & (PPF_FLAG_DIVBY0 | PPF_FLAG_ALLOC)) ==
          (PPF_FLAG_DIVBY0 | PPF_FLAG_ALLOC));
    s.fail_at = 0;
    ppf_clear(&a); ppf_clear(&b);
    if (op == URANDOMB) ppf_thread_cleanup();
    else ppf_context_clear(ctx);
    ppf_free_cache();
    ppf_thread_cleanup();
    gmp_randclear(random_state);
    CHECK(s.live_blocks == 0 && s.live_bytes == 0);
    return calls;
}

static void test_random_failure_sweeps(void) {
    for (int op = URANDOMB; op <= GRANDOM; ++op) {
        size_t calls = exercise_random((enum random_operation)op, 24, 0, 0, PPF_RNDN);
        CHECK(calls > 0);
        for (size_t fail = 1; fail <= calls; ++fail)
            (void)exercise_random((enum random_operation)op, 24, fail, 0, PPF_RNDN);
        /* Cross the inline staging boundary and exercise external storage. */
        (void)exercise_random((enum random_operation)op, 1100, 1, 0, PPF_RNDN);
        (void)exercise_random((enum random_operation)op, 24, 0, 1, PPF_RNDN);
        (void)exercise_random((enum random_operation)op, 24, 1, 1, PPF_RNDN);
        for (int rnd = PPF_RNDZ; rnd <= PPF_RNDF; ++rnd)
            (void)exercise_random((enum random_operation)op, 24, 1, 0, (ppf_rnd_t)rnd);
        printf("random %s: %zu fail-at-N positions passed\n", operation_names[op], calls);
    }
}

static void test_invalid_custom_kind(void) {
    mp_limb_t storage = 0;
    ppf_t object, before;
    memset(&object, 0x5a, sizeof(object));
    before = object;
    CHECK(ppf_custom_init_set(&object, INT_MIN, 0, 24, &storage) == -1);
    CHECK(memcmp(&object, &before, sizeof(object)) == 0);
    CHECK(ppf_custom_init_set(&object, INT_MAX, 0, 24, &storage) == -1);
    CHECK(memcmp(&object, &before, sizeof(object)) == 0);
}

static void test_optional_normal_output(void) {
    ppf_context_t ctx;
    ppf_t a, b;
    gmp_randstate_t first, second;
    int raw, encoded;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 24, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 24, &ctx) == 0);
    gmp_randinit_mt(first); gmp_randinit_mt(second);
    gmp_randseed_ui(first, 934UL); gmp_randseed_ui(second, 934UL);
    raw = ppf_nrandom(&a, first, PPF_RNDN, &ctx);
    encoded = ppf_grandom(&b, NULL, second, PPF_RNDN, &ctx);
    CHECK(raw != PPF_TERNARY_ERROR);
    CHECK(encoded == (raw > 0 ? 1 : raw < 0 ? 2 : 0));
    CHECK(ppf_equal_p(&a, &b));
    ppf_clear(&a); ppf_clear(&b); ppf_context_clear(&ctx);
    gmp_randclear(first); gmp_randclear(second);
    ppf_free_cache(); ppf_thread_cleanup();
}

static size_t exercise_reldiff(size_t fail_at, int alias, int external) {
    allocation_state s = {0};
    ppf_allocator_t allocator = {tracked_malloc, tracked_realloc, tracked_free, &s};
    ppf_context_t ctx;
    ppf_t a, b, output, before;
    ppf_t *destination;
    mp_limb_t storage[1] = {0}, copy[1];
    size_t calls;
    CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
    CHECK(ppf_init2_ctx(&a, 24, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 24, &ctx) == 0);
    if (external)
        CHECK(ppf_custom_init_set(&output, PPF_ZERO_KIND, 0, 24, storage) == 0);
    else CHECK(ppf_init2_ctx(&output, 24, &ctx) == 0);
    CHECK(ppf_set_ui(&a, 3, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_ui(&b, 1, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_ui(&output, 7, PPF_RNDN, &ctx) == 0);
    destination = alias ? &a : &output;
    before = *destination;
    memcpy(copy, destination->limbs, sizeof(copy));
    ctx.flags = PPF_FLAG_DIVBY0;
    s.calls = 0; s.fail_at = fail_at;
    ppf_reldiff(destination, &a, &b, PPF_RNDN, &ctx);
    calls = s.calls;
    if (s.failures) {
        CHECK((ctx.flags & PPF_FLAG_ALLOC) != 0);
        CHECK(memcmp(destination, &before, sizeof(before)) == 0);
        CHECK(memcmp(destination->limbs, copy, sizeof(copy)) == 0);
    } else {
        CHECK(fail_at == 0);
        CHECK(ppf_validate(destination));
        CHECK(destination->limbs == before.limbs);
        CHECK(destination->storage_flags == before.storage_flags);
        CHECK(ppf_get_d(destination, PPF_RNDN, &ctx) > 0.6666);
        CHECK(ppf_get_d(destination, PPF_RNDN, &ctx) < 0.6667);
    }
    CHECK((ctx.flags & PPF_FLAG_DIVBY0) != 0);
    s.fail_at = 0;
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&output);
    ppf_context_clear(&ctx);
    CHECK(s.live_blocks == 0 && s.live_bytes == 0);
    return calls;
}

static void test_reldiff_failure_sweep(void) {
    size_t calls = exercise_reldiff(0, 0, 0);
    CHECK(calls > 0);
    for (size_t fail = 1; fail <= calls; ++fail) {
        (void)exercise_reldiff(fail, 0, 0);
        (void)exercise_reldiff(fail, 1, 0);
        (void)exercise_reldiff(fail, 0, 1);
    }
    printf("reldiff: %zu fail-at-N positions passed with ordinary, alias, and external outputs\n", calls);
}

static void test_text_no_conversion(void) {
    static const char *const invalid[] = {
        "", " ", "\t\n", "+", "-", ".", "  . ", "  +  "
    };
    ppf_context_t ctx;
    ppf_t x, before;
    mp_limb_t copy;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 24, &ctx) == 0);
    CHECK(ppf_set_ui(&x, 7, PPF_RNDN, &ctx) == 0);
    before = x;
    copy = x.limbs[0];
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(ppf_set_str(&x, invalid[i], 0, PPF_RNDN, &ctx) == -1);
        CHECK(memcmp(&x, &before, sizeof(x)) == 0);
        CHECK(x.limbs[0] == copy);
    }
    CHECK(ppf_set_str(&x, "  0  ", 0, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_zero_p(&x));
    ppf_clear(&x); ppf_context_clear(&ctx);
}

static void test_minimum_dynamic_width(void) {
    char buffer[32], before[32];
    char *text = (char *)1;
    ppf_t x;
    CHECK(ppf_init2(&x, 24) == 0);
    CHECK(ppf_set_ui(&x, 7, PPF_RNDN, NULL) == 0);
    memset(buffer, 0x5a, sizeof(buffer));
    memcpy(before, buffer, sizeof(buffer));
    ppf_clear_flags(); errno = 0;
    CHECK(ppf_snprintf(buffer, sizeof(buffer), "%*Rg", INT_MIN, &x) == -1);
    CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
    CHECK(ppf_erangeflag_p());
#ifdef EOVERFLOW
    CHECK(errno == EOVERFLOW);
#endif
    CHECK(ppf_asprintf(&text, "%*d", INT_MIN, 7) == -1);
    CHECK(text == NULL);
    CHECK(ppf_snprintf(buffer, sizeof(buffer), "%*d", -4, 7) == 4);
    CHECK(strcmp(buffer, "7   ") == 0);
    ppf_clear(&x); ppf_thread_cleanup();
}

static void test_finite_number_predicate(void) {
    ppf_t x;
    CHECK(ppf_init2(&x, 24) == 0);
    CHECK(!ppf_number_p(NULL));
    ppf_set_nan(&x, 1); CHECK(!ppf_number_p(&x));
    ppf_set_inf(&x, 1); CHECK(!ppf_number_p(&x));
    ppf_set_inf(&x, -1); CHECK(!ppf_number_p(&x));
    ppf_set_zero(&x, 1); CHECK(ppf_number_p(&x));
    ppf_set_zero(&x, -1); CHECK(ppf_number_p(&x));
    CHECK(ppf_set_ui(&x, 7, PPF_RNDN, NULL) == 0);
    CHECK(ppf_number_p(&x));
    ppf_clear(&x); ppf_thread_cleanup();
}

static void test_text_power_limit_range(void) {
    ppf_context_t ctx;
    ppf_t actual, expected, before;
    mpz_t one;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_context_set_range(&ctx, -16, 16) == 0);
    CHECK(ppf_init2_ctx(&actual, 24, &ctx) == 0);
    CHECK(ppf_init2_ctx(&expected, 24, &ctx) == 0);
    mpz_init(one);
    for (int sign = -1; sign <= 1; sign += 2) {
        for (int rnd = PPF_RNDN; rnd <= PPF_RNDA; ++rnd) {
            for (int overflow = 0; overflow <= 1; ++overflow) {
                const char *text = overflow ? (sign < 0 ? "-1e2000001" : "1e2000001")
                                            : (sign < 0 ? "-1e-2000001" : "1e-2000001");
                char *end = NULL;
                ppf_flags_t expected_flags;
                int oracle, result;
                mpz_set_si(one, sign);
                ctx.flags = 0;
                /* An exact dyadic well beyond the same range boundary has
                   the same directed result and ternary sign. */
                oracle = ppf_set_z_2exp(&expected, one, overflow ? 26 : -26,
                                        (ppf_rnd_t)rnd, &ctx);
                expected_flags = ctx.flags;
                ctx.flags = 0;
                result = ppf_strtofr(&actual, text, &end, 10, (ppf_rnd_t)rnd, &ctx);
                CHECK(result != PPF_TERNARY_ERROR);
                CHECK((result > 0) - (result < 0) == (oracle > 0) - (oracle < 0));
                CHECK(end != NULL && *end == '\0');
                CHECK(actual.kind == expected.kind && actual.sign == expected.sign);
                CHECK(ppf_equal_p(&actual, &expected));
                CHECK(ctx.flags == expected_flags);
            }
        }
    }
    {
        char large_significand[272];
        memset(large_significand, '0', 256);
        large_significand[0] = '1';
        strcpy(large_significand + 256, "@2000001");
        CHECK(ppf_context_set_range(&ctx, -2000100, 2000150) == 0);
        ctx.flags = 0;
        CHECK(ppf_set_str(&actual, large_significand, 2, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_inf_p(&actual));
        CHECK(ctx.flags == (PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT));

        strcpy(large_significand + 256, "@-2000001");
        CHECK(ppf_context_set_range(&ctx, -1999900, 16) == 0);
        CHECK(ppf_set_ui(&actual, 7, PPF_RNDN, &ctx) == 0);
        before = actual;
        {
            mp_limb_t stored = actual.limbs[0];
            ctx.flags = 0;
            /* Near-range cancellation beyond the declared exact-power limit
               must report the resource bound, rather than false underflow. */
            CHECK(ppf_set_str(&actual, large_significand, 2, PPF_RNDN, &ctx) == -1);
            CHECK(memcmp(&actual, &before, sizeof(actual)) == 0);
            CHECK(actual.limbs[0] == stored);
            CHECK(ctx.flags == PPF_FLAG_ERANGE);
        }
    }
    mpz_clear(one);
    ppf_clear(&actual); ppf_clear(&expected); ppf_context_clear(&ctx);
}

int main(int argc, char **argv) {
    /* Narrow modes support original-source negative controls. */
    if (argc == 2) {
        if (strcmp(argv[1], "kind") == 0) test_invalid_custom_kind();
        else if (strcmp(argv[1], "parser") == 0) test_text_no_conversion();
        else if (strcmp(argv[1], "width") == 0) test_minimum_dynamic_width();
        else if (strcmp(argv[1], "finite") == 0) test_finite_number_predicate();
        else if (strcmp(argv[1], "range") == 0) test_text_power_limit_range();
        else if (strcmp(argv[1], "random") == 0)
            (void)exercise_random(ERANDOM, 24, 2, 0, PPF_RNDN);
        else if (strcmp(argv[1], "reldiff") == 0)
            (void)exercise_reldiff(1, 0, 0);
        else return 2;
        return 0;
    }
    test_invalid_custom_kind();
    test_text_no_conversion();
    test_minimum_dynamic_width();
    test_finite_number_predicate();
    test_text_power_limit_range();
    test_random_failure_sweeps();
    test_optional_normal_output();
    test_reldiff_failure_sweep();
    puts("API lifecycle audit regressions passed");
    return 0;
}
