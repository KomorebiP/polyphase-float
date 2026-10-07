/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

#ifndef PPF_ARENA_DEFAULT_BYTES
#define PPF_ARENA_DEFAULT_BYTES ((size_t)64 * 1024)
#endif

typedef struct ppf_arena_block {
    struct ppf_arena_block *next;
    void *allocation;
    size_t allocation_bytes;
    size_t capacity;
    size_t used;
    max_align_t alignment_guard;
    unsigned char data[];
} ppf_arena_block_t;

_Static_assert((alignof(ppf_arena_block_t) &
                (alignof(ppf_arena_block_t) - 1U)) == 0U,
               "arena block alignment must be a power of two");

static _Thread_local uint64_t tls_allocation_failure_epoch;

uint64_t ppf_allocation_failure_epoch_internal(void) {
    return tls_allocation_failure_epoch;
}

void ppf_note_allocation_failure_internal(void) {
    ++tls_allocation_failure_epoch;
}

static void *default_malloc(void *opaque, size_t size) {
    (void)opaque;
    return malloc(size);
}

static void *default_realloc(void *opaque, void *ptr,
                             size_t old_size, size_t new_size) {
    (void)opaque;
    (void)old_size;
    return realloc(ptr, new_size);
}

static void default_free(void *opaque, void *ptr, size_t size) {
    (void)opaque;
    (void)size;
    free(ptr);
}

ppf_allocator_t ppf_default_allocator(void) {
    ppf_allocator_t a;
    a.malloc_fn = default_malloc;
    a.realloc_fn = default_realloc;
    a.free_fn = default_free;
    a.opaque = NULL;
    return a;
}

void *ppf_context_malloc_internal(ppf_context_t *ctx, size_t bytes) {
    void *out;
    ctx = ppf_resolve_context(ctx);
    if (!ctx || bytes == 0) return NULL;
    out = ctx->allocator.malloc_fn(ctx->allocator.opaque, bytes);
    if (!out) {
        ppf_note_allocation_failure_internal();
        ppf_raise(ctx, PPF_FLAG_ALLOC);
    }
    return out;
}

void *ppf_context_calloc_internal(ppf_context_t *ctx, size_t count,
                                  size_t bytes) {
    size_t total;
    void *out;
    ctx = ppf_resolve_context(ctx);
    if (!ctx || count == 0 || bytes == 0) return NULL;
    if (count > SIZE_MAX / bytes) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return NULL;
    }
    total = count * bytes;
    out = ppf_context_malloc_internal(ctx, total);
    if (out) memset(out, 0, total);
    return out;
}

void *ppf_context_realloc_internal(ppf_context_t *ctx, void *ptr,
                                   size_t old_bytes, size_t new_bytes) {
    void *out;
    ctx = ppf_resolve_context(ctx);
    if (!ctx || new_bytes == 0) return NULL;
    out = ctx->allocator.realloc_fn(ctx->allocator.opaque, ptr,
                                    old_bytes, new_bytes);
    if (!out) {
        ppf_note_allocation_failure_internal();
        ppf_raise(ctx, PPF_FLAG_ALLOC);
    }
    return out;
}

void ppf_context_free_internal(ppf_context_t *ctx, void *ptr, size_t bytes) {
    ctx = ppf_resolve_context(ctx);
    if (!ctx || !ptr) return;
    ctx->allocator.free_fn(ctx->allocator.opaque, ptr, bytes);
}

#define PPF_OWNED_STRING_MAGIC UINT64_C(0x5050465354523031)

typedef struct {
    uint64_t magic;
    size_t allocation_bytes;
    ppf_allocator_t allocator;
    max_align_t alignment_guard;
    unsigned char data[];
} ppf_owned_string_header_t;

char *ppf_owned_string_alloc_internal(ppf_context_t *ctx, size_t bytes) {
    ppf_owned_string_header_t *header;
    size_t prefix = offsetof(ppf_owned_string_header_t, data);
    size_t total;

    ctx = ppf_resolve_context(ctx);
    if (!ctx || bytes == 0) return NULL;
    if (bytes > SIZE_MAX - prefix) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return NULL;
    }
    total = prefix + bytes;
    header = (ppf_owned_string_header_t *)
        ppf_context_malloc_internal(ctx, total);
    if (!header) return NULL;
    header->magic = PPF_OWNED_STRING_MAGIC;
    header->allocation_bytes = total;
    header->allocator = ctx->allocator;
    return (char *)header->data;
}

