/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "constant_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define PPF_CONSTANT_MAX_ATTEMPTS 16UL
#define PPF_CONSTANT_GROWTH_EXTRA 64UL

static atomic_ulong g_constant_workers = 1UL;
static _Thread_local ppf_constant_stats_t g_last_constant_stats;

static int constant_error(ppf_t *rop, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, 1);
    ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

void ppf_constant_interval_init(ppf_constant_interval_t *x) {
    if (!x) return;
    mpz_init(x->lower);
    mpz_init(x->upper);
    x->exponent = 0;
}

void ppf_constant_interval_clear(ppf_constant_interval_t *x) {
    if (!x) return;
    mpz_clear(x->lower);
    mpz_clear(x->upper);
    x->exponent = 0;
}

void ppf_constant_interval_set(ppf_constant_interval_t *dst,
                               const ppf_constant_interval_t *src) {
    mpz_set(dst->lower, src->lower);
    mpz_set(dst->upper, src->upper);
    dst->exponent = src->exponent;
}

void ppf_constant_interval_swap(ppf_constant_interval_t *a,
                                ppf_constant_interval_t *b) {
    ppf_exp_t exponent;
    if (!a || !b || a == b) return;
    mpz_swap(a->lower, b->lower);
    mpz_swap(a->upper, b->upper);
    exponent = a->exponent;
    a->exponent = b->exponent;
    b->exponent = exponent;
}

ppf_prec_t ppf_constant_interval_precision(
    const ppf_constant_interval_t *x) {
    mpz_t width;
    size_t width_bits;
    uint64_t negative_exponent;
    if (!x || mpz_cmp(x->lower, x->upper) > 0) return 0;
    mpz_init(width);
    mpz_sub(width, x->upper, x->lower);
    if (mpz_sgn(width) == 0) {
        mpz_clear(width);
        return UINT64_MAX;
    }
    width_bits = mpz_sizeinbase(width, 2);
    mpz_clear(width);
    if (x->exponent >= 0) return 1;
    negative_exponent = (uint64_t)(-(x->exponent + 1)) + 1u;
    if (negative_exponent <= width_bits) return 1;
    return (ppf_prec_t)(negative_exponent - width_bits);
}

void ppf_constant_cache_init(ppf_constant_cache_t *cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
    pthread_mutex_init(&cache->mutex, NULL);
    ppf_constant_interval_init(&cache->interval);
    cache->initialized = 1;
}

void ppf_constant_cache_reset(ppf_constant_cache_t *cache) {
    if (!cache || !cache->initialized) return;
    pthread_mutex_lock(&cache->mutex);
    mpz_set_ui(cache->interval.lower, 0UL);
    mpz_set_ui(cache->interval.upper, 0UL);
    cache->interval.exponent = 0;
    cache->certified_precision = 0;
    cache->valid = 0;
    for (unsigned int i = 0U; i < 6U; ++i) {
        cache->rounded_valid[i] = 0U;
        cache->rounded_precision[i] = 0;
        cache->rounded_ternary[i] = 0;
    }
    pthread_mutex_unlock(&cache->mutex);
}

void ppf_constant_cache_purge(ppf_constant_cache_t *cache) {
    if (!cache || !cache->initialized) return;
    pthread_mutex_lock(&cache->mutex);
    mpz_set_ui(cache->interval.lower, 0UL);
    mpz_set_ui(cache->interval.upper, 0UL);
    /* Keep the mpz objects valid while releasing oversized cached storage. */
    mpz_realloc2(cache->interval.lower, 1UL);
    mpz_realloc2(cache->interval.upper, 1UL);
    cache->interval.exponent = 0;
    cache->certified_precision = 0;
    cache->valid = 0;
    for (unsigned int i = 0U; i < 6U; ++i) {
        if (cache->rounded_initialized[i]) {
            ppf_clear(&cache->rounded[i]);
            cache->rounded_initialized[i] = 0U;
        }
        cache->rounded_valid[i] = 0U;
        cache->rounded_precision[i] = 0;
        cache->rounded_ternary[i] = 0;
    }
    pthread_mutex_unlock(&cache->mutex);
}

