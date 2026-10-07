/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <math.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISOLATION_THREADS 8
#define EXIT_THREADS 24
#define CACHE_WORKERS 4
#define CACHE_ITERATIONS 10
#define CLEAN_ITERATIONS 48

static unsigned stress_multiplier(void) {
    const char *text = getenv("PPF_THREAD_STRESS_MULTIPLIER");
    char *end = NULL;
    unsigned long value;
    if (!text || !*text) return 1U;
    value = strtoul(text, &end, 10);
    if (!end || *end != '\0' || value == 0UL || value > 32UL) return 1U;
    return (unsigned)value;
}

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    unsigned parties;
    unsigned waiting;
    unsigned generation;
} test_barrier_t;

static int barrier_init(test_barrier_t *barrier, unsigned parties) {
    if (!barrier || parties == 0U) return -1;
    memset(barrier, 0, sizeof(*barrier));
    barrier->parties = parties;
    if (pthread_mutex_init(&barrier->mutex, NULL) != 0) return -1;
    if (pthread_cond_init(&barrier->condition, NULL) != 0) {
        (void)pthread_mutex_destroy(&barrier->mutex);
        return -1;
    }
    return 0;
}

static void barrier_destroy(test_barrier_t *barrier) {
    if (!barrier) return;
    (void)pthread_cond_destroy(&barrier->condition);
    (void)pthread_mutex_destroy(&barrier->mutex);
}

static int barrier_wait(test_barrier_t *barrier) {
    unsigned generation;
    if (!barrier || pthread_mutex_lock(&barrier->mutex) != 0) return -1;
    generation = barrier->generation;
    barrier->waiting += 1U;
    if (barrier->waiting == barrier->parties) {
        barrier->waiting = 0U;
        barrier->generation += 1U;
        (void)pthread_cond_broadcast(&barrier->condition);
    } else {
        while (generation == barrier->generation) {
            if (pthread_cond_wait(&barrier->condition,
                                  &barrier->mutex) != 0) {
                (void)pthread_mutex_unlock(&barrier->mutex);
                return -1;
            }
        }
    }
    return pthread_mutex_unlock(&barrier->mutex);
}

typedef struct {
    test_barrier_t barrier;
    atomic_int failed;
    const ppf_context_t *addresses[ISOLATION_THREADS];
} isolation_state_t;

typedef struct {
    isolation_state_t *state;
    unsigned index;
} isolation_arg_t;

static void record_failure(atomic_int *failed, const char *message,
                           unsigned index) {
    if (atomic_exchange_explicit(failed, 1, memory_order_relaxed) == 0)
        fprintf(stderr, "thread-safety failure[%u]: %s\n", index, message);
}