void ppf_owned_string_free_internal(char *text) {
    ppf_owned_string_header_t *header;
    ppf_allocator_t allocator;
    size_t total;

    if (!text) return;
    header = (ppf_owned_string_header_t *)
        ((unsigned char *)text - offsetof(ppf_owned_string_header_t, data));
    if (header->magic != PPF_OWNED_STRING_MAGIC) {
        /* ppf_free_str is defined only for storage returned by ppf_get_str.
           Avoid routing an unrecognized pointer through an unknown allocator. */
        return;
    }
    allocator = header->allocator;
    total = header->allocation_bytes;
    header->magic = 0;
    allocator.free_fn(allocator.opaque, header, total);
}


#define PPF_OWNED_BUFFER_MAGIC UINT64_C(0x5050464255463031)

typedef struct {
    uint64_t magic;
    size_t allocation_bytes;
    size_t payload_bytes;
    ppf_allocator_t allocator;
    max_align_t alignment_guard;
    unsigned char data[];
} ppf_owned_buffer_header_t;

static ppf_owned_buffer_header_t *owned_buffer_header(void *ptr) {
    ppf_owned_buffer_header_t *header;
    if (!ptr) return NULL;
    header = (ppf_owned_buffer_header_t *)
        ((unsigned char *)ptr - offsetof(ppf_owned_buffer_header_t, data));
    return header->magic == PPF_OWNED_BUFFER_MAGIC ? header : NULL;
}

void *ppf_owned_buffer_alloc_internal(ppf_context_t *ctx, size_t bytes) {
    ppf_owned_buffer_header_t *header;
    size_t prefix = offsetof(ppf_owned_buffer_header_t, data);
    size_t total;

    ctx = ppf_resolve_context(ctx);
    if (!ctx || bytes == 0) return NULL;
    if (bytes > SIZE_MAX - prefix) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return NULL;
    }
    total = prefix + bytes;
    header = (ppf_owned_buffer_header_t *)
        ppf_context_malloc_internal(ctx, total);
    if (!header) return NULL;
    header->magic = PPF_OWNED_BUFFER_MAGIC;
    header->allocation_bytes = total;
    header->payload_bytes = bytes;
    header->allocator = ctx->allocator;
    return header->data;
}

void *ppf_owned_buffer_calloc_internal(ppf_context_t *ctx, size_t count,
                                       size_t bytes) {
    size_t total;
    void *out;
    ctx = ppf_resolve_context(ctx);
    if (!ctx || count == 0 || bytes == 0) return NULL;
    if (count > SIZE_MAX / bytes) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return NULL;
    }
    total = count * bytes;
    out = ppf_owned_buffer_alloc_internal(ctx, total);
    if (out) memset(out, 0, total);
    return out;
}

void *ppf_owned_buffer_realloc_internal(ppf_context_t *ctx, void *ptr,
                                        size_t bytes) {
    ppf_owned_buffer_header_t *header;
    ppf_owned_buffer_header_t *next;
    ppf_allocator_t allocator;
    size_t prefix = offsetof(ppf_owned_buffer_header_t, data);
    size_t total;

    ctx = ppf_resolve_context(ctx);
    if (!ptr) return ppf_owned_buffer_alloc_internal(ctx, bytes);
    if (!ctx || bytes == 0) return NULL;
    header = owned_buffer_header(ptr);
    if (!header) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return NULL;
    }
    if (bytes > SIZE_MAX - prefix) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return NULL;
    }
    total = prefix + bytes;
    allocator = header->allocator;
    next = (ppf_owned_buffer_header_t *)allocator.realloc_fn(
        allocator.opaque, header, header->allocation_bytes, total);
    if (!next) {
        ppf_note_allocation_failure_internal();
        ppf_raise(ctx, PPF_FLAG_ALLOC);
        return NULL;
    }
    next->magic = PPF_OWNED_BUFFER_MAGIC;
    next->allocation_bytes = total;
    next->payload_bytes = bytes;
    next->allocator = allocator;
    return next->data;
}

void ppf_owned_buffer_free_internal(void *ptr) {
    ppf_owned_buffer_header_t *header;
    ppf_allocator_t allocator;
    size_t total;

    if (!ptr) return;
    header = owned_buffer_header(ptr);
    if (!header) return;
    allocator = header->allocator;
    total = header->allocation_bytes;
    header->magic = 0;
    allocator.free_fn(allocator.opaque, header, total);
}