unsigned long ppf_constant_resolve_workers(unsigned long workers) {
#ifndef _OPENMP
    (void)workers;
    return 1UL;
#else
    if (workers == 0UL) workers = atomic_load(&g_constant_workers);
    if (workers == 0UL) workers = 1UL;
    if (workers > (unsigned long)INT_MAX) workers = (unsigned long)INT_MAX;
    return workers;
#endif
}

double ppf_constant_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
}

void ppf_set_last_constant_stats(const ppf_constant_stats_t *stats) {
    if (stats) g_last_constant_stats = *stats;
    else memset(&g_last_constant_stats, 0, sizeof(g_last_constant_stats));
}

static ppf_prec_t grow_precision(ppf_prec_t p) {
    ppf_prec_t extra = p / 10u + PPF_CONSTANT_GROWTH_EXTRA;
    if (p > UINT64_MAX - extra) return p;
    return p + extra;
}

static int allocation_failed_since(uint64_t epoch) {
    return ppf_allocation_failure_epoch_internal() != epoch;
}

static int interval_is_better(const ppf_constant_cache_t *cache,
                              const ppf_constant_interval_t *candidate) {
    ppf_prec_t p = ppf_constant_interval_precision(candidate);
    return !cache->valid || p > cache->certified_precision;
}

static int rounding_slot(ppf_rnd_t rnd) {
    switch (rnd) {
        case PPF_RNDN: return 0;
        case PPF_RNDZ: return 1;
        case PPF_RNDU: return 2;
        case PPF_RNDD: return 3;
        case PPF_RNDA: return 4;
        case PPF_RNDF: return 5;
        default: return -1;
    }
}

static int normal_constant_range(const ppf_constant_spec_t *spec,
                                 const ppf_context_t *ctx) {
    return ctx->emin <= spec->required_emin &&
           ctx->emax >= spec->required_emax;
}

static int store_rounded_result(ppf_constant_cache_t *cache, int slot,
                                const ppf_t *value, int ternary) {
    ppf_context_t cache_ctx;
    ppf_context_t wide;
    ppf_t candidate;
    int candidate_initialized = 0;
    int rc = -1;

    if (!cache || !value || slot < 0 || slot >= 6) return -1;
    memset(&candidate, 0, sizeof(candidate));
    if (ppf_context_init(&cache_ctx) != 0) return -1;
    if (ppf_context_clone(&wide, &cache_ctx) != 0) {
        ppf_context_clear(&cache_ctx);
        return -1;
    }
    wide.emin = PPF_EMIN_MIN;
    wide.emax = PPF_EMAX_MAX;
    wide.flags = 0;
    if (ppf_init2_ctx(&candidate, value->precision, &cache_ctx) != 0)
        goto done;
    candidate_initialized = 1;
    if (ppf_set(&candidate, value, PPF_RNDN, &wide) == PPF_TERNARY_ERROR)
        goto done;

    if (cache->rounded_initialized[slot]) {
        ppf_swap(&cache->rounded[slot], &candidate);
    } else {
        cache->rounded[slot] = candidate;
        memset(&candidate, 0, sizeof(candidate));
        candidate_initialized = 0;
        cache->rounded_initialized[slot] = 1U;
    }
    cache->rounded_precision[slot] = value->precision;
    cache->rounded_ternary[slot] = ternary;
    cache->rounded_valid[slot] = 1U;
    rc = 0;

done:
    if (candidate_initialized) ppf_clear(&candidate);
    ppf_context_clear(&wide);
    ppf_context_clear(&cache_ctx);
    return rc;
}

