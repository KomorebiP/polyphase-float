/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "polyphase_float.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    if (state->fail_at != 0 && state->allocation_calls == state->fail_at) {
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

static void assert_alloc_flag(const ppf_context_t *ctx) {
    CHECK((ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0);
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

static long stream_size(FILE *stream) {
    long current, length;
    CHECK(stream != NULL);
    CHECK(fflush(stream) == 0);
    current = ftell(stream);
    CHECK(current >= 0);
    CHECK(fseek(stream, 0, SEEK_END) == 0);
    length = ftell(stream);
    CHECK(length >= 0);
    CHECK(fseek(stream, current, SEEK_SET) == 0);
    return length;
}

static FILE *stream_from_text(const char *text) {
    FILE *stream = tmpfile();
    size_t bytes = strlen(text);
    CHECK(stream != NULL);
    CHECK(fwrite(text, 1, bytes, stream) == bytes);
    CHECK(fflush(stream) == 0);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    return stream;
}

static void test_get_str_allocator_provenance(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t x;
    ppf_exp_t exponent = 0;
    char *text;

    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&x, 256, &ctx) == 0);
    CHECK(ppf_set_ui(&x, 12345u, PPF_RNDN, &ctx) == 0);
    disable_failure(&state);
    text = ppf_get_str(NULL, &exponent, 10, 0, &x, PPF_RNDN, &ctx);
    CHECK(text != NULL);
    CHECK(state.allocation_calls >= 1u);

    /* The returned string owns a copy of its allocator provenance and can be
       released after the originating numerical context has been cleared. */
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    CHECK(state.live_count == 1u);
    ppf_free_str(text);
    assert_clean(&state);
}

static size_t get_str_allocation_count(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t x;
    ppf_exp_t exponent = 0;
    char *text;
    size_t calls;

    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&x, 256, &ctx) == 0);
    CHECK(ppf_set_ui(&x, 12345u, PPF_RNDN, &ctx) == 0);
    disable_failure(&state);
    text = ppf_get_str(NULL, &exponent, 10, 0, &x, PPF_RNDN, &ctx);
    CHECK(text != NULL);
    calls = state.allocation_calls;
    CHECK(calls >= 1u);
    ppf_free_str(text);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    return calls;
}

static void test_get_str_and_out_str_failures(void) {
    size_t calls = get_str_allocation_count();

    for (size_t fail_at = 1u; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t x;
        object_snapshot_t before;
        ppf_exp_t exponent = 0;
        char *text;

        setup_context(&ctx, &state);
        CHECK(ppf_init2_ctx(&x, 256, &ctx) == 0);
        CHECK(ppf_set_ui(&x, 12345u, PPF_RNDN, &ctx) == 0);
        snapshot_take(&before, &x);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, fail_at);
        text = ppf_get_str(NULL, &exponent, 10, 0, &x, PPF_RNDN, &ctx);
        CHECK(state.injected_failures == 1u);
        assert_alloc_flag(&ctx);
        snapshot_assert_equal(&before, &x);
        if (text) {
            CHECK(*text != 0);
            ppf_free_str(text);
        }
        disable_failure(&state);
        text = ppf_get_str(NULL, &exponent, 10, 0, &x, PPF_RNDN, &ctx);
        CHECK(text != NULL);
        ppf_free_str(text);
        snapshot_clear(&before);
        ppf_clear(&x);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }

    /* The exact-output path has one project allocation before the first byte
       becomes visible in the stream. */
    {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t x;
        object_snapshot_t before;
        FILE *stream;
        setup_context(&ctx, &state);
        CHECK(ppf_init2_ctx(&x, 256, &ctx) == 0);
        CHECK(ppf_set_ui(&x, 12345u, PPF_RNDN, &ctx) == 0);
        snapshot_take(&before, &x);
        stream = tmpfile();
        CHECK(stream != NULL);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, 1u);
        CHECK(ppf_out_str(stream, 10, 0, &x, PPF_RNDN, &ctx) == 0u);
        CHECK(state.injected_failures == 1u);
        CHECK(stream_size(stream) == 0);
        assert_alloc_flag(&ctx);
        snapshot_assert_equal(&before, &x);
        fclose(stream);
        snapshot_clear(&before);
        disable_failure(&state);
        ppf_clear(&x);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }
}

