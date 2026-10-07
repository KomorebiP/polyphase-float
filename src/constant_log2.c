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

#include "ppf_openmp_compat.h"

#define PPF_LOG2_GUARD_BITS 128UL
#define PPF_LOG2_RATIO_DENOM_SQUARED 9UL
#ifndef PPF_LOG2_REDUCE_GCD
#define PPF_LOG2_REDUCE_GCD 0
#endif
#ifndef PPF_LOG2_ROOT_REDUCE
#define PPF_LOG2_ROOT_REDUCE 0
#endif

typedef struct {
    mpz_t p;
    mpz_t q;
    mpz_t t;
} log2_packet_t;

static ppf_constant_cache_t g_log2_cache;
static pthread_once_t g_log2_once = PTHREAD_ONCE_INIT;
static ppf_constant_spec_t g_log2_spec;

static void packet_init(log2_packet_t *x) {
    mpz_inits(x->p, x->q, x->t, NULL);
}

static void packet_clear(log2_packet_t *x) {
    mpz_clears(x->p, x->q, x->t, NULL);
}

static void packet_leaf(log2_packet_t *out, unsigned long k) {
    unsigned long two_k_plus_1;
    unsigned long two_k_plus_3;
    if (k > (ULONG_MAX - 3UL) / 2UL) abort();
    two_k_plus_1 = 2UL * k + 1UL;
    two_k_plus_3 = two_k_plus_1 + 2UL;
    mpz_set_ui(out->p, two_k_plus_1);
    mpz_set_ui(out->q, two_k_plus_3);
    mpz_mul_ui(out->q, out->q, PPF_LOG2_RATIO_DENOM_SQUARED);
    mpz_set(out->t, out->q);
}

static void packet_combine(log2_packet_t *left,
                           const log2_packet_t *right,
                           mpz_ptr temp_t, mpz_ptr temp_pt) {
    mpz_mul(temp_t, left->t, right->q);
    mpz_mul(temp_pt, left->p, right->t);
    mpz_add(temp_t, temp_t, temp_pt);
    mpz_mul(left->p, left->p, right->p);
    mpz_mul(left->q, left->q, right->q);
    mpz_swap(left->t, temp_t);
#if PPF_LOG2_REDUCE_GCD
    mpz_gcd(temp_t, left->p, left->q);
    mpz_gcd(temp_pt, temp_t, left->t);
    if (mpz_cmp_ui(temp_pt, 1UL) > 0) {
        mpz_divexact(left->p, left->p, temp_pt);
        mpz_divexact(left->q, left->q, temp_pt);
        mpz_divexact(left->t, left->t, temp_pt);
    }
#endif
}

static void packet_bs(log2_packet_t *out,
                      unsigned long a, unsigned long b,
                      log2_packet_t *scratch, unsigned long depth,
                      mpz_ptr temp_t, mpz_ptr temp_pt) {
    unsigned long middle;
    if (b - a == 1UL) {
        packet_leaf(out, a);
        return;
    }
    middle = a + (b - a) / 2UL;
    packet_bs(out, a, middle, scratch, depth + 1UL, temp_t, temp_pt);
    packet_bs(&scratch[depth], middle, b, scratch, depth + 1UL,
              temp_t, temp_pt);
    packet_combine(out, &scratch[depth], temp_t, temp_pt);
}

static unsigned long next_power_of_two(unsigned long x) {
    unsigned long p = 1UL;
    while (p < x && p <= ULONG_MAX / 2UL) p <<= 1UL;
    return p;
}

static unsigned long packet_count_for(unsigned long terms,
                                      unsigned long workers) {
    unsigned long target;
    unsigned long packets;
    if (workers == 0UL) workers = 1UL;
    target = workers > ULONG_MAX / 4UL ? workers : 4UL * workers;
    packets = next_power_of_two(target);
    while (packets > terms && packets > 1UL) packets >>= 1UL;
    return packets > 0UL ? packets : 1UL;
}

static unsigned long depth_for_width(unsigned long width) {
    unsigned long depth = 2UL;
    while (width > 1UL) {
        width = (width + 1UL) >> 1UL;
        ++depth;
    }
    return depth;
}