int ppf_evaluate_constant_spec(const ppf_constant_spec_t *spec,
                               ppf_t *rop, ppf_rnd_t rnd,
                               unsigned long workers, int use_cache,
                               ppf_context_t *ctx) {
    ppf_constant_interval_t interval;
    ppf_constant_stats_t stats;
    ppf_interval_status_t status;
    ppf_t staged;
    ppf_prec_t working;
    int staged_initialized = 0;
    int interval_initialized = 0;
    int alloc_was_set = 0;
    int ternary = 0;
    int decided = 0;
    int slot;
    uint64_t allocation_epoch;
    double started;

    if (!spec || !rop || !spec->cache || !spec->build_interval)
        return constant_error(rop, ctx);
    ctx = ppf_resolve_context(ctx);
    if (!ctx) return constant_error(rop, ctx);
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!ppf_rounding_valid(rnd)) return constant_error(rop, ctx);
    alloc_was_set = (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0;
    if (alloc_was_set) ppf_context_flags_clear(ctx, PPF_FLAG_ALLOC);
    allocation_epoch = ppf_allocation_failure_epoch_internal();

    memset(&staged, 0, sizeof(staged));
    if (ppf_init2_ctx(&staged, rop->precision, ctx) != 0) {
        int rc = ppf_allocation_error(rop, ctx);
        if (alloc_was_set) ppf_context_flags_set(ctx, PPF_FLAG_ALLOC);
        return rc;
    }
    staged_initialized = 1;

    workers = ppf_constant_resolve_workers(workers);
    slot = rounding_slot(rnd);
    memset(&stats, 0, sizeof(stats));
    stats.constant_id = spec->id;
    stats.requested_precision = rop->precision;
    stats.workers = workers;
    started = ppf_constant_now();

    pthread_mutex_lock(&spec->cache->mutex);
    if (use_cache && slot >= 0 && normal_constant_range(spec, ctx) &&
        spec->cache->rounded_valid[slot] &&
        spec->cache->rounded_precision[slot] == rop->precision) {
        if (ppf_set(&staged, &spec->cache->rounded[slot],
                    PPF_RNDN, ctx) != PPF_TERNARY_ERROR &&
            (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) == 0 &&
            !allocation_failed_since(allocation_epoch)) {
            ternary = spec->cache->rounded_ternary[slot];
            decided = 1;
            stats.cache_hit = 1;
            stats.cached_precision = spec->cache->certified_precision;
            stats.computed_precision = spec->cache->certified_precision;
            stats.interval_exponent = spec->cache->interval.exponent;
        }
    }
    if (!decided && (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) == 0 &&
        !allocation_failed_since(allocation_epoch) &&
        use_cache && spec->cache->valid) {
        status = ppf_set_dyadic_interval(
            &staged, spec->cache->interval.lower,
            spec->cache->interval.upper,
            spec->cache->interval.exponent,
            rnd, &ternary, ctx);
        if (status == PPF_INTERVAL_DECIDED &&
            (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) == 0 &&
            !allocation_failed_since(allocation_epoch)) {
            decided = 1;
            stats.cache_hit = 1;
            stats.cached_precision = spec->cache->certified_precision;
            stats.computed_precision = spec->cache->certified_precision;
            stats.interval_exponent = spec->cache->interval.exponent;
            if (slot >= 0 && normal_constant_range(spec, ctx))
                (void)store_rounded_result(spec->cache, slot, &staged,
                                           ternary);
        }
    }

    ppf_constant_interval_init(&interval);
    interval_initialized = 1;
    working = rop->precision;
    if (!decided && use_cache && spec->cache->valid &&
        spec->cache->certified_precision > working) {
        working = spec->cache->certified_precision;
    }

    for (unsigned long attempt = 1UL;
         !decided && (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) == 0 &&
         !allocation_failed_since(allocation_epoch) &&
         attempt <= PPF_CONSTANT_MAX_ATTEMPTS; ++attempt) {
        ppf_constant_stats_t build_stats;
        memset(&build_stats, 0, sizeof(build_stats));
        build_stats.constant_id = spec->id;
        build_stats.requested_precision = rop->precision;
        build_stats.workers = workers;
        build_stats.attempts = attempt;
        if (spec->build_interval(&interval, working, workers,
                                 &build_stats, ctx) != 0 ||
            (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0 ||
            allocation_failed_since(allocation_epoch)) {
            break;
        }
        stats = build_stats;
        stats.constant_id = spec->id;
        stats.requested_precision = rop->precision;
        stats.workers = workers;
        stats.attempts = attempt;
        status = ppf_set_dyadic_interval(
            &staged, interval.lower, interval.upper, interval.exponent,
            rnd, &ternary, ctx);
        if (status == PPF_INTERVAL_DECIDED &&
            (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) == 0 &&
            !allocation_failed_since(allocation_epoch)) {
            decided = 1;
            if (use_cache && interval_is_better(spec->cache, &interval)) {
                ppf_constant_interval_swap(&spec->cache->interval, &interval);
                spec->cache->certified_precision =
                    ppf_constant_interval_precision(&spec->cache->interval);
                spec->cache->valid = 1;
            }
            if (use_cache && spec->cache->valid)
                stats.cached_precision = spec->cache->certified_precision;
            if (use_cache && slot >= 0 && normal_constant_range(spec, ctx))
                (void)store_rounded_result(spec->cache, slot, &staged,
                                           ternary);
            break;
        }
        if ((ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0 ||
            allocation_failed_since(allocation_epoch)) break;
        {
            ppf_prec_t next = grow_precision(working);
            if (next <= working) break;
            working = next;
        }
    }

    if (interval_initialized) ppf_constant_interval_clear(&interval);
    pthread_mutex_unlock(&spec->cache->mutex);

    if (!decided) {
        int allocation_failed =
            (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0 ||
            allocation_failed_since(allocation_epoch);
        if (allocation_failed)
            ppf_raise(ctx, PPF_FLAG_ALLOC);
        stats.total_seconds = ppf_constant_now() - started;
        ppf_set_last_constant_stats(&stats);
        if (staged_initialized) ppf_clear(&staged);
        if (allocation_failed) {
            if (alloc_was_set) ppf_context_flags_set(ctx, PPF_FLAG_ALLOC);
            return PPF_TERNARY_ERROR;
        }
        {
            int rc = constant_error(rop, ctx);
            if (alloc_was_set) ppf_context_flags_set(ctx, PPF_FLAG_ALLOC);
            return rc;
        }
    }

    ppf_swap(rop, &staged);
    ppf_clear(&staged);
    stats.ternary = ternary;
    stats.total_seconds = ppf_constant_now() - started;
    stats.rounding_seconds = stats.total_seconds > stats.compute_seconds
        ? stats.total_seconds - stats.compute_seconds : 0.0;
    if (ternary != 0) ppf_raise(ctx, PPF_FLAG_INEXACT);
    if (alloc_was_set) ppf_context_flags_set(ctx, PPF_FLAG_ALLOC);
    ppf_set_last_constant_stats(&stats);
    return ternary;
}

int ppf_precompute_constant_spec(const ppf_constant_spec_t *spec,
                                 ppf_prec_t precision,
                                 unsigned long workers) {
    ppf_constant_interval_t interval;
    ppf_constant_stats_t stats;
    ppf_context_t *ctx;
    ppf_prec_t working;
    uint64_t allocation_epoch;
    int alloc_was_set;
    int built = 0;

    if (!spec || !spec->cache || !spec->build_interval ||
        precision < PPF_PREC_MIN) return -1;
    ctx = ppf_default_context();
    if (!ctx) return -1;
    alloc_was_set = (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0;
    if (alloc_was_set) ppf_context_flags_clear(ctx, PPF_FLAG_ALLOC);
    allocation_epoch = ppf_allocation_failure_epoch_internal();
    workers = ppf_constant_resolve_workers(workers);
    pthread_mutex_lock(&spec->cache->mutex);
    if (spec->cache->valid &&
        spec->cache->certified_precision >= precision) {
        pthread_mutex_unlock(&spec->cache->mutex);
        if (alloc_was_set) ppf_context_flags_set(ctx, PPF_FLAG_ALLOC);
        return 0;
    }
    working = precision;
    if (spec->cache->valid &&
        spec->cache->certified_precision > working)
        working = spec->cache->certified_precision;
    ppf_constant_interval_init(&interval);
    memset(&stats, 0, sizeof(stats));
    stats.constant_id = spec->id;
    stats.requested_precision = precision;
    stats.workers = workers;
    for (unsigned long attempt = 1UL;
         attempt <= PPF_CONSTANT_MAX_ATTEMPTS; ++attempt) {
        ppf_prec_t certified;
        memset(&stats, 0, sizeof(stats));
        stats.constant_id = spec->id;
        stats.requested_precision = precision;
        stats.workers = workers;
        stats.attempts = attempt;
        if (spec->build_interval(&interval, working, workers,
                                 &stats, ctx) != 0 ||
            (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0 ||
            allocation_failed_since(allocation_epoch))
            break;
        certified = ppf_constant_interval_precision(&interval);
        if (certified >= precision) {
            built = 1;
            break;
        }
        {
            ppf_prec_t next = grow_precision(working);
            if (next <= working) break;
            working = next;
        }
    }
    if (!built) {
        if (allocation_failed_since(allocation_epoch))
            ppf_raise(ctx, PPF_FLAG_ALLOC);
        ppf_constant_interval_clear(&interval);
        pthread_mutex_unlock(&spec->cache->mutex);
        if (alloc_was_set) ppf_context_flags_set(ctx, PPF_FLAG_ALLOC);
        return -1;
    }
    if (interval_is_better(spec->cache, &interval)) {
        ppf_constant_interval_swap(&spec->cache->interval, &interval);
        spec->cache->certified_precision =
            ppf_constant_interval_precision(&spec->cache->interval);
        spec->cache->valid = 1;
    }
    stats.cached_precision = spec->cache->certified_precision;
    ppf_set_last_constant_stats(&stats);
    ppf_constant_interval_clear(&interval);
    pthread_mutex_unlock(&spec->cache->mutex);
    if (alloc_was_set) ppf_context_flags_set(ctx, PPF_FLAG_ALLOC);
    return 0;
}

int ppf_export_constant_interval_spec(const ppf_constant_spec_t *spec,
                                      mpz_ptr lower, mpz_ptr upper,
                                      ppf_exp_t *exponent,
                                      ppf_prec_t precision,
                                      unsigned long workers) {
    if (!spec || !lower || !upper || !exponent || lower == upper) return -1;
    if (ppf_precompute_constant_spec(spec, precision, workers) != 0)
        return -1;
    pthread_mutex_lock(&spec->cache->mutex);
    if (!spec->cache->valid) {
        pthread_mutex_unlock(&spec->cache->mutex);
        return -1;
    }
    mpz_set(lower, spec->cache->interval.lower);
    mpz_set(upper, spec->cache->interval.upper);
    *exponent = spec->cache->interval.exponent;
    pthread_mutex_unlock(&spec->cache->mutex);
    return 0;
}

static const ppf_constant_spec_t *spec_for(ppf_constant_id_t id) {
    switch (id) {
        case PPF_CONSTANT_PI: return ppf_pi_constant_spec();
        case PPF_CONSTANT_LOG2: return ppf_log2_constant_spec();
        case PPF_CONSTANT_EULER: return ppf_euler_constant_spec();
        case PPF_CONSTANT_CATALAN: return ppf_catalan_constant_spec();
        default: return NULL;
    }
}

int ppf_const_pi(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_pi_constant_spec(), rop, rnd,
                                      0UL, 1, ctx);
}

int ppf_const_pi_threads(ppf_t *rop, ppf_rnd_t rnd,
                         unsigned long workers, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_pi_constant_spec(), rop, rnd,
                                      workers, 1, ctx);
}

int ppf_const_pi_uncached(ppf_t *rop, ppf_rnd_t rnd,
                          unsigned long workers, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_pi_constant_spec(), rop, rnd,
                                      workers, 0, ctx);
}

int ppf_const_log2(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_log2_constant_spec(), rop, rnd,
                                      0UL, 1, ctx);
}

int ppf_const_log2_threads(ppf_t *rop, ppf_rnd_t rnd,
                           unsigned long workers, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_log2_constant_spec(), rop, rnd,
                                      workers, 1, ctx);
}

