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
    exit(1); } } while (0)

#define MAX_LIVE_RECORDS 4096u

typedef struct {
    void *ptr;
    size_t size;
} live_record_t;

typedef struct {
    size_t allocation_calls;
    size_t malloc_calls;
    size_t realloc_calls;
    size_t free_calls;
    size_t fail_at;
    size_t injected_failures;
    size_t live_count;
    size_t live_bytes;
    size_t peak_bytes;
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

static int should_fail(fault_state_t *state) {
    state->allocation_calls++;
    if (state->fail_at != 0 && state->allocation_calls == state->fail_at) {
        state->injected_failures++;
        return 1;
    }
    return 0;
}

static size_t find_live(const fault_state_t *state, const void *ptr) {
    size_t i;
    for (i = 0; i < state->live_count; ++i)
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
    if (state->live_bytes > state->peak_bytes)
        state->peak_bytes = state->live_bytes;
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

static void *fault_malloc(void *opaque, size_t size) {
    fault_state_t *state = (fault_state_t *)opaque;
    void *ptr;
    state->malloc_calls++;
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
    state->realloc_calls++;
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
    state->free_calls++;
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
    state->allocation_calls = 0;
    state->injected_failures = 0;
    state->fail_at = fail_at;
}

static void disable_failure(fault_state_t *state) {
    arm_failure(state, 0);
}

static void assert_clean(const fault_state_t *state) {
    CHECK(state->bookkeeping_error == 0);
    CHECK(state->live_count == 0);
    CHECK(state->live_bytes == 0);
}

static void assert_allocation_only(const ppf_context_t *ctx) {
    ppf_flags_t flags = ppf_context_flags(ctx);
    if (flags != PPF_FLAG_ALLOC)
        fprintf(stderr, "unexpected flags: 0x%x (expected 0x%x)\n",
                (unsigned)flags, (unsigned)PPF_FLAG_ALLOC);
    CHECK(flags == PPF_FLAG_ALLOC);
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

static void setup_context(ppf_context_t *ctx, fault_state_t *state) {
    ppf_allocator_t allocator = fault_allocator(state);
    CHECK(ppf_context_init_custom(ctx, &allocator) == 0);
}

static void test_allocation_flag_api(void) {
    ppf_clear_flags();
    CHECK(!ppf_allocflag_p());
    ppf_set_allocflag();
    CHECK(ppf_allocflag_p());
    ppf_clear_allocflag();
    CHECK(!ppf_allocflag_p());
    ppf_flags_set(PPF_FLAG_ALLOC | PPF_FLAG_ERANGE);
    CHECK((ppf_flags_test(PPF_FLAGS_ALL) &
           (PPF_FLAG_ALLOC | PPF_FLAG_ERANGE)) ==
          (PPF_FLAG_ALLOC | PPF_FLAG_ERANGE));
    ppf_clear_flags();
}

static void test_init_failure(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t x;
    setup_context(&ctx, &state);
    memset(&x, 0xa5, sizeof(x));
    arm_failure(&state, 1);
    CHECK(ppf_init2_ctx(&x, 256, &ctx) == -1);
    CHECK(state.injected_failures == 1);
    assert_allocation_only(&ctx);
    {
        const unsigned char *bytes = (const unsigned char *)&x;
        size_t i;
        for (i = 0; i < sizeof(x); ++i) CHECK(bytes[i] == 0);
    }
    ppf_context_clear(&ctx);
    assert_clean(&state);
}

static void test_reserve_and_set_prec_transaction(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t x;
    object_snapshot_t snapshot;
    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&x, 64, &ctx) == 0);
    CHECK(ppf_set_ui(&x, 123, PPF_RNDN, &ctx) == 0);

    snapshot_take(&snapshot, &x);
    arm_failure(&state, 1);
    CHECK(ppf_reserve(&x, 65536) == -1);
    CHECK(state.injected_failures == 1);
    snapshot_assert_equal(&snapshot, &x);
    snapshot_clear(&snapshot);

    snapshot_take(&snapshot, &x);
    arm_failure(&state, 1);
    CHECK(ppf_set_prec(&x, 65536) == -1);
    CHECK(state.injected_failures == 1);
    snapshot_assert_equal(&snapshot, &x);
    snapshot_clear(&snapshot);

    disable_failure(&state);
    CHECK(ppf_reserve(&x, 65536) == 0);
    CHECK(ppf_validate(&x));
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    assert_clean(&state);
}

static void test_arena_failure_and_recovery(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_arena_mark_t before, after;
    void *first;
    setup_context(&ctx, &state);

    before = ppf_arena_mark(&ctx);
    arm_failure(&state, 1);
    CHECK(ppf_arena_alloc(&ctx, 1024, 64) == NULL);
    CHECK(state.injected_failures == 1);
    assert_allocation_only(&ctx);
    after = ppf_arena_mark(&ctx);
    CHECK(before.block == after.block && before.used == after.used);

    disable_failure(&state);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    first = ppf_arena_alloc(&ctx, 1024, 64);
    CHECK(first != NULL);
    before = ppf_arena_mark(&ctx);
    arm_failure(&state, 1);
    CHECK(ppf_arena_alloc(&ctx, 200000, 64) == NULL);
    CHECK(state.injected_failures == 1);
    after = ppf_arena_mark(&ctx);
    CHECK(before.block == after.block && before.used == after.used);

    disable_failure(&state);
    CHECK(ppf_arena_alloc(&ctx, 2048, 32) != NULL);
    ppf_context_clear(&ctx);
    assert_clean(&state);
}

static size_t plan_allocation_count(ppf_prec_t precision,
                                    unsigned long expected_applications) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t denominator;
    ppf_divisor_plan_t *plan = NULL;
    size_t calls;
    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&denominator, precision, &ctx) == 0);
    CHECK(ppf_set_ui(&denominator, 3, PPF_RNDN, &ctx) == 0);
    disable_failure(&state);
    CHECK(ppf_divisor_plan_create_ex(&plan, &denominator, precision,
                                     expected_applications, &ctx) == 0);
    calls = state.allocation_calls;
    CHECK(calls >= 2);
    ppf_divisor_plan_destroy(plan);
    ppf_clear(&denominator);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    return calls;
}

