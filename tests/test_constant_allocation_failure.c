/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); \
} } while (0)

#define MAX_LIVE_RECORDS 8192u

typedef struct {
    void *ptr;
    size_t size;
} live_record_t;

typedef struct {
    size_t allocation_calls;
    size_t fail_at;
    size_t injected_failures;
    size_t live_count;
    size_t live_bytes;
    int bookkeeping_error;
    live_record_t live[MAX_LIVE_RECORDS];
} fault_state_t;

typedef struct {
    mp_limb_t *pointer;
    mp_limb_t *copy;
    size_t allocated_limbs;
    size_t used_limbs;
    ppf_prec_t precision;
    ppf_exp_t exponent;
    int sign;
    ppf_kind_t kind;
    uint32_t storage_flags;
} object_snapshot_t;

typedef int (*constant_eval_fn)(ppf_t *, ppf_rnd_t,
                                unsigned long, ppf_context_t *);

typedef struct {
    const char *name;
    ppf_constant_id_t id;
    constant_eval_fn evaluate;
} constant_case_t;

static size_t find_live(const fault_state_t *state, const void *ptr) {
    for (size_t i = 0; i < state->live_count; ++i)
        if (state->live[i].ptr == ptr) return i;
    return SIZE_MAX;
}

static void add_live(fault_state_t *state, void *ptr, size_t size) {
    if (!ptr) return;
    if (state->live_count == MAX_LIVE_RECORDS) {
        state->bookkeeping_error = 1;
        return;
    }
    state->live[state->live_count].ptr = ptr;
    state->live[state->live_count].size = size;
    state->live_count++;
    state->live_bytes += size;
}

static void remove_live(fault_state_t *state, void *ptr, size_t size) {
    size_t index;
    if (!ptr) return;
    index = find_live(state, ptr);
    if (index == SIZE_MAX) {
        state->bookkeeping_error = 1;
        return;
    }
    if (state->live[index].size != size || state->live_bytes < size)
        state->bookkeeping_error = 1;
    else
        state->live_bytes -= size;
    state->live[index] = state->live[state->live_count - 1u];
    state->live_count--;
}

static int should_fail(fault_state_t *state) {
    state->allocation_calls++;
    if (state->fail_at != 0u && state->allocation_calls == state->fail_at) {
        state->injected_failures++;
        return 1;
    }
    return 0;
}

static void *fault_malloc(void *opaque, size_t size) {
    fault_state_t *state = (fault_state_t *)opaque;
    void *ptr;
    if (should_fail(state)) return NULL;
    ptr = malloc(size);
    add_live(state, ptr, size);
    return ptr;
}

static void *fault_realloc(void *opaque, void *ptr,
                           size_t old_size, size_t new_size) {
    fault_state_t *state = (fault_state_t *)opaque;
    size_t index = ptr ? find_live(state, ptr) : SIZE_MAX;
    void *next;
    if (ptr && (index == SIZE_MAX || state->live[index].size != old_size))
        state->bookkeeping_error = 1;
    if (should_fail(state)) return NULL;
    next = malloc(new_size);
    if (!next) return NULL;
    if (ptr) {
        memcpy(next, ptr, old_size < new_size ? old_size : new_size);
        remove_live(state, ptr, old_size);
        free(ptr);
    }
    add_live(state, next, new_size);
    return next;
}

static void fault_free(void *opaque, void *ptr, size_t size) {
    fault_state_t *state = (fault_state_t *)opaque;
    remove_live(state, ptr, size);
    free(ptr);
}

static ppf_allocator_t fault_allocator(fault_state_t *state) {
    ppf_allocator_t allocator = {
        fault_malloc, fault_realloc, fault_free, state
    };
    return allocator;
}

static void arm_failure(fault_state_t *state, size_t fail_at) {
    state->allocation_calls = 0u;
    state->injected_failures = 0u;
    state->fail_at = fail_at;
}