static size_t set_str_allocation_count(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t x;
    size_t calls;

    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&x, 256, &ctx) == 0);
    CHECK(ppf_set_ui(&x, 777u, PPF_RNDN, &ctx) == 0);
    disable_failure(&state);
    CHECK(ppf_set_str(&x, "123456789.125", 10, PPF_RNDN, &ctx) == 0);
    calls = state.allocation_calls;
    CHECK(calls >= 1u);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    return calls;
}

static void test_parsing_transactions(void) {
    size_t calls = set_str_allocation_count();

    for (size_t fail_at = 1u; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t x;
        object_snapshot_t before;
        int rc;
        setup_context(&ctx, &state);
        CHECK(ppf_init2_ctx(&x, 256, &ctx) == 0);
        CHECK(ppf_set_ui(&x, 777u, PPF_RNDN, &ctx) == 0);
        snapshot_take(&before, &x);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, fail_at);
        rc = ppf_set_str(&x, "123456789.125", 10, PPF_RNDN, &ctx);
        CHECK(state.injected_failures == 1u);
        assert_alloc_flag(&ctx);
        if (rc != 0) snapshot_assert_equal(&before, &x);
        else CHECK(ppf_cmp_ui(&x, 777u, &ctx) != 0);
        disable_failure(&state);
        CHECK(ppf_set_str(&x, "123456789.125", 10,
                          PPF_RNDN, &ctx) == 0);
        snapshot_clear(&before);
        ppf_clear(&x);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }

    /* A full-string parse must not commit a valid prefix followed by junk. */
    {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t x;
        object_snapshot_t before;
        setup_context(&ctx, &state);
        CHECK(ppf_init2_ctx(&x, 128, &ctx) == 0);
        CHECK(ppf_set_ui(&x, 777u, PPF_RNDN, &ctx) == 0);
        snapshot_take(&before, &x);
        disable_failure(&state);
        CHECK(ppf_set_str(&x, "12345junk", 10, PPF_RNDN, &ctx) == -1);
        snapshot_assert_equal(&before, &x);
        snapshot_clear(&before);
        ppf_clear(&x);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }

    /* ppf_strtofr still accepts a numeric prefix, but commits only after the
       parsed temporary is complete. */
    {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t x;
        char *end = NULL;
        setup_context(&ctx, &state);
        CHECK(ppf_init2_ctx(&x, 128, &ctx) == 0);
        CHECK(ppf_set_ui(&x, 777u, PPF_RNDN, &ctx) == 0);
        disable_failure(&state);
        CHECK(ppf_strtofr(&x, "12345junk", &end, 10,
                          PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
        CHECK(end != NULL && strcmp(end, "junk") == 0);
        CHECK(ppf_cmp_ui(&x, 12345u, &ctx) == 0);
        ppf_clear(&x);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }
}

static size_t inp_str_allocation_count(const char *input) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t x;
    FILE *stream;
    size_t calls;

    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&x, 512, &ctx) == 0);
    CHECK(ppf_set_ui(&x, 777u, PPF_RNDN, &ctx) == 0);
    stream = stream_from_text(input);
    disable_failure(&state);
    CHECK(ppf_inp_str(&x, stream, 10, PPF_RNDN, &ctx) > 0u);
    calls = state.allocation_calls;
    CHECK(calls >= 2u);
    fclose(stream);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    return calls;
}

static void test_inp_str_failures(void) {
    static const char input[] =
        "  123456789012345678901234567890123456789012345678901234567890"
        "123456789012345678901234567890";
    size_t calls = inp_str_allocation_count(input);

    for (size_t fail_at = 1u; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t ctx;
        ppf_t x;
        object_snapshot_t before;
        FILE *stream;
        setup_context(&ctx, &state);
        CHECK(ppf_init2_ctx(&x, 512, &ctx) == 0);
        CHECK(ppf_set_ui(&x, 777u, PPF_RNDN, &ctx) == 0);
        snapshot_take(&before, &x);
        stream = stream_from_text(input);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        arm_failure(&state, fail_at);
        CHECK(ppf_inp_str(&x, stream, 10, PPF_RNDN, &ctx) == 0u);
        CHECK(state.injected_failures == 1u);
        assert_alloc_flag(&ctx);
        snapshot_assert_equal(&before, &x);
        fclose(stream);

        disable_failure(&state);
        stream = stream_from_text(input);
        CHECK(ppf_inp_str(&x, stream, 10, PPF_RNDN, &ctx) > 0u);
        fclose(stream);
        snapshot_clear(&before);
        ppf_clear(&x);
        ppf_context_clear(&ctx);
        assert_clean(&state);
    }
}

