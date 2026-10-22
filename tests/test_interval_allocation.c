/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "special_internal.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

typedef struct { size_t calls, fail_at, live; } allocation_state;
static void *fault_malloc(void *opaque, size_t size) {
    allocation_state *state = opaque;
    void *ptr;
    if (++state->calls == state->fail_at) return NULL;
    ptr = malloc(size); CHECK(ptr != NULL); ++state->live; return ptr;
}
static void *fault_realloc(void *opaque, void *ptr, size_t old, size_t size) {
    allocation_state *state = opaque;
    void *next;
    int new_allocation = ptr == NULL;
    (void)old;
    if (++state->calls == state->fail_at) return NULL;
    next = realloc(ptr, size); CHECK(next != NULL);
    if (new_allocation) ++state->live;
    return next;
}
static void fault_free(void *opaque, void *ptr, size_t size) {
    allocation_state *state = opaque;
    (void)size;
    CHECK(ptr != NULL && state->live > 0); --state->live; free(ptr);
}

static void failure_case(size_t fail_at) {
    allocation_state state = {0};
    ppf_allocator_t allocator = {fault_malloc, fault_realloc, fault_free, &state};
    ppf_context_t ctx;
    ppf_special_iv_t input, output;
    size_t live;
    CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
    CHECK(ppf_special_iv_init(&input, 128, &ctx) == 0);
    CHECK(ppf_special_iv_init(&output, 128, &ctx) == 0);
    CHECK(ppf_set_si(&input.lo, -2, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_ui(&input.hi, 3, PPF_RNDN, &ctx) == 0);
    live = state.live;
    state.calls = 0; state.fail_at = fail_at; ctx.flags = 0;
    CHECK(ppf_special_iv_abs(&output, &input, &ctx) == -1);
    CHECK(state.calls == fail_at && state.live == live);
    CHECK((ctx.flags & PPF_FLAG_ALLOC) != 0);
    CHECK(ppf_validate(&input.lo) && ppf_validate(&input.hi));
    state.fail_at = 0; ctx.flags = 0;
    CHECK(ppf_special_iv_abs(&output, &input, &ctx) == 0);
    CHECK(ppf_zero_p(&output.lo) && ppf_cmp_ui(&output.hi, 3, &ctx) == 0);
    ppf_special_iv_clear(&input); ppf_special_iv_clear(&output);
    ppf_context_clear(&ctx); CHECK(state.live == 0);
}

static void dyadic_endpoint_cases(void) {
    ppf_context_t ctx;
    ppf_special_iv_t interval;
    mpz_t lower, upper;
    ppf_exp_t exponent = 12345;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_context_set_range(&ctx, PPF_EMIN_MIN, PPF_EMAX_MAX) == 0);
    CHECK(ppf_special_iv_init(&interval, 128, &ctx) == 0);
    mpz_init_set_ui(lower, 17); mpz_init_set_ui(upper, 19);
    CHECK(ppf_set_si(&interval.lo, -1, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_exp(&interval.lo, PPF_EMAX_MAX, &ctx) == 0);
    ppf_set_zero(&interval.hi, 1);
    CHECK(ppf_special_iv_to_dyadic(lower, upper, &exponent, &interval) == 0);
    CHECK(mpz_sgn(lower) < 0 && mpz_sgn(upper) == 0);
    CHECK(exponent == PPF_EMAX_MAX - 128);
    ppf_set_zero(&interval.lo, -1);
    CHECK(ppf_set_ui(&interval.hi, 1, PPF_RNDN, &ctx) == 0);
    CHECK(ppf_set_exp(&interval.hi, PPF_EMIN_MIN, &ctx) == 0);
    CHECK(ppf_special_iv_to_dyadic(lower, upper, &exponent, &interval) == 0);
    CHECK(mpz_sgn(lower) == 0 && mpz_sgn(upper) > 0);
    CHECK(exponent == PPF_EMIN_MIN - 128);
    for (int bad = 0; bad < 4; ++bad) {
        CHECK(ppf_set_ui(&interval.lo, 2, PPF_RNDN, &ctx) == 0);
        CHECK(ppf_set_ui(&interval.hi, 1, PPF_RNDN, &ctx) == 0);
        if (bad == 0) ppf_set_inf(&interval.hi, 1);
        if (bad == 1) ppf_set_nan(&interval.lo, 1);
        if (bad == 3) CHECK(ppf_set_exp(&interval.lo, -1000001, &ctx) == 0);
        mpz_set_ui(lower, 17); mpz_set_ui(upper, 19); exponent = 12345;
        CHECK(ppf_special_iv_to_dyadic(lower, upper, &exponent, &interval) == -1);
        CHECK(mpz_cmp_ui(lower, 17) == 0 && mpz_cmp_ui(upper, 19) == 0);
        CHECK(exponent == 12345);
    }
    mpz_clear(lower); mpz_clear(upper);
    ppf_special_iv_clear(&interval); ppf_context_clear(&ctx);
}

int main(void) {
    failure_case(1); failure_case(2); dyadic_endpoint_cases();
    ppf_thread_cleanup();
    puts("Interval absolute-value allocation regressions passed");
    return 0;
}