static void sweep_plan_failures(ppf_prec_t precision,
                                unsigned long expected_applications) {
    size_t calls = plan_allocation_count(precision, expected_applications);
    size_t fail_at;
    for (fail_at = 1; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t denominator;
        ppf_divisor_plan_t *plan = (ppf_divisor_plan_t *)(uintptr_t)1;
        object_snapshot_t source;
        setup_context(&ctx, &state);
        CHECK(ppf_init2_ctx(&denominator, precision, &ctx) == 0);
        CHECK(ppf_set_ui(&denominator, 3, PPF_RNDN, &ctx) == 0);
        snapshot_take(&source, &denominator);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, fail_at);
        CHECK(ppf_divisor_plan_create_ex(&plan, &denominator, precision,
                                         expected_applications, &ctx) == -1);
        CHECK(plan == NULL);
        CHECK(state.injected_failures == 1);
        assert_allocation_only(&ctx);
        snapshot_assert_equal(&source, &denominator);
        snapshot_clear(&source);
        disable_failure(&state);
        ppf_clear(&denominator);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }
}

static void test_divisor_plan_failure_sweep(void) {
    sweep_plan_failures(512, 1);
    /* This precision enters the portable reciprocal-plan construction path
       and therefore exercises all three context-owned allocations. */
    sweep_plan_failures(262144, 8);
}