static void disable_failure(fault_state_t *state) {
    arm_failure(state, 0u);
}

static void assert_clean(const fault_state_t *state) {
    CHECK(state->bookkeeping_error == 0);
    CHECK(state->live_count == 0u);
    CHECK(state->live_bytes == 0u);
}

static void setup_context(ppf_context_t *ctx, fault_state_t *state) {
    ppf_allocator_t allocator = fault_allocator(state);
    CHECK(ppf_context_init_custom(ctx, &allocator) == 0);
}

static void snapshot_take(object_snapshot_t *snapshot, const ppf_t *x) {
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->pointer = x->limbs;
    snapshot->allocated_limbs = x->allocated_limbs;
    snapshot->used_limbs = x->used_limbs;
    snapshot->precision = x->precision;
    snapshot->exponent = x->exponent;
    snapshot->sign = x->sign;
    snapshot->kind = x->kind;
    snapshot->storage_flags = x->storage_flags;
    snapshot->copy = malloc(x->allocated_limbs * sizeof(*snapshot->copy));
    CHECK(snapshot->copy != NULL);
    memcpy(snapshot->copy, x->limbs,
           x->allocated_limbs * sizeof(*snapshot->copy));
}

static void snapshot_assert_equal(const object_snapshot_t *snapshot,
                                  const ppf_t *x) {
    CHECK(x->limbs == snapshot->pointer);
    CHECK(x->allocated_limbs == snapshot->allocated_limbs);
    CHECK(x->used_limbs == snapshot->used_limbs);
    CHECK(x->precision == snapshot->precision);
    CHECK(x->exponent == snapshot->exponent);
    CHECK(x->sign == snapshot->sign);
    CHECK(x->kind == snapshot->kind);
    CHECK(x->storage_flags == snapshot->storage_flags);
    CHECK(memcmp(x->limbs, snapshot->copy,
                 x->allocated_limbs * sizeof(*snapshot->copy)) == 0);
}

static void snapshot_clear(object_snapshot_t *snapshot) {
    free(snapshot->copy);
    memset(snapshot, 0, sizeof(*snapshot));
}

static int eval_pi(ppf_t *rop, ppf_rnd_t rnd,
                   unsigned long workers, ppf_context_t *ctx) {
    (void)workers;
    return ppf_const_pi(rop, rnd, ctx);
}

static int eval_pi_threads(ppf_t *rop, ppf_rnd_t rnd,
                           unsigned long workers, ppf_context_t *ctx) {
    return ppf_const_pi_threads(rop, rnd, workers, ctx);
}

static int eval_pi_uncached(ppf_t *rop, ppf_rnd_t rnd,
                            unsigned long workers, ppf_context_t *ctx) {
    return ppf_const_pi_uncached(rop, rnd, workers, ctx);
}

static int eval_log2(ppf_t *rop, ppf_rnd_t rnd,
                     unsigned long workers, ppf_context_t *ctx) {
    (void)workers;
    return ppf_const_log2(rop, rnd, ctx);
}

static int eval_log2_threads(ppf_t *rop, ppf_rnd_t rnd,
                             unsigned long workers, ppf_context_t *ctx) {
    return ppf_const_log2_threads(rop, rnd, workers, ctx);
}

static int eval_log2_uncached(ppf_t *rop, ppf_rnd_t rnd,
                              unsigned long workers, ppf_context_t *ctx) {
    return ppf_const_log2_uncached(rop, rnd, workers, ctx);
}

static int eval_euler(ppf_t *rop, ppf_rnd_t rnd,
                      unsigned long workers, ppf_context_t *ctx) {
    (void)workers;
    return ppf_const_euler(rop, rnd, ctx);
}

static int eval_euler_threads(ppf_t *rop, ppf_rnd_t rnd,
                              unsigned long workers, ppf_context_t *ctx) {
    return ppf_const_euler_threads(rop, rnd, workers, ctx);
}