static void *isolation_worker(void *opaque) {
    static const ppf_flags_t flags[ISOLATION_THREADS] = {
        PPF_FLAG_UNDERFLOW, PPF_FLAG_OVERFLOW, PPF_FLAG_NAN,
        PPF_FLAG_INEXACT, PPF_FLAG_ERANGE, PPF_FLAG_DIVBY0,
        PPF_FLAG_ALLOC, PPF_FLAG_INEXACT | PPF_FLAG_ERANGE
    };
    isolation_arg_t *arg = (isolation_arg_t *)opaque;
    isolation_state_t *state = arg->state;
    const unsigned index = arg->index;
    const ppf_prec_t precision = (ppf_prec_t)(80U + index * 11U);
    const ppf_exp_t emin = (ppf_exp_t)(-1000 - (int)index);
    const ppf_exp_t emax = (ppf_exp_t)(1000 + (int)index);
    const ppf_rnd_t rounding = (ppf_rnd_t)(index % 6U);
    ppf_context_t *ctx = ppf_default_context();
    ppf_t value;
    unsigned char *arena;

    memset(&value, 0, sizeof(value));
    if (!ctx) {
        record_failure(&state->failed, "default context unavailable", index);
        return NULL;
    }
    state->addresses[index] = ctx;
    if (ppf_context_set_default_precision(ctx, precision) != 0 ||
        ppf_context_set_range(ctx, emin, emax) != 0 ||
        ppf_context_set_rounding(ctx, rounding) != 0) {
        record_failure(&state->failed, "context configuration failed", index);
        return NULL;
    }
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
    ppf_context_flags_set(ctx, flags[index]);
    arena = (unsigned char *)ppf_arena_alloc(
        ctx, (size_t)(65536U + index * 257U), alignof(max_align_t));
    if (!arena) {
        record_failure(&state->failed, "arena allocation failed", index);
        return NULL;
    }
    memset(arena, (int)(index + 1U), 4096U);
    if (barrier_wait(&state->barrier) != 0) {
        record_failure(&state->failed, "barrier wait failed", index);
        return NULL;
    }
    if (ctx != state->addresses[index] ||
        ctx->default_precision != precision || ctx->emin != emin ||
        ctx->emax != emax || ctx->default_rounding != rounding ||
        ppf_context_flags(ctx) != flags[index]) {
        record_failure(&state->failed, "thread-local context was contaminated",
                       index);
        return NULL;
    }
    for (unsigned other = 0U; other < ISOLATION_THREADS; ++other) {
        if (other != index && state->addresses[other] == ctx) {
            record_failure(&state->failed, "two threads shared one context",
                           index);
            return NULL;
        }
    }
    if (ppf_init(&value) != 0 || ppf_get_prec(&value) != precision ||
        ppf_set_ui(&value, index + 3UL, PPF_RNDN, ctx) ==
            PPF_TERNARY_ERROR) {
        record_failure(&state->failed, "default-context arithmetic failed",
                       index);
        if (value.limbs) ppf_clear(&value);
        return NULL;
    }
    ppf_clear(&value);

    if ((index & 1U) == 0U) {
        ppf_context_t *replacement;
        ppf_thread_cleanup();
        replacement = ppf_default_context();
        if (!replacement || replacement != ctx ||
            replacement->default_precision != PPF_PREC_DEFAULT ||
            replacement->emin != PPF_EMIN_DEFAULT ||
            replacement->emax != PPF_EMAX_DEFAULT ||
            replacement->default_rounding != PPF_RNDN ||
            ppf_context_flags(replacement) != 0U) {
            record_failure(&state->failed,
                           "explicit thread cleanup did not reset state", index);
        }
        ppf_thread_cleanup();
    }
    /* Odd-indexed threads intentionally omit cleanup.  The pthread-key
       destructor must release their arena and planner state at thread exit. */
    return NULL;
}

static int run_direct_default_context_clear(void) {
    ppf_context_t *ctx = ppf_default_context();
    ppf_context_t *replacement;
    if (!ctx || ppf_context_set_default_precision(ctx, 173U) != 0)
        return -1;
    ppf_context_clear(ctx);
    replacement = ppf_default_context();
    if (!replacement || replacement != ctx ||
        replacement->default_precision != PPF_PREC_DEFAULT ||
        replacement->emin != PPF_EMIN_DEFAULT ||
        replacement->emax != PPF_EMAX_DEFAULT ||
        replacement->default_rounding != PPF_RNDN ||
        ppf_context_flags(replacement) != 0U)
        return -1;
    ppf_thread_cleanup();
    return 0;
}

static int run_context_isolation(void) {
    isolation_state_t state;
    isolation_arg_t args[ISOLATION_THREADS];
    pthread_t threads[ISOLATION_THREADS];
    memset(&state, 0, sizeof(state));
    atomic_init(&state.failed, 0);
    if (barrier_init(&state.barrier, ISOLATION_THREADS) != 0) return -1;
    for (unsigned i = 0U; i < ISOLATION_THREADS; ++i) {
        args[i].state = &state;
        args[i].index = i;
        if (pthread_create(&threads[i], NULL, isolation_worker, &args[i]) != 0) {
            record_failure(&state.failed, "pthread_create failed", i);
            for (unsigned j = 0U; j < i; ++j) (void)pthread_join(threads[j], NULL);
            barrier_destroy(&state.barrier);
            return -1;
        }
    }
    for (unsigned i = 0U; i < ISOLATION_THREADS; ++i)
        (void)pthread_join(threads[i], NULL);
    barrier_destroy(&state.barrier);
    return atomic_load_explicit(&state.failed, memory_order_relaxed) ? -1 : 0;
}