int ppf_const_log2_uncached(ppf_t *rop, ppf_rnd_t rnd,
                            unsigned long workers, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_log2_constant_spec(), rop, rnd,
                                      workers, 0, ctx);
}

int ppf_const_euler(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_euler_constant_spec(), rop, rnd,
                                      0UL, 1, ctx);
}

int ppf_const_euler_threads(ppf_t *rop, ppf_rnd_t rnd,
                            unsigned long workers, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_euler_constant_spec(), rop, rnd,
                                      workers, 1, ctx);
}

int ppf_const_euler_uncached(ppf_t *rop, ppf_rnd_t rnd,
                             unsigned long workers, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_euler_constant_spec(), rop, rnd,
                                      workers, 0, ctx);
}

int ppf_const_catalan(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_catalan_constant_spec(), rop, rnd,
                                      0UL, 1, ctx);
}

int ppf_const_catalan_threads(ppf_t *rop, ppf_rnd_t rnd,
                              unsigned long workers, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_catalan_constant_spec(), rop, rnd,
                                      workers, 1, ctx);
}

int ppf_const_catalan_uncached(ppf_t *rop, ppf_rnd_t rnd,
                               unsigned long workers, ppf_context_t *ctx) {
    return ppf_evaluate_constant_spec(ppf_catalan_constant_spec(), rop, rnd,
                                      workers, 0, ctx);
}