static int eval_euler_uncached(ppf_t *rop, ppf_rnd_t rnd,
                               unsigned long workers, ppf_context_t *ctx) {
    return ppf_const_euler_uncached(rop, rnd, workers, ctx);
}

static int eval_catalan(ppf_t *rop, ppf_rnd_t rnd,
                        unsigned long workers, ppf_context_t *ctx) {
    (void)workers;
    return ppf_const_catalan(rop, rnd, ctx);
}

static int eval_catalan_threads(ppf_t *rop, ppf_rnd_t rnd,
                                unsigned long workers, ppf_context_t *ctx) {
    return ppf_const_catalan_threads(rop, rnd, workers, ctx);
}

static int eval_catalan_uncached(ppf_t *rop, ppf_rnd_t rnd,
                                 unsigned long workers, ppf_context_t *ctx) {
    return ppf_const_catalan_uncached(rop, rnd, workers, ctx);
}

static const constant_case_t constant_cases[] = {
    { "pi", PPF_CONSTANT_PI, eval_pi },
    { "pi_threads", PPF_CONSTANT_PI, eval_pi_threads },
    { "pi_uncached", PPF_CONSTANT_PI, eval_pi_uncached },
    { "log2", PPF_CONSTANT_LOG2, eval_log2 },
    { "log2_threads", PPF_CONSTANT_LOG2, eval_log2_threads },
    { "log2_uncached", PPF_CONSTANT_LOG2, eval_log2_uncached },
    { "euler", PPF_CONSTANT_EULER, eval_euler },
    { "euler_threads", PPF_CONSTANT_EULER, eval_euler_threads },
    { "euler_uncached", PPF_CONSTANT_EULER, eval_euler_uncached },
    { "catalan", PPF_CONSTANT_CATALAN, eval_catalan },
    { "catalan_threads", PPF_CONSTANT_CATALAN, eval_catalan_threads },
    { "catalan_uncached", PPF_CONSTANT_CATALAN, eval_catalan_uncached }
};

static size_t evaluation_allocation_count(const constant_case_t *test) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t value;
    size_t calls;

    ppf_free_constant_caches();
    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&value, 64u, &ctx) == 0);
    CHECK(ppf_set_ui(&value, 12345u, PPF_RNDN, &ctx) == 0);
    disable_failure(&state);
    CHECK(test->evaluate(&value, PPF_RNDN, 2UL, &ctx) !=
          PPF_TERNARY_ERROR);
    calls = state.allocation_calls;
    CHECK(calls >= 1u);
    ppf_clear(&value);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    ppf_free_constant_caches();
    return calls;
}

static void test_evaluation_atomicity(void) {
    for (size_t c = 0u;
         c < sizeof(constant_cases) / sizeof(constant_cases[0]); ++c) {
        const constant_case_t *test = &constant_cases[c];
        size_t calls = evaluation_allocation_count(test);
        for (size_t fail_at = 1u; fail_at <= calls; ++fail_at) {
            fault_state_t state = {0};
            ppf_context_t ctx;
            ppf_t value;
            object_snapshot_t before;
            size_t baseline_count;
            size_t baseline_bytes;
            int rc;

            ppf_free_constant_caches();
            setup_context(&ctx, &state);
            CHECK(ppf_init2_ctx(&value, 64u, &ctx) == 0);
            CHECK(ppf_set_ui(&value, 12345u, PPF_RNDN, &ctx) == 0);
            snapshot_take(&before, &value);
            baseline_count = state.live_count;
            baseline_bytes = state.live_bytes;
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            arm_failure(&state, fail_at);
            rc = test->evaluate(&value, PPF_RNDN, 2UL, &ctx);
            if (rc != PPF_TERNARY_ERROR || state.injected_failures != 1u) {
                fprintf(stderr,
                        "constant failure mismatch case=%s fail_at=%lu calls=%lu rc=%d injected=%lu\n",
                        test->name, (unsigned long)fail_at,
                        (unsigned long)calls, rc,
                        (unsigned long)state.injected_failures);
                exit(1);
            }
            CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ALLOC) != 0);
            snapshot_assert_equal(&before, &value);
            CHECK(state.live_count == baseline_count);
            CHECK(state.live_bytes == baseline_bytes);
            CHECK(state.bookkeeping_error == 0);

            /* Allocation flags are sticky, but a later successful call must
               remain usable and preserve the preexisting sticky bit. */
            disable_failure(&state);
            CHECK(test->evaluate(&value, PPF_RNDN, 2UL, &ctx) !=
                  PPF_TERNARY_ERROR);
            CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ALLOC) != 0);

            snapshot_clear(&before);
            ppf_clear(&value);
            ppf_context_clear(&ctx);
            assert_clean(&state);
            ppf_free_constant_caches();
        }
    }
}