static size_t plan_apply_allocation_count(ppf_prec_t precision,
                                          unsigned long expected_applications) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t denominator, numerator, output;
    ppf_divisor_plan_t *plan = NULL;
    size_t calls;
    setup_context(&ctx, &state);
    ctx.arena_block_bytes = 64u;
    CHECK(ppf_init2_ctx(&denominator, precision, &ctx) == 0);
    CHECK(ppf_init2_ctx(&numerator, precision, &ctx) == 0);
    CHECK(ppf_init2_ctx(&output, precision, &ctx) == 0);
    CHECK(ppf_set_ui(&denominator, 3, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_ui(&numerator, 6, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_ui(&output, 777, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_divisor_plan_create_ex(&plan, &denominator, precision,
                                     expected_applications, &ctx) == 0);
    disable_failure(&state);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    CHECK(ppf_div_with_plan(&output, &numerator, plan,
                            PPF_RNDN, &ctx) == 0);
    CHECK(ppf_cmp_ui(&output, 2, &ctx) == 0);
    calls = state.allocation_calls;
    CHECK(calls >= 1u);
    ppf_divisor_plan_destroy(plan);
    ppf_clear(&denominator);
    ppf_clear(&numerator);
    ppf_clear(&output);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    return calls;
}

static void sweep_plan_apply_failures(ppf_prec_t precision,
                                      unsigned long expected_applications) {
    size_t calls = plan_apply_allocation_count(precision,
                                                expected_applications);
    size_t fail_at;
    for (fail_at = 1; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t denominator, numerator, output;
        ppf_divisor_plan_t *plan = NULL;
        object_snapshot_t numerator_before, output_before;
        int rc;
        setup_context(&ctx, &state);
        ctx.arena_block_bytes = 64u;
        CHECK(ppf_init2_ctx(&denominator, precision, &ctx) == 0);
        CHECK(ppf_init2_ctx(&numerator, precision, &ctx) == 0);
        CHECK(ppf_init2_ctx(&output, precision, &ctx) == 0);
        CHECK(ppf_set_ui(&denominator, 3, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_set_ui(&numerator, 6, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_set_ui(&output, 777, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_divisor_plan_create_ex(&plan, &denominator, precision,
                                         expected_applications, &ctx) == 0);
        snapshot_take(&numerator_before, &numerator);
        snapshot_take(&output_before, &output);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, fail_at);
        rc = ppf_div_with_plan(&output, &numerator, plan,
                               PPF_RNDN, &ctx);
        CHECK(state.injected_failures == 1);
        CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ALLOC) != 0);
        snapshot_assert_equal(&numerator_before, &numerator);
        if (rc == PPF_TERNARY_ERROR) {
            snapshot_assert_equal(&output_before, &output);
            CHECK(ppf_divisor_plan_applications(plan) == 0UL);
        } else {
            CHECK(ppf_cmp_ui(&output, 2, &ctx) == 0);
            CHECK(ppf_divisor_plan_applications(plan) == 1UL);
        }
        snapshot_clear(&numerator_before);
        snapshot_clear(&output_before);
        disable_failure(&state);
        ppf_divisor_plan_destroy(plan);
        ppf_clear(&denominator);
        ppf_clear(&numerator);
        ppf_clear(&output);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }
}

static void test_divisor_plan_apply_failure_sweep(void) {
    sweep_plan_apply_failures(4096, 1);
    sweep_plan_apply_failures(262144, 8);
}

static void init_small_values(ppf_context_t *ctx, ppf_t values[],
                              size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) {
        CHECK(ppf_init2_ctx(&values[i], 256, ctx) == 0);
        CHECK(ppf_set_ui(&values[i], (unsigned long)(i + 1u),
                         PPF_RNDN, ctx) == 0);
    }
}

static void test_sum_and_dot_failures(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t values[4], output;
    const ppf_t *sum_inputs[4];
    const ppf_t *dot_a[4], *dot_b[4];
    object_snapshot_t sources[4], output_before;
    size_t i;
    setup_context(&ctx, &state);
    init_small_values(&ctx, values, 4);
    CHECK(ppf_init2_ctx(&output, 256, &ctx) == 0);
    for (i = 0; i < 4; ++i) {
        sum_inputs[i] = &values[i];
        dot_a[i] = &values[i];
        dot_b[i] = &values[3u - i];
        snapshot_take(&sources[i], &values[i]);
    }

    CHECK(ppf_set_ui(&output, 777, PPF_RNDN, &ctx) == 0);
    snapshot_take(&output_before, &output);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    arm_failure(&state, 1);
    CHECK(ppf_sum(&output, sum_inputs, 4, PPF_RNDN, &ctx) ==
          PPF_TERNARY_ERROR);
    CHECK(state.injected_failures == 1);
    assert_allocation_only(&ctx);
    snapshot_assert_equal(&output_before, &output);
    for (i = 0; i < 4; ++i) snapshot_assert_equal(&sources[i], &values[i]);

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    arm_failure(&state, 1);
    CHECK(ppf_dot(&output, dot_a, dot_b, 4, PPF_RNDN, &ctx) ==
          PPF_TERNARY_ERROR);
    CHECK(state.injected_failures == 1);
    assert_allocation_only(&ctx);
    snapshot_assert_equal(&output_before, &output);
    for (i = 0; i < 4; ++i) snapshot_assert_equal(&sources[i], &values[i]);

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    arm_failure(&state, 1);
    CHECK(ppf_sum(&values[0], sum_inputs, 4, PPF_RNDN, &ctx) ==
          PPF_TERNARY_ERROR);
    CHECK(state.injected_failures == 1);
    assert_allocation_only(&ctx);
    for (i = 0; i < 4; ++i) snapshot_assert_equal(&sources[i], &values[i]);

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    arm_failure(&state, 1);
    CHECK(ppf_dot(&values[0], dot_a, dot_b, 4, PPF_RNDN, &ctx) ==
          PPF_TERNARY_ERROR);
    CHECK(state.injected_failures == 1);
    assert_allocation_only(&ctx);
    for (i = 0; i < 4; ++i) snapshot_assert_equal(&sources[i], &values[i]);

    snapshot_clear(&output_before);
    disable_failure(&state);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    CHECK(ppf_sum(&output, sum_inputs, 4, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_cmp_ui(&output, 10, &ctx) == 0);
    CHECK(ppf_dot(&output, dot_a, dot_b, 4, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_cmp_ui(&output, 20, &ctx) == 0);

    for (i = 0; i < 4; ++i) {
        snapshot_clear(&sources[i]);
        ppf_clear(&values[i]);
    }
    ppf_clear(&output);
    ppf_context_clear(&ctx);
    assert_clean(&state);
}

static size_t multiplication_allocation_count(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t a, b, output;
    size_t calls;
    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&a, 4096, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 4096, &ctx) == 0);
    CHECK(ppf_init2_ctx(&output, 4096, &ctx) == 0);
    CHECK(ppf_set_ui(&a, 3, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_ui(&b, 5, PPF_RNDN, &ctx) == 0);
    disable_failure(&state);
    CHECK(ppf_mul_algorithm_threads(&output, &a, &b, PPF_RNDN, 1,
                                    PPF_PRODUCT_FULL, &ctx) == 0);
    CHECK(ppf_cmp_ui(&output, 15, &ctx) == 0);
    calls = state.allocation_calls;
    CHECK(calls >= 1);
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&output);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    return calls;
}

static void test_multiplication_failure_sweep(void) {
    size_t calls = multiplication_allocation_count();
    size_t fail_at;
    for (fail_at = 1; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t a, b, output;
        object_snapshot_t a_before, b_before, output_before;
        int rc;
        setup_context(&ctx, &state);
        CHECK(ppf_init2_ctx(&a, 4096, &ctx) == 0);
        CHECK(ppf_init2_ctx(&b, 4096, &ctx) == 0);
        CHECK(ppf_init2_ctx(&output, 4096, &ctx) == 0);
        CHECK(ppf_set_ui(&a, 3, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_set_ui(&b, 5, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_set_ui(&output, 777, PPF_RNDN, &ctx) == 0);
        snapshot_take(&a_before, &a);
        snapshot_take(&b_before, &b);
        snapshot_take(&output_before, &output);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, fail_at);
        rc = ppf_mul_algorithm_threads(&output, &a, &b, PPF_RNDN, 1,
                                       PPF_PRODUCT_FULL, &ctx);
        CHECK(state.injected_failures == 1);
        if (rc == PPF_TERNARY_ERROR) {
            snapshot_assert_equal(&output_before, &output);
            assert_allocation_only(&ctx);
        } else {
            /* An exact fallback is allowed to recover after a failed optional
               scratch allocation, but it must still produce the right value. */
            CHECK(ppf_cmp_ui(&output, 15, &ctx) == 0);
        }
        snapshot_assert_equal(&a_before, &a);
        snapshot_assert_equal(&b_before, &b);
        snapshot_clear(&a_before);
        snapshot_clear(&b_before);
        snapshot_clear(&output_before);
        disable_failure(&state);
        ppf_clear(&a); ppf_clear(&b); ppf_clear(&output);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }
}

static void test_inplace_mul_and_square_transactions(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t a, b;
    object_snapshot_t a_before, b_before;

    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&a, 4096, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 4096, &ctx) == 0);
    CHECK(ppf_set_ui(&a, 6, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_ui(&b, 3, PPF_RNDN, &ctx) == 0);
    snapshot_take(&a_before, &a);
    snapshot_take(&b_before, &b);

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    arm_failure(&state, 1);
    CHECK(ppf_mul_algorithm_threads(&a, &a, &b, PPF_RNDN, 1,
                                    PPF_PRODUCT_FULL, &ctx) ==
          PPF_TERNARY_ERROR);
    CHECK(state.injected_failures == 1);
    assert_allocation_only(&ctx);
    snapshot_assert_equal(&a_before, &a);
    snapshot_assert_equal(&b_before, &b);

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    arm_failure(&state, 1);
    CHECK(ppf_sqr(&a, &a, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR);
    CHECK(state.injected_failures == 1);
    assert_allocation_only(&ctx);
    snapshot_assert_equal(&a_before, &a);
    snapshot_assert_equal(&b_before, &b);

    snapshot_clear(&a_before);
    snapshot_clear(&b_before);
    disable_failure(&state);
    ppf_clear(&a);
    ppf_clear(&b);
    ppf_context_clear(&ctx);
    assert_clean(&state);
}

static size_t division_allocation_count(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t a, b;
    size_t calls;
    setup_context(&ctx, &state);
    ctx.arena_block_bytes = 64u;
    CHECK(ppf_init2_ctx(&a, 4096, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 4096, &ctx) == 0);
    CHECK(ppf_set_ui(&a, 6, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_ui(&b, 3, PPF_RNDN, &ctx) == 0);
    disable_failure(&state);
    CHECK(ppf_div(&a, &a, &b, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_cmp_ui(&a, 2, &ctx) == 0);
    calls = state.allocation_calls;
    CHECK(calls >= 2);
    ppf_clear(&a);
    ppf_clear(&b);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    return calls;
}

static void test_inplace_division_failure_sweep(void) {
    size_t calls = division_allocation_count();
    size_t fail_at;
    for (fail_at = 1; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t a, b;
        object_snapshot_t a_before, b_before;
        setup_context(&ctx, &state);
        ctx.arena_block_bytes = 64u;
        CHECK(ppf_init2_ctx(&a, 4096, &ctx) == 0);
        CHECK(ppf_init2_ctx(&b, 4096, &ctx) == 0);
        CHECK(ppf_set_ui(&a, 6, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_set_ui(&b, 3, PPF_RNDN, &ctx) == 0);
        snapshot_take(&a_before, &a);
        snapshot_take(&b_before, &b);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, fail_at);
        CHECK(ppf_div(&a, &a, &b, PPF_RNDN, &ctx) ==
              PPF_TERNARY_ERROR);
        CHECK(state.injected_failures == 1);
        assert_allocation_only(&ctx);
        snapshot_assert_equal(&a_before, &a);
        snapshot_assert_equal(&b_before, &b);
        snapshot_clear(&a_before);
        snapshot_clear(&b_before);
        disable_failure(&state);
        ppf_clear(&a);
        ppf_clear(&b);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }
}

static void test_sqrt_ui_initialization_transaction(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t output;
    object_snapshot_t before;
    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&output, 256, &ctx) == 0);
    CHECK(ppf_set_ui(&output, 777, PPF_RNDN, &ctx) == 0);
    snapshot_take(&before, &output);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    arm_failure(&state, 1);
    CHECK(ppf_sqrt_ui(&output, 4, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR);
    CHECK(state.injected_failures == 1);
    assert_allocation_only(&ctx);
    snapshot_assert_equal(&before, &output);
    snapshot_clear(&before);
    disable_failure(&state);
    ppf_clear(&output);
    ppf_context_clear(&ctx);
    assert_clean(&state);
}

static size_t recsqrt_allocation_count(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t value;
    size_t calls;
    setup_context(&ctx, &state);
    ctx.arena_block_bytes = 64u;
    CHECK(ppf_init2_ctx(&value, 1024, &ctx) == 0);
    CHECK(ppf_set_ui(&value, 4, PPF_RNDN, &ctx) == 0);
    disable_failure(&state);
    CHECK(ppf_rec_sqrt(&value, &value, PPF_RNDN, &ctx) !=
          PPF_TERNARY_ERROR);
    CHECK(ppf_validate(&value));
    calls = state.allocation_calls;
    CHECK(calls >= 2u);
    ppf_clear(&value);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    return calls;
}

static void test_inplace_recsqrt_failure_sweep(void) {
    size_t calls = recsqrt_allocation_count();
    size_t fail_at;
    for (fail_at = 1; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t value;
        object_snapshot_t before;
        int rc;
        setup_context(&ctx, &state);
        ctx.arena_block_bytes = 64u;
        CHECK(ppf_init2_ctx(&value, 1024, &ctx) == 0);
        CHECK(ppf_set_ui(&value, 4, PPF_RNDN, &ctx) == 0);
        snapshot_take(&before, &value);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, fail_at);
        rc = ppf_rec_sqrt(&value, &value, PPF_RNDN, &ctx);
        CHECK(state.injected_failures == 1);
        if ((ppf_context_flags(&ctx) & PPF_FLAG_ALLOC) == 0) {
            fprintf(stderr, "recsqrt fail_at=%zu calls=%zu rc=%d flags=0x%x\n",
                    fail_at, calls, rc, (unsigned)ppf_context_flags(&ctx));
        }
        CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ALLOC) != 0);
        if (rc == PPF_TERNARY_ERROR) {
            snapshot_assert_equal(&before, &value);
        } else {
            /* A later optional scratch failure may be recovered by another
               certified path.  Recovery is successful only with a valid
               result, while the sticky allocation flag records the event. */
            CHECK(ppf_validate(&value));
            CHECK(ppf_cmp_d(&value, 0.5, &ctx) == 0);
        }
        snapshot_clear(&before);
        disable_failure(&state);
        ppf_clear(&value);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }
}


