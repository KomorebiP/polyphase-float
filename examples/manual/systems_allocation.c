/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Core-only example: one controlled allocation failure and recovery. */
#include <polyphase_float.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int fail_next;
    size_t live_blocks;
} allocation_state;

static void *tracked_malloc(void *opaque, size_t bytes) {
    allocation_state *state = opaque;
    void *block;
    if (state->fail_next) { state->fail_next = 0; return NULL; }
    block = malloc(bytes);
    if (block) ++state->live_blocks;
    return block;
}

static void *tracked_realloc(void *opaque, void *block,
                             size_t old_bytes, size_t new_bytes) {
    allocation_state *state = opaque;
    int was_null = block == NULL;
    void *replacement;
    (void)old_bytes;
    if (state->fail_next) { state->fail_next = 0; return NULL; }
    replacement = realloc(block, new_bytes);
    if (replacement && was_null) ++state->live_blocks;
    return replacement;
}

static void tracked_free(void *opaque, void *block, size_t bytes) {
    allocation_state *state = opaque;
    (void)bytes;
    if (block) { --state->live_blocks; free(block); }
}

int main(void) {
    allocation_state state = {0, 0};
    ppf_allocator_t allocator = {
        tracked_malloc, tracked_realloc, tracked_free, &state
    };
    ppf_context_t ctx;
    ppf_t value = {0};
    int status = EXIT_FAILURE;

    if (ppf_context_init_custom(&ctx, &allocator) != 0) return status;
    if (ppf_init2_ctx(&value, 80, &ctx) != 0 ||
        ppf_set_ui(&value, 37, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        goto cleanup;

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    state.fail_next = 1;
    if (ppf_set_str(&value, "1.25", 10, PPF_RNDN, &ctx) == 0 ||
        state.fail_next != 0 || ppf_cmp_ui(&value, 37, &ctx) != 0 ||
        !(ppf_context_flags(&ctx) & PPF_FLAG_ALLOC)) goto cleanup;

    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    if (ppf_set_str(&value, "1.25", 10, PPF_RNDN, &ctx) != 0 ||
        ppf_cmp_d(&value, 1.25, &ctx) != 0 ||
        (ppf_context_flags(&ctx) & PPF_FLAG_ALLOC)) goto cleanup;
    status = EXIT_SUCCESS;

cleanup:
    ppf_clear(&value);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    if (state.live_blocks != 0) status = EXIT_FAILURE;
    if (status == EXIT_SUCCESS) {
        if (puts("allocation failure preserved 37; retry produced 1.25") == EOF)
            status = EXIT_FAILURE;
    } else {
        fputs("allocation example failed\n", stderr);
    }
    return status;
}
