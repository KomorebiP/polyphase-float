/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); \
} } while (0)

#define MAX_LIVE_RECORDS 32768u
#define TEST_PRECISION 1088u

typedef struct {
    void *ptr;
    size_t size;
} live_record_t;

typedef struct {
    atomic_flag lock;
    size_t allocation_calls;
    size_t fail_at;
    size_t injected_failures;
    size_t live_count;
    size_t live_bytes;
    int bookkeeping_error;
    live_record_t live[MAX_LIVE_RECORDS];
} fault_state_t;

#define FAULT_STATE_INITIALIZER { .lock = ATOMIC_FLAG_INIT }

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

typedef struct {
    ppf_context_t ctx;
    ppf_t a;
    ppf_t b;
    ppf_t c;
    ppf_t d;
    ppf_t out1;
    ppf_t out2;
    ppf_t out3;
    mpz_t z;
    mpq_t q;
    ppf_divisor_plan_t *plan;
    int ternaries[3];
} fixture_t;

typedef enum {
    CASE_ADD,
    CASE_ADD_UI,
    CASE_ADD_SI,
    CASE_ADD_D,
    CASE_ADD_Z,
    CASE_ADD_Q,
    CASE_SUB,
    CASE_UI_SUB,
    CASE_SUB_UI,
    CASE_SI_SUB,
    CASE_SUB_SI,
    CASE_D_SUB,
    CASE_SUB_D,
    CASE_Z_SUB,
    CASE_SUB_Z,
    CASE_SUB_Q,
    CASE_MUL,
    CASE_MUL_UI,
    CASE_MUL_SI,
    CASE_MUL_D,
    CASE_MUL_Z,
    CASE_MUL_Q,
    CASE_MUL_THREADS,
    CASE_SQR_THREADS,
    CASE_SQR_ALGORITHM_THREADS,
    CASE_UI_DIV,
    CASE_DIV_UI,
    CASE_SI_DIV,
    CASE_DIV_SI,
    CASE_D_DIV,
    CASE_DIV_D,
    CASE_DIV_Z,
    CASE_DIV_Q,
    CASE_DIV_WITH_PLAN_MANY,
    CASE_FMA,
    CASE_FMA_THREADS,
    CASE_FMA_ALGORITHM_THREADS,
    CASE_FMS,
    CASE_FMMA,
    CASE_FMMS,
    CASE_FMS_THREADS,
    CASE_FMS_ALGORITHM_THREADS,
    CASE_MUL_2UI,
    CASE_MUL_2SI,
    CASE_DIV_2UI,
    CASE_DIV_2SI,
    CASE_FAC_UI,
    CASE_DOT_THREADS,
    CASE_SQRT,
    CASE_REC_SQRT_THREADS,
    CASE_REC_SQRT_ALGORITHM_THREADS,
    CASE_CBRT,
    CASE_ROOTN_UI,
    CASE_ROOTN_SI,
    CASE_ROOT,
    CASE_HYPOT
} operation_t;

typedef struct {
    const char *name;
    operation_t operation;
    int batch;
} function_case_t;

