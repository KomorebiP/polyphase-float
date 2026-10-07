/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "constant_internal.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The Chudnovsky engine is embedded here, but its C work arrays are routed
   through the active numerical context.  The allocator callback is serialized
   because OpenMP workers may request packet scratch concurrently and public
   custom allocators are not required to be internally synchronized. */
static ppf_context_t *g_pi_core_context;
static pthread_mutex_t g_pi_core_allocator_mutex = PTHREAD_MUTEX_INITIALIZER;

static void pi_core_set_context(ppf_context_t *ctx) {
    pthread_mutex_lock(&g_pi_core_allocator_mutex);
    g_pi_core_context = ctx;
    pthread_mutex_unlock(&g_pi_core_allocator_mutex);
}

static void *pi_core_malloc(size_t bytes) {
    void *out;
    pthread_mutex_lock(&g_pi_core_allocator_mutex);
    out = g_pi_core_context
        ? ppf_owned_buffer_alloc_internal(g_pi_core_context, bytes)
        : malloc(bytes);
    pthread_mutex_unlock(&g_pi_core_allocator_mutex);
    return out;
}

static void *pi_core_calloc(size_t count, size_t bytes) {
    void *out;
    pthread_mutex_lock(&g_pi_core_allocator_mutex);
    out = g_pi_core_context
        ? ppf_owned_buffer_calloc_internal(g_pi_core_context, count, bytes)
        : calloc(count, bytes);
    pthread_mutex_unlock(&g_pi_core_allocator_mutex);
    return out;
}

static void *pi_core_realloc(void *ptr, size_t bytes) {
    void *out;
    pthread_mutex_lock(&g_pi_core_allocator_mutex);
    out = g_pi_core_context
        ? ppf_owned_buffer_realloc_internal(g_pi_core_context, ptr, bytes)
        : realloc(ptr, bytes);
    pthread_mutex_unlock(&g_pi_core_allocator_mutex);
    return out;
}

static void pi_core_free(void *ptr) {
    pthread_mutex_lock(&g_pi_core_allocator_mutex);
    if (g_pi_core_context) ppf_owned_buffer_free_internal(ptr);
    else free(ptr);
    pthread_mutex_unlock(&g_pi_core_allocator_mutex);
}

#define PPF_CORE_MALLOC(bytes) pi_core_malloc(bytes)
#define PPF_CORE_CALLOC(count, bytes) pi_core_calloc((count), (bytes))
#define PPF_CORE_REALLOC(ptr, bytes) pi_core_realloc((ptr), (bytes))
#define PPF_CORE_FREE(ptr) pi_core_free(ptr)
#define main ppf_embedded_polyphase_cli_main
#include "polyphase_chudnovsky_core.c"
#undef main
#undef PPF_CORE_FREE
#undef PPF_CORE_REALLOC
#undef PPF_CORE_CALLOC
#undef PPF_CORE_MALLOC

#define PPF_PI_INITIAL_GUARD_BITS 192UL
#define PPF_PI_MIN_GUARD_BITS 128UL
#define PPF_PI_INTERVAL_RADIUS 4UL
#define PPF_PI_CHUD_R_TEXT "151931373056000"

static ppf_constant_cache_t g_pi_cache;
static pthread_once_t g_pi_once = PTHREAD_ONCE_INIT;
static ppf_constant_spec_t g_pi_spec;

static unsigned long pi_digits_for_bits(ppf_prec_t bits) {
    long double value = ceill((long double)bits / (long double)LOG2_10);
    if (value < 1.0L) return 1UL;
    if (value > (long double)ULONG_MAX) return ULONG_MAX;
    return (unsigned long)value;
}