int ppf_precompute_pi(ppf_prec_t precision, unsigned long workers) {
    return ppf_precompute_constant_spec(ppf_pi_constant_spec(), precision,
                                        workers);
}

int ppf_precompute_log2(ppf_prec_t precision, unsigned long workers) {
    return ppf_precompute_constant_spec(ppf_log2_constant_spec(), precision,
                                        workers);
}

int ppf_precompute_euler(ppf_prec_t precision, unsigned long workers) {
    return ppf_precompute_constant_spec(ppf_euler_constant_spec(), precision,
                                        workers);
}

int ppf_precompute_catalan(ppf_prec_t precision, unsigned long workers) {
    return ppf_precompute_constant_spec(ppf_catalan_constant_spec(), precision,
                                        workers);
}

int ppf_precompute_constant(ppf_constant_id_t id, ppf_prec_t precision,
                            unsigned long workers) {
    const ppf_constant_spec_t *spec = spec_for(id);
    return spec ? ppf_precompute_constant_spec(spec, precision, workers) : -1;
}

int ppf_get_constant_interval(ppf_constant_id_t id,
                              mpz_ptr lower, mpz_ptr upper,
                              ppf_exp_t *exponent,
                              ppf_prec_t precision,
                              unsigned long workers) {
    const ppf_constant_spec_t *spec = spec_for(id);
    return spec ? ppf_export_constant_interval_spec(
        spec, lower, upper, exponent, precision, workers) : -1;
}

