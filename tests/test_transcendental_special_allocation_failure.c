/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); \
} } while (0)

#define MAX_LIVE_RECORDS 16384u
#define DEFAULT_PRECISION 96u

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

typedef struct {
    ppf_context_t ctx;
    ppf_t quarter;
    ppf_t three_quarters;
    ppf_t five_quarters;
    ppf_t nine_quarters;
    ppf_t five_halves;
    ppf_t out1;
    ppf_t out2;
    mpz_t integer_exponent;
    int sign_result;
} fixture_t;

typedef enum {
    CASE_LOG,
    CASE_LOG_UI,
    CASE_LOG2,
    CASE_LOG10,
    CASE_LOG1P,
    CASE_LOG2P1,
    CASE_LOG10P1,
    CASE_EXP,
    CASE_EXP2,
    CASE_EXP10,
    CASE_EXPM1,
    CASE_EXP2M1,
    CASE_EXP10M1,
    CASE_POW,
    CASE_POWR,
    CASE_POW_UI,
    CASE_POW_SI,
    CASE_POW_UJ,
    CASE_POW_SJ,
    CASE_POWN,
    CASE_POW_Z,
    CASE_UI_POW_UI,
    CASE_UI_POW,
    CASE_COMPOUND_SI,
    CASE_COS,
    CASE_SIN,
    CASE_TAN,
    CASE_COSU,
    CASE_SINU,
    CASE_TANU,
    CASE_COSPI,
    CASE_SINPI,
    CASE_TANPI,
    CASE_SIN_COS,
    CASE_SEC,
    CASE_CSC,
    CASE_COT,
    CASE_ACOS,
    CASE_ASIN,
    CASE_ATAN,
    CASE_ACOSU,
    CASE_ASINU,
    CASE_ATANU,
    CASE_ACOSPI,
    CASE_ASINPI,
    CASE_ATANPI,
    CASE_ATAN2,
    CASE_ATAN2U,
    CASE_ATAN2PI,
    CASE_COSH,
    CASE_SINH,
    CASE_TANH,
    CASE_SINH_COSH,
    CASE_SECH,
    CASE_CSCH,
    CASE_COTH,
    CASE_ACOSH,
    CASE_ASINH,
    CASE_ATANH,
    CASE_EINT,
    CASE_LI2,
    CASE_GAMMA,
    CASE_GAMMA_INC,
    CASE_LNGAMMA,
    CASE_LGAMMA,
    CASE_DIGAMMA,
    CASE_BETA,
    CASE_ZETA,
    CASE_ZETA_UI,
    CASE_ERF,
    CASE_ERFC,
    CASE_J0,
    CASE_J1,
    CASE_JN,
    CASE_Y0,
    CASE_Y1,
    CASE_YN,
    CASE_AGM,
    CASE_AI
} operation_t;

typedef struct {
    const char *name;
    operation_t operation;
    int paired_output;
    int sign_output;
    int exhaustive;
} function_case_t;