static void test_fpif_export_staging(void) {
    fault_state_t state = {0};
    ppf_context_t ctx;
    ppf_t x;
    object_snapshot_t before;
    FILE *stream;

    setup_context(&ctx, &state);
    CHECK(ppf_init2_ctx(&x, 257, &ctx) == 0);
    CHECK(ppf_set_ui_2exp(&x, 0xabcdefu, 200,
                          PPF_RNDN, &ctx) == 0);
    snapshot_take(&before, &x);
    ppf_clear_flags();
    arm_failure(&state, 1u);
    stream = tmpfile();
    CHECK(stream != NULL);
    CHECK(ppf_fpif_export(stream, &x) == -1);
    CHECK(state.injected_failures == 1u);
    CHECK(stream_size(stream) == 0);
    CHECK(ppf_allocflag_p());
    snapshot_assert_equal(&before, &x);
    fclose(stream);

    disable_failure(&state);
    ppf_clear_flags();
    stream = tmpfile();
    CHECK(stream != NULL);
    CHECK(ppf_fpif_export(stream, &x) == 0);
    CHECK(stream_size(stream) > 0);
    fclose(stream);
    snapshot_clear(&before);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    assert_clean(&state);
    ppf_clear_flags();
}

static ppf_context_t *install_default_fault_allocator(fault_state_t *state) {
    ppf_context_t *ctx;
    ppf_allocator_t allocator = fault_allocator(state);
    ppf_default_context_cleanup();
    ctx = ppf_default_context();
    CHECK(ctx != NULL);
    CHECK(ctx->arena_head == NULL && ctx->arena_current == NULL);
    ctx->allocator = allocator;
    ctx->arena_block_bytes = 64u;
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
    return ctx;
}

static void release_default_fault_allocator(fault_state_t *state) {
    ppf_default_context_cleanup();
    assert_clean(state);
}

static int call_vasprintf(char **text, const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vasprintf(text, format, ap);
    va_end(ap);
    return rc;
}

static int call_vsnprintf(char *buffer, size_t size,
                          const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vsnprintf(buffer, size, format, ap);
    va_end(ap);
    return rc;
}

static int call_vsprintf(char *buffer, const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vsprintf(buffer, format, ap);
    va_end(ap);
    return rc;
}

static int call_vfprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vfprintf(stream, format, ap);
    va_end(ap);
    return rc;
}

static int call_vprintf(const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vprintf(format, ap);
    va_end(ap);
    return rc;
}

static size_t snprintf_allocation_count(const ppf_t *x, char *expected,
                                        size_t expected_size) {
    fault_state_t state = {0};
    ppf_context_t *ctx = install_default_fault_allocator(&state);
    int rc;
    disable_failure(&state);
    rc = ppf_snprintf(expected, expected_size, "%.80Rf", x);
    CHECK(rc > 0);
    CHECK(state.allocation_calls >= 2u);
    CHECK(state.live_count == 0u);
    (void)ctx;
    release_default_fault_allocator(&state);
    return state.allocation_calls;
}