typedef union allocation_header {
    max_align_t alignment;
    struct {
        size_t bytes;
    } metadata;
} allocation_header_t;

typedef struct {
    atomic_long live_blocks;
    atomic_long live_bytes;
    atomic_int failed;
} allocation_tracker_t;

static void tracker_mark_failed(allocation_tracker_t *tracker) {
    atomic_store_explicit(&tracker->failed, 1, memory_order_relaxed);
}

static void *tracked_malloc(void *opaque, size_t size) {
    allocation_tracker_t *tracker = (allocation_tracker_t *)opaque;
    allocation_header_t *header;
    if (!tracker || size > SIZE_MAX - sizeof(*header)) return NULL;
    header = (allocation_header_t *)malloc(sizeof(*header) + size);
    if (!header) return NULL;
    header->metadata.bytes = size;
    atomic_fetch_add_explicit(&tracker->live_blocks, 1,
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&tracker->live_bytes, (long)size,
                              memory_order_relaxed);
    return (void *)(header + 1);
}

static void *tracked_realloc(void *opaque, void *ptr,
                             size_t old_size, size_t new_size) {
    allocation_tracker_t *tracker = (allocation_tracker_t *)opaque;
    allocation_header_t *header;
    allocation_header_t *replacement;
    size_t recorded;
    if (!ptr) return tracked_malloc(opaque, new_size);
    if (!tracker || new_size > SIZE_MAX - sizeof(*header)) return NULL;
    header = ((allocation_header_t *)ptr) - 1;
    recorded = header->metadata.bytes;
    if (recorded != old_size) tracker_mark_failed(tracker);
    replacement = (allocation_header_t *)realloc(
        header, sizeof(*replacement) + new_size);
    if (!replacement) return NULL;
    replacement->metadata.bytes = new_size;
    atomic_fetch_add_explicit(&tracker->live_bytes,
                              (long)new_size - (long)recorded,
                              memory_order_relaxed);
    return (void *)(replacement + 1);
}

static void tracked_free(void *opaque, void *ptr, size_t size) {
    allocation_tracker_t *tracker = (allocation_tracker_t *)opaque;
    allocation_header_t *header;
    size_t recorded;
    if (!ptr) return;
    if (!tracker) {
        free(((allocation_header_t *)ptr) - 1);
        return;
    }
    header = ((allocation_header_t *)ptr) - 1;
    recorded = header->metadata.bytes;
    if (recorded != size) tracker_mark_failed(tracker);
    atomic_fetch_sub_explicit(&tracker->live_blocks, 1,
                              memory_order_relaxed);
    atomic_fetch_sub_explicit(&tracker->live_bytes, (long)recorded,
                              memory_order_relaxed);
    free(header);
}

typedef struct {
    atomic_int *failed;
    allocation_tracker_t *tracker;
    unsigned index;
} exit_arg_t;