static const function_case_t function_cases[] = {
    { "ppf_log", CASE_LOG, 0, 0, 1 },
    { "ppf_log_ui", CASE_LOG_UI, 0, 0, 0 },
    { "ppf_log2", CASE_LOG2, 0, 0, 0 },
    { "ppf_log10", CASE_LOG10, 0, 0, 0 },
    { "ppf_log1p", CASE_LOG1P, 0, 0, 0 },
    { "ppf_log2p1", CASE_LOG2P1, 0, 0, 0 },
    { "ppf_log10p1", CASE_LOG10P1, 0, 0, 0 },
    { "ppf_exp", CASE_EXP, 0, 0, 1 },
    { "ppf_exp2", CASE_EXP2, 0, 0, 0 },
    { "ppf_exp10", CASE_EXP10, 0, 0, 0 },
    { "ppf_expm1", CASE_EXPM1, 0, 0, 0 },
    { "ppf_exp2m1", CASE_EXP2M1, 0, 0, 0 },
    { "ppf_exp10m1", CASE_EXP10M1, 0, 0, 0 },
    { "ppf_pow", CASE_POW, 0, 0, 1 },
    { "ppf_powr", CASE_POWR, 0, 0, 0 },
    { "ppf_pow_ui", CASE_POW_UI, 0, 0, 0 },
    { "ppf_pow_si", CASE_POW_SI, 0, 0, 0 },
    { "ppf_pow_uj", CASE_POW_UJ, 0, 0, 0 },
    { "ppf_pow_sj", CASE_POW_SJ, 0, 0, 0 },
    { "ppf_pown", CASE_POWN, 0, 0, 0 },
    { "ppf_pow_z", CASE_POW_Z, 0, 0, 0 },
    { "ppf_ui_pow_ui", CASE_UI_POW_UI, 0, 0, 0 },
    { "ppf_ui_pow", CASE_UI_POW, 0, 0, 0 },
    { "ppf_compound_si", CASE_COMPOUND_SI, 0, 0, 0 },
    { "ppf_cos", CASE_COS, 0, 0, 0 },
    { "ppf_sin", CASE_SIN, 0, 0, 1 },
    { "ppf_tan", CASE_TAN, 0, 0, 0 },
    { "ppf_cosu", CASE_COSU, 0, 0, 0 },
    { "ppf_sinu", CASE_SINU, 0, 0, 0 },
    { "ppf_tanu", CASE_TANU, 0, 0, 0 },
    { "ppf_cospi", CASE_COSPI, 0, 0, 0 },
    { "ppf_sinpi", CASE_SINPI, 0, 0, 0 },
    { "ppf_tanpi", CASE_TANPI, 0, 0, 0 },
    { "ppf_sin_cos", CASE_SIN_COS, 1, 0, 1 },
    { "ppf_sec", CASE_SEC, 0, 0, 0 },
    { "ppf_csc", CASE_CSC, 0, 0, 0 },
    { "ppf_cot", CASE_COT, 0, 0, 0 },
    { "ppf_acos", CASE_ACOS, 0, 0, 0 },
    { "ppf_asin", CASE_ASIN, 0, 0, 0 },
    { "ppf_atan", CASE_ATAN, 0, 0, 0 },
    { "ppf_acosu", CASE_ACOSU, 0, 0, 0 },
    { "ppf_asinu", CASE_ASINU, 0, 0, 0 },
    { "ppf_atanu", CASE_ATANU, 0, 0, 0 },
    { "ppf_acospi", CASE_ACOSPI, 0, 0, 0 },
    { "ppf_asinpi", CASE_ASINPI, 0, 0, 0 },
    { "ppf_atanpi", CASE_ATANPI, 0, 0, 0 },
    { "ppf_atan2", CASE_ATAN2, 0, 0, 1 },
    { "ppf_atan2u", CASE_ATAN2U, 0, 0, 0 },
    { "ppf_atan2pi", CASE_ATAN2PI, 0, 0, 0 },
    { "ppf_cosh", CASE_COSH, 0, 0, 0 },
    { "ppf_sinh", CASE_SINH, 0, 0, 0 },
    { "ppf_tanh", CASE_TANH, 0, 0, 0 },
    { "ppf_sinh_cosh", CASE_SINH_COSH, 1, 0, 1 },
    { "ppf_sech", CASE_SECH, 0, 0, 0 },
    { "ppf_csch", CASE_CSCH, 0, 0, 0 },
    { "ppf_coth", CASE_COTH, 0, 0, 0 },
    { "ppf_acosh", CASE_ACOSH, 0, 0, 0 },
    { "ppf_asinh", CASE_ASINH, 0, 0, 0 },
    { "ppf_atanh", CASE_ATANH, 0, 0, 0 },
    { "ppf_eint", CASE_EINT, 0, 0, 0 },
    { "ppf_li2", CASE_LI2, 0, 0, 0 },
    { "ppf_gamma", CASE_GAMMA, 0, 0, 1 },
    { "ppf_gamma_inc", CASE_GAMMA_INC, 0, 0, 0 },
    { "ppf_lngamma", CASE_LNGAMMA, 0, 0, 0 },
    { "ppf_lgamma", CASE_LGAMMA, 0, 1, 1 },
    { "ppf_digamma", CASE_DIGAMMA, 0, 0, 0 },
    { "ppf_beta", CASE_BETA, 0, 0, 0 },
    { "ppf_zeta", CASE_ZETA, 0, 0, 1 },
    { "ppf_zeta_ui", CASE_ZETA_UI, 0, 0, 0 },
    { "ppf_erf", CASE_ERF, 0, 0, 0 },
    { "ppf_erfc", CASE_ERFC, 0, 0, 0 },
    { "ppf_j0", CASE_J0, 0, 0, 0 },
    { "ppf_j1", CASE_J1, 0, 0, 0 },
    { "ppf_jn", CASE_JN, 0, 0, 1 },
    { "ppf_y0", CASE_Y0, 0, 0, 0 },
    { "ppf_y1", CASE_Y1, 0, 0, 0 },
    { "ppf_yn", CASE_YN, 0, 0, 0 },
    { "ppf_agm", CASE_AGM, 0, 0, 1 },
    { "ppf_ai", CASE_AI, 0, 0, 0 },
};

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