static int build_log2_packet(log2_packet_t *root,
                             unsigned long terms,
                             unsigned long workers,
                             unsigned long *packet_count_out,
                             ppf_context_t *ctx) {
    unsigned long packets = packet_count_for(terms, workers);
    unsigned long max_width = (terms + packets - 1UL) / packets;
    unsigned long depth = depth_for_width(max_width);
    size_t scratch_count;
    unsigned long *bounds;
    log2_packet_t *nodes;
    log2_packet_t *scratch_all;

    if (workers == 0UL || depth == 0UL ||
        workers > (unsigned long)(SIZE_MAX / depth)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return -1;
    }
    scratch_count = (size_t)workers * (size_t)depth;
    bounds = (unsigned long *)ppf_owned_buffer_calloc_internal(
        ctx, (size_t)packets + 1U, sizeof(*bounds));
    nodes = (log2_packet_t *)ppf_owned_buffer_calloc_internal(
        ctx, (size_t)packets, sizeof(*nodes));
    scratch_all = (log2_packet_t *)ppf_owned_buffer_calloc_internal(
        ctx, scratch_count, sizeof(*scratch_all));
    if (!bounds || !nodes || !scratch_all) {
        ppf_owned_buffer_free_internal(scratch_all);
        ppf_owned_buffer_free_internal(nodes);
        ppf_owned_buffer_free_internal(bounds);
        return -1;
    }

    for (unsigned long i = 0UL; i <= packets; ++i) {
#if defined(__SIZEOF_INT128__)
        bounds[i] = (unsigned long)(((__uint128_t)i * terms) / packets);
#else
        bounds[i] = (terms / packets) * i +
                    ((terms % packets) * i) / packets;
#endif
    }
    for (unsigned long i = 0UL; i < packets; ++i) packet_init(&nodes[i]);
    for (size_t i = 0U; i < scratch_count; ++i)
        packet_init(&scratch_all[i]);

#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel num_threads(workers))
#endif
    {
        unsigned long tid = (unsigned long)omp_get_thread_num();
        log2_packet_t *scratch = &scratch_all[(size_t)tid * (size_t)depth];
        mpz_t temp_t, temp_pt;
        mpz_inits(temp_t, temp_pt, NULL);
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp for schedule(dynamic,1))
#endif
        for (long i = 0L; i < (long)packets; ++i) {
            packet_bs(&nodes[(unsigned long)i],
                      bounds[(unsigned long)i],
                      bounds[(unsigned long)i + 1UL],
                      scratch, 0UL, temp_t, temp_pt);
        }
        mpz_clears(temp_t, temp_pt, NULL);
    }

    for (unsigned long stride = 1UL; stride < packets; stride <<= 1UL) {
        unsigned long step = stride << 1UL;
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel num_threads(workers))
#endif
        {
            mpz_t temp_t, temp_pt;
            mpz_inits(temp_t, temp_pt, NULL);
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp for schedule(static))
#endif
            for (long left = 0L; left < (long)packets; left += (long)step) {
                unsigned long right = (unsigned long)left + stride;
                if (right < packets) {
                    packet_combine(&nodes[(unsigned long)left],
                                   &nodes[right], temp_t, temp_pt);
                }
            }
            mpz_clears(temp_t, temp_pt, NULL);
        }
    }

    mpz_set(root->p, nodes[0].p);
    mpz_set(root->q, nodes[0].q);
    mpz_set(root->t, nodes[0].t);
    for (size_t i = 0U; i < scratch_count; ++i)
        packet_clear(&scratch_all[i]);
    for (unsigned long i = 0UL; i < packets; ++i) packet_clear(&nodes[i]);
    ppf_owned_buffer_free_internal(scratch_all);
    ppf_owned_buffer_free_internal(nodes);
    ppf_owned_buffer_free_internal(bounds);
    if (packet_count_out) *packet_count_out = packets;
    return 0;
}

static unsigned long terms_for_scale(unsigned long scale_bits) {
    long double n = ceill(((long double)scale_bits + 16.0L) /
                          log2l(9.0L)) + 1.0L;
    if (n < 1.0L) return 1UL;
    if (n > (long double)ULONG_MAX) return ULONG_MAX;
    return (unsigned long)n;
}

static unsigned long width_bits(mpz_srcptr lower, mpz_srcptr upper) {
    mpz_t width;
    unsigned long bits;
    mpz_init(width);
    mpz_sub(width, upper, lower);
    bits = mpz_sgn(width) == 0 ? 0UL
        : (unsigned long)mpz_sizeinbase(width, 2);
    mpz_clear(width);
    return bits;
}