static void reset_core_policy(void) {
    g_weighted_packets = 1;
    g_directed_cancel = 1;
    g_channel_kernel = 1;
    g_cancel_block1 = DEFAULT_CANCEL_BLOCK1;
    g_cancel_block2 = DEFAULT_CANCEL_BLOCK2;
    g_adic_factors = 0;
    g_auto_mode = 0;
    g_auto_selected_band = 0;
    g_adic_gauge = 0;
    g_adic_block_terms = DEFAULT_ADIC_BLOCK_TERMS;
    g_adic_cancel_min_span = 2UL;
    g_adic_prime_bound = 0UL;
    g_channel_metadata = 0;
    g_full_upper_gcd = 0;
    g_factored_upper = 0;
    g_unit_prime_bound = 0UL;
    g_unit_min_span = 4UL;
    g_p_elision = 1;
    g_split_q2 = 1;
    g_pair_phase = 0;
    g_phase_align = 1;
    g_symbolic_top_levels = 0UL;
    g_affine_root = 0;
    g_affine_root_cancel = 1;
    g_parallel_final_mul = 1;
    g_parallel_mul_min_bits = DEFAULT_PARALLEL_MUL_MIN_BITS;
    g_parallel_final_channels = 0UL;
    g_certified_decimal = 0;
}

static int capped_power_gt(mpz_srcptr limit, mpz_srcptr base_in,
                           unsigned long exp) {
    mpz_t acc, base, temp, cap;
    size_t limit_bits;
    mpz_inits(acc, base, temp, cap, NULL);
    mpz_set_ui(acc, 1UL);
    mpz_set(base, base_in);
    mpz_add_ui(cap, limit, 1UL);
    limit_bits = mpz_sizeinbase(limit, 2);
    while (exp > 0UL) {
        if (exp & 1UL) {
            size_t ab = mpz_sizeinbase(acc, 2);
            size_t bb = mpz_sizeinbase(base, 2);
            if (mpz_cmp(acc, limit) > 0 || mpz_cmp(base, limit) > 0 ||
                ab + bb > limit_bits + 2U) {
                mpz_clears(acc, base, temp, cap, NULL);
                return 1;
            }
            mpz_mul(temp, acc, base);
            if (mpz_cmp(temp, limit) > 0) {
                mpz_clears(acc, base, temp, cap, NULL);
                return 1;
            }
            mpz_swap(acc, temp);
        }
        exp >>= 1UL;
        if (exp == 0UL) break;
        {
            size_t bb = mpz_sizeinbase(base, 2);
            if (mpz_cmp(base, limit) > 0 ||
                2U * bb > limit_bits + 2U) {
                mpz_set(base, cap);
            } else {
                mpz_mul(temp, base, base);
                if (mpz_cmp(temp, limit) > 0) mpz_set(base, cap);
                else mpz_swap(base, temp);
            }
        }
    }
    {
        int answer = mpz_cmp(acc, limit) > 0;
        mpz_clears(acc, base, temp, cap, NULL);
        return answer;
    }
}

static int tail_under_one_unit(unsigned long terms,
                               unsigned long scale_bits) {
    mpz_t linear, left, denominator, limit, base;
    int answer;
    mpz_inits(linear, left, denominator, limit, base, NULL);
    mpz_set_ui(linear, terms);
    mpz_mul_ui(linear, linear, CHUD_B);
    mpz_add_ui(linear, linear, CHUD_A);
    mpz_mul_ui(left, linear, 426880UL * 101UL);
    mpz_mul_2exp(left, left, scale_bits);
    mpz_set_ui(denominator, CHUD_A - 1UL);
    mpz_mul(denominator, denominator, denominator);
    mpz_cdiv_q(limit, left, denominator);
    mpz_set_str(base, PPF_PI_CHUD_R_TEXT, 10);
    answer = capped_power_gt(limit, base, terms);
    mpz_clears(linear, left, denominator, limit, base, NULL);
    return answer;
}

static unsigned long interval_width_bits(mpz_srcptr lower,
                                         mpz_srcptr upper) {
    mpz_t width;
    unsigned long bits;
    mpz_init(width);
    mpz_sub(width, upper, lower);
    bits = mpz_sgn(width) == 0 ? 0UL
        : (unsigned long)mpz_sizeinbase(width, 2);
    mpz_clear(width);
    return bits;
}