static void *exit_cleanup_worker(void *opaque) {
    exit_arg_t *arg = (exit_arg_t *)opaque;
    ppf_t a, b, product;
    ppf_context_t *ctx;
    void *arena;
    int rc = PPF_TERNARY_ERROR;
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    memset(&product, 0, sizeof(product));
    ctx = ppf_default_context();
    if (!ctx) goto failed;
    /* A fresh default context has not allocated memory yet.  Install a
       counting allocator before its first arena or object allocation so the
       test can prove that the automatic destructor releases all
       context-owned blocks, rather than relying only on LeakSanitizer. */
    ctx->allocator.malloc_fn = tracked_malloc;
    ctx->allocator.realloc_fn = tracked_realloc;
    ctx->allocator.free_fn = tracked_free;
    ctx->allocator.opaque = arg->tracker;
    arena = ppf_arena_alloc(ctx, 262144U + arg->index * 1024U, 64U);
    if (!arena) goto failed;
    memset(arena, (int)(arg->index + 1U), 4096U);
    if (ppf_init2(&a, 4096U) != 0 || ppf_init2(&b, 4096U) != 0 ||
        ppf_init2(&product, 4096U) != 0)
        goto failed;
    if (ppf_set_ui_2exp(&a, 3UL, 4090, PPF_RNDN, ctx) ==
            PPF_TERNARY_ERROR ||
        ppf_set_ui_2exp(&b, 5UL, 4088, PPF_RNDN, ctx) ==
            PPF_TERNARY_ERROR)
        goto failed;
    rc = ppf_mul_algorithm_threads(&product, &a, &b, PPF_RNDN, 1UL,
                                   PPF_PRODUCT_TOOM3, ctx);
    if (rc == PPF_TERNARY_ERROR || !ppf_number_p(&product)) goto failed;
    ppf_clear(&product); ppf_clear(&b); ppf_clear(&a);
    /* Deliberately omit ppf_thread_cleanup.  LeakSanitizer verifies that the
       automatic thread-exit destructor frees the arena and Toom workspace. */
    return NULL;

failed:
    if (product.limbs) ppf_clear(&product);
    if (b.limbs) ppf_clear(&b);
    if (a.limbs) ppf_clear(&a);
    record_failure(arg->failed, "thread-exit cleanup setup failed", arg->index);
    return NULL;
}

static void *explicit_toom_exit_worker(void *opaque) {
    exit_arg_t *arg = (exit_arg_t *)opaque;
    ppf_context_t ctx;
    ppf_t a, b, product;
    int rc = PPF_TERNARY_ERROR;
    int ctx_initialized = 0;
    memset(&ctx, 0, sizeof(ctx));
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memset(&product, 0, sizeof(product));
    if (ppf_context_init(&ctx) != 0)
        goto failed;
    ctx_initialized = 1;
    if (ppf_init2_ctx(&a, 4096U, &ctx) != 0 ||
        ppf_init2_ctx(&b, 4096U, &ctx) != 0 ||
        ppf_init2_ctx(&product, 4096U, &ctx) != 0)
        goto failed;
    if (ppf_set_ui_2exp(&a, 7UL, 4085, PPF_RNDN, &ctx) ==
            PPF_TERNARY_ERROR ||
        ppf_set_ui_2exp(&b, 11UL, 4082, PPF_RNDN, &ctx) ==
            PPF_TERNARY_ERROR)
        goto failed;
    rc = ppf_mul_algorithm_threads(&product, &a, &b, PPF_RNDN, 1UL,
                                   PPF_PRODUCT_TOOM3, &ctx);
    if (rc == PPF_TERNARY_ERROR || !ppf_number_p(&product)) goto failed;
    ppf_clear(&product);
    ppf_clear(&b);
    ppf_clear(&a);
    /* Explicit contexts remain application-owned and must be cleared by the
       thread that owns them.  The clear operation also releases this thread's
       planner scratch, while the separate default-context tests exercise the
       automatic pthread-key destructor. */
    ppf_context_clear(&ctx);
    return NULL;

failed:
    if (product.limbs) ppf_clear(&product);
    if (b.limbs) ppf_clear(&b);
    if (a.limbs) ppf_clear(&a);
    if (ctx_initialized) ppf_context_clear(&ctx);
    record_failure(arg->failed,
                   "explicit-context Toom thread-exit setup failed",
                   arg->index);
    return NULL;
}

static int run_explicit_context_toom_cleanup(void) {
    pthread_t threads[EXIT_THREADS];
    exit_arg_t args[EXIT_THREADS];
    atomic_int failed;
    atomic_init(&failed, 0);
    for (unsigned i = 0U; i < EXIT_THREADS; ++i) {
        args[i].failed = &failed;
        args[i].tracker = NULL;
        args[i].index = i;
        if (pthread_create(&threads[i], NULL, explicit_toom_exit_worker,
                           &args[i]) != 0) {
            record_failure(&failed, "pthread_create failed", i);
            for (unsigned j = 0U; j < i; ++j)
                (void)pthread_join(threads[j], NULL);
            return -1;
        }
    }
    for (unsigned i = 0U; i < EXIT_THREADS; ++i)
        (void)pthread_join(threads[i], NULL);
    return atomic_load_explicit(&failed, memory_order_relaxed) ? -1 : 0;
}