static int allocator_valid(const ppf_allocator_t *a) {
    return a && a->malloc_fn && a->realloc_fn && a->free_fn;
}

int ppf_context_init_custom(ppf_context_t *ctx,
                            const ppf_allocator_t *allocator) {
    if (!ctx || !allocator_valid(allocator)) return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->default_precision = PPF_PREC_DEFAULT;
    ctx->emin = PPF_EMIN_DEFAULT;
    ctx->emax = PPF_EMAX_DEFAULT;
    ctx->default_rounding = PPF_RNDN;
    ctx->allocator = *allocator;
    ctx->arena_block_bytes = PPF_ARENA_DEFAULT_BYTES;
    return 0;
}

int ppf_context_init(ppf_context_t *ctx) {
    ppf_allocator_t a = ppf_default_allocator();
    return ppf_context_init_custom(ctx, &a);
}

int ppf_context_clone(ppf_context_t *dst, const ppf_context_t *src) {
    if (!dst || !src) return -1;
    if (ppf_context_init_custom(dst, &src->allocator) != 0) return -1;
    dst->default_precision = src->default_precision;
    dst->emin = src->emin;
    dst->emax = src->emax;
    dst->default_rounding = src->default_rounding;
    dst->flags = src->flags;
    dst->arena_block_bytes = src->arena_block_bytes;
    return 0;
}

static void free_blocks_after(ppf_context_t *ctx, ppf_arena_block_t *keep) {
    ppf_arena_block_t *block;
    if (!ctx) return;
    block = keep ? keep->next : (ppf_arena_block_t *)ctx->arena_head;
    while (block) {
        ppf_arena_block_t *next = block->next;
        void *allocation = block->allocation;
        size_t allocation_bytes = block->allocation_bytes;
        ctx->allocator.free_fn(ctx->allocator.opaque, allocation,
                               allocation_bytes);
        block = next;
    }
    if (keep) keep->next = NULL;
    else ctx->arena_head = NULL;
    ctx->arena_current = keep;
}


static _Thread_local ppf_context_t tls_context;
static _Thread_local int tls_context_initialized;
static _Thread_local int tls_cleanup_registered;

void ppf_context_clear(ppf_context_t *ctx) {
    int was_default;
    if (!ctx) return;
    was_default = ctx == &tls_context && tls_context_initialized;
    free_blocks_after(ctx, NULL);
    /* Product-planner workspaces are thread-local and independent of a
       particular numerical context.  Releasing them here keeps explicit
       contexts and sanitizer runs leak-free without affecting correctness. */
    ppf_toom3_limb_cleanup_thread();
    memset(ctx, 0, sizeof(*ctx));
    if (was_default) tls_context_initialized = 0;
}

static pthread_key_t g_thread_cleanup_key;
static pthread_once_t g_thread_cleanup_once = PTHREAD_ONCE_INIT;
static atomic_int g_thread_cleanup_key_ready;
static unsigned char g_thread_cleanup_marker;

static void ppf_thread_cleanup_internal(int clear_key);

static void ppf_thread_cleanup_destructor(void *value) {
    if (!value) return;
    /* POSIX clears the key value before invoking the destructor.  Do not set
       it again here, or the implementation may invoke the destructor in a
       second pass. */
    ppf_thread_cleanup_internal(0);
}

static void ppf_thread_cleanup_key_initialize(void) {
    int ready = pthread_key_create(&g_thread_cleanup_key,
                                   ppf_thread_cleanup_destructor) == 0;
    atomic_store_explicit(&g_thread_cleanup_key_ready, ready,
                          memory_order_release);
}

void ppf_thread_state_touch(void) {
    if (tls_cleanup_registered) return;
    (void)pthread_once(&g_thread_cleanup_once,
                       ppf_thread_cleanup_key_initialize);
    if (atomic_load_explicit(&g_thread_cleanup_key_ready,
                             memory_order_acquire) != 0 &&
        pthread_setspecific(g_thread_cleanup_key,
                            &g_thread_cleanup_marker) == 0) {
        tls_cleanup_registered = 1;
    }
}