static const function_case_t function_cases[] = {
    {"ppf_add", CASE_ADD, 0},
    {"ppf_add_ui", CASE_ADD_UI, 0},
    {"ppf_add_si", CASE_ADD_SI, 0},
    {"ppf_add_d", CASE_ADD_D, 0},
    {"ppf_add_z", CASE_ADD_Z, 0},
    {"ppf_add_q", CASE_ADD_Q, 0},
    {"ppf_sub", CASE_SUB, 0},
    {"ppf_ui_sub", CASE_UI_SUB, 0},
    {"ppf_sub_ui", CASE_SUB_UI, 0},
    {"ppf_si_sub", CASE_SI_SUB, 0},
    {"ppf_sub_si", CASE_SUB_SI, 0},
    {"ppf_d_sub", CASE_D_SUB, 0},
    {"ppf_sub_d", CASE_SUB_D, 0},
    {"ppf_z_sub", CASE_Z_SUB, 0},
    {"ppf_sub_z", CASE_SUB_Z, 0},
    {"ppf_sub_q", CASE_SUB_Q, 0},
    {"ppf_mul", CASE_MUL, 0},
    {"ppf_mul_ui", CASE_MUL_UI, 0},
    {"ppf_mul_si", CASE_MUL_SI, 0},
    {"ppf_mul_d", CASE_MUL_D, 0},
    {"ppf_mul_z", CASE_MUL_Z, 0},
    {"ppf_mul_q", CASE_MUL_Q, 0},
    {"ppf_mul_threads", CASE_MUL_THREADS, 0},
    {"ppf_sqr_threads", CASE_SQR_THREADS, 0},
    {"ppf_sqr_algorithm_threads", CASE_SQR_ALGORITHM_THREADS, 0},
    {"ppf_ui_div", CASE_UI_DIV, 0},
    {"ppf_div_ui", CASE_DIV_UI, 0},
    {"ppf_si_div", CASE_SI_DIV, 0},
    {"ppf_div_si", CASE_DIV_SI, 0},
    {"ppf_d_div", CASE_D_DIV, 0},
    {"ppf_div_d", CASE_DIV_D, 0},
    {"ppf_div_z", CASE_DIV_Z, 0},
    {"ppf_div_q", CASE_DIV_Q, 0},
    {"ppf_div_with_plan_many", CASE_DIV_WITH_PLAN_MANY, 1},
    {"ppf_fma", CASE_FMA, 0},
    {"ppf_fma_threads", CASE_FMA_THREADS, 0},
    {"ppf_fma_algorithm_threads", CASE_FMA_ALGORITHM_THREADS, 0},
    {"ppf_fms", CASE_FMS, 0},
    {"ppf_fmma", CASE_FMMA, 0},
    {"ppf_fmms", CASE_FMMS, 0},
    {"ppf_fms_threads", CASE_FMS_THREADS, 0},
    {"ppf_fms_algorithm_threads", CASE_FMS_ALGORITHM_THREADS, 0},
    {"ppf_mul_2ui", CASE_MUL_2UI, 0},
    {"ppf_mul_2si", CASE_MUL_2SI, 0},
    {"ppf_div_2ui", CASE_DIV_2UI, 0},
    {"ppf_div_2si", CASE_DIV_2SI, 0},
    {"ppf_fac_ui", CASE_FAC_UI, 0},
    {"ppf_dot_threads", CASE_DOT_THREADS, 0},
    {"ppf_sqrt", CASE_SQRT, 0},
    {"ppf_rec_sqrt_threads", CASE_REC_SQRT_THREADS, 0},
    {"ppf_rec_sqrt_algorithm_threads", CASE_REC_SQRT_ALGORITHM_THREADS, 0},
    {"ppf_cbrt", CASE_CBRT, 0},
    {"ppf_rootn_ui", CASE_ROOTN_UI, 0},
    {"ppf_rootn_si", CASE_ROOTN_SI, 0},
    {"ppf_root", CASE_ROOT, 0},
    {"ppf_hypot", CASE_HYPOT, 0},
};


static void fault_lock(fault_state_t *state) {
    while (atomic_flag_test_and_set_explicit(&state->lock,
                                             memory_order_acquire)) {
    }
}

static void fault_unlock(fault_state_t *state) {
    atomic_flag_clear_explicit(&state->lock, memory_order_release);
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
    if (state->fail_at != 0U && state->allocation_calls == state->fail_at) {
        state->injected_failures++;
        return 1;
    }
    return 0;
}

static void *fault_malloc(void *opaque, size_t size) {
    fault_state_t *state = (fault_state_t *)opaque;
    void *ptr;
    fault_lock(state);
    if (should_fail(state)) {
        fault_unlock(state);
        return NULL;
    }
    ptr = malloc(size);
    add_live(state, ptr, size);
    fault_unlock(state);
    return ptr;
}

static void *fault_realloc(void *opaque, void *ptr,
                           size_t old_size, size_t new_size) {
    fault_state_t *state = (fault_state_t *)opaque;
    size_t index;
    void *next;
    fault_lock(state);
    index = ptr ? find_live(state, ptr) : SIZE_MAX;
    if (ptr && (index == SIZE_MAX || state->live[index].size != old_size))
        state->bookkeeping_error = 1;
    if (should_fail(state)) {
        fault_unlock(state);
        return NULL;
    }
    next = malloc(new_size);
    if (!next) {
        fault_unlock(state);
        return NULL;
    }
    if (ptr) {
        memcpy(next, ptr, old_size < new_size ? old_size : new_size);
        remove_live(state, ptr, old_size);
        free(ptr);
    }
    add_live(state, next, new_size);
    fault_unlock(state);
    return next;
}