static int run_thread_exit_cleanup(void) {
    pthread_t threads[EXIT_THREADS];
    exit_arg_t args[EXIT_THREADS];
    atomic_int failed;
    allocation_tracker_t tracker;
    atomic_init(&failed, 0);
    atomic_init(&tracker.live_blocks, 0);
    atomic_init(&tracker.live_bytes, 0);
    atomic_init(&tracker.failed, 0);
    for (unsigned i = 0U; i < EXIT_THREADS; ++i) {
        args[i].failed = &failed;
        args[i].tracker = &tracker;
        args[i].index = i;
        if (pthread_create(&threads[i], NULL, exit_cleanup_worker, &args[i]) != 0) {
            record_failure(&failed, "pthread_create failed", i);
            for (unsigned j = 0U; j < i; ++j) (void)pthread_join(threads[j], NULL);
            return -1;
        }
    }
    for (unsigned i = 0U; i < EXIT_THREADS; ++i)
        (void)pthread_join(threads[i], NULL);
    if (atomic_load_explicit(&tracker.failed, memory_order_relaxed) != 0 ||
        atomic_load_explicit(&tracker.live_blocks,
                             memory_order_relaxed) != 0 ||
        atomic_load_explicit(&tracker.live_bytes,
                             memory_order_relaxed) != 0) {
        record_failure(&failed,
                       "automatic thread cleanup left context allocations live",
                       0U);
    }
    return atomic_load_explicit(&failed, memory_order_relaxed) ? -1 : 0;
}

typedef struct {
    test_barrier_t barrier;
    atomic_int failed;
} cache_state_t;

typedef struct {
    cache_state_t *state;
    unsigned index;
} cache_arg_t;

static int close_double(double actual, double expected, double tolerance) {
    return isfinite(actual) && fabs(actual - expected) <= tolerance;
}