static int precompute_named(ppf_constant_id_t id, ppf_prec_t precision,
                            unsigned long workers) {
    switch (id) {
        case PPF_CONSTANT_PI:
            return ppf_precompute_pi(precision, workers);
        case PPF_CONSTANT_LOG2:
            return ppf_precompute_log2(precision, workers);
        case PPF_CONSTANT_EULER:
            return ppf_precompute_euler(precision, workers);
        case PPF_CONSTANT_CATALAN:
            return ppf_precompute_catalan(precision, workers);
        default:
            return -1;
    }
}

static size_t precompute_allocation_count(ppf_constant_id_t id,
                                          int generic) {
    fault_state_t state = {0};
    ppf_context_t *ctx = ppf_default_context();
    ppf_allocator_t saved;
    ppf_allocator_t fault;
    size_t calls;

    CHECK(ctx != NULL);
    ppf_free_constant_caches();
    saved = ctx->allocator;
    fault = fault_allocator(&state);
    ctx->allocator = fault;
    disable_failure(&state);
    CHECK((generic
        ? ppf_precompute_constant(id, 96u, 2UL)
        : precompute_named(id, 96u, 2UL)) == 0);
    calls = state.allocation_calls;
    CHECK(calls >= 1u);
    ctx->allocator = saved;
    assert_clean(&state);
    ppf_free_constant_caches();
    return calls;
}