void ppf_clear_constant_cache(ppf_constant_id_t id) {
    const ppf_constant_spec_t *spec = spec_for(id);
    if (spec) ppf_constant_cache_reset(spec->cache);
}

void ppf_free_constant_caches(void) {
    ppf_constant_cache_purge(ppf_pi_constant_spec()->cache);
    ppf_constant_cache_purge(ppf_log2_constant_spec()->cache);
    ppf_constant_cache_purge(ppf_euler_constant_spec()->cache);
    ppf_constant_cache_purge(ppf_catalan_constant_spec()->cache);
}

ppf_prec_t ppf_constant_cache_precision(ppf_constant_id_t id) {
    const ppf_constant_spec_t *spec = spec_for(id);
    ppf_prec_t result = 0;
    if (!spec) return 0;
    pthread_mutex_lock(&spec->cache->mutex);
    if (spec->cache->valid) result = spec->cache->certified_precision;
    pthread_mutex_unlock(&spec->cache->mutex);
    return result;
}

void ppf_set_constant_threads(unsigned long workers) {
    if (workers == 0UL) workers = 1UL;
    atomic_store(&g_constant_workers, workers);
}

unsigned long ppf_get_constant_threads(void) {
    unsigned long workers = atomic_load(&g_constant_workers);
    return workers > 0UL ? workers : 1UL;
}

int ppf_get_last_constant_stats(ppf_constant_stats_t *out) {
    if (!out) return -1;
    *out = g_last_constant_stats;
    return 0;
}