static void fault_free(void *opaque, void *ptr, size_t size) {
    fault_state_t *state = (fault_state_t *)opaque;
    fault_lock(state);
    remove_live(state, ptr, size);
    free(ptr);
    fault_unlock(state);
}

static ppf_allocator_t fault_allocator(fault_state_t *state) {
    ppf_allocator_t allocator = {
        fault_malloc, fault_realloc, fault_free, state
    };
    return allocator;
}

static void reset_counter(fault_state_t *state, size_t fail_at) {
    state->allocation_calls = 0U;
    state->injected_failures = 0U;
    state->fail_at = fail_at;
}

static void stop_injection(fault_state_t *state) {
    state->fail_at = 0U;
}

static const char *diagnostic_case = "unassigned";

static void assert_clean(const fault_state_t *state) {
    if (state->bookkeeping_error != 0 || state->live_count != 0U ||
        state->live_bytes != 0U) {
        size_t i;
        fprintf(stderr, "allocator not clean case=%s error=%d live=%zu bytes=%zu calls=%zu\n",
                diagnostic_case, state->bookkeeping_error, state->live_count,
                state->live_bytes, state->allocation_calls);
        for (i = 0; i < state->live_count && i < 8U; ++i)
            fprintf(stderr, "  live[%zu]=%p size=%zu\n", i,
                    state->live[i].ptr, state->live[i].size);
    }
    CHECK(state->bookkeeping_error == 0);
    CHECK(state->live_count == 0U);
    CHECK(state->live_bytes == 0U);
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
    snapshot->copy = (mp_limb_t *)malloc(
        x->allocated_limbs * sizeof(*snapshot->copy));
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

static void fixture_init(fixture_t *fixture, fault_state_t *state,
                         ppf_prec_t precision, int need_plan) {
    ppf_allocator_t allocator = fault_allocator(state);
    ppf_arena_mark_t mark;
    memset(fixture, 0, sizeof(*fixture));
    CHECK(ppf_context_init_custom(&fixture->ctx, &allocator) == 0);
    fixture->ctx.arena_block_bytes = 65536U;
#define INIT_OBJECT(name) \
    CHECK(ppf_init2_ctx(&fixture->name, precision, &fixture->ctx) == 0)
    INIT_OBJECT(a);
    INIT_OBJECT(b);
    INIT_OBJECT(c);
    INIT_OBJECT(d);
    INIT_OBJECT(out1);
    INIT_OBJECT(out2);
    INIT_OBJECT(out3);
#undef INIT_OBJECT
    CHECK(ppf_set_ui_2exp(&fixture->a, 13UL, -2,
                          PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui_2exp(&fixture->b, 3UL, -1,
                          PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui_2exp(&fixture->c, 5UL, -2,
                          PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui_2exp(&fixture->d, 7UL, -2,
                          PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui(&fixture->out1, 12345UL,
                     PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_si(&fixture->out2, -6789L,
                     PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui(&fixture->out3, 2468UL,
                     PPF_RNDN, &fixture->ctx) == 0);
    mpz_init_set_ui(fixture->z, 7UL);
    mpq_init(fixture->q);
    mpq_set_ui(fixture->q, 7UL, 3UL);
    mpq_canonicalize(fixture->q);
    fixture->ternaries[0] = 13579;
    fixture->ternaries[1] = -24680;
    fixture->ternaries[2] = 11223;

    /* Keep one reusable arena block in the baseline live set. */
    mark = ppf_arena_mark(&fixture->ctx);
    CHECK(ppf_arena_alloc(&fixture->ctx, 1U,
                          _Alignof(mp_limb_t)) != NULL);
    ppf_arena_release(&fixture->ctx, mark);

    if (need_plan) {
        CHECK(ppf_divisor_plan_create_ex(&fixture->plan, &fixture->b,
                                         precision, 16UL,
                                         &fixture->ctx) == 0);
    }
}

static void fixture_clear(fixture_t *fixture) {
    if (fixture->plan) ppf_divisor_plan_destroy(fixture->plan);
    mpq_clear(fixture->q);
    mpz_clear(fixture->z);
    ppf_clear(&fixture->a);
    ppf_clear(&fixture->b);
    ppf_clear(&fixture->c);
    ppf_clear(&fixture->d);
    ppf_clear(&fixture->out1);
    ppf_clear(&fixture->out2);
    ppf_clear(&fixture->out3);
    ppf_context_clear(&fixture->ctx);
    memset(fixture, 0, sizeof(*fixture));
}

static int evaluate_case(const function_case_t *test, fixture_t *f) {
    const ppf_t *left[3] = {&f->a, &f->b, &f->c};
    const ppf_t *right[3] = {&f->b, &f->c, &f->d};
    ppf_t *outputs[3] = {&f->out1, &f->out2, &f->out3};
    const ppf_t *numerators[3] = {&f->a, &f->c, &f->d};
    switch (test->operation) {
        case CASE_ADD: return ppf_add(&f->out1, &f->a, &f->b, PPF_RNDN, &f->ctx);
        case CASE_ADD_UI: return ppf_add_ui(&f->out1, &f->a, 7UL, PPF_RNDN, &f->ctx);
        case CASE_ADD_SI: return ppf_add_si(&f->out1, &f->a, -7L, PPF_RNDN, &f->ctx);
        case CASE_ADD_D: return ppf_add_d(&f->out1, &f->a, 1.25, PPF_RNDN, &f->ctx);
        case CASE_ADD_Z: return ppf_add_z(&f->out1, &f->a, f->z, PPF_RNDN, &f->ctx);
        case CASE_ADD_Q: return ppf_add_q(&f->out1, &f->a, f->q, PPF_RNDN, &f->ctx);
        case CASE_SUB: return ppf_sub(&f->out1, &f->a, &f->b, PPF_RNDN, &f->ctx);
        case CASE_UI_SUB: return ppf_ui_sub(&f->out1, 7UL, &f->a, PPF_RNDN, &f->ctx);
        case CASE_SUB_UI: return ppf_sub_ui(&f->out1, &f->a, 7UL, PPF_RNDN, &f->ctx);
        case CASE_SI_SUB: return ppf_si_sub(&f->out1, -7L, &f->a, PPF_RNDN, &f->ctx);
        case CASE_SUB_SI: return ppf_sub_si(&f->out1, &f->a, -7L, PPF_RNDN, &f->ctx);
        case CASE_D_SUB: return ppf_d_sub(&f->out1, 7.25, &f->a, PPF_RNDN, &f->ctx);
        case CASE_SUB_D: return ppf_sub_d(&f->out1, &f->a, 1.25, PPF_RNDN, &f->ctx);
        case CASE_Z_SUB: return ppf_z_sub(&f->out1, f->z, &f->a, PPF_RNDN, &f->ctx);
        case CASE_SUB_Z: return ppf_sub_z(&f->out1, &f->a, f->z, PPF_RNDN, &f->ctx);
        case CASE_SUB_Q: return ppf_sub_q(&f->out1, &f->a, f->q, PPF_RNDN, &f->ctx);
        case CASE_MUL: return ppf_mul(&f->out1, &f->a, &f->b, PPF_RNDN, &f->ctx);
        case CASE_MUL_UI: return ppf_mul_ui(&f->out1, &f->a, 7UL, PPF_RNDN, &f->ctx);
        case CASE_MUL_SI: return ppf_mul_si(&f->out1, &f->a, -7L, PPF_RNDN, &f->ctx);
        case CASE_MUL_D: return ppf_mul_d(&f->out1, &f->a, 1.25, PPF_RNDN, &f->ctx);
        case CASE_MUL_Z: return ppf_mul_z(&f->out1, &f->a, f->z, PPF_RNDN, &f->ctx);
        case CASE_MUL_Q: return ppf_mul_q(&f->out1, &f->a, f->q, PPF_RNDN, &f->ctx);
        case CASE_MUL_THREADS: return ppf_mul_threads(&f->out1, &f->a, &f->b, PPF_RNDN, 4UL, &f->ctx);
        case CASE_SQR_THREADS: return ppf_sqr_threads(&f->out1, &f->a, PPF_RNDN, 4UL, &f->ctx);
        case CASE_SQR_ALGORITHM_THREADS: return ppf_sqr_algorithm_threads(&f->out1, &f->a, PPF_RNDN, 4UL, PPF_PRODUCT_AUTO, &f->ctx);
        case CASE_UI_DIV: return ppf_ui_div(&f->out1, 7UL, &f->b, PPF_RNDN, &f->ctx);
        case CASE_DIV_UI: return ppf_div_ui(&f->out1, &f->a, 7UL, PPF_RNDN, &f->ctx);
        case CASE_SI_DIV: return ppf_si_div(&f->out1, -7L, &f->b, PPF_RNDN, &f->ctx);
        case CASE_DIV_SI: return ppf_div_si(&f->out1, &f->a, -7L, PPF_RNDN, &f->ctx);
        case CASE_D_DIV: return ppf_d_div(&f->out1, 7.25, &f->b, PPF_RNDN, &f->ctx);
        case CASE_DIV_D: return ppf_div_d(&f->out1, &f->a, 1.25, PPF_RNDN, &f->ctx);
        case CASE_DIV_Z: return ppf_div_z(&f->out1, &f->a, f->z, PPF_RNDN, &f->ctx);
        case CASE_DIV_Q: return ppf_div_q(&f->out1, &f->a, f->q, PPF_RNDN, &f->ctx);
        case CASE_DIV_WITH_PLAN_MANY: return ppf_div_with_plan_many(outputs, numerators, 3U, f->plan, PPF_RNDN, 4UL, f->ternaries, &f->ctx);
        case CASE_FMA: return ppf_fma(&f->out1, &f->a, &f->b, &f->c, PPF_RNDN, &f->ctx);
        case CASE_FMA_THREADS: return ppf_fma_threads(&f->out1, &f->a, &f->b, &f->c, PPF_RNDN, 4UL, &f->ctx);
        case CASE_FMA_ALGORITHM_THREADS: return ppf_fma_algorithm_threads(&f->out1, &f->a, &f->b, &f->c, PPF_RNDN, 4UL, PPF_PRODUCT_AUTO, &f->ctx);
        case CASE_FMS: return ppf_fms(&f->out1, &f->a, &f->b, &f->c, PPF_RNDN, &f->ctx);
        case CASE_FMMA: return ppf_fmma(&f->out1, &f->a, &f->b, &f->c, &f->d, PPF_RNDN, &f->ctx);
        case CASE_FMMS: return ppf_fmms(&f->out1, &f->a, &f->b, &f->c, &f->d, PPF_RNDN, &f->ctx);
        case CASE_FMS_THREADS: return ppf_fms_threads(&f->out1, &f->a, &f->b, &f->c, PPF_RNDN, 4UL, &f->ctx);
        case CASE_FMS_ALGORITHM_THREADS: return ppf_fms_algorithm_threads(&f->out1, &f->a, &f->b, &f->c, PPF_RNDN, 4UL, PPF_PRODUCT_AUTO, &f->ctx);
        case CASE_MUL_2UI: return ppf_mul_2ui(&f->out1, &f->a, 5UL, PPF_RNDN, &f->ctx);
        case CASE_MUL_2SI: return ppf_mul_2si(&f->out1, &f->a, -5L, PPF_RNDN, &f->ctx);
        case CASE_DIV_2UI: return ppf_div_2ui(&f->out1, &f->a, 5UL, PPF_RNDN, &f->ctx);
        case CASE_DIV_2SI: return ppf_div_2si(&f->out1, &f->a, -5L, PPF_RNDN, &f->ctx);
        case CASE_FAC_UI: return ppf_fac_ui(&f->out1, 20UL, PPF_RNDN, &f->ctx);
        case CASE_DOT_THREADS: return ppf_dot_threads(&f->out1, left, right, 3U, PPF_RNDN, 4UL, &f->ctx);
        case CASE_SQRT: return ppf_sqrt(&f->out1, &f->a, PPF_RNDN, &f->ctx);
        case CASE_REC_SQRT_THREADS: return ppf_rec_sqrt_threads(&f->out1, &f->a, PPF_RNDN, 4UL, &f->ctx);
        case CASE_REC_SQRT_ALGORITHM_THREADS: return ppf_rec_sqrt_algorithm_threads(&f->out1, &f->a, PPF_RNDN, 4UL, PPF_PRODUCT_AUTO, &f->ctx);
        case CASE_CBRT: return ppf_cbrt(&f->out1, &f->a, PPF_RNDN, &f->ctx);
        case CASE_ROOTN_UI: return ppf_rootn_ui(&f->out1, &f->a, 3UL, PPF_RNDN, &f->ctx);
        case CASE_ROOTN_SI: return ppf_rootn_si(&f->out1, &f->a, -3L, PPF_RNDN, &f->ctx);
        case CASE_ROOT: return ppf_root(&f->out1, &f->a, 5UL, PPF_RNDN, &f->ctx);
        case CASE_HYPOT: return ppf_hypot(&f->out1, &f->a, &f->b, PPF_RNDN, &f->ctx);
        default: return PPF_TERNARY_ERROR;
    }
}

static size_t evaluation_allocation_count(const function_case_t *test) {
    fault_state_t state = FAULT_STATE_INITIALIZER;
    fixture_t fixture;
    size_t calls;
    int rc;
    diagnostic_case = test->name;
    fixture_init(&fixture, &state, TEST_PRECISION, test->batch);
    reset_counter(&state, 0U);
    ppf_context_flags_clear(&fixture.ctx, PPF_FLAGS_ALL);
    rc = evaluate_case(test, &fixture);
    if ((test->batch && rc != 0) || (!test->batch && rc == PPF_TERNARY_ERROR)) {
        fprintf(stderr, "baseline failed case=%s rc=%d flags=0x%x\n",
                test->name, rc, (unsigned)ppf_context_flags(&fixture.ctx));
        exit(1);
    }
    calls = state.allocation_calls;
    CHECK(calls >= 1U);
    stop_injection(&state);
    fixture_clear(&fixture);
    assert_clean(&state);
    return calls;
}

static void run_failure_attempt(const function_case_t *test,
                                size_t fail_at, size_t expected_calls) {
    fault_state_t state = FAULT_STATE_INITIALIZER;
    fixture_t fixture;
    object_snapshot_t before1;
    object_snapshot_t before2;
    object_snapshot_t before3;
    size_t baseline_count;
    size_t baseline_bytes;
    int ternaries_before[3];
    int rc;

    diagnostic_case = test->name;
    fixture_init(&fixture, &state, TEST_PRECISION, test->batch);
    snapshot_take(&before1, &fixture.out1);
    snapshot_take(&before2, &fixture.out2);
    snapshot_take(&before3, &fixture.out3);
    memcpy(ternaries_before, fixture.ternaries, sizeof(ternaries_before));
    baseline_count = state.live_count;
    baseline_bytes = state.live_bytes;
    ppf_context_flags_clear(&fixture.ctx, PPF_FLAGS_ALL);
    reset_counter(&state, fail_at);
    rc = evaluate_case(test, &fixture);
    stop_injection(&state);

    if ((test->batch ? rc != -1 : rc != PPF_TERNARY_ERROR) ||
        state.injected_failures != 1U) {
        fprintf(stderr,
                "failure mismatch case=%s fail_at=%zu baseline_calls=%zu "
                "observed_calls=%zu rc=%d injected=%zu flags=0x%x\n",
                test->name, fail_at, expected_calls,
                state.allocation_calls, rc, state.injected_failures,
                (unsigned)ppf_context_flags(&fixture.ctx));
        exit(1);
    }
    CHECK((ppf_context_flags(&fixture.ctx) & PPF_FLAG_ALLOC) != 0U);
    snapshot_assert_equal(&before1, &fixture.out1);
    snapshot_assert_equal(&before2, &fixture.out2);
    snapshot_assert_equal(&before3, &fixture.out3);
    CHECK(memcmp(ternaries_before, fixture.ternaries,
                 sizeof(ternaries_before)) == 0);
    CHECK(state.live_count == baseline_count);
    CHECK(state.live_bytes == baseline_bytes);
    CHECK(state.bookkeeping_error == 0);

    snapshot_clear(&before1);
    snapshot_clear(&before2);
    snapshot_clear(&before3);
    fixture_clear(&fixture);
    assert_clean(&state);
}

static void test_all_public_endpoints(void) {
    const size_t count = sizeof(function_cases) / sizeof(function_cases[0]);
    size_t i;
    CHECK(count == 56U);
    for (i = 0; i < count; ++i) {
        size_t calls = evaluation_allocation_count(&function_cases[i]);
        size_t fail_at;
        for (fail_at = 1U; fail_at <= calls; ++fail_at)
            run_failure_attempt(&function_cases[i], fail_at, calls);
    }
}

static void test_inplace_failure_and_sticky_recovery(void) {
    fault_state_t state = FAULT_STATE_INITIALIZER;
    ppf_allocator_t allocator = fault_allocator(&state);
    ppf_context_t ctx;
    ppf_t value;
    ppf_t factor;
    object_snapshot_t before;
    size_t baseline_count;
    size_t baseline_bytes;
    int rc;

    CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
    ctx.arena_block_bytes = 64U;
    CHECK(ppf_init2_ctx(&value, 96U, &ctx) == 0);
    CHECK(ppf_init2_ctx(&factor, 96U, &ctx) == 0);
    CHECK(ppf_set_ui(&value, 13UL, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_ui(&factor, 7UL, PPF_RNDN, &ctx) == 0);
    snapshot_take(&before, &value);
    baseline_count = state.live_count;
    baseline_bytes = state.live_bytes;
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    reset_counter(&state, 1U);
    rc = ppf_mul(&value, &value, &factor, PPF_RNDN, &ctx);
    stop_injection(&state);
    CHECK(rc == PPF_TERNARY_ERROR);
    CHECK(state.injected_failures == 1U);
    CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ALLOC) != 0U);
    snapshot_assert_equal(&before, &value);
    CHECK(state.live_count == baseline_count);
    CHECK(state.live_bytes == baseline_bytes);

    /* A pre-existing allocation flag must remain sticky without preventing a
       later successful transaction from committing. */
    reset_counter(&state, 0U);
    rc = ppf_mul(&value, &value, &factor, PPF_RNDN, &ctx);
    CHECK(rc != PPF_TERNARY_ERROR);
    CHECK((ppf_context_flags(&ctx) & PPF_FLAG_ALLOC) != 0U);
    CHECK(ppf_cmp_ui(&value, 91UL, &ctx) == 0);

    snapshot_clear(&before);
    ppf_clear(&factor);
    ppf_clear(&value);
    ppf_context_clear(&ctx);
    assert_clean(&state);
}

static void test_external_storage_transaction(void) {
    fault_state_t state = FAULT_STATE_INITIALIZER;
    ppf_allocator_t allocator = fault_allocator(&state);
    ppf_context_t ctx;
    ppf_t input;
    ppf_t output;
    object_snapshot_t before;
    const ppf_prec_t precision = TEST_PRECISION;
    size_t bytes = ppf_custom_get_size(precision);
    size_t alignment = _Alignof(mp_limb_t);
    size_t rounded = (bytes + alignment - 1U) / alignment * alignment;
    void *storage = aligned_alloc(alignment, rounded);
    size_t baseline_count;
    size_t baseline_bytes;

    CHECK(storage != NULL);
    CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
    CHECK(ppf_init2_ctx(&input, precision, &ctx) == 0);
    CHECK(ppf_set_ui(&input, 13UL, PPF_RNDN, &ctx) == 0);
    ppf_custom_init(storage, precision);
    CHECK(ppf_custom_init_set(&output, PPF_ZERO_KIND, 0,
                              precision, storage) == 0);
    CHECK(ppf_set_ui(&output, 12345UL, PPF_RNDN, &ctx) == 0);
    snapshot_take(&before, &output);
    baseline_count = state.live_count;
    baseline_bytes = state.live_bytes;
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    reset_counter(&state, 1U);
    CHECK(ppf_mul_ui(&output, &input, 7UL, PPF_RNDN, &ctx) ==
          PPF_TERNARY_ERROR);
    stop_injection(&state);
    CHECK(state.injected_failures == 1U);
    snapshot_assert_equal(&before, &output);
    CHECK(ppf_custom_get_significand(&output) == storage);
    CHECK(state.live_count == baseline_count);
    CHECK(state.live_bytes == baseline_bytes);

    reset_counter(&state, 0U);
    CHECK(ppf_mul_ui(&output, &input, 7UL, PPF_RNDN, &ctx) !=
          PPF_TERNARY_ERROR);
    CHECK(ppf_custom_get_significand(&output) == storage);
    CHECK(ppf_cmp_ui(&output, 91UL, &ctx) == 0);

    snapshot_clear(&before);
    ppf_clear(&output);
    ppf_clear(&input);
    ppf_context_clear(&ctx);
    free(storage);
    assert_clean(&state);
}

static void test_batch_same_index_alias(void) {
    fault_state_t state = FAULT_STATE_INITIALIZER;
    fixture_t fixture;
    ppf_t *outputs[3];
    const ppf_t *numerators[3];
    object_snapshot_t before[3];
    int ternaries_before[3];
    size_t baseline_count;
    size_t baseline_bytes;
    size_t calls;
    size_t i;

    fixture_init(&fixture, &state, TEST_PRECISION, 1);
    outputs[0] = &fixture.a;
    outputs[1] = &fixture.c;
    outputs[2] = &fixture.d;
    numerators[0] = &fixture.a;
    numerators[1] = &fixture.c;
    numerators[2] = &fixture.d;
    for (i = 0; i < 3U; ++i) snapshot_take(&before[i], outputs[i]);
    memcpy(ternaries_before, fixture.ternaries, sizeof(ternaries_before));
    baseline_count = state.live_count;
    baseline_bytes = state.live_bytes;

    reset_counter(&state, 0U);
    CHECK(ppf_div_with_plan_many(outputs, numerators, 3U, fixture.plan,
                                 PPF_RNDN, 4UL, fixture.ternaries,
                                 &fixture.ctx) == 0);
    calls = state.allocation_calls;
    CHECK(calls >= 1U);
    for (i = 0; i < 3U; ++i) {
        snapshot_clear(&before[i]);
    }
    fixture_clear(&fixture);
    assert_clean(&state);

    fixture_init(&fixture, &state, TEST_PRECISION, 1);
    outputs[0] = &fixture.a;
    outputs[1] = &fixture.c;
    outputs[2] = &fixture.d;
    numerators[0] = &fixture.a;
    numerators[1] = &fixture.c;
    numerators[2] = &fixture.d;
    for (i = 0; i < 3U; ++i) snapshot_take(&before[i], outputs[i]);
    memcpy(ternaries_before, fixture.ternaries, sizeof(ternaries_before));
    baseline_count = state.live_count;
    baseline_bytes = state.live_bytes;
    ppf_context_flags_clear(&fixture.ctx, PPF_FLAGS_ALL);
    reset_counter(&state, calls);
    CHECK(ppf_div_with_plan_many(outputs, numerators, 3U, fixture.plan,
                                 PPF_RNDN, 4UL, fixture.ternaries,
                                 &fixture.ctx) == -1);
    stop_injection(&state);
    CHECK(state.injected_failures == 1U);
    CHECK((ppf_context_flags(&fixture.ctx) & PPF_FLAG_ALLOC) != 0U);
    for (i = 0; i < 3U; ++i) snapshot_assert_equal(&before[i], outputs[i]);
    CHECK(memcmp(ternaries_before, fixture.ternaries,
                 sizeof(ternaries_before)) == 0);
    CHECK(state.live_count == baseline_count);
    CHECK(state.live_bytes == baseline_bytes);
    for (i = 0; i < 3U; ++i) snapshot_clear(&before[i]);
    fixture_clear(&fixture);
    assert_clean(&state);
}

int main(void) {
    test_all_public_endpoints();
    test_inplace_failure_and_sticky_recovery();
    test_external_storage_transaction();
    test_batch_same_index_alias();
    ppf_default_context_cleanup();
    puts("arithmetic, fused-operation, and root allocation transactions passed");
    return 0;
}