static void test_formatting_failure_sweep(void) {
    ppf_context_t source_ctx;
    ppf_t x;
    char expected[256];
    size_t calls;

    CHECK(ppf_context_init(&source_ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 256, &source_ctx) == 0);
    CHECK(ppf_set_d(&x, 1.0 / 7.0, PPF_RNDN, &source_ctx) !=
          PPF_TERNARY_ERROR);
    calls = snprintf_allocation_count(&x, expected, sizeof(expected));

    for (size_t fail_at = 1u; fail_at <= calls; ++fail_at) {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        char buffer[256], before[256];
        int rc;
        memset(buffer, 0xa5, sizeof(buffer));
        memcpy(before, buffer, sizeof(buffer));
        arm_failure(&state, fail_at);
        rc = ppf_snprintf(buffer, sizeof(buffer), "%.80Rf", &x);
        CHECK(state.injected_failures == 1u);
        assert_alloc_flag(ctx);
        if (rc < 0) CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
        else CHECK(strcmp(buffer, expected) == 0);
        CHECK(state.live_count == 0u);
        release_default_fault_allocator(&state);
    }

    /* The public wrappers share the same staged implementation.  Inject the
       first allocation into each wrapper, including every va_list form. */
    {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        char *text = (char *)(uintptr_t)1u;
        arm_failure(&state, 1u);
        CHECK(ppf_asprintf(&text, "%.40Rf", &x) < 0);
        CHECK(text == NULL);
        assert_alloc_flag(ctx);
        release_default_fault_allocator(&state);
    }
    {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        char *text = (char *)(uintptr_t)1u;
        arm_failure(&state, 1u);
        CHECK(call_vasprintf(&text, "%.40Rf", &x) < 0);
        CHECK(text == NULL);
        assert_alloc_flag(ctx);
        release_default_fault_allocator(&state);
    }
    {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        char buffer[128], before[128];
        memset(buffer, 0x5a, sizeof(buffer));
        memcpy(before, buffer, sizeof(buffer));
        arm_failure(&state, 1u);
        CHECK(call_vsnprintf(buffer, sizeof(buffer), "%.40Rf", &x) < 0);
        CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
        assert_alloc_flag(ctx);
        release_default_fault_allocator(&state);
    }
    {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        char buffer[128], before[128];
        memset(buffer, 0x5a, sizeof(buffer));
        memcpy(before, buffer, sizeof(buffer));
        arm_failure(&state, 1u);
        CHECK(ppf_sprintf(buffer, "%.40Rf", &x) < 0);
        CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
        assert_alloc_flag(ctx);
        release_default_fault_allocator(&state);
    }
    {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        char buffer[128], before[128];
        memset(buffer, 0x5a, sizeof(buffer));
        memcpy(before, buffer, sizeof(buffer));
        arm_failure(&state, 1u);
        CHECK(call_vsprintf(buffer, "%.40Rf", &x) < 0);
        CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
        assert_alloc_flag(ctx);
        release_default_fault_allocator(&state);
    }
    {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        FILE *stream = tmpfile();
        CHECK(stream != NULL);
        arm_failure(&state, 1u);
        CHECK(ppf_fprintf(stream, "%.40Rf", &x) < 0);
        CHECK(stream_size(stream) == 0);
        assert_alloc_flag(ctx);
        fclose(stream);
        release_default_fault_allocator(&state);
    }
    {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        FILE *stream = tmpfile();
        CHECK(stream != NULL);
        arm_failure(&state, 1u);
        CHECK(call_vfprintf(stream, "%.40Rf", &x) < 0);
        CHECK(stream_size(stream) == 0);
        assert_alloc_flag(ctx);
        fclose(stream);
        release_default_fault_allocator(&state);
    }
    {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        FILE *stream = tmpfile();
        int saved_stdout;
        CHECK(stream != NULL);
        CHECK(fflush(stdout) == 0);
        saved_stdout = dup(fileno(stdout));
        CHECK(saved_stdout >= 0);
        CHECK(dup2(fileno(stream), fileno(stdout)) >= 0);
        arm_failure(&state, 1u);
        CHECK(ppf_printf("%.40Rf", &x) < 0);
        CHECK(fflush(stdout) == 0);
        CHECK(stream_size(stream) == 0);
        CHECK(dup2(saved_stdout, fileno(stdout)) >= 0);
        close(saved_stdout);
        assert_alloc_flag(ctx);
        fclose(stream);
        release_default_fault_allocator(&state);
    }
    {
        fault_state_t state = {0};
        ppf_context_t *ctx = install_default_fault_allocator(&state);
        FILE *stream = tmpfile();
        int saved_stdout;
        CHECK(stream != NULL);
        CHECK(fflush(stdout) == 0);
        saved_stdout = dup(fileno(stdout));
        CHECK(saved_stdout >= 0);
        CHECK(dup2(fileno(stream), fileno(stdout)) >= 0);
        arm_failure(&state, 1u);
        CHECK(call_vprintf("%.40Rf", &x) < 0);
        CHECK(fflush(stdout) == 0);
        CHECK(stream_size(stream) == 0);
        CHECK(dup2(saved_stdout, fileno(stdout)) >= 0);
        close(saved_stdout);
        assert_alloc_flag(ctx);
        fclose(stream);
        release_default_fault_allocator(&state);
    }

    ppf_clear(&x);
    ppf_context_clear(&source_ctx);
    ppf_default_context_cleanup();
}

int main(void) {
    test_get_str_allocator_provenance();
    test_get_str_and_out_str_failures();
    test_parsing_transactions();
    test_inp_str_failures();
    test_fpif_export_staging();
    test_formatting_failure_sweep();
    ppf_default_context_cleanup();
    puts("I/O allocation-failure and commit-boundary tests passed");
    return 0;
}
