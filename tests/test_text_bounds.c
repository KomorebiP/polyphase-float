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
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

static size_t allocation_calls;
static void *count_malloc(void *opaque, size_t size) {
    (void)opaque; ++allocation_calls; return malloc(size);
}
static void *count_realloc(void *opaque, void *ptr, size_t old, size_t size) {
    (void)opaque; (void)old; ++allocation_calls; return realloc(ptr, size);
}
static void count_free(void *opaque, void *ptr, size_t size) {
    (void)opaque; (void)size; free(ptr);
}

static void rejected_output(ppf_t *x, ppf_context_t *ctx, int base,
                            size_t digits, int expect_range) {
    char output[32], before[32];
    ppf_exp_t exponent = 12345;
    size_t calls = allocation_calls;
    memset(output, 'X', sizeof(output)); memcpy(before, output, sizeof(output));
    ctx->flags = 0;
    CHECK(ppf_get_str(output, &exponent, base, digits, x, PPF_RNDN, ctx) == NULL);
    CHECK(exponent == 12345 && memcmp(output, before, sizeof(output)) == 0);
    CHECK(allocation_calls == calls);
    if (expect_range) CHECK((ctx->flags & PPF_FLAG_ERANGE) != 0);
    ctx->flags = 0;
    CHECK(ppf_get_str(NULL, &exponent, base, digits, x, PPF_RNDN, ctx) == NULL);
    CHECK(exponent == 12345 && allocation_calls == calls);
    if (expect_range) CHECK((ctx->flags & PPF_FLAG_ERANGE) != 0);
}

int main(void) {
    static const int invalid[] = {INT_MIN, INT_MAX, -63, 63, -1, 0, 1};
    static const int valid[] = {-62, -36, -16, -2, 2, 16, 36, 62};
    ppf_allocator_t allocator = {count_malloc, count_realloc, count_free, NULL};
    ppf_context_t ctx;
    ppf_t x;
    CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
    CHECK(ppf_init2_ctx(&x, 128, &ctx) == 0);
    ppf_set_zero(&x, -1);
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(ppf_get_str_ndigits(invalid[i], 128) == 0);
        rejected_output(&x, &ctx, invalid[i], 5, 0);
    }
    rejected_output(&x, &ctx, 10, SIZE_MAX, 1);
    rejected_output(&x, &ctx, 10, SIZE_MAX - 1u, 1);
    ppf_set_zero(&x, 1);
    rejected_output(&x, &ctx, 10, SIZE_MAX, 1);
    ppf_set_zero(&x, -1);
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        ppf_exp_t exponent = 12345;
        char *text;
        CHECK(ppf_get_str_ndigits(valid[i], 128) > 0);
        text = ppf_get_str(NULL, &exponent, valid[i], 5, &x, PPF_RNDN, &ctx);
        CHECK(text && strcmp(text, "-00000") == 0 && exponent == 0);
        ppf_free_str(text);
    }
    ppf_clear(&x); ppf_context_clear(&ctx); ppf_thread_cleanup();
    puts("Radix and text-length boundary regressions passed");
    return 0;
}