/*
   log(2) = (2/3) * sum_{k>=0} prod_{j<k}(2j+1)/(9(2j+3)).
   If T/Q is the first n terms and P/Q is the first omitted product,

       8T/(12Q) < log(2) < (8T+9P)/(12Q).
*/
static int build_log2_interval(ppf_constant_interval_t *out,
                               ppf_prec_t requested_precision,
                               unsigned long workers,
                               ppf_constant_stats_t *stats,
                               ppf_context_t *ctx) {
    unsigned long scale_bits;
    unsigned long terms;
    unsigned long packets = 0UL;
    log2_packet_t root;
    mpz_t denominator, lower_num, upper_num, temp;
    int rc;
    double started;
    if (!out || requested_precision > (ppf_prec_t)ULONG_MAX -
                                     PPF_LOG2_GUARD_BITS)
        return -1;
    scale_bits = (unsigned long)requested_precision + PPF_LOG2_GUARD_BITS;
    if (scale_bits > (unsigned long)INT64_MAX) return -1;
    terms = terms_for_scale(scale_bits);
    if (terms == ULONG_MAX) return -1;
    packet_init(&root);
    started = ppf_constant_now();
    rc = build_log2_packet(&root, terms, workers, &packets, ctx);
    if (rc != 0) {
        packet_clear(&root);
        return -1;
    }
    mpz_inits(denominator, lower_num, upper_num, temp, NULL);
#if PPF_LOG2_ROOT_REDUCE
    /* At the root only the partial sum T/Q is still needed.  Reduce it once,
       and use the closed geometric tail bound
           tail < 3 / (4 * 9^terms * (2*terms+1)). */
    mpz_gcd(temp, root.t, root.q);
    if (mpz_cmp_ui(temp, 1UL) > 0) {
        mpz_divexact(root.t, root.t, temp);
        mpz_divexact(root.q, root.q, temp);
    }
    mpz_mul_ui(denominator, root.q, 3UL);
    mpz_mul_ui(lower_num, root.t, 2UL);
    mpz_mul_2exp(lower_num, lower_num, scale_bits);
    mpz_fdiv_q(out->lower, lower_num, denominator);
    mpz_cdiv_q(out->upper, lower_num, denominator);

    mpz_ui_pow_ui(temp, 9UL, terms);
    mpz_mul_ui(temp, temp, 2UL * terms + 1UL);
    mpz_mul_ui(temp, temp, 4UL);
    mpz_set_ui(upper_num, 3UL);
    mpz_mul_2exp(upper_num, upper_num, scale_bits);
    mpz_cdiv_q(upper_num, upper_num, temp);
    mpz_add(out->upper, out->upper, upper_num);
#else
    mpz_mul_ui(denominator, root.q, 12UL);

    mpz_mul_ui(lower_num, root.t, 8UL);
    mpz_mul_2exp(lower_num, lower_num, scale_bits);
    mpz_fdiv_q(out->lower, lower_num, denominator);

    mpz_mul_ui(upper_num, root.t, 8UL);
    mpz_mul_ui(temp, root.p, 9UL);
    mpz_add(upper_num, upper_num, temp);
    mpz_mul_2exp(upper_num, upper_num, scale_bits);
    mpz_cdiv_q(out->upper, upper_num, denominator);
#endif
    out->exponent = -(ppf_exp_t)scale_bits;
    if (mpz_cmp(out->lower, out->upper) >= 0) rc = -1;
    if (stats && rc == 0) {
        memset(stats, 0, sizeof(*stats));
        stats->constant_id = PPF_CONSTANT_LOG2;
        stats->requested_precision = requested_precision;
        stats->computed_precision = scale_bits;
        stats->workers = workers;
        stats->attempts = 1UL;
        stats->terms = terms;
        stats->packets = packets;
        stats->guard_bits = PPF_LOG2_GUARD_BITS;
        stats->interval_width_bits = width_bits(out->lower, out->upper);
        stats->interval_exponent = out->exponent;
        stats->compute_seconds = ppf_constant_now() - started;
    }
    mpz_clears(denominator, lower_num, upper_num, temp, NULL);
    packet_clear(&root);
    return rc;
}

static void log2_initialize(void) {
    ppf_constant_cache_init(&g_log2_cache);
    memset(&g_log2_spec, 0, sizeof(g_log2_spec));
    g_log2_spec.id = PPF_CONSTANT_LOG2;
    g_log2_spec.name = "log2";
    g_log2_spec.normal_exponent = 0;
    g_log2_spec.required_emin = -1;
    g_log2_spec.required_emax = 1;
    g_log2_spec.build_interval = build_log2_interval;
    g_log2_spec.cache = &g_log2_cache;
}

const ppf_constant_spec_t *ppf_log2_constant_spec(void) {
    pthread_once(&g_log2_once, log2_initialize);
    return &g_log2_spec;
}