static void test_precompute_cache_atomicity(void) {
    static const ppf_constant_id_t ids[] = {
        PPF_CONSTANT_PI, PPF_CONSTANT_LOG2,
        PPF_CONSTANT_EULER, PPF_CONSTANT_CATALAN
    };
    for (size_t i = 0u; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        for (int generic = 0; generic <= 1; ++generic) {
            ppf_constant_id_t id = ids[i];
            size_t calls = precompute_allocation_count(id, generic);
            for (size_t fail_at = 1u; fail_at <= calls; ++fail_at) {
                fault_state_t state = {0};
                ppf_context_t *ctx = ppf_default_context();
                ppf_allocator_t saved;
                ppf_allocator_t fault;
                int rc;

                CHECK(ctx != NULL);
                ppf_free_constant_caches();
                saved = ctx->allocator;
                fault = fault_allocator(&state);
                ctx->allocator = fault;
                ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
                arm_failure(&state, fail_at);
                rc = generic
                    ? ppf_precompute_constant(id, 96u, 2UL)
                    : precompute_named(id, 96u, 2UL);
                CHECK(rc != 0);
                CHECK(state.injected_failures == 1u);
                CHECK((ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0);
                CHECK(ppf_constant_cache_precision(id) == 0u);
                assert_clean(&state);

                disable_failure(&state);
                CHECK((generic
                    ? ppf_precompute_constant(id, 96u, 2UL)
                    : precompute_named(id, 96u, 2UL)) == 0);
                CHECK(ppf_constant_cache_precision(id) >= 96u);
                ctx->allocator = saved;
                assert_clean(&state);
                ppf_free_constant_caches();
            }
        }
    }
}

static size_t interval_allocation_count(ppf_constant_id_t id) {
    fault_state_t state = {0};
    ppf_context_t *ctx = ppf_default_context();
    ppf_allocator_t saved;
    ppf_allocator_t fault;
    mpz_t lower, upper;
    ppf_exp_t exponent = 0;
    size_t calls;

    CHECK(ctx != NULL);
    ppf_free_constant_caches();
    saved = ctx->allocator;
    fault = fault_allocator(&state);
    ctx->allocator = fault;
    mpz_inits(lower, upper, NULL);
    disable_failure(&state);
    CHECK(ppf_get_constant_interval(id, lower, upper, &exponent,
                                    96u, 2UL) == 0);
    calls = state.allocation_calls;
    CHECK(calls >= 1u);
    mpz_clears(lower, upper, NULL);
    ctx->allocator = saved;
    assert_clean(&state);
    ppf_free_constant_caches();
    return calls;
}

static void test_interval_output_atomicity(void) {
    static const ppf_constant_id_t ids[] = {
        PPF_CONSTANT_PI, PPF_CONSTANT_LOG2,
        PPF_CONSTANT_EULER, PPF_CONSTANT_CATALAN
    };
    for (size_t i = 0u; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        ppf_constant_id_t id = ids[i];
        size_t calls = interval_allocation_count(id);
        for (size_t fail_at = 1u; fail_at <= calls; ++fail_at) {
            fault_state_t state = {0};
            ppf_context_t *ctx = ppf_default_context();
            ppf_allocator_t saved;
            ppf_allocator_t fault;
            mpz_t lower, upper, lower_before, upper_before;
            ppf_exp_t exponent = 777;
            int rc;

            CHECK(ctx != NULL);
            ppf_free_constant_caches();
            saved = ctx->allocator;
            fault = fault_allocator(&state);
            ctx->allocator = fault;
            mpz_inits(lower, upper, lower_before, upper_before, NULL);
            mpz_set_si(lower, -12345);
            mpz_set_ui(upper, 67890u);
            mpz_set(lower_before, lower);
            mpz_set(upper_before, upper);
            ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
            arm_failure(&state, fail_at);
            rc = ppf_get_constant_interval(id, lower, upper, &exponent,
                                           96u, 2UL);
            CHECK(rc != 0);
            CHECK(state.injected_failures == 1u);
            CHECK((ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0);
            CHECK(mpz_cmp(lower, lower_before) == 0);
            CHECK(mpz_cmp(upper, upper_before) == 0);
            CHECK(exponent == 777);
            CHECK(ppf_constant_cache_precision(id) == 0u);
            assert_clean(&state);

            disable_failure(&state);
            CHECK(ppf_get_constant_interval(id, lower, upper, &exponent,
                                            96u, 2UL) == 0);
            CHECK(mpz_cmp(lower, upper) < 0);
            ctx->allocator = saved;
            mpz_clears(lower, upper, lower_before, upper_before, NULL);
            assert_clean(&state);
            ppf_free_constant_caches();
        }
    }
}

static void test_public_constant_controls(void) {
    ppf_constant_stats_t stats;
    ppf_set_constant_threads(2UL);
    CHECK(ppf_get_constant_threads() == 2UL);
    CHECK(ppf_get_last_constant_stats(&stats) == 0);
    ppf_clear_constant_cache(PPF_CONSTANT_PI);
    CHECK(ppf_constant_cache_precision(PPF_CONSTANT_PI) == 0u);
    ppf_free_constant_caches();
}

int main(void) {
    ppf_set_constant_threads(2UL);
    test_evaluation_atomicity();
    test_precompute_cache_atomicity();
    test_interval_output_atomicity();
    test_public_constant_controls();
    puts("constant allocation failure atomicity: PASS");
    return 0;
}
