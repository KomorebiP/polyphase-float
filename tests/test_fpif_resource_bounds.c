/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

#ifndef PPF_FPIF_MAX_IMPORT_BYTES
#define PPF_FPIF_MAX_IMPORT_BYTES ((size_t)64u * 1024u * 1024u)
#endif

typedef struct {
    size_t calls, max_request, fail_at, live_blocks, live_bytes;
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

static int reject_request(allocation_state *s, size_t bytes) {
    ++s->calls;
    if (bytes > s->max_request) s->max_request = bytes;
    /* The harness never executes a metadata-sized oversized allocation. */
    return bytes > 16384u || (s->fail_at && s->calls == s->fail_at);
}

static void *bounded_malloc(void *opaque, size_t bytes) {
    allocation_state *s = opaque;
    allocation_record *record;
    if (reject_request(s, bytes)) return NULL;
    record = malloc(offsetof(allocation_record, data) + bytes);
    CHECK(record != NULL);
    record->bytes = bytes;
    ++s->live_blocks; s->live_bytes += bytes;
    return record->data;
}

static void *bounded_realloc(void *opaque, void *ptr,
                             size_t old_bytes, size_t new_bytes) {
    allocation_state *s = opaque;
    allocation_record *record = ptr ? record_for(ptr) : NULL;
    CHECK(record ? record->bytes == old_bytes : old_bytes == 0);
    if (reject_request(s, new_bytes)) return NULL;
    record = realloc(record, offsetof(allocation_record, data) + new_bytes);
    CHECK(record != NULL);
    record->bytes = new_bytes;
    if (!ptr) ++s->live_blocks;
    s->live_bytes = s->live_bytes - old_bytes + new_bytes;
    return record->data;
}

static void bounded_free(void *opaque, void *ptr, size_t bytes) {
    allocation_state *s = opaque;
    allocation_record *record;
    if (!ptr) return;
    record = record_for(ptr);
    CHECK(record->bytes == bytes);
    CHECK(s->live_blocks > 0 && s->live_bytes >= bytes);
    --s->live_blocks; s->live_bytes -= bytes;
    free(record);
}

static void store_u64(unsigned char *out, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        out[7u - i] = (unsigned char)(value >> (8u * i));
}

static FILE *export_record(const ppf_t *value) {
    FILE *stream = tmpfile();
    CHECK(stream != NULL);
    CHECK(ppf_fpif_export(stream, value) == 0);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    return stream;
}

static void assert_unchanged(const ppf_t *value, const ppf_t *before,
                             const mp_limb_t *limbs) {
    CHECK(memcmp(value, before, sizeof(*value)) == 0);
    CHECK(memcmp(value->limbs, limbs,
                 value->allocated_limbs * sizeof(*limbs)) == 0);
}

static size_t test_valid_and_failure_record(ppf_kind_t kind, size_t fail_at) {
    allocation_state state = {0};
    ppf_allocator_t allocator = {bounded_malloc, bounded_realloc, bounded_free, &state};
    ppf_context_t ctx, source_ctx;
    ppf_t source, output, before;
    mp_limb_t limbs[2];
    FILE *stream;
    size_t calls;
    int result;
    CHECK(ppf_context_init(&source_ctx) == 0);
    CHECK(ppf_init2_ctx(&source, 65537u, &source_ctx) == 0);
    CHECK(ppf_set_si(&source, -7, PPF_RNDN, &source_ctx) == 0);
    if (kind == PPF_KIND_ZERO) ppf_set_zero(&source, -1);
    else if (kind == PPF_KIND_INF) ppf_set_inf(&source, -1);
    else if (kind == PPF_KIND_NAN) ppf_set_nan(&source, -1);
    stream = export_record(&source);
    CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
    CHECK(ppf_init2_ctx(&output, 128u, &ctx) == 0);
    CHECK(ppf_set_ui(&output, 11, PPF_RNDN, &ctx) == 0);
    before = output;
    memcpy(limbs, output.limbs, sizeof(limbs));
    state.calls = state.max_request = 0;
    state.fail_at = fail_at;
    result = ppf_fpif_import(&output, stream, &ctx);
    calls = state.calls;
    if (fail_at) {
        CHECK(result == -1);
        CHECK((ctx.flags & PPF_FLAG_ALLOC) != 0);
        assert_unchanged(&output, &before, limbs);
    } else {
        CHECK(result == 0);
        CHECK(ppf_validate(&output));
        CHECK(output.precision == source.precision);
        CHECK(output.kind == source.kind && output.sign == source.sign);
        CHECK(kind == PPF_KIND_NAN || ppf_equal_p(&output, &source));
        CHECK(ctx.flags == 0);
    }
    CHECK(state.max_request <= 16384u);
    state.fail_at = 0;
    ppf_clear(&output); ppf_context_clear(&ctx);
    CHECK(state.live_blocks == 0 && state.live_bytes == 0);
    ppf_clear(&source); ppf_context_clear(&source_ctx);
    fclose(stream);
    return calls;
}

static void test_metadata_admission(int include_bound_checks) {
    ppf_context_t source_ctx;
    ppf_t source;
    unsigned char header[36];
    FILE *encoded;
    CHECK(ppf_context_init(&source_ctx) == 0);
    CHECK(ppf_init2_ctx(&source, 128u, &source_ctx) == 0);
    CHECK(ppf_set_ui(&source, 7, PPF_RNDN, &source_ctx) == 0);
    encoded = export_record(&source);
    CHECK(fread(header, 1, sizeof(header), encoded) == sizeof(header));
    fclose(encoded);

    for (int kind = PPF_KIND_REGULAR; include_bound_checks && kind <= PPF_KIND_NAN; ++kind) {
        allocation_state state = {0};
        ppf_allocator_t allocator = {bounded_malloc, bounded_realloc, bounded_free, &state};
        ppf_context_t ctx;
        ppf_t output, before;
        mp_limb_t limbs[2];
        FILE *stream = tmpfile();
        uint64_t precision = (uint64_t)PPF_FPIF_MAX_IMPORT_BYTES * 8u + 64u;
        CHECK(stream != NULL);
        header[8] = (unsigned char)kind;
        store_u64(header + 12, precision);
        store_u64(header + 28, kind == PPF_KIND_REGULAR ? (precision + 7u) / 8u : 0u);
        CHECK(fwrite(header, 1, sizeof(header), stream) == sizeof(header));
        rewind(stream);
        CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
        CHECK(ppf_init2_ctx(&output, 128u, &ctx) == 0);
        CHECK(ppf_set_ui(&output, 11, PPF_RNDN, &ctx) == 0);
        before = output; memcpy(limbs, output.limbs, sizeof(limbs));
        state.calls = state.max_request = 0;
        CHECK(ppf_fpif_import(&output, stream, &ctx) == -1);
        CHECK(ctx.flags == PPF_FLAG_ERANGE);
        CHECK(state.calls == 0 && state.max_request == 0);
        assert_unchanged(&output, &before, limbs);
        ppf_clear(&output); ppf_context_clear(&ctx); fclose(stream);
        CHECK(state.live_blocks == 0 && state.live_bytes == 0);
    }

    {
        allocation_state state = {0};
        ppf_allocator_t allocator = {bounded_malloc, bounded_realloc, bounded_free, &state};
        ppf_context_t ctx;
        ppf_t output, before;
        mp_limb_t limbs[2];
        FILE *stream = tmpfile();
        CHECK(stream != NULL);
        header[8] = PPF_KIND_REGULAR;
        store_u64(header + 12, 4194304u);
        store_u64(header + 28, 524288u);
        CHECK(fwrite(header, 1, sizeof(header), stream) == sizeof(header));
        rewind(stream);
        CHECK(ppf_context_init_custom(&ctx, &allocator) == 0);
        CHECK(ppf_init2_ctx(&output, 128u, &ctx) == 0);
        CHECK(ppf_set_ui(&output, 11, PPF_RNDN, &ctx) == 0);
        before = output; memcpy(limbs, output.limbs, sizeof(limbs));
        state.calls = state.max_request = 0;
        CHECK(ppf_fpif_import(&output, stream, &ctx) == -1);
        CHECK((ctx.flags & PPF_FLAG_ALLOC) == 0);
        CHECK(state.calls == 1 && state.max_request <= 4096u);
        assert_unchanged(&output, &before, limbs);
        ppf_clear(&output); ppf_context_clear(&ctx); fclose(stream);
        CHECK(state.live_blocks == 0 && state.live_bytes == 0);
    }
    ppf_clear(&source); ppf_context_clear(&source_ctx);
}

int main(int argc, char **argv) {
    if (argc == 2) {
        if (strcmp(argv[1], "stream") == 0) test_metadata_admission(0);
        else if (strcmp(argv[1], "metadata") == 0) test_metadata_admission(1);
        else return 2;
        return 0;
    }
    test_metadata_admission(1);
    for (int kind = PPF_KIND_REGULAR; kind <= PPF_KIND_NAN; ++kind) {
        size_t calls = test_valid_and_failure_record((ppf_kind_t)kind, 0);
        for (size_t failure = 1; failure <= calls; ++failure)
            (void)test_valid_and_failure_record((ppf_kind_t)kind, failure);
        printf("FPIF category %d: %zu allocation failure positions passed\n", kind, calls);
    }
    ppf_thread_cleanup();
    puts("FPIF resource admission and streaming regressions passed");
    return 0;
}