static void reset_counter(fault_state_t *state, size_t fail_at) {
    state->allocation_calls = 0u;
    state->injected_failures = 0u;
    state->fail_at = fail_at;
}

static void stop_injection(fault_state_t *state) {
    state->fail_at = 0u;
}

static void assert_clean(const fault_state_t *state) {
    CHECK(state->bookkeeping_error == 0);
    CHECK(state->live_count == 0u);
    CHECK(state->live_bytes == 0u);
}

static void purge_caches(void) {
    ppf_free_cache();
    ppf_free_constant_caches();
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
                         ppf_prec_t precision) {
    ppf_allocator_t allocator = fault_allocator(state);
    memset(fixture, 0, sizeof(*fixture));
    CHECK(ppf_context_init_custom(&fixture->ctx, &allocator) == 0);
#define INIT_OBJECT(name) \
    CHECK(ppf_init2_ctx(&fixture->name, precision, &fixture->ctx) == 0)
    INIT_OBJECT(quarter);
    INIT_OBJECT(three_quarters);
    INIT_OBJECT(five_quarters);
    INIT_OBJECT(nine_quarters);
    INIT_OBJECT(five_halves);
    INIT_OBJECT(out1);
    INIT_OBJECT(out2);
#undef INIT_OBJECT
    CHECK(ppf_set_ui_2exp(&fixture->quarter, 1UL, -2,
                          PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui_2exp(&fixture->three_quarters, 3UL, -2,
                          PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui_2exp(&fixture->five_quarters, 5UL, -2,
                          PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui_2exp(&fixture->nine_quarters, 9UL, -2,
                          PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui_2exp(&fixture->five_halves, 5UL, -1,
                          PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_ui(&fixture->out1, 12345UL,
                     PPF_RNDN, &fixture->ctx) == 0);
    CHECK(ppf_set_si(&fixture->out2, -6789L,
                     PPF_RNDN, &fixture->ctx) == 0);
    mpz_init_set_si(fixture->integer_exponent, -3L);
    fixture->sign_result = 8675309;
}

static void fixture_clear(fixture_t *fixture) {
    mpz_clear(fixture->integer_exponent);
    ppf_clear(&fixture->quarter);
    ppf_clear(&fixture->three_quarters);
    ppf_clear(&fixture->five_quarters);
    ppf_clear(&fixture->nine_quarters);
    ppf_clear(&fixture->five_halves);
    ppf_clear(&fixture->out1);
    ppf_clear(&fixture->out2);
    ppf_context_clear(&fixture->ctx);
    memset(fixture, 0, sizeof(*fixture));
}

static int evaluate_case(const function_case_t *test, fixture_t *f) {
    switch (test->operation) {
        case CASE_LOG: return ppf_log(&f->out1, &f->five_quarters, PPF_RNDN, &f->ctx);
        case CASE_LOG_UI: return ppf_log_ui(&f->out1, 3UL, PPF_RNDN, &f->ctx);
        case CASE_LOG2: return ppf_log2(&f->out1, &f->five_quarters, PPF_RNDN, &f->ctx);
        case CASE_LOG10: return ppf_log10(&f->out1, &f->five_quarters, PPF_RNDN, &f->ctx);
        case CASE_LOG1P: return ppf_log1p(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_LOG2P1: return ppf_log2p1(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_LOG10P1: return ppf_log10p1(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_EXP: return ppf_exp(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_EXP2: return ppf_exp2(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_EXP10: return ppf_exp10(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_EXPM1: return ppf_expm1(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_EXP2M1: return ppf_exp2m1(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_EXP10M1: return ppf_exp10m1(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_POW: return ppf_pow(&f->out1, &f->five_quarters, &f->nine_quarters, PPF_RNDN, &f->ctx);
        case CASE_POWR: return ppf_powr(&f->out1, &f->five_quarters, &f->nine_quarters, PPF_RNDN, &f->ctx);
        case CASE_POW_UI: return ppf_pow_ui(&f->out1, &f->five_quarters, 3UL, PPF_RNDN, &f->ctx);
        case CASE_POW_SI: return ppf_pow_si(&f->out1, &f->five_quarters, -3L, PPF_RNDN, &f->ctx);
        case CASE_POW_UJ: return ppf_pow_uj(&f->out1, &f->five_quarters, (uintmax_t)3, PPF_RNDN, &f->ctx);
        case CASE_POW_SJ: return ppf_pow_sj(&f->out1, &f->five_quarters, (intmax_t)-3, PPF_RNDN, &f->ctx);
        case CASE_POWN: return ppf_pown(&f->out1, &f->five_quarters, (intmax_t)3, PPF_RNDN, &f->ctx);
        case CASE_POW_Z: return ppf_pow_z(&f->out1, &f->five_quarters, f->integer_exponent, PPF_RNDN, &f->ctx);
        case CASE_UI_POW_UI: return ppf_ui_pow_ui(&f->out1, 3UL, 5UL, PPF_RNDN, &f->ctx);
        case CASE_UI_POW: return ppf_ui_pow(&f->out1, 3UL, &f->five_quarters, PPF_RNDN, &f->ctx);
        case CASE_COMPOUND_SI: return ppf_compound_si(&f->out1, &f->quarter, 3L, PPF_RNDN, &f->ctx);
        case CASE_COS: return ppf_cos(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_SIN: return ppf_sin(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_TAN: return ppf_tan(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_COSU: return ppf_cosu(&f->out1, &f->quarter, 7UL, PPF_RNDN, &f->ctx);
        case CASE_SINU: return ppf_sinu(&f->out1, &f->quarter, 7UL, PPF_RNDN, &f->ctx);
        case CASE_TANU: return ppf_tanu(&f->out1, &f->quarter, 7UL, PPF_RNDN, &f->ctx);
        case CASE_COSPI: return ppf_cospi(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_SINPI: return ppf_sinpi(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_TANPI: return ppf_tanpi(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_SIN_COS: return ppf_sin_cos(&f->out1, &f->out2, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_SEC: return ppf_sec(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_CSC: return ppf_csc(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_COT: return ppf_cot(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ACOS: return ppf_acos(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ASIN: return ppf_asin(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ATAN: return ppf_atan(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ACOSU: return ppf_acosu(&f->out1, &f->quarter, 7UL, PPF_RNDN, &f->ctx);
        case CASE_ASINU: return ppf_asinu(&f->out1, &f->quarter, 7UL, PPF_RNDN, &f->ctx);
        case CASE_ATANU: return ppf_atanu(&f->out1, &f->quarter, 7UL, PPF_RNDN, &f->ctx);
        case CASE_ACOSPI: return ppf_acospi(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ASINPI: return ppf_asinpi(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ATANPI: return ppf_atanpi(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ATAN2: return ppf_atan2(&f->out1, &f->three_quarters, &f->five_quarters, PPF_RNDN, &f->ctx);
        case CASE_ATAN2U: return ppf_atan2u(&f->out1, &f->three_quarters, &f->five_quarters, 7UL, PPF_RNDN, &f->ctx);
        case CASE_ATAN2PI: return ppf_atan2pi(&f->out1, &f->three_quarters, &f->five_quarters, PPF_RNDN, &f->ctx);
        case CASE_COSH: return ppf_cosh(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_SINH: return ppf_sinh(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_TANH: return ppf_tanh(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_SINH_COSH: return ppf_sinh_cosh(&f->out1, &f->out2, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_SECH: return ppf_sech(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_CSCH: return ppf_csch(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_COTH: return ppf_coth(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ACOSH: return ppf_acosh(&f->out1, &f->five_quarters, PPF_RNDN, &f->ctx);
        case CASE_ASINH: return ppf_asinh(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ATANH: return ppf_atanh(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_EINT: return ppf_eint(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_LI2: return ppf_li2(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_GAMMA: return ppf_gamma(&f->out1, &f->five_halves, PPF_RNDN, &f->ctx);
        case CASE_GAMMA_INC: return ppf_gamma_inc(&f->out1, &f->five_halves, &f->five_quarters, PPF_RNDN, &f->ctx);
        case CASE_LNGAMMA: return ppf_lngamma(&f->out1, &f->five_halves, PPF_RNDN, &f->ctx);
        case CASE_LGAMMA: return ppf_lgamma(&f->out1, &f->sign_result, &f->five_halves, PPF_RNDN, &f->ctx);
        case CASE_DIGAMMA: return ppf_digamma(&f->out1, &f->five_halves, PPF_RNDN, &f->ctx);
        case CASE_BETA: return ppf_beta(&f->out1, &f->five_halves, &f->five_quarters, PPF_RNDN, &f->ctx);
        case CASE_ZETA: return ppf_zeta(&f->out1, &f->nine_quarters, PPF_RNDN, &f->ctx);
        case CASE_ZETA_UI: return ppf_zeta_ui(&f->out1, 3UL, PPF_RNDN, &f->ctx);
        case CASE_ERF: return ppf_erf(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_ERFC: return ppf_erfc(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        case CASE_J0: return ppf_j0(&f->out1, &f->three_quarters, PPF_RNDN, &f->ctx);
        case CASE_J1: return ppf_j1(&f->out1, &f->three_quarters, PPF_RNDN, &f->ctx);
        case CASE_JN: return ppf_jn(&f->out1, 3L, &f->three_quarters, PPF_RNDN, &f->ctx);
        case CASE_Y0: return ppf_y0(&f->out1, &f->three_quarters, PPF_RNDN, &f->ctx);
        case CASE_Y1: return ppf_y1(&f->out1, &f->three_quarters, PPF_RNDN, &f->ctx);
        case CASE_YN: return ppf_yn(&f->out1, 3L, &f->three_quarters, PPF_RNDN, &f->ctx);
        case CASE_AGM: return ppf_agm(&f->out1, &f->five_quarters, &f->three_quarters, PPF_RNDN, &f->ctx);
        case CASE_AI: return ppf_ai(&f->out1, &f->quarter, PPF_RNDN, &f->ctx);
        default: return PPF_TERNARY_ERROR;
    }
}

static size_t evaluation_allocation_count(const function_case_t *test,
                                          ppf_prec_t precision,
                                          unsigned long trig_threads) {
    fault_state_t state = {0};
    fixture_t fixture;
    size_t calls;
    int rc;
    purge_caches();
    ppf_set_trig_threads(trig_threads);
    fixture_init(&fixture, &state, precision);
    reset_counter(&state, 0u);
    ppf_context_flags_clear(&fixture.ctx, PPF_FLAGS_ALL);
    rc = evaluate_case(test, &fixture);
    if (rc == PPF_TERNARY_ERROR) {
        fprintf(stderr, "successful baseline failed case=%s flags=0x%x\n",
                test->name, (unsigned)ppf_context_flags(&fixture.ctx));
        exit(1);
    }
    calls = state.allocation_calls;
    CHECK(calls >= (test->paired_output ? 2u : 1u));
    stop_injection(&state);
    purge_caches();
    fixture_clear(&fixture);
    assert_clean(&state);
    return calls;
}

static size_t selected_positions(size_t calls, size_t positions[7]) {
    size_t candidates[7] = {1u, 2u, calls / 4u, calls / 2u,
                             (3u * calls) / 4u,
                             calls > 1u ? calls - 1u : 1u, calls};
    size_t count = 0u;
    for (size_t i = 0u; i < 7u; ++i) {
        size_t value = candidates[i];
        int duplicate = 0;
        if (value < 1u || value > calls) continue;
        for (size_t j = 0u; j < count; ++j)
            if (positions[j] == value) duplicate = 1;
        if (!duplicate) positions[count++] = value;
    }
    return count;
}

static void run_failure_attempt(const function_case_t *test,
                                ppf_prec_t precision,
                                unsigned long trig_threads,
                                size_t fail_at, size_t expected_calls) {
    fault_state_t state = {0};
    fixture_t fixture;
    object_snapshot_t before1, before2;
    size_t baseline_count, baseline_bytes;
    int sign_before, rc;

    purge_caches();
    ppf_set_trig_threads(trig_threads);
    fixture_init(&fixture, &state, precision);
    snapshot_take(&before1, &fixture.out1);
    snapshot_take(&before2, &fixture.out2);
    sign_before = fixture.sign_result;
    baseline_count = state.live_count;
    baseline_bytes = state.live_bytes;
    ppf_context_flags_clear(&fixture.ctx, PPF_FLAGS_ALL);
    reset_counter(&state, fail_at);
    rc = evaluate_case(test, &fixture);
    stop_injection(&state);
    purge_caches();

    if (rc != PPF_TERNARY_ERROR || state.injected_failures != 1u) {
        fprintf(stderr,
                "failure mismatch case=%s fail_at=%zu baseline_calls=%zu "
                "observed_calls=%zu rc=%d injected=%zu flags=0x%x\n",
                test->name, fail_at, expected_calls, state.allocation_calls,
                rc, state.injected_failures,
                (unsigned)ppf_context_flags(&fixture.ctx));
        exit(1);
    }
    CHECK((ppf_context_flags(&fixture.ctx) & PPF_FLAG_ALLOC) != 0u);
    snapshot_assert_equal(&before1, &fixture.out1);
    snapshot_assert_equal(&before2, &fixture.out2);
    if (test->sign_output) CHECK(fixture.sign_result == sign_before);
    CHECK(state.live_count == baseline_count);
    CHECK(state.live_bytes == baseline_bytes);
    CHECK(state.bookkeeping_error == 0);

    snapshot_clear(&before1);
    snapshot_clear(&before2);
    fixture_clear(&fixture);
    assert_clean(&state);
}

static void test_public_endpoint_atomicity(int probe_only) {
    const size_t count = sizeof(function_cases) / sizeof(function_cases[0]);
    CHECK(count == 79u);
    ppf_set_trig_threads(1UL);
    for (size_t i = 0u; i < count; ++i) {
        const function_case_t *test = &function_cases[i];
        size_t calls = evaluation_allocation_count(
            test, DEFAULT_PRECISION, 1UL);
        if (probe_only) {
            printf("%-20s calls=%zu exhaustive=%d\n",
                   test->name, calls, test->exhaustive);
            continue;
        }
        if (test->exhaustive) {
            for (size_t fail_at = 1u; fail_at <= calls; ++fail_at)
                run_failure_attempt(test, DEFAULT_PRECISION, 1UL,
                                    fail_at, calls);
        } else {
            size_t positions[7];
            size_t n = selected_positions(calls, positions);
            for (size_t j = 0u; j < n; ++j)
                run_failure_attempt(test, DEFAULT_PRECISION, 1UL,
                                    positions[j], calls);
        }
    }
}

static void test_alias_failure_atomicity(void) {
    fault_state_t state = {0};
    fixture_t fixture;
    object_snapshot_t before;
    size_t baseline_count, baseline_bytes;

    purge_caches();
    fixture_init(&fixture, &state, DEFAULT_PRECISION);
    snapshot_take(&before, &fixture.five_quarters);
    baseline_count = state.live_count;
    baseline_bytes = state.live_bytes;
    ppf_context_flags_clear(&fixture.ctx, PPF_FLAGS_ALL);
    reset_counter(&state, 1u);
    CHECK(ppf_log(&fixture.five_quarters, &fixture.five_quarters,
                  PPF_RNDN, &fixture.ctx) == PPF_TERNARY_ERROR);
    stop_injection(&state);
    purge_caches();
    CHECK(state.injected_failures == 1u);
    CHECK((ppf_context_flags(&fixture.ctx) & PPF_FLAG_ALLOC) != 0u);
    snapshot_assert_equal(&before, &fixture.five_quarters);
    CHECK(state.live_count == baseline_count);
    CHECK(state.live_bytes == baseline_bytes);
    snapshot_clear(&before);
    fixture_clear(&fixture);
    assert_clean(&state);
}

static void test_external_storage_atomicity(void) {
    fault_state_t state = {0};
    ppf_allocator_t allocator = fault_allocator(&state);
    ppf_context_t ctx;
    ppf_t input, output;
    object_snapshot_t before;
    const ppf_prec_t precision = 160u;
    size_t bytes = ppf_custom_get_size(precision);
    size_t alignment = _Alignof(mp_limb_t);
    size_t rounded = (bytes + alignment - 1u) / alignment * alignment;
    void *storage = aligned_alloc(alignment, rounded);
    size_t baseline_count, baseline_bytes;

    CHECK(storage != NULL);
    CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
    CHECK(ppf_init2_ctx(&input, precision, &ctx) == 0);
    CHECK(ppf_set_ui_2exp(&input, 1UL, -2, PPF_RNDN, &ctx) == 0);
    ppf_custom_init(storage, precision);
    CHECK(ppf_custom_init_set(&output, PPF_ZERO_KIND, 0,
                              precision, storage) == 0);
    CHECK(ppf_set_ui(&output, 12345UL, PPF_RNDN, &ctx) == 0);
    snapshot_take(&before, &output);
    baseline_count = state.live_count;
    baseline_bytes = state.live_bytes;
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    reset_counter(&state, 1u);
    CHECK(ppf_exp(&output, &input, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR);
    stop_injection(&state);
    purge_caches();
    CHECK(state.injected_failures == 1u);
    snapshot_assert_equal(&before, &output);
    CHECK(ppf_custom_get_significand(&output) == storage);
    CHECK(state.live_count == baseline_count);
    CHECK(state.live_bytes == baseline_bytes);
    snapshot_clear(&before);
    ppf_clear(&output);
    ppf_clear(&input);
    ppf_context_clear(&ctx);
    free(storage);
    assert_clean(&state);
}

static void test_high_precision_workspaces(void) {
    const function_case_t *sin_case = NULL;
    const function_case_t *log_case = NULL;
    const size_t count = sizeof(function_cases) / sizeof(function_cases[0]);
    for (size_t i = 0u; i < count; ++i) {
        if (function_cases[i].operation == CASE_SIN) sin_case = &function_cases[i];
        if (function_cases[i].operation == CASE_LOG) log_case = &function_cases[i];
    }
    CHECK(sin_case != NULL && log_case != NULL);
    {
        size_t calls = evaluation_allocation_count(sin_case, 2048u, 4UL);
        size_t positions[7];
        size_t n = selected_positions(calls, positions);
        CHECK(calls >= 3u);
        for (size_t i = 0u; i < n; ++i)
            run_failure_attempt(sin_case, 2048u, 4UL, positions[i], calls);
    }
    {
        size_t calls = evaluation_allocation_count(log_case, 2048u, 1UL);
        size_t positions[7];
        size_t n = selected_positions(calls, positions);
        CHECK(calls >= 3u);
        for (size_t i = 0u; i < n; ++i)
            run_failure_attempt(log_case, 2048u, 1UL, positions[i], calls);
    }
    ppf_set_trig_threads(1UL);
}

static void test_sticky_allocation_flag_recovery(void) {
    fault_state_t state = {0};
    fixture_t fixture;
    purge_caches();
    fixture_init(&fixture, &state, DEFAULT_PRECISION);
    ppf_context_flags_clear(&fixture.ctx, PPF_FLAGS_ALL);
    reset_counter(&state, 1u);
    CHECK(ppf_exp(&fixture.out1, &fixture.quarter,
                  PPF_RNDN, &fixture.ctx) == PPF_TERNARY_ERROR);
    CHECK(state.injected_failures == 1u);
    stop_injection(&state);
    CHECK((ppf_context_flags(&fixture.ctx) & PPF_FLAG_ALLOC) != 0u);
    CHECK(ppf_exp(&fixture.out1, &fixture.quarter,
                  PPF_RNDN, &fixture.ctx) != PPF_TERNARY_ERROR);
    CHECK((ppf_context_flags(&fixture.ctx) & PPF_FLAG_ALLOC) != 0u);
    purge_caches();
    fixture_clear(&fixture);
    assert_clean(&state);
}

int main(void) {
    int probe_only = getenv("PPF_ALLOCATION_PROBE") != NULL;
    test_public_endpoint_atomicity(probe_only);
    if (probe_only) return 0;
    test_alias_failure_atomicity();
    test_external_storage_atomicity();
    test_high_precision_workspaces();
    test_sticky_allocation_flag_recovery();
    puts("transcendental and special allocation atomicity: PASS");
    return 0;
}