void ppf_context_release_pool_internal(ppf_context_t *ctx) {
    if (!ctx) {
        if (tls_context_initialized)
            free_blocks_after(&tls_context, NULL);
        ppf_toom3_limb_cleanup_thread();
        return;
    }
    free_blocks_after(ctx, NULL);
    ppf_toom3_limb_cleanup_thread();
}

static void ppf_thread_cleanup_internal(int clear_key) {
    if (tls_context_initialized) {
        ppf_context_clear(&tls_context);
        tls_context_initialized = 0;
    } else {
        /* Direct-limb product workspaces can be used with explicit contexts,
           so they require cleanup even when the default context was never
           initialized in this thread. */
        ppf_toom3_limb_cleanup_thread();
    }
    if (clear_key && tls_cleanup_registered &&
        atomic_load_explicit(&g_thread_cleanup_key_ready,
                             memory_order_acquire) != 0) {
        (void)pthread_setspecific(g_thread_cleanup_key, NULL);
    }
    tls_cleanup_registered = 0;
}

ppf_context_t *ppf_default_context(void) {
    ppf_thread_state_touch();
    if (!tls_context_initialized) {
        if (ppf_context_init(&tls_context) != 0) return NULL;
        tls_context_initialized = 1;
    }
    return &tls_context;
}

void ppf_thread_cleanup(void) {
    ppf_thread_cleanup_internal(1);
}

void ppf_default_context_cleanup(void) {
    ppf_thread_cleanup();
}

int ppf_buildopt_thread_cleanup_p(void) {
    (void)pthread_once(&g_thread_cleanup_once,
                       ppf_thread_cleanup_key_initialize);
    return atomic_load_explicit(&g_thread_cleanup_key_ready,
                                memory_order_acquire) != 0;
}

ppf_context_t *ppf_resolve_context(ppf_context_t *ctx) {
    return ctx ? ctx : ppf_default_context();
}

int ppf_context_set_default_precision(ppf_context_t *ctx,
                                      ppf_prec_t precision) {
    ctx = ppf_resolve_context(ctx);
    if (!ctx || !ppf_precision_valid(precision)) return -1;
    ctx->default_precision = precision;
    return 0;
}

int ppf_context_set_range(ppf_context_t *ctx,
                          ppf_exp_t emin, ppf_exp_t emax) {
    ctx = ppf_resolve_context(ctx);
    if (!ctx || emin < PPF_EMIN_MIN || emax > PPF_EMAX_MAX || emin > emax)
        return -1;
    ctx->emin = emin;
    ctx->emax = emax;
    return 0;
}

int ppf_context_set_rounding(ppf_context_t *ctx, ppf_rnd_t rnd) {
    ctx = ppf_resolve_context(ctx);
    if (!ctx || !ppf_rounding_valid(rnd)) return -1;
    ctx->default_rounding = rnd;
    return 0;
}

ppf_flags_t ppf_context_flags(const ppf_context_t *ctx) {
    if (!ctx) ctx = ppf_default_context();
    return ctx ? (ctx->flags & PPF_FLAGS_ALL) : 0;
}

void ppf_context_flags_clear(ppf_context_t *ctx, ppf_flags_t mask) {
    ctx = ppf_resolve_context(ctx);
    if (ctx) ctx->flags &= ~(mask & PPF_FLAGS_ALL);
}

void ppf_context_flags_set(ppf_context_t *ctx, ppf_flags_t mask) {
    ctx = ppf_resolve_context(ctx);
    if (ctx) ctx->flags |= mask & PPF_FLAGS_ALL;
}

ppf_flags_t ppf_context_flags_save(const ppf_context_t *ctx) {
    return ppf_context_flags(ctx);
}

void ppf_context_flags_restore(ppf_context_t *ctx,
                               ppf_flags_t flags,
                               ppf_flags_t mask) {
    ctx = ppf_resolve_context(ctx);
    if (!ctx) return;
    mask &= PPF_FLAGS_ALL;
    ctx->flags = (ctx->flags & ~mask) | (flags & mask);
}

void ppf_raise(ppf_context_t *ctx, ppf_flags_t flags) {
    ppf_context_flags_set(ctx, flags);
}

static int power_of_two(size_t x) {
    return x && ((x & (x - 1)) == 0);
}