static unsigned char *make_fpif_blob(ppf_t *source, size_t *size_out) {
    FILE *stream = tmpfile();
    unsigned char *blob;
    long length;
    CHECK(stream != NULL);
    CHECK(ppf_fpif_export(stream, source) == 0);
    CHECK(fflush(stream) == 0);
    CHECK(fseek(stream, 0, SEEK_END) == 0);
    length = ftell(stream);
    CHECK(length > 0);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    blob = (unsigned char *)malloc((size_t)length);
    CHECK(blob != NULL);
    CHECK(fread(blob, 1, (size_t)length, stream) == (size_t)length);
    fclose(stream);
    *size_out = (size_t)length;
    return blob;
}

static FILE *open_fpif_blob(const unsigned char *blob, size_t size) {
    FILE *stream = tmpfile();
    CHECK(stream != NULL);
    CHECK(fwrite(blob, 1, size, stream) == size);
    CHECK(fflush(stream) == 0);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    return stream;
}

static size_t fpif_import_allocation_count(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t source, output;
    unsigned char *blob;
    size_t blob_size, calls;
    FILE *stream;
    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&source, 257u, &ctx) == 0);
    CHECK(ppf_init2_ctx(&output, 128u, &ctx) == 0);
    CHECK(ppf_set_ui_2exp(&source, 0xabcdefUL, 200,
                          PPF_RNDN, &ctx) == 0);
    blob = make_fpif_blob(&source, &blob_size);
    stream = open_fpif_blob(blob, blob_size);
    disable_failure(&state);
    CHECK(ppf_fpif_import(&output, stream, &ctx) == 0);
    CHECK(ppf_cmp(&source, &output, &ctx) == 0);
    calls = state.allocation_calls;
    CHECK(calls >= 2u);
    fclose(stream);
    free(blob);
    ppf_clear(&source);
    ppf_clear(&output);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    return calls;
}