static void *cache_worker(void *opaque) {
    cache_arg_t *arg = (cache_arg_t *)opaque;
    cache_state_t *state = arg->state;
    ppf_context_t *ctx = ppf_default_context();
    ppf_t x, y, z;
    memset(&x, 0, sizeof(x)); memset(&y, 0, sizeof(y));
    memset(&z, 0, sizeof(z));
    if (!ctx || ppf_init2(&x, 96U) != 0 || ppf_init2(&y, 96U) != 0 ||
        ppf_init2(&z, 96U) != 0) {
        record_failure(&state->failed, "cache worker initialization failed",
                       arg->index);
        goto done;
    }
    if (barrier_wait(&state->barrier) != 0) {
        record_failure(&state->failed, "cache worker barrier failed", arg->index);
        goto done;
    }
    for (unsigned iteration = 0U; iteration < CACHE_ITERATIONS; ++iteration) {
        const double input = 1.125 + 0.03125 * (double)(arg->index + 1U) +
                             0.00390625 * (double)iteration;
        int rc;
        rc = ppf_const_pi(&y, PPF_RNDN, ctx);
        if (rc == PPF_TERNARY_ERROR ||
            !close_double(ppf_get_d(&y, PPF_RNDN, ctx),
                          3.14159265358979323846, 2.0e-15)) {
            record_failure(&state->failed, "concurrent pi evaluation failed",
                           arg->index);
            break;
        }
        rc = ppf_const_log2(&z, PPF_RNDN, ctx);
        if (rc == PPF_TERNARY_ERROR ||
            !close_double(ppf_get_d(&z, PPF_RNDN, ctx),
                          0.69314718055994530942, 2.0e-15)) {
            record_failure(&state->failed, "concurrent log2 evaluation failed",
                           arg->index);
            break;
        }
        if (ppf_set_d(&x, input, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_log(&y, &x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            !close_double(ppf_get_d(&y, PPF_RNDN, ctx), log(input), 5.0e-14) ||
            ppf_log10(&z, &x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            !close_double(ppf_get_d(&z, PPF_RNDN, ctx), log10(input),
                          5.0e-14)) {
            record_failure(&state->failed,
                           "concurrent transcendental cache evaluation failed",
                           arg->index);
            break;
        }
        (void)ppf_constant_cache_precision(PPF_CONSTANT_EULER);
        (void)ppf_constant_cache_precision(PPF_CONSTANT_CATALAN);
        if ((iteration & 1U) == 0U)
            ppf_free_cache2(PPF_FREE_LOCAL_CACHE);
    }

done:
    if (z.limbs) ppf_clear(&z);
    if (y.limbs) ppf_clear(&y);
    if (x.limbs) ppf_clear(&x);
    /* Exercise automatic destruction on every cache worker as well. */
    return NULL;
}

static void *cache_cleaner(void *opaque) {
    cache_state_t *state = (cache_state_t *)opaque;
    if (barrier_wait(&state->barrier) != 0) {
        record_failure(&state->failed, "cache cleaner barrier failed", 0U);
        return NULL;
    }
    for (unsigned iteration = 0U; iteration < CLEAN_ITERATIONS; ++iteration) {
        ppf_set_constant_threads(1UL + (unsigned long)(iteration % 3U));
        ppf_free_cache2(PPF_FREE_GLOBAL_CACHE);
        ppf_clear_constant_cache((iteration & 1U) != 0U
                                 ? PPF_CONSTANT_EULER
                                 : PPF_CONSTANT_CATALAN);
        (void)ppf_constant_cache_precision(PPF_CONSTANT_PI);
        (void)ppf_constant_cache_precision(PPF_CONSTANT_LOG2);
    }
    return NULL;
}

static int verify_post_cleanup_constants(void) {
    ppf_context_t ctx;
    ppf_t value;
    int rc = -1;
    memset(&ctx, 0, sizeof(ctx)); memset(&value, 0, sizeof(value));
    if (ppf_context_init(&ctx) != 0 || ppf_init2_ctx(&value, 80U, &ctx) != 0)
        goto done;
    if (ppf_const_euler(&value, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        !close_double(ppf_get_d(&value, PPF_RNDN, &ctx),
                      0.57721566490153286061, 2.0e-15))
        goto done;
    if (ppf_const_catalan(&value, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        !close_double(ppf_get_d(&value, PPF_RNDN, &ctx),
                      0.91596559417721901505, 2.0e-15))
        goto done;
    rc = 0;

done:
    if (value.limbs) ppf_clear(&value);
    ppf_context_clear(&ctx);
    ppf_free_cache2(PPF_FREE_GLOBAL_CACHE);
    return rc;
}

static int run_concurrent_cache_cleanup(void) {
    cache_state_t state;
    cache_arg_t args[CACHE_WORKERS];
    pthread_t workers[CACHE_WORKERS];
    pthread_t cleaner;
    memset(&state, 0, sizeof(state));
    atomic_init(&state.failed, 0);
    if (barrier_init(&state.barrier, CACHE_WORKERS + 1U) != 0) return -1;
    for (unsigned i = 0U; i < CACHE_WORKERS; ++i) {
        args[i].state = &state;
        args[i].index = i;
        if (pthread_create(&workers[i], NULL, cache_worker, &args[i]) != 0) {
            record_failure(&state.failed, "cache pthread_create failed", i);
            barrier_destroy(&state.barrier);
            return -1;
        }
    }
    if (pthread_create(&cleaner, NULL, cache_cleaner, &state) != 0) {
        record_failure(&state.failed, "cleaner pthread_create failed", 0U);
        for (unsigned i = 0U; i < CACHE_WORKERS; ++i)
            (void)pthread_join(workers[i], NULL);
        barrier_destroy(&state.barrier);
        return -1;
    }
    for (unsigned i = 0U; i < CACHE_WORKERS; ++i)
        (void)pthread_join(workers[i], NULL);
    (void)pthread_join(cleaner, NULL);
    ppf_set_constant_threads(1UL);
    barrier_destroy(&state.barrier);
    if (atomic_load_explicit(&state.failed, memory_order_relaxed)) return -1;
    return verify_post_cleanup_constants();
}

#define SHARED_THREADS 6
#define SHARED_ITERATIONS 64

typedef struct {
    const ppf_t *a;
    const ppf_t *b;
    const ppf_t *product;
    const ppf_t *numerator;
    const ppf_t *quotient;
    const ppf_divisor_plan_t *plan;
    atomic_int failed;
} shared_state_t;

typedef struct {
    shared_state_t *state;
    unsigned index;
} shared_arg_t;

static void *shared_read_worker(void *opaque) {
    shared_arg_t *arg = (shared_arg_t *)opaque;
    shared_state_t *state = arg->state;
    ppf_context_t ctx;
    ppf_t out;
    memset(&ctx, 0, sizeof(ctx));
    memset(&out, 0, sizeof(out));
    if (ppf_context_init(&ctx) != 0 || ppf_init2_ctx(&out, 256U, &ctx) != 0) {
        record_failure(&state->failed, "shared-read worker setup failed",
                       arg->index);
        goto done;
    }
    for (unsigned iteration = 0U; iteration < SHARED_ITERATIONS; ++iteration) {
        if (ppf_mul(&out, state->a, state->b, PPF_RNDN, &ctx) ==
                PPF_TERNARY_ERROR ||
            ppf_cmp(&out, state->product, &ctx) != 0) {
            record_failure(&state->failed,
                           "shared read-only multiplication failed",
                           arg->index);
            break;
        }
        if (ppf_div_with_plan(&out, state->numerator, state->plan,
                              PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
            ppf_cmp(&out, state->quotient, &ctx) != 0) {
            record_failure(&state->failed,
                           "shared divisor-plan application failed",
                           arg->index);
            break;
        }
    }

done:
    if (out.limbs) ppf_clear(&out);
    ppf_context_clear(&ctx);
    return NULL;
}

static int run_shared_read_only_state(void) {
    ppf_context_t ctx;
    ppf_t a, b, product, numerator, denominator, quotient;
    ppf_divisor_plan_t *plan = NULL;
    shared_state_t state;
    shared_arg_t args[SHARED_THREADS];
    pthread_t threads[SHARED_THREADS];
    unsigned created = 0U;
    int rc = -1;
    memset(&ctx, 0, sizeof(ctx));
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    memset(&product, 0, sizeof(product));
    memset(&numerator, 0, sizeof(numerator));
    memset(&denominator, 0, sizeof(denominator));
    memset(&quotient, 0, sizeof(quotient));
    memset(&state, 0, sizeof(state));
    atomic_init(&state.failed, 0);
    if (ppf_context_init(&ctx) != 0 ||
        ppf_init2_ctx(&a, 256U, &ctx) != 0 ||
        ppf_init2_ctx(&b, 256U, &ctx) != 0 ||
        ppf_init2_ctx(&product, 256U, &ctx) != 0 ||
        ppf_init2_ctx(&numerator, 256U, &ctx) != 0 ||
        ppf_init2_ctx(&denominator, 256U, &ctx) != 0 ||
        ppf_init2_ctx(&quotient, 256U, &ctx) != 0)
        goto done;
    if (ppf_set_ui(&a, 1234567UL, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&b, 7654321UL, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&numerator, 123456789UL, PPF_RNDN, &ctx) ==
            PPF_TERNARY_ERROR ||
        ppf_set_ui(&denominator, 37UL, PPF_RNDN, &ctx) ==
            PPF_TERNARY_ERROR ||
        ppf_mul(&product, &a, &b, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_div(&quotient, &numerator, &denominator, PPF_RNDN, &ctx) ==
            PPF_TERNARY_ERROR ||
        ppf_divisor_plan_create_ex(&plan, &denominator, 256U,
                                   SHARED_THREADS * SHARED_ITERATIONS,
                                   &ctx) != 0)
        goto done;
    state.a = &a;
    state.b = &b;
    state.product = &product;
    state.numerator = &numerator;
    state.quotient = &quotient;
    state.plan = plan;
    for (unsigned i = 0U; i < SHARED_THREADS; ++i) {
        args[i].state = &state;
        args[i].index = i;
        if (pthread_create(&threads[i], NULL, shared_read_worker, &args[i]) != 0) {
            record_failure(&state.failed, "shared-read pthread_create failed", i);
            break;
        }
        created += 1U;
    }
    for (unsigned i = 0U; i < created; ++i)
        (void)pthread_join(threads[i], NULL);
    if (created != SHARED_THREADS ||
        atomic_load_explicit(&state.failed, memory_order_relaxed) != 0)
        goto done;
    if (ppf_divisor_plan_applications(plan) !=
        (unsigned long)(SHARED_THREADS * SHARED_ITERATIONS)) {
        fprintf(stderr, "shared divisor-plan application count mismatch\n");
        goto done;
    }
    rc = 0;

done:
    ppf_divisor_plan_destroy(plan);
    if (quotient.limbs) ppf_clear(&quotient);
    if (denominator.limbs) ppf_clear(&denominator);
    if (numerator.limbs) ppf_clear(&numerator);
    if (product.limbs) ppf_clear(&product);
    if (b.limbs) ppf_clear(&b);
    if (a.limbs) ppf_clear(&a);
    ppf_context_clear(&ctx);
    return rc;
}

static int run_arena_alignment(void) {
    static const size_t alignments[] = {1U, 2U, 4U, 8U, 16U, 32U, 64U, 128U};
    ppf_context_t ctx;
    ppf_arena_mark_t empty;
    memset(&ctx, 0, sizeof(ctx));
    if (ppf_context_init(&ctx) != 0) return -1;
    ctx.arena_block_bytes = 64U;
    empty = ppf_arena_mark(&ctx);
    for (size_t i = 0U; i < sizeof(alignments) / sizeof(alignments[0]); ++i) {
        void *ptr = ppf_arena_alloc(&ctx, 33U + i, alignments[i]);
        if (!ptr || ((uintptr_t)ptr & (alignments[i] - 1U)) != 0U) {
            fprintf(stderr, "arena alignment failure for %zu bytes\n",
                    alignments[i]);
            ppf_context_clear(&ctx);
            return -1;
        }
        memset(ptr, (int)(0x40U + i), 33U + i);
    }
    ppf_arena_release(&ctx, empty);
    if (ctx.arena_current == NULL || ctx.arena_head == NULL) {
        fprintf(stderr, "arena empty-mark reuse was not preserved\n");
        ppf_context_clear(&ctx);
        return -1;
    }
    ppf_context_clear(&ctx);
    return 0;
}

int main(void) {
    if (!ppf_buildopt_tls_p() || !ppf_buildopt_thread_cleanup_p()) {
        fprintf(stderr, "automatic thread cleanup is unavailable\n");
        return EXIT_FAILURE;
    }
    if (run_arena_alignment() != 0) return EXIT_FAILURE;
    if (run_direct_default_context_clear() != 0) return EXIT_FAILURE;
    for (unsigned cycle = 0U; cycle < stress_multiplier(); ++cycle) {
        if (run_context_isolation() != 0) return EXIT_FAILURE;
        if (run_thread_exit_cleanup() != 0) return EXIT_FAILURE;
        if (run_explicit_context_toom_cleanup() != 0) return EXIT_FAILURE;
        if (run_shared_read_only_state() != 0) return EXIT_FAILURE;
        if (run_concurrent_cache_cleanup() != 0) return EXIT_FAILURE;
    }
    ppf_thread_cleanup();
    puts("thread safety and cache lifecycle tests passed");
    return EXIT_SUCCESS;
}