void *ppf_arena_alloc(ppf_context_t *ctx, size_t bytes, size_t alignment) {
    ppf_arena_block_t *block;
    size_t aligned, required, capacity, total;
    ctx = ppf_resolve_context(ctx);
    if (!ctx || bytes == 0) return NULL;
    if (alignment == 0) alignment = alignof(max_align_t);
    if (!power_of_two(alignment)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return NULL;
    }

    block = (ppf_arena_block_t *)ctx->arena_current;
    if (block) {
        uintptr_t base = (uintptr_t)block->data;
        uintptr_t current = base + block->used;
        uintptr_t address = (current + alignment - 1) & ~(uintptr_t)(alignment - 1);
        aligned = (size_t)(address - base);
        if (aligned <= block->capacity && bytes <= block->capacity - aligned) {
            void *out = (void *)address;
            block->used = aligned + bytes;
            return out;
        }
    }

    capacity = ctx->arena_block_bytes ? ctx->arena_block_bytes
                                      : PPF_ARENA_DEFAULT_BYTES;
    if (bytes > SIZE_MAX - (alignment - 1U)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return NULL;
    }
    required = bytes + alignment - 1U;
    if (capacity < required) capacity = required;
    {
        const size_t block_alignment = alignof(ppf_arena_block_t);
        const size_t alignment_slack = block_alignment - 1U;
        void *allocation;
        uintptr_t raw_address;
        uintptr_t block_address;
        if (capacity > SIZE_MAX - sizeof(ppf_arena_block_t) ||
            sizeof(ppf_arena_block_t) + capacity >
                SIZE_MAX - alignment_slack) {
            ppf_raise(ctx, PPF_FLAG_ERANGE);
            return NULL;
        }
        total = sizeof(ppf_arena_block_t) + capacity + alignment_slack;
        allocation = ppf_context_malloc_internal(ctx, total);
        if (!allocation) return NULL;
        raw_address = (uintptr_t)allocation;
        if (raw_address > UINTPTR_MAX - alignment_slack) {
            ctx->allocator.free_fn(ctx->allocator.opaque, allocation, total);
            ppf_raise(ctx, PPF_FLAG_ERANGE);
            return NULL;
        }
        block_address = (raw_address + alignment_slack) &
                        ~(uintptr_t)alignment_slack;
        block = (ppf_arena_block_t *)block_address;
        block->allocation = allocation;
        block->allocation_bytes = total;
    }
    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;

    if (ctx->arena_current)
        ((ppf_arena_block_t *)ctx->arena_current)->next = block;
    else
        ctx->arena_head = block;
    ctx->arena_current = block;

    {
        uintptr_t base = (uintptr_t)block->data;
        uintptr_t address = (base + alignment - 1) & ~(uintptr_t)(alignment - 1);
        aligned = (size_t)(address - base);
        block->used = aligned + bytes;
        return (void *)address;
    }
}

ppf_arena_mark_t ppf_arena_mark(const ppf_context_t *ctx) {
    ppf_arena_mark_t mark;
    ppf_arena_block_t *block;
    if (!ctx) ctx = ppf_default_context();
    block = ctx ? (ppf_arena_block_t *)ctx->arena_current : NULL;
    mark.block = block;
    mark.used = block ? block->used : 0;
    return mark;
}

void ppf_arena_release(ppf_context_t *ctx, ppf_arena_mark_t mark) {
    ppf_arena_block_t *block, *scan;
    int found = 0;
    ctx = ppf_resolve_context(ctx);
    if (!ctx) return;
    block = (ppf_arena_block_t *)mark.block;
    if (!block) {
        /* A mark taken before the first temporary allocation denotes an
           empty arena.  Keep the first block for reuse instead of turning
           every arithmetic call into a malloc/free pair. */
        ppf_arena_block_t *head = (ppf_arena_block_t *)ctx->arena_head;
        if (!head) return;
        free_blocks_after(ctx, head);
        head->used = 0;
        return;
    }
    for (scan = (ppf_arena_block_t *)ctx->arena_head; scan; scan = scan->next) {
        if (scan == block) { found = 1; break; }
    }
    if (!found || mark.used > block->capacity) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return;
    }
    free_blocks_after(ctx, block);
    block->used = mark.used;
}

void ppf_arena_reset(ppf_context_t *ctx) {
    ppf_arena_block_t *head;
    ctx = ppf_resolve_context(ctx);
    if (!ctx) return;
    head = (ppf_arena_block_t *)ctx->arena_head;
    if (!head) return;
    free_blocks_after(ctx, head);
    head->used = 0;
}