static void test_fpif_import_failure_sweep(void) {
    size_t calls = fpif_import_allocation_count();
    size_t fail_at;
    for (fail_at = 1u; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t source, output;
        object_snapshot_t before;
        unsigned char *blob;
        size_t blob_size;
        FILE *stream;
        setup_context(&ctx, &state);
        CHECK(ppf_init2_ctx(&source, 257u, &ctx) == 0);
        CHECK(ppf_init2_ctx(&output, 128u, &ctx) == 0);
        CHECK(ppf_set_ui_2exp(&source, 0xabcdefUL, 200,
                              PPF_RNDN, &ctx) == 0);
        CHECK(ppf_set_ui(&output, 777u, PPF_RNDN, &ctx) == 0);
        blob = make_fpif_blob(&source, &blob_size);
        stream = open_fpif_blob(blob, blob_size);
        snapshot_take(&before, &output);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, fail_at);
        CHECK(ppf_fpif_import(&output, stream, &ctx) == -1);
        CHECK(state.injected_failures == 1u);
        assert_allocation_only(&ctx);
        snapshot_assert_equal(&before, &output);
        snapshot_clear(&before);
        fclose(stream);
        free(blob);
        disable_failure(&state);
        ppf_clear(&source);
        ppf_clear(&output);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }
}

int main(void) {
    test_allocation_flag_api();
    test_init_failure();
    test_reserve_and_set_prec_transaction();
    test_arena_failure_and_recovery();
    test_divisor_plan_failure_sweep();
    test_divisor_plan_apply_failure_sweep();
    test_sum_and_dot_failures();
    test_multiplication_failure_sweep();
    test_inplace_mul_and_square_transactions();
    test_inplace_division_failure_sweep();
    test_sqrt_ui_initialization_transaction();
    test_inplace_recsqrt_failure_sweep();
    test_fpif_import_failure_sweep();
    ppf_default_context_cleanup();
    puts("allocation-failure and transactional-state tests passed");
    return 0;
}