static int build_pi_interval(ppf_constant_interval_t *out,
                             ppf_prec_t requested_precision,
                             unsigned long workers,
                             ppf_constant_stats_t *stats,
                             ppf_context_t *ctx) {
    unsigned long guard = PPF_PI_INITIAL_GUARD_BITS;
    unsigned long digits;
    if (!out || requested_precision > (ppf_prec_t)ULONG_MAX)
        return -1;
    digits = pi_digits_for_bits(requested_precision);
    if (digits == ULONG_MAX) return -1;
    if (guard < PPF_PI_MIN_GUARD_BITS) guard = PPF_PI_MIN_GUARD_BITS;

    for (unsigned long attempt = 1UL; attempt <= 10UL; ++attempt) {
        run_result_t result;
        int rc;
        double started;
        reset_core_policy();
        started = ppf_constant_now();
        pi_core_set_context(ctx);
        rc = compute_polyphase_chudnovsky(
            &result, digits, guard, workers, 0UL, 0UL,
            DEFAULT_RATIO_EXTRA_BITS, 1);
        if (rc != 0) {
            pi_core_set_context(NULL);
            return -1;
        }
        if (tail_under_one_unit(result.terms, result.scale_bits)) {
            if (result.scale_bits > (unsigned long)INT64_MAX) {
                mpz_clear(result.value);
                pi_core_free(result.certified_text);
                pi_core_set_context(NULL);
                return -1;
            }
            mpz_sub_ui(out->lower, result.value, PPF_PI_INTERVAL_RADIUS);
            mpz_add_ui(out->upper, result.value, PPF_PI_INTERVAL_RADIUS);
            out->exponent = -(ppf_exp_t)result.scale_bits;
            if (stats) {
                memset(stats, 0, sizeof(*stats));
                stats->constant_id = PPF_CONSTANT_PI;
                stats->requested_precision = requested_precision;
                stats->computed_precision = result.scale_bits;
                stats->workers = workers;
                stats->attempts = attempt;
                stats->terms = result.terms;
                stats->packets = result.packets;
                stats->guard_bits = guard;
                stats->interval_width_bits = interval_width_bits(
                    out->lower, out->upper);
                stats->interval_exponent = out->exponent;
                stats->compute_seconds = ppf_constant_now() - started;
            }
            mpz_clear(result.value);
            pi_core_free(result.certified_text);
            pi_core_set_context(NULL);
            return 0;
        }
        mpz_clear(result.value);
        pi_core_free(result.certified_text);
        pi_core_set_context(NULL);
        if (guard > ULONG_MAX / 2UL) break;
        guard *= 2UL;
    }
    return -1;
}

static void pi_initialize(void) {
    ppf_constant_cache_init(&g_pi_cache);
    mpz_init_set_str(g_chud_c3_over_24, CHUD_C3_OVER_24_TEXT, 10);
    if (!verify_global_alternating_bound() || CHUD_A - 1UL <= 426880UL)
        abort();
    {
        mpz_t left, right;
        mpz_inits(left, right, NULL);
        mpz_set_ui(left, CHUD_A + CHUD_B);
        mpz_mul_ui(left, left, 10UL);
        mpz_set(right, g_chud_c3_over_24);
        if (mpz_cmp(left, right) >= 0) abort();
        mpz_clears(left, right, NULL);
    }
    memset(&g_pi_spec, 0, sizeof(g_pi_spec));
    g_pi_spec.id = PPF_CONSTANT_PI;
    g_pi_spec.name = "pi";
    g_pi_spec.normal_exponent = 2;
    g_pi_spec.required_emin = 1;
    g_pi_spec.required_emax = 3;
    g_pi_spec.build_interval = build_pi_interval;
    g_pi_spec.cache = &g_pi_cache;
}

const ppf_constant_spec_t *ppf_pi_constant_spec(void) {
    pthread_once(&g_pi_once, pi_initialize);
    return &g_pi_spec;
}
