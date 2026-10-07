/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <getopt.h>
#include <gmp.h>
#include <limits.h>
#include <math.h>
#include "ppf_openmp_compat.h"
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#ifndef PPF_CORE_MALLOC
#define PPF_CORE_MALLOC(bytes) malloc(bytes)
#endif
#ifndef PPF_CORE_CALLOC
#define PPF_CORE_CALLOC(count, bytes) calloc((count), (bytes))
#endif
#ifndef PPF_CORE_REALLOC
#define PPF_CORE_REALLOC(ptr, bytes) realloc((ptr), (bytes))
#endif
#ifndef PPF_CORE_FREE
#define PPF_CORE_FREE(ptr) free(ptr)
#endif

#ifndef POLYPHASE_CHUDNOVSKY_VERSION
#define POLYPHASE_CHUDNOVSKY_VERSION "0.18.0"
#endif

/* mpz_get_str(NULL, ...) is owned by GMP, not by PPF_CORE_MALLOC.  Release
   that storage through GMP's active deallocator even when the engine is
   embedded under a context-owned C allocator. */
static void ppf_core_free_gmp_string(char *text) {
    void (*free_fn)(void *, size_t) = NULL;
    if (!text) return;
    mp_get_memory_functions(NULL, NULL, &free_fn);
    if (free_fn) free_fn(text, strlen(text) + 1U);
}

static char *ppf_core_strdup(const char *text) {
    size_t bytes;
    char *copy;
    if (!text) return NULL;
    bytes = strlen(text) + 1U;
    copy = (char *)PPF_CORE_MALLOC(bytes);
    if (copy) memcpy(copy, text, bytes);
    return copy;
}

#define LOG2_10 3.32192809488736234787
#define CHUD_A 13591409UL
#define CHUD_B 545140134UL
#define CHUD_C3_OVER_24_U64 UINT64_C(10939058860032000)
#define CHUD_C3_OVER_24_TEXT "10939058860032000"
#define CHUD_C3_OVER_24_ODD_U64 UINT64_C(333833583375)
#define CHUD_C3_OVER_24_V2 15UL
#define CHUD_DIGITS_PER_TERM 14.1816474627254776555
#define DEFAULT_GUARD_BITS 80UL
#define DEFAULT_RATIO_EXTRA_BITS 64UL
#define DEFAULT_CANCEL_BLOCK1 64UL
#define DEFAULT_CANCEL_BLOCK2 256UL
#define DEFAULT_ADIC_BLOCK_TERMS 8000UL
#define PARALLEL_CANCEL_DIV_BITS 12000000UL
#define SYMBOLIC_ROOT_MIN_SUBTREE_BITS 4000000UL
#define AUTO_PRIME_BAND_BOUND 65535UL
#define DEFAULT_PARALLEL_MUL_MIN_BITS 20000000UL

/* Read-only after program initialization. GMP permits concurrent reads. */
static mpz_t g_chud_c3_over_24;

static int g_weighted_packets = 1;
static int g_directed_cancel = 1;
static int g_channel_kernel = 1;
static unsigned long g_cancel_block1 = DEFAULT_CANCEL_BLOCK1;
static unsigned long g_cancel_block2 = DEFAULT_CANCEL_BLOCK2;
static int g_adic_factors = 1;
static int g_auto_mode = 1;
static int g_auto_selected_band = 0;
static int g_adic_gauge = 0;
static unsigned long g_adic_block_terms = DEFAULT_ADIC_BLOCK_TERMS;
static unsigned long g_adic_cancel_min_span = 2UL;
static unsigned long g_adic_prime_bound = 0UL; /* 0 = all relevant primes */
static int g_channel_metadata = 1;
static int g_full_upper_gcd = 0;
static int g_factored_upper = 0;
static unsigned long g_unit_prime_bound = 0UL;
static unsigned long g_unit_min_span = 4UL;
static int g_p_elision = 1;
static int g_split_q2 = 1;
static int g_pair_phase = 1;
static int g_phase_align = 1;
static unsigned long g_symbolic_top_levels = 1UL;
static int g_affine_root = 0;
static int g_affine_root_cancel = 1;
static int g_parallel_final_mul = 1;
static unsigned long g_parallel_mul_min_bits = DEFAULT_PARALLEL_MUL_MIN_BITS;
static unsigned long g_parallel_final_channels = 0UL; /* 0 = auto, otherwise 3 or 4 */
static int g_certified_decimal = 0;

typedef struct {
    mpz_t p;
    /* Odd part of Q when the exact 2-adic split is enabled. */
    mpz_t q;
    mpz_t t;
    unsigned long q2;
} bs_result_t;

typedef struct {
    mpz_t p;
    mpz_t q;
    mpz_t tq;
    mpz_t pt;
    mpz_t cancel;
    unsigned long left;
    unsigned long right;
    int omit_p;
} channel_product_t;



typedef struct {
    uint32_t prime_index;
    uint32_t exponent;
} factor_entry_t;

typedef struct {
    factor_entry_t *entry;
    size_t size;
    size_t capacity;
} factor_vec_t;

typedef struct {
    factor_vec_t p;
    factor_vec_t q;
} factor_meta_t;

typedef struct {
    uint32_t limit;
    uint32_t max_factor;
    int complete;
    uint32_t *spf;
    uint32_t *prime_pos;
    uint32_t *primes;
    size_t prime_count;
    size_t prime_capacity;
    double *log2_prime;
} factor_sieve_t;

typedef struct {
    uint64_t *exp_p;
    uint64_t *exp_q;
    uint32_t *touched_p;
    uint32_t *touched_q;
    size_t touched_p_count;
    size_t touched_q_count;
} factor_build_scratch_t;

typedef struct {
    mpz_t temp;
    mpz_t buckets[64];
    unsigned char used[64];
} factor_product_ctx_t;

typedef struct {
    double removed_bits;
    uint64_t cancellation_events;
    uint64_t cancelled_prime_powers;
} factor_stats_t;

typedef struct {
    mpz_t value;
    unsigned long digits;
    unsigned long scale_bits;
    unsigned long terms;
    unsigned long packets;
    unsigned long max_packet_width;
    unsigned long ratio_keep_bits;
    unsigned long ratio_shift_bits;
    unsigned long ratio_extra_bits;
    int strict_final;
    double boundary_seconds;
    double sieve_seconds;
    double factor_metadata_seconds;
    double packet_seconds;
    double reduction_seconds;
    double factor_cancel_seconds;
    double final_seconds;
    double final_mul_seconds;
    double final_div_seconds;
    int parallel_final_mul_used;
    unsigned long parallel_final_mul_channels;
    double kernel_seconds;
    double total_seconds;
    unsigned long root_q_bits;
    unsigned long root_q_odd_bits;
    unsigned long root_q2_bits;
    unsigned long root_t_bits;
    double pair_phase_removed_bits;
    uint64_t pair_phase_events;
    unsigned long sieve_limit;
    unsigned long sieve_prime_count;
    unsigned long adic_block_terms;
    unsigned long adic_prime_bound;
    int adic_metadata_complete;
    double adic_removed_bits;
    double gauge_seconds;
    unsigned long max_gauge_bits;
    uint64_t adic_cancellation_events;
    uint64_t adic_cancelled_prime_powers;
    int adic_factors;
    int affine_root;
    unsigned long affine_tail_bits;
    unsigned long affine_attempts;
    unsigned long affine_left_q_bits;
    unsigned long affine_right_q_bits;
    int certified_decimal;
    int certificate_decisive;
    unsigned long certificate_interval_width_bits;
    double certificate_seconds;
    char *certified_text;
} run_result_t;

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static unsigned long bits_for_digits(unsigned long digits) {
    return (unsigned long)ceil((double)digits * LOG2_10);
}

static unsigned long nearest_power_of_two(unsigned long x) {
    if (x <= 1UL) return 1UL;
    unsigned long lower = 1UL;
    while (lower <= ULONG_MAX / 2UL && (lower << 1UL) <= x) lower <<= 1UL;
    unsigned long upper = lower <= ULONG_MAX / 2UL ? lower << 1UL : lower;
    if (upper == lower) return lower;
    return (x - lower <= upper - x) ? lower : upper;
}

/* Empirical production policy. The packet count, not the packet width, is
   quantized to a power of two so the upper reduction is a complete tree. */
static unsigned long choose_packet_count(unsigned long terms, unsigned long workers) {
    if (terms <= 64UL || workers <= 1UL) return 1UL;
    unsigned long raw;
    if (terms < 5000UL * workers) {
        raw = 2UL * workers;
    } else if (terms < 50000UL * workers) {
        raw = 4UL * workers;
    } else if (terms < 100000UL * workers) {
        raw = 8UL * workers;
    } else {
        raw = 32UL * workers;
    }
    unsigned long packets = nearest_power_of_two(raw);
    if (packets < workers) packets = nearest_power_of_two(workers);
    if (packets > terms) packets = nearest_power_of_two(terms);
    if (packets > terms) packets >>= 1UL;
    if (packets < 1UL) packets = 1UL;
    return packets;
}

#if ULONG_MAX < UINT64_MAX
static void mpz_set_u64(mpz_t out, uint64_t value) {
    mpz_import(out, 1, 1, sizeof(value), 0, 0, &value);
}
#endif

static void mpz_round_shift(mpz_t out, const mpz_t in, unsigned long bits) {
    if (bits == 0UL) {
        mpz_set(out, in);
        return;
    }
    int sign = mpz_sgn(in);
    if (sign == 0) {
        mpz_set_ui(out, 0UL);
        return;
    }
    mpz_abs(out, in);
    int round_up = mpz_tstbit(out, bits - 1UL);
    mpz_fdiv_q_2exp(out, out, bits);
    if (round_up) mpz_add_ui(out, out, 1UL);
    if (sign < 0) mpz_neg(out, out);
}

static void mpz_round_div(mpz_t out, const mpz_t numerator, const mpz_t denominator) {
    if (mpz_sgn(denominator) <= 0) {
        fprintf(stderr, "internal error: nonpositive denominator\n");
        abort();
    }
    mpz_t remainder;
    mpz_init(remainder);
    int sign = mpz_sgn(numerator);
    mpz_tdiv_qr(out, remainder, numerator, denominator);
    mpz_abs(remainder, remainder);
    mpz_mul_2exp(remainder, remainder, 1UL);
    if (mpz_cmp(remainder, denominator) >= 0) {
        if (sign >= 0) mpz_add_ui(out, out, 1UL);
        else mpz_sub_ui(out, out, 1UL);
    }
    mpz_clear(remainder);
}


/* Exact terminal multiplication packet.  With four workers the production
   path uses the four ordinary half-products, which have a slightly larger
   total work count but better occupancy and smaller reconstruction overhead
   than the three-product Karatsuba packet on the measured GMP backend.  The
   three-channel identity remains the fallback for exactly three workers. */
static int mpz_mul_parallel_split(
    mpz_t out, const mpz_t x, const mpz_t y, unsigned long workers
) {
    size_t xb = mpz_sgn(x) == 0 ? 0U : mpz_sizeinbase(x, 2);
    size_t yb = mpz_sgn(y) == 0 ? 0U : mpz_sizeinbase(y, 2);
    size_t maximum = xb > yb ? xb : yb;
    size_t minimum = xb < yb ? xb : yb;
    unsigned long channels = g_parallel_final_channels;
    if (channels == 0UL) channels = 3UL;
    if (!g_parallel_final_mul || workers < channels || channels < 3UL ||
        mpz_sgn(x) < 0 || mpz_sgn(y) < 0 ||
        minimum < (size_t)g_parallel_mul_min_bits ||
        maximum > 2U * minimum) {
        mpz_mul(out, x, y);
        return 0;
    }

    unsigned long split = (unsigned long)(maximum >> 1U);
    if (split == 0UL) {
        mpz_mul(out, x, y);
        return 0;
    }

    mpz_t x0, x1, y0, y1, sx, sy, p0, p1, p2, p3, sum;
    mpz_inits(x0, x1, y0, y1, sx, sy, p0, p1, p2, p3, sum, NULL);
    mpz_fdiv_r_2exp(x0, x, split); mpz_fdiv_q_2exp(x1, x, split);
    mpz_fdiv_r_2exp(y0, y, split); mpz_fdiv_q_2exp(y1, y, split);

    if (channels >= 4UL) {
PPF_OMP_PRAGMA(omp parallel sections num_threads(4))
        {
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(p0, x0, y0); }
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(p1, x0, y1); }
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(p2, x1, y0); }
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(p3, x1, y1); }
        }
        mpz_add(p1, p1, p2);
        mpz_mul_2exp(p1, p1, split);
        mpz_mul_2exp(p3, p3, 2UL * split);
        mpz_add(sum, p0, p1);
        mpz_add(out, sum, p3);
        channels = 4UL;
    } else {
        mpz_add(sx, x0, x1);
        mpz_add(sy, y0, y1);
PPF_OMP_PRAGMA(omp parallel sections num_threads(3))
        {
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(p0, x0, y0); }
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(p1, sx, sy); }
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(p2, x1, y1); }
        }
        mpz_sub(p1, p1, p0);
        mpz_sub(p1, p1, p2);
        mpz_mul_2exp(p1, p1, split);
        mpz_mul_2exp(p2, p2, 2UL * split);
        mpz_add(sum, p0, p1);
        mpz_add(out, sum, p2);
        channels = 3UL;
    }
    mpz_clears(x0, x1, y0, y1, sx, sy, p0, p1, p2, p3, sum, NULL);
    return (int)channels;
}

static char *scaled_to_decimal(const mpz_t value, unsigned long scale_bits, unsigned long digits) {
    mpz_t tenpow, numerator, rounded;
    mpz_inits(tenpow, numerator, rounded, NULL);
    mpz_ui_pow_ui(tenpow, 10UL, digits);
    mpz_mul(numerator, value, tenpow);
    mpz_round_shift(rounded, numerator, scale_bits);
    char *raw = mpz_get_str(NULL, 10, rounded);
    if (!raw) {
        mpz_clears(tenpow, numerator, rounded, NULL);
        return NULL;
    }
    int negative = raw[0] == '-';
    const char *digits_raw = raw + negative;
    size_t len = strlen(digits_raw);
    size_t integer_len = len > digits ? len - digits : 1UL;
    size_t total = (negative ? 1UL : 0UL) + integer_len +
                   (digits ? 1UL + digits : 0UL) + 1UL;
    char *out = (char *)PPF_CORE_MALLOC(total);
    if (!out) {
        ppf_core_free_gmp_string(raw);
        mpz_clears(tenpow, numerator, rounded, NULL);
        return NULL;
    }
    char *p = out;
    if (negative) *p++ = '-';
    if (len <= digits) {
        *p++ = '0';
        if (digits) *p++ = '.';
        size_t zeros = digits - len;
        memset(p, '0', zeros);
        p += zeros;
        memcpy(p, digits_raw, len);
        p += len;
    } else {
        size_t int_len = len - digits;
        memcpy(p, digits_raw, int_len);
        p += int_len;
        if (digits) {
            *p++ = '.';
            memcpy(p, digits_raw + int_len, digits);
            p += digits;
        }
    }
    *p = '\0';
    ppf_core_free_gmp_string(raw);
    mpz_clears(tenpow, numerator, rounded, NULL);
    return out;
}

static void bs_init(bs_result_t *r) {
    mpz_inits(r->p, r->q, r->t, NULL);
    r->q2 = 0UL;
}

static void bs_clear(bs_result_t *r) {
    mpz_clears(r->p, r->q, r->t, NULL);
    r->q2 = 0UL;
}

static void bs_set(bs_result_t *dst, const bs_result_t *src) {
    mpz_set(dst->p, src->p);
    mpz_set(dst->q, src->q);
    mpz_set(dst->t, src->t);
    dst->q2 = src->q2;
}

static unsigned long v2_ulong(unsigned long x) {
    if (x == 0UL) return 0UL;
#if defined(__GNUC__) || defined(__clang__)
    return (unsigned long)__builtin_ctzl(x);
#else
    unsigned long v = 0UL;
    while ((x & 1UL) == 0UL) { x >>= 1UL; ++v; }
    return v;
#endif
}

static unsigned long valuation_ulong(unsigned long x, unsigned long prime) {
    unsigned long v = 0UL;
    while (x > 0UL && x % prime == 0UL) { x /= prime; ++v; }
    return v;
}

/* Exact binary valuation of the packet denominator over [a,b). */
static unsigned long q2_interval(unsigned long a, unsigned long b) {
    if (!g_split_q2 || b <= 1UL || b <= a) return 0UL;
    unsigned long begin = a < 1UL ? 1UL : a;
    if (begin >= b) return 0UL;
    unsigned long count = b - begin;
    unsigned long valuation = CHUD_C3_OVER_24_V2 * count;
    unsigned long top = b - 1UL;
    for (unsigned long power = 2UL; power <= top;) {
        unsigned long multiples = top / power - (begin - 1UL) / power;
        if (multiples > (ULONG_MAX - valuation) / 3UL) return ULONG_MAX;
        valuation += 3UL * multiples;
        if (power > top / 2UL) break;
        power <<= 1UL;
    }
    return valuation;
}

static size_t q_full_bits(const bs_result_t *r) {
    size_t bits = mpz_sgn(r->q) == 0 ? 0U : mpz_sizeinbase(r->q, 2);
    if (!g_split_q2) return bits;
    if (SIZE_MAX - bits < (size_t)r->q2) return SIZE_MAX;
    return bits + (size_t)r->q2;
}

static int is_power_of_two_ul(unsigned long x) {
    return x > 0UL && (x & (x - 1UL)) == 0UL;
}

static unsigned long reduction_limit(unsigned long packets, int strict_final) {
    if (g_affine_root && !strict_final && packets >= 2UL &&
        is_power_of_two_ul(packets)) {
        return packets >> 1UL;
    }
    return packets;
}

static void mpz_mul_q_scaled(mpz_t out, const mpz_t left, const bs_result_t *right) {
    mpz_mul(out, left, right->q);
    if (g_split_q2 && right->q2 > 0UL) mpz_mul_2exp(out, out, right->q2);
}


static void factor_vec_init(factor_vec_t *v) {
    memset(v, 0, sizeof(*v));
}

static void factor_vec_clear(factor_vec_t *v) {
    PPF_CORE_FREE(v->entry);
    memset(v, 0, sizeof(*v));
}

static int factor_vec_reserve(factor_vec_t *v, size_t capacity) {
    if (capacity <= v->capacity) return 0;
    factor_entry_t *next = (factor_entry_t *)PPF_CORE_REALLOC(v->entry, capacity * sizeof(*next));
    if (!next) return -1;
    v->entry = next;
    v->capacity = capacity;
    return 0;
}

static int factor_vec_append(factor_vec_t *v, uint32_t prime_index, uint64_t exponent) {
    if (exponent == 0U) return 0;
    if (exponent > UINT32_MAX) return -1;
    if (v->size == v->capacity) {
        size_t next = v->capacity ? v->capacity * 2U : 16U;
        if (next < v->size + 1U) next = v->size + 1U;
        if (factor_vec_reserve(v, next) != 0) return -1;
    }
    v->entry[v->size].prime_index = prime_index;
    v->entry[v->size].exponent = (uint32_t)exponent;
    ++v->size;
    return 0;
}

static int factor_vec_copy(factor_vec_t *out, const factor_vec_t *in) {
    if (factor_vec_reserve(out, in->size) != 0) return -1;
    if (in->size > 0U) {
        memcpy(out->entry, in->entry, in->size * sizeof(*in->entry));
    }
    out->size = in->size;
    return 0;
}

static double factor_vec_log2(const factor_vec_t *v, const factor_sieve_t *sieve) {
    double bits = 0.0;
    for (size_t i = 0U; i < v->size; ++i) {
        bits += (double)v->entry[i].exponent *
                sieve->log2_prime[v->entry[i].prime_index];
    }
    return bits;
}

static int compare_u32(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a;
    uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

static int factor_sieve_push_prime(factor_sieve_t *sieve, uint32_t prime) {
    if (sieve->prime_count == sieve->prime_capacity) {
        size_t next = sieve->prime_capacity ? sieve->prime_capacity * 2U : 1024U;
        uint32_t *new_primes = (uint32_t *)PPF_CORE_MALLOC(next * sizeof(*new_primes));
        double *new_logs = (double *)PPF_CORE_MALLOC(next * sizeof(*new_logs));
        if (!new_primes || !new_logs) {
            PPF_CORE_FREE(new_primes);
            PPF_CORE_FREE(new_logs);
            return -1;
        }
        if (sieve->prime_count > 0U) {
            memcpy(new_primes, sieve->primes,
                   sieve->prime_count * sizeof(*new_primes));
            memcpy(new_logs, sieve->log2_prime,
                   sieve->prime_count * sizeof(*new_logs));
        }
        PPF_CORE_FREE(sieve->primes);
        PPF_CORE_FREE(sieve->log2_prime);
        sieve->primes = new_primes;
        sieve->log2_prime = new_logs;
        sieve->prime_capacity = next;
    }
    size_t index = sieve->prime_count++;
    sieve->primes[index] = prime;
    sieve->log2_prime[index] = log2((double)prime);
    sieve->prime_pos[prime] = (uint32_t)(index + 1U);
    return 0;
}

static int factor_sieve_init(
    factor_sieve_t *sieve,
    unsigned long terms,
    unsigned long requested_prime_bound
) {
    memset(sieve, 0, sizeof(*sieve));
    unsigned long max_factor = terms <= 1UL ? 2UL : 6UL * (terms - 1UL) - 1UL;
    if (max_factor > (unsigned long)UINT32_MAX) {
        fprintf(stderr, "p-adic sieve limit exceeds 32-bit implementation\n");
        return -1;
    }
    unsigned long prime_bound = requested_prime_bound;
    if (prime_bound == 0UL || prime_bound > max_factor) prime_bound = max_factor;
    if (prime_bound < 29UL) prime_bound = 29UL;
    sieve->limit = (uint32_t)prime_bound;
    sieve->max_factor = (uint32_t)max_factor;
    sieve->complete = prime_bound >= max_factor;
    size_t count = (size_t)sieve->limit + 1U;
    sieve->spf = (uint32_t *)PPF_CORE_CALLOC(count, sizeof(*sieve->spf));
    sieve->prime_pos = (uint32_t *)PPF_CORE_CALLOC(count, sizeof(*sieve->prime_pos));
    if (!sieve->spf || !sieve->prime_pos) {
        PPF_CORE_FREE(sieve->spf);
        PPF_CORE_FREE(sieve->prime_pos);
        memset(sieve, 0, sizeof(*sieve));
        return -1;
    }

    for (uint32_t n = 2U; n <= sieve->limit; ++n) {
        if (sieve->spf[n] == 0U) {
            sieve->spf[n] = n;
            if (factor_sieve_push_prime(sieve, n) != 0) {
                PPF_CORE_FREE(sieve->spf);
                PPF_CORE_FREE(sieve->prime_pos);
                PPF_CORE_FREE(sieve->primes);
                PPF_CORE_FREE(sieve->log2_prime);
                memset(sieve, 0, sizeof(*sieve));
                return -1;
            }
            if ((uint64_t)n * (uint64_t)n <= (uint64_t)sieve->limit) {
                uint64_t start = (uint64_t)n * (uint64_t)n;
                for (uint64_t m = start; m <= sieve->limit; m += n) {
                    if (sieve->spf[(uint32_t)m] == 0U) sieve->spf[(uint32_t)m] = n;
                }
            }
        }
    }
    return 0;
}

static void factor_sieve_clear(factor_sieve_t *sieve) {
    PPF_CORE_FREE(sieve->spf);
    PPF_CORE_FREE(sieve->prime_pos);
    PPF_CORE_FREE(sieve->primes);
    PPF_CORE_FREE(sieve->log2_prime);
    memset(sieve, 0, sizeof(*sieve));
}

static int factor_build_scratch_init(
    factor_build_scratch_t *scratch,
    const factor_sieve_t *sieve
) {
    memset(scratch, 0, sizeof(*scratch));
    size_t count = sieve->prime_count ? sieve->prime_count : 1U;
    scratch->exp_p = (uint64_t *)PPF_CORE_CALLOC(count, sizeof(*scratch->exp_p));
    scratch->exp_q = (uint64_t *)PPF_CORE_CALLOC(count, sizeof(*scratch->exp_q));
    scratch->touched_p = (uint32_t *)PPF_CORE_MALLOC(count * sizeof(*scratch->touched_p));
    scratch->touched_q = (uint32_t *)PPF_CORE_MALLOC(count * sizeof(*scratch->touched_q));
    if (!scratch->exp_p || !scratch->exp_q ||
        !scratch->touched_p || !scratch->touched_q) {
        PPF_CORE_FREE(scratch->exp_p);
        PPF_CORE_FREE(scratch->exp_q);
        PPF_CORE_FREE(scratch->touched_p);
        PPF_CORE_FREE(scratch->touched_q);
        memset(scratch, 0, sizeof(*scratch));
        return -1;
    }
    return 0;
}

static void factor_build_scratch_clear(factor_build_scratch_t *scratch) {
    PPF_CORE_FREE(scratch->exp_p);
    PPF_CORE_FREE(scratch->exp_q);
    PPF_CORE_FREE(scratch->touched_p);
    PPF_CORE_FREE(scratch->touched_q);
    memset(scratch, 0, sizeof(*scratch));
}

static void factor_touch_add(
    uint64_t *exponents,
    uint32_t *touched,
    size_t *touched_count,
    uint32_t prime_index,
    uint64_t amount
) {
    if (amount == 0U) return;
    if (exponents[prime_index] == 0U) {
        touched[(*touched_count)++] = prime_index;
    }
    exponents[prime_index] += amount;
}

static int factor_integer_add(
    uint32_t value,
    uint64_t multiplier,
    uint64_t *exponents,
    uint32_t *touched,
    size_t *touched_count,
    const factor_sieve_t *sieve
) {
    if (value <= 1U || multiplier == 0U) return 0;
    if (value > sieve->limit) return -1;
    while (value > 1U) {
        uint32_t prime = sieve->spf[value];
        if (prime == 0U) prime = value;
        uint64_t exponent = 0U;
        do {
            value /= prime;
            ++exponent;
        } while (value > 1U && value % prime == 0U);
        uint32_t position = sieve->prime_pos[prime];
        if (position == 0U) return -1;
        factor_touch_add(exponents, touched, touched_count,
                         position - 1U, multiplier * exponent);
    }
    return 0;
}

static int factor_vec_from_touched(
    factor_vec_t *out,
    uint64_t *exponents,
    uint32_t *touched,
    size_t touched_count
) {
    qsort(touched, touched_count, sizeof(*touched), compare_u32);
    if (factor_vec_reserve(out, touched_count) != 0) return -1;
    out->size = 0U;
    for (size_t i = 0U; i < touched_count; ++i) {
        uint32_t index = touched[i];
        uint64_t exponent = exponents[index];
        if (exponent > 0U) {
            if (exponent > UINT32_MAX) return -1;
            out->entry[out->size].prime_index = index;
            out->entry[out->size].exponent = (uint32_t)exponent;
            ++out->size;
        }
        exponents[index] = 0U;
    }
    return 0;
}

static int factor_packet_metadata(
    factor_meta_t *meta,
    unsigned long a,
    unsigned long b,
    const factor_sieve_t *sieve,
    factor_build_scratch_t *scratch
) {
    scratch->touched_p_count = 0U;
    scratch->touched_q_count = 0U;
    unsigned long begin = a < 1UL ? 1UL : a;
    for (unsigned long k = begin; k < b; ++k) {
        uint64_t v1 = 2ULL * (uint64_t)k - 1ULL;
        uint64_t v2 = 6ULL * (uint64_t)k - 5ULL;
        uint64_t v3 = 6ULL * (uint64_t)k - 1ULL;
        if (v3 > sieve->limit || k > sieve->limit) return -1;
        if (factor_integer_add((uint32_t)v1, 1U, scratch->exp_p,
                               scratch->touched_p, &scratch->touched_p_count,
                               sieve) != 0 ||
            factor_integer_add((uint32_t)v2, 1U, scratch->exp_p,
                               scratch->touched_p, &scratch->touched_p_count,
                               sieve) != 0 ||
            factor_integer_add((uint32_t)v3, 1U, scratch->exp_p,
                               scratch->touched_p, &scratch->touched_p_count,
                               sieve) != 0 ||
            factor_integer_add((uint32_t)(g_split_q2 ? (k >> v2_ulong(k)) : k),
                               3U, scratch->exp_q,
                               scratch->touched_q, &scratch->touched_q_count,
                               sieve) != 0) {
            return -1;
        }
    }

    uint64_t width = b > begin ? (uint64_t)(b - begin) : 0U;
    static const uint32_t constant_prime[] = {2U, 3U, 5U, 23U, 29U};
    static const uint32_t constant_exponent[] = {15U, 2U, 3U, 3U, 3U};
    for (size_t i = 0U; i < sizeof(constant_prime) / sizeof(constant_prime[0]); ++i) {
        if (g_split_q2 && constant_prime[i] == 2U) continue;
        uint32_t position = sieve->prime_pos[constant_prime[i]];
        if (position == 0U) return -1;
        factor_touch_add(scratch->exp_q, scratch->touched_q,
                         &scratch->touched_q_count, position - 1U,
                         width * (uint64_t)constant_exponent[i]);
    }

    if (factor_vec_from_touched(&meta->p, scratch->exp_p, scratch->touched_p,
                                scratch->touched_p_count) != 0 ||
        factor_vec_from_touched(&meta->q, scratch->exp_q, scratch->touched_q,
                                scratch->touched_q_count) != 0) {
        return -1;
    }
    return 0;
}


static void factor_meta_init(factor_meta_t *meta);
static void factor_meta_clear(factor_meta_t *meta);

static int add_residue_channel(
    uint64_t *counts,
    const unsigned long *bounds,
    unsigned long packets,
    unsigned long terms,
    uint64_t modulus,
    uint64_t residue,
    uint64_t multiplier
) {
    if (modulus == 0U || multiplier == 0U || terms <= 1UL) return 0;
    residue %= modulus;
    uint64_t first = residue;
    if (first < 1U) {
        uint64_t steps = (1U - first + modulus - 1U) / modulus;
        first += steps * modulus;
    }
    if (first >= (uint64_t)terms) return 0;
    unsigned long packet = 0UL;
    while (packet + 1UL < packets &&
           (uint64_t)bounds[packet + 1UL] <= first) ++packet;
    for (uint64_t k = first; k < (uint64_t)terms; k += modulus) {
        while (packet + 1UL < packets &&
               k >= (uint64_t)bounds[packet + 1UL]) ++packet;
        if (packet >= packets) return -1;
        counts[packet] += multiplier;
        if (UINT64_MAX - k < modulus) break;
    }
    return 0;
}

static uint32_t constant_q_exponent(uint32_t prime) {
    switch (prime) {
        case 2U: return 15U;
        case 3U: return 2U;
        case 5U: return 3U;
        case 23U: return 3U;
        case 29U: return 3U;
        default: return 0U;
    }
}

static int factor_channel_metadata_all(
    factor_meta_t *metadata,
    const unsigned long *bounds,
    unsigned long packets,
    unsigned long terms,
    const factor_sieve_t *sieve,
    unsigned long workers
) {
    unsigned long thread_count = workers;
    if (thread_count < 1UL) thread_count = 1UL;
    if (thread_count > sieve->prime_count) thread_count = (unsigned long)sieve->prime_count;
    if (thread_count < 1UL) thread_count = 1UL;

    factor_meta_t *local = (factor_meta_t *)PPF_CORE_CALLOC(
        thread_count * packets, sizeof(*local));
    if (!local) return -1;
    for (unsigned long i = 0UL; i < thread_count * packets; ++i) {
        factor_meta_init(&local[i]);
    }

    int failed = 0;
    uint64_t max_factor = terms <= 1UL ? 2U : 6U * ((uint64_t)terms - 1U) - 1U;
PPF_OMP_PRAGMA(omp parallel num_threads(thread_count) shared(failed, local, max_factor))
    {
        unsigned long tid = (unsigned long)omp_get_thread_num();
        size_t prime_begin = (sieve->prime_count * tid) / thread_count;
        size_t prime_end = (sieve->prime_count * (tid + 1UL)) / thread_count;
        uint64_t *count_p = (uint64_t *)PPF_CORE_CALLOC(packets, sizeof(*count_p));
        uint64_t *count_q = (uint64_t *)PPF_CORE_CALLOC(packets, sizeof(*count_q));
        if (!count_p || !count_q) {
PPF_OMP_PRAGMA(omp atomic write)
            failed = 1;
        } else {
            for (size_t prime_index = prime_begin;
                 prime_index < prime_end && !failed; ++prime_index) {
                memset(count_p, 0, packets * sizeof(*count_p));
                memset(count_q, 0, packets * sizeof(*count_q));
                uint32_t prime = sieve->primes[prime_index];

                uint32_t constant_exp = constant_q_exponent(prime);
                if (g_split_q2 && prime == 2U) constant_exp = 0U;
                if (constant_exp > 0U) {
                    for (unsigned long packet = 0UL; packet < packets; ++packet) {
                        unsigned long begin = bounds[packet] < 1UL ? 1UL : bounds[packet];
                        unsigned long width = bounds[packet + 1UL] > begin
                            ? bounds[packet + 1UL] - begin : 0UL;
                        count_q[packet] += (uint64_t)constant_exp * (uint64_t)width;
                    }
                }

                uint64_t power = (uint64_t)prime;
                if (!(g_split_q2 && prime == 2U)) {
                    while (power < (uint64_t)terms) {
                        if (add_residue_channel(count_q, bounds, packets, terms,
                                                power, 0U, 3U) != 0) {
PPF_OMP_PRAGMA(omp atomic write)
                            failed = 1;
                            break;
                        }
                        if (power > ((uint64_t)terms - 1U) / prime) break;
                        power *= prime;
                    }
                }

                if (prime >= 3U) {
                    power = (uint64_t)prime;
                    while (power <= max_factor) {
                        uint64_t inverse2 = (power + 1U) / 2U;
                        if (add_residue_channel(count_p, bounds, packets, terms,
                                                power, inverse2, 1U) != 0) {
PPF_OMP_PRAGMA(omp atomic write)
                            failed = 1;
                            break;
                        }
                        if (prime >= 5U) {
                            uint64_t inverse6 = power % 6U == 1U
                                ? (1U + 5U * power) / 6U
                                : (1U + power) / 6U;
                            uint64_t root_6k_5 = (5U * inverse6) % power;
                            if (add_residue_channel(count_p, bounds, packets, terms,
                                                    power, root_6k_5, 1U) != 0 ||
                                add_residue_channel(count_p, bounds, packets, terms,
                                                    power, inverse6, 1U) != 0) {
PPF_OMP_PRAGMA(omp atomic write)
                                failed = 1;
                                break;
                            }
                        }
                        if (power > (uint64_t)sieve->limit / prime) break;
                        power *= prime;
                    }
                }

                for (unsigned long packet = 0UL; packet < packets; ++packet) {
                    factor_meta_t *dst = &local[tid * packets + packet];
                    if ((count_p[packet] > 0U &&
                         factor_vec_append(&dst->p, (uint32_t)prime_index,
                                           count_p[packet]) != 0) ||
                        (count_q[packet] > 0U &&
                         factor_vec_append(&dst->q, (uint32_t)prime_index,
                                           count_q[packet]) != 0)) {
PPF_OMP_PRAGMA(omp atomic write)
                        failed = 1;
                        break;
                    }
                }
            }
        }
        PPF_CORE_FREE(count_p);
        PPF_CORE_FREE(count_q);
    }

    if (!failed) {
        for (unsigned long packet = 0UL; packet < packets; ++packet) {
            size_t total_p = 0U, total_q = 0U;
            for (unsigned long tid = 0UL; tid < thread_count; ++tid) {
                factor_meta_t *src = &local[tid * packets + packet];
                total_p += src->p.size;
                total_q += src->q.size;
            }
            if (factor_vec_reserve(&metadata[packet].p, total_p) != 0 ||
                factor_vec_reserve(&metadata[packet].q, total_q) != 0) {
                failed = 1;
                break;
            }
            metadata[packet].p.size = 0U;
            metadata[packet].q.size = 0U;
            for (unsigned long tid = 0UL; tid < thread_count; ++tid) {
                factor_meta_t *src = &local[tid * packets + packet];
                if (src->p.size > 0U) {
                    memcpy(metadata[packet].p.entry + metadata[packet].p.size,
                           src->p.entry, src->p.size * sizeof(*src->p.entry));
                    metadata[packet].p.size += src->p.size;
                }
                if (src->q.size > 0U) {
                    memcpy(metadata[packet].q.entry + metadata[packet].q.size,
                           src->q.entry, src->q.size * sizeof(*src->q.entry));
                    metadata[packet].q.size += src->q.size;
                }
            }
        }
    }

    for (unsigned long i = 0UL; i < thread_count * packets; ++i) {
        factor_meta_clear(&local[i]);
    }
    PPF_CORE_FREE(local);
    return failed ? -1 : 0;
}

static void factor_meta_init(factor_meta_t *meta) {
    factor_vec_init(&meta->p);
    factor_vec_init(&meta->q);
}

static void factor_meta_clear(factor_meta_t *meta) {
    factor_vec_clear(&meta->p);
    factor_vec_clear(&meta->q);
}

static int factor_vec_merge_add(factor_vec_t *left, factor_vec_t *right) {
    size_t left_size = left->size;
    size_t right_size = right->size;
    size_t capacity = left_size + right_size;
    if (factor_vec_reserve(left, capacity) != 0) return -1;

    size_t i = left_size;
    size_t j = right_size;
    size_t out = capacity;
    while (i > 0U || j > 0U) {
        if (j == 0U ||
            (i > 0U && left->entry[i - 1U].prime_index >
                         right->entry[j - 1U].prime_index)) {
            left->entry[--out] = left->entry[--i];
        } else if (i == 0U ||
                   right->entry[j - 1U].prime_index >
                       left->entry[i - 1U].prime_index) {
            left->entry[--out] = right->entry[--j];
        } else {
            uint32_t prime_index = left->entry[i - 1U].prime_index;
            uint64_t exponent = (uint64_t)left->entry[i - 1U].exponent +
                                (uint64_t)right->entry[j - 1U].exponent;
            if (exponent > UINT32_MAX) return -1;
            --i;
            --j;
            left->entry[--out].prime_index = prime_index;
            left->entry[out].exponent = (uint32_t)exponent;
        }
    }
    size_t merged_size = capacity - out;
    if (out > 0U && merged_size > 0U) {
        memmove(left->entry, left->entry + out,
                merged_size * sizeof(*left->entry));
    }
    left->size = merged_size;
    factor_vec_clear(right);
    return 0;
}

static int factor_product_ctx_init(factor_product_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    mpz_init(ctx->temp);
    for (size_t i = 0U; i < 64U; ++i) mpz_init(ctx->buckets[i]);
    return 0;
}

static void factor_product_ctx_clear(factor_product_ctx_t *ctx) {
    mpz_clear(ctx->temp);
    for (size_t i = 0U; i < 64U; ++i) mpz_clear(ctx->buckets[i]);
    memset(ctx, 0, sizeof(*ctx));
}

static int factor_product_build(
    mpz_t out,
    const factor_vec_t *factors,
    const factor_sieve_t *sieve,
    factor_product_ctx_t *ctx
) {
    memset(ctx->used, 0, sizeof(ctx->used));
    for (size_t i = 0U; i < factors->size; ++i) {
        uint64_t exponent = factors->entry[i].exponent;
        if (exponent > (uint64_t)ULONG_MAX) return -1;
        uint32_t prime = sieve->primes[factors->entry[i].prime_index];
        mpz_ui_pow_ui(ctx->temp, (unsigned long)prime, (unsigned long)exponent);
        size_t level = 0U;
        while (level < 64U && ctx->used[level]) {
            mpz_mul(ctx->temp, ctx->temp, ctx->buckets[level]);
            ctx->used[level] = 0U;
            ++level;
        }
        if (level >= 64U) return -1;
        mpz_swap(ctx->temp, ctx->buckets[level]);
        ctx->used[level] = 1U;
    }
    mpz_set_ui(out, 1UL);
    for (size_t level = 0U; level < 64U; ++level) {
        if (ctx->used[level]) mpz_mul(out, out, ctx->buckets[level]);
    }
    return 0;
}

static int factor_vec_cancel(
    factor_vec_t *left,
    factor_vec_t *right,
    factor_vec_t *cancelled,
    const factor_sieve_t *sieve,
    factor_stats_t *stats
) {
    cancelled->size = 0U;
    size_t i = 0U, j = 0U, write_left = 0U, write_right = 0U;
    while (i < left->size || j < right->size) {
        if (j >= right->size ||
            (i < left->size &&
             left->entry[i].prime_index < right->entry[j].prime_index)) {
            left->entry[write_left++] = left->entry[i++];
        } else if (i >= left->size ||
                   right->entry[j].prime_index < left->entry[i].prime_index) {
            right->entry[write_right++] = right->entry[j++];
        } else {
            uint32_t index = left->entry[i].prime_index;
            uint32_t delta = left->entry[i].exponent < right->entry[j].exponent
                ? left->entry[i].exponent : right->entry[j].exponent;
            uint32_t remain_left = left->entry[i].exponent - delta;
            uint32_t remain_right = right->entry[j].exponent - delta;
            if (delta > 0U) {
                if (factor_vec_append(cancelled, index, delta) != 0) return -1;
                if (stats) {
                    stats->removed_bits += (double)delta * sieve->log2_prime[index];
                    ++stats->cancellation_events;
                    stats->cancelled_prime_powers += delta;
                }
            }
            if (remain_left > 0U) {
                left->entry[write_left].prime_index = index;
                left->entry[write_left].exponent = remain_left;
                ++write_left;
            }
            if (remain_right > 0U) {
                right->entry[write_right].prime_index = index;
                right->entry[write_right].exponent = remain_right;
                ++write_right;
            }
            ++i;
            ++j;
        }
    }
    left->size = write_left;
    right->size = write_right;
    return 0;
}


static int factor_vec_subtract_exact(
    factor_vec_t *target,
    const factor_vec_t *subtract
) {
    size_t i = 0U, j = 0U, write = 0U;
    while (i < target->size) {
        factor_entry_t entry = target->entry[i++];
        while (j < subtract->size &&
               subtract->entry[j].prime_index < entry.prime_index) {
            return -1;
        }
        if (j < subtract->size &&
            subtract->entry[j].prime_index == entry.prime_index) {
            if (subtract->entry[j].exponent > entry.exponent) return -1;
            entry.exponent -= subtract->entry[j].exponent;
            ++j;
        }
        if (entry.exponent > 0U) target->entry[write++] = entry;
    }
    if (j != subtract->size) return -1;
    target->size = write;
    return 0;
}


/* Width-two polyphase cancellation is supported on exactly
   {3,5,7,11,23,29}.  For r=k+1, the variable part follows from
   gcd(2k-1,r)|3, gcd(6k-5,r)|11, and gcd(6k-1,r)|7; the remaining primes
   come from the fixed odd denominator. */
static const unsigned long g_pair_primes[6] = {3UL, 5UL, 7UL, 11UL, 23UL, 29UL};

static void pair_phase_accumulate(
    unsigned long a,
    unsigned long b,
    uint64_t exponent[6],
    uint64_t *events
) {
    unsigned long width = b - a;
    if (width <= 1UL) return;
    if (width == 2UL) {
        unsigned long k = a;
        unsigned long r = a + 1UL;
        if (k == 0UL) return;
        int nontrivial = 0;
        for (size_t i = 0U; i < 6U; ++i) {
            unsigned long prime = g_pair_primes[i];
            unsigned long vp =
                valuation_ulong(2UL * k - 1UL, prime) +
                valuation_ulong(6UL * k - 5UL, prime) +
                valuation_ulong(6UL * k - 1UL, prime);
            unsigned long vq = 3UL * valuation_ulong(r, prime) +
                (unsigned long)constant_q_exponent((uint32_t)prime);
            unsigned long delta = vp < vq ? vp : vq;
            exponent[i] += (uint64_t)delta;
            if (delta > 0UL) nontrivial = 1;
        }
        if (nontrivial) ++*events;
        return;
    }
    unsigned long m = a + width / 2UL;
    pair_phase_accumulate(a, m, exponent, events);
    pair_phase_accumulate(m, b, exponent, events);
}

static int factor_pair_phase_adjust(
    factor_meta_t *meta,
    unsigned long a,
    unsigned long b,
    const factor_sieve_t *sieve,
    factor_stats_t *stats
) {
    if (!g_pair_phase || !g_directed_cancel || b - a < 2UL) return 0;
    uint64_t exponent[6] = {0U, 0U, 0U, 0U, 0U, 0U};
    uint64_t events = 0U;
    pair_phase_accumulate(a, b, exponent, &events);

    factor_vec_t cancelled;
    factor_vec_init(&cancelled);
    for (size_t i = 0U; i < 6U; ++i) {
        if (exponent[i] == 0U) continue;
        unsigned long prime = g_pair_primes[i];
        if (prime > sieve->limit || sieve->prime_pos[prime] == 0U ||
            exponent[i] > UINT32_MAX) {
            factor_vec_clear(&cancelled);
            return -1;
        }
        uint32_t index = sieve->prime_pos[prime] - 1U;
        if (factor_vec_append(&cancelled, index, exponent[i]) != 0) {
            factor_vec_clear(&cancelled);
            return -1;
        }
        if (stats) {
            stats->removed_bits += (double)exponent[i] * sieve->log2_prime[index];
            stats->cancelled_prime_powers += exponent[i];
        }
    }
    if (cancelled.size > 0U &&
        (factor_vec_subtract_exact(&meta->p, &cancelled) != 0 ||
         factor_vec_subtract_exact(&meta->q, &cancelled) != 0)) {
        factor_vec_clear(&cancelled);
        return -1;
    }
    if (stats) stats->cancellation_events += events;
    factor_vec_clear(&cancelled);
    return 0;
}

/* Remove a common projective small-prime scale revealed by T.  For a full
   packet the divisor must divide P,Q,T; for a Q/T-only right-spine packet it
   need only divide Q,T.  The exact p-adic valuation of T is obtained with
   mpz_remove, so this is a correctness path rather than a heuristic. */
static int factor_unit_projective_normalize(
    bs_result_t *node,
    factor_meta_t *meta,
    int qt_only,
    const factor_sieve_t *sieve,
    factor_product_ctx_t *product_ctx,
    factor_vec_t *primitive,
    mpz_t unit_quotient,
    mpz_t prime_z,
    factor_stats_t *stats
) {
    primitive->size = 0U;
    if (g_unit_prime_bound < 2UL) return 0;

    if (qt_only) {
        for (size_t iq = 0U; iq < meta->q.size; ++iq) {
            factor_entry_t eq = meta->q.entry[iq];
            uint32_t prime = sieve->primes[eq.prime_index];
            if ((unsigned long)prime > g_unit_prime_bound) break;
            mpz_set_ui(prime_z, (unsigned long)prime);
            unsigned long vt = mpz_remove(unit_quotient, node->t, prime_z);
            uint32_t delta = eq.exponent < vt ? eq.exponent : (uint32_t)vt;
            if (delta > 0U &&
                factor_vec_append(primitive, eq.prime_index, delta) != 0) {
                return -1;
            }
        }
    } else {
        size_t ip = 0U, iq = 0U;
        while (ip < meta->p.size && iq < meta->q.size) {
            factor_entry_t ep = meta->p.entry[ip];
            factor_entry_t eq = meta->q.entry[iq];
            if (ep.prime_index < eq.prime_index) { ++ip; continue; }
            if (eq.prime_index < ep.prime_index) { ++iq; continue; }
            uint32_t prime = sieve->primes[ep.prime_index];
            if ((unsigned long)prime > g_unit_prime_bound) break;
            uint32_t cap = ep.exponent < eq.exponent ? ep.exponent : eq.exponent;
            mpz_set_ui(prime_z, (unsigned long)prime);
            unsigned long vt = mpz_remove(unit_quotient, node->t, prime_z);
            uint32_t delta = cap < vt ? cap : (uint32_t)vt;
            if (delta > 0U &&
                factor_vec_append(primitive, ep.prime_index, delta) != 0) {
                return -1;
            }
            ++ip;
            ++iq;
        }
    }

    if (primitive->size == 0U) return 0;
    if (factor_product_build(product_ctx->temp, primitive, sieve, product_ctx) != 0) {
        return -1;
    }
    if (!qt_only) mpz_divexact(node->p, node->p, product_ctx->temp);
    mpz_divexact(node->q, node->q, product_ctx->temp);
    mpz_divexact(node->t, node->t, product_ctx->temp);
    if (!qt_only && factor_vec_subtract_exact(&meta->p, primitive) != 0) return -1;
    if (factor_vec_subtract_exact(&meta->q, primitive) != 0) return -1;
    if (stats) {
        for (size_t i = 0U; i < primitive->size; ++i) {
            uint32_t index = primitive->entry[i].prime_index;
            uint32_t exponent = primitive->entry[i].exponent;
            stats->removed_bits += (double)exponent * sieve->log2_prime[index];
            ++stats->cancellation_events;
            stats->cancelled_prime_powers += exponent;
        }
    }
    return 0;
}

static int factor_directed_cancel(
    bs_result_t *left,
    bs_result_t *right,
    factor_meta_t *left_meta,
    factor_meta_t *right_meta,
    const factor_sieve_t *sieve,
    factor_product_ctx_t *product_ctx,
    factor_vec_t *cancelled,
    factor_stats_t *stats
) {
    if (!g_directed_cancel) return 0;
    if (factor_vec_cancel(&left_meta->p, &right_meta->q, cancelled,
                          sieve, stats) != 0) return -1;
    if (cancelled->size == 0U) return 0;
    if (factor_product_build(product_ctx->temp, cancelled, sieve, product_ctx) != 0) {
        return -1;
    }
    mpz_divexact(left->p, left->p, product_ctx->temp);
    mpz_divexact(right->q, right->q, product_ctx->temp);
    return 0;
}

/* Prepare the exact p-adic cancellation scalar without applying the two
   large exact divisions.  Channel mode applies those divisions as separate
   OpenMP jobs so both operands are reduced concurrently. */
static int factor_prepare_directed_cancel(
    mpz_t cancel,
    factor_meta_t *left_meta,
    factor_meta_t *right_meta,
    const factor_sieve_t *sieve,
    factor_product_ctx_t *product_ctx,
    factor_vec_t *cancelled,
    factor_stats_t *stats
) {
    mpz_set_ui(cancel, 1UL);
    if (!g_directed_cancel) return 0;
    if (factor_vec_cancel(&left_meta->p, &right_meta->q, cancelled,
                          sieve, stats) != 0) return -1;
    if (cancelled->size == 0U) return 0;
    return factor_product_build(cancel, cancelled, sieve, product_ctx);
}

static void chud_leaf(bs_result_t *out, unsigned long k) {
    if (k == 0UL) {
        mpz_set_ui(out->p, 1UL);
        mpz_set_ui(out->q, 1UL);
        mpz_set_ui(out->t, CHUD_A);
        out->q2 = 0UL;
        return;
    }

#if defined(__SIZEOF_INT128__) && ULONG_MAX >= UINT64_MAX
    __uint128_t kk = (__uint128_t)k;
    __uint128_t p128 = (6U * kk - 5U) * (2U * kk - 1U) * (6U * kk - 1U);
    if (p128 <= (__uint128_t)ULONG_MAX) {
        mpz_set_ui(out->p, (unsigned long)p128);
    } else {
        mpz_set_ui(out->p, 6UL * k - 5UL);
        mpz_mul_ui(out->p, out->p, 2UL * k - 1UL);
        mpz_mul_ui(out->p, out->p, 6UL * k - 1UL);
    }
#else
    mpz_set_ui(out->p, 6UL * k - 5UL);
    mpz_mul_ui(out->p, out->p, 2UL * k - 1UL);
    mpz_mul_ui(out->p, out->p, 6UL * k - 1UL);
#endif

    if (!g_split_q2) {
        mpz_set_ui(out->q, k);
        mpz_mul_ui(out->q, out->q, k);
        mpz_mul_ui(out->q, out->q, k);
        mpz_mul(out->q, out->q, g_chud_c3_over_24);
        out->q2 = 0UL;
    } else {
        unsigned long vk = v2_ulong(k);
        unsigned long ko = k >> vk;
        mpz_set_ui(out->q, ko);
        mpz_mul_ui(out->q, out->q, ko);
        mpz_mul_ui(out->q, out->q, ko);
        mpz_mul_ui(out->q, out->q, (unsigned long)CHUD_C3_OVER_24_ODD_U64);
        out->q2 = 3UL * vk + CHUD_C3_OVER_24_V2;
    }

    uint64_t linear = (uint64_t)CHUD_A + (uint64_t)CHUD_B * (uint64_t)k;
#if ULONG_MAX >= UINT64_MAX
    mpz_mul_ui(out->t, out->p, (unsigned long)linear);
#else
    mpz_t linear_z;
    mpz_init(linear_z);
    mpz_set_u64(linear_z, linear);
    mpz_mul(out->t, out->p, linear_z);
    mpz_clear(linear_z);
#endif
    if (k & 1UL) mpz_neg(out->t, out->t);
}

/* If g=gcd(P_L,Q_R), dividing P_L and Q_R by g divides the reconstructed
   P,Q,T by the same g and leaves T/Q invariant exactly. */
static void directed_cancel(bs_result_t *left, bs_result_t *right, mpz_t gcd_scratch) {
    mpz_gcd(gcd_scratch, left->p, right->q);
    if (mpz_cmp_ui(gcd_scratch, 1UL) > 0) {
        mpz_divexact(left->p, left->p, gcd_scratch);
        mpz_divexact(right->q, right->q, gcd_scratch);
    }
}


/* Closed-form width-two divisor.  This is identical to gcd(P_k,Q_{k+1})
   but avoids invoking a general multiprecision GCD at every pair leaf. */
static void pair_directed_cancel(
    bs_result_t *left,
    bs_result_t *right,
    unsigned long k,
    mpz_t divisor
) {
    if (k == 0UL) return;
    mpz_set_ui(divisor, 1UL);
    unsigned long r = k + 1UL;
    for (size_t i = 0U; i < 6U; ++i) {
        unsigned long prime = g_pair_primes[i];
        unsigned long vp =
            valuation_ulong(2UL * k - 1UL, prime) +
            valuation_ulong(6UL * k - 5UL, prime) +
            valuation_ulong(6UL * k - 1UL, prime);
        unsigned long vq = 3UL * valuation_ulong(r, prime) +
            (unsigned long)constant_q_exponent((uint32_t)prime);
        unsigned long delta = vp < vq ? vp : vq;
        while (delta-- > 0UL) mpz_mul_ui(divisor, divisor, prime);
    }
    if (mpz_cmp_ui(divisor, 1UL) > 0) {
        mpz_divexact(left->p, left->p, divisor);
        mpz_divexact(right->q, right->q, divisor);
    }
}

static void chud_combine_inplace(bs_result_t *left, const bs_result_t *right, mpz_t scratch_t) {
    mpz_mul_q_scaled(scratch_t, left->t, right);
    mpz_addmul(scratch_t, left->p, right->t);
    mpz_mul(left->p, left->p, right->p);
    mpz_mul(left->q, left->q, right->q);
    if (g_split_q2) left->q2 += right->q2;
    mpz_swap(left->t, scratch_t);
}

/* The final value uses only Q and T. Along the right spine of the
   reduction tree, the parent P is never consumed: its parent is itself
   another Q/T-only node.  This exact combine therefore omits P_L P_R. */
static void chud_combine_qt_only(bs_result_t *left, const bs_result_t *right, mpz_t scratch_t) {
    mpz_mul_q_scaled(scratch_t, left->t, right);
    mpz_addmul(scratch_t, left->p, right->t);
    mpz_mul(left->q, left->q, right->q);
    if (g_split_q2) left->q2 += right->q2;
    mpz_swap(left->t, scratch_t);
    mpz_set_ui(left->p, 0UL);
}


/* Format an integer already scaled by 10^digits. */
static char *decimal_integer_to_string(const mpz_t value, unsigned long digits) {
    char *raw = mpz_get_str(NULL, 10, value);
    if (!raw) return NULL;
    int negative = raw[0] == '-';
    const char *digits_raw = raw + negative;
    size_t len = strlen(digits_raw);
    size_t integer_len = len > digits ? len - digits : 1UL;
    size_t total = (negative ? 1UL : 0UL) + integer_len +
                   (digits ? 1UL + digits : 0UL) + 1UL;
    char *out = (char *)PPF_CORE_MALLOC(total);
    if (!out) {
        ppf_core_free_gmp_string(raw);
        return NULL;
    }
    char *cursor = out;
    if (negative) *cursor++ = '-';
    if (len <= digits) {
        *cursor++ = '0';
        if (digits) *cursor++ = '.';
        size_t zeros = digits - len;
        memset(cursor, '0', zeros);
        cursor += zeros;
        memcpy(cursor, digits_raw, len);
        cursor += len;
    } else {
        size_t whole = len - digits;
        memcpy(cursor, digits_raw, whole);
        cursor += whole;
        if (digits) {
            *cursor++ = '.';
            memcpy(cursor, digits_raw + whole, digits);
            cursor += digits;
        }
    }
    *cursor = '\0';
    ppf_core_free_gmp_string(raw);
    return out;
}

/* Correct round-to-nearest, ties-to-even of

       binary_value * 10^digits / 2^scale_bits.

   Since 10^digits = 2^digits 5^digits and scale_bits > digits in every
   supported run, the only division is an exact power-of-two shift. */
static int round_dyadic_to_decimal_integer(
    mpz_t out,
    const mpz_t binary_value,
    unsigned long scale_bits,
    unsigned long digits,
    const mpz_t pow5
) {
    if (mpz_sgn(binary_value) < 0 || scale_bits < digits) return -1;
    mpz_t product;
    mpz_init(product);
    mpz_mul(product, binary_value, pow5);
    unsigned long shift = scale_bits - digits;
    if (shift == 0UL) {
        mpz_set(out, product);
        mpz_clear(product);
        return 0;
    }
    mpz_fdiv_q_2exp(out, product, shift);
    if (mpz_tstbit(product, shift - 1UL)) {
        mp_bitcnt_t first_one = mpz_scan1(product, 0UL);
        int strictly_above_half = first_one < (mp_bitcnt_t)(shift - 1UL);
        if (strictly_above_half || mpz_odd_p(out)) {
            mpz_add_ui(out, out, 1UL);
        }
    }
    mpz_clear(product);
    return 0;
}

static int verify_global_alternating_bound(void) {
    mpz_t left, right;
    mpz_inits(left, right, NULL);
    mpz_set_ui(left, CHUD_A + CHUD_B);
    mpz_mul_ui(left, left, 1728UL);
    mpz_mul_ui(right, g_chud_c3_over_24, 24UL);
    mpz_mul_ui(right, right, CHUD_A);
    int ok = mpz_cmp(left, right) < 0;
    mpz_clears(left, right, NULL);
    return ok;
}

/* Construct a formal enclosure and certify the emitted decimal value.

   Let S_n=T_n/Q_n be the n-term Chudnovsky sum.  The absolute terms are
   strictly decreasing because, for k>=0,

     |u_(k+1)/u_k|
       < [1728/C^3] [(A+B)/A] < 1.

   Hence the exact infinite sum lies between S_n and S_(n+1).  If

     a = floor(2^m sqrt(10005)),

   then a/2^m <= sqrt(10005) < (a+1)/2^m.  Combining the two exact
   enclosures gives outward integer bounds on 2^m*pi.  The decimal result is
   emitted only when both endpoints round to the same nearest-even integer. */
static int certify_decimal_from_root(
    char **text_out,
    mpz_t rounded_decimal,
    unsigned long *interval_width_bits,
    const bs_result_t *root,
    const mpz_t sqrt_floor,
    unsigned long scale_bits,
    unsigned long terms,
    unsigned long digits,
    unsigned long workers
) {
#ifndef _OPENMP
    (void)workers;
#endif
    if (!text_out || !verify_global_alternating_bound() ||
        mpz_sgn(root->p) <= 0 || mpz_sgn(root->q) <= 0 ||
        mpz_sgn(root->t) <= 0) {
        return -1;
    }
    *text_out = NULL;

    bs_result_t extended, next;
    bs_init(&extended);
    bs_init(&next);
    bs_set(&extended, root);
    chud_leaf(&next, terms);
    mpz_t combine_scratch;
    mpz_init(combine_scratch);
    chud_combine_inplace(&extended, &next, combine_scratch);

    if (mpz_sgn(extended.t) <= 0) {
        mpz_clear(combine_scratch);
        bs_clear(&next);
        bs_clear(&extended);
        return -1;
    }

    const bs_result_t *sum_lo;
    const bs_result_t *sum_hi;
    if ((terms & 1UL) == 0UL) {
        /* The next term is positive. */
        sum_lo = root;
        sum_hi = &extended;
    } else {
        /* The next term is negative. */
        sum_lo = &extended;
        sum_hi = root;
    }

    mpz_t lower_binary, upper_binary;
    mpz_t lower_numerator, upper_numerator, sqrt_upper;
    mpz_t rounded_lo, rounded_hi, pow5, width;
    mpz_inits(lower_binary, upper_binary, lower_numerator, upper_numerator,
              sqrt_upper, rounded_lo, rounded_hi, pow5, width, NULL);

    mpz_add_ui(sqrt_upper, sqrt_floor, 1UL);
PPF_OMP_PRAGMA(omp parallel sections num_threads(2) if(workers >= 2UL))
    {
PPF_OMP_PRAGMA(omp section)
        {
            /* floor(2^m*pi_lower) = floor(426880*a*Q_hi/T_hi). */
            mpz_mul_ui(lower_numerator, sqrt_floor, 426880UL);
            mpz_mul(lower_numerator, lower_numerator, sum_hi->q);
            if (g_split_q2 && sum_hi->q2 > 0UL) {
                mpz_mul_2exp(lower_numerator, lower_numerator, sum_hi->q2);
            }
            mpz_fdiv_q(lower_binary, lower_numerator, sum_hi->t);
        }
PPF_OMP_PRAGMA(omp section)
        {
            /* ceil(2^m*pi_upper) = ceil(426880*(a+1)*Q_lo/T_lo). */
            mpz_mul_ui(upper_numerator, sqrt_upper, 426880UL);
            mpz_mul(upper_numerator, upper_numerator, sum_lo->q);
            if (g_split_q2 && sum_lo->q2 > 0UL) {
                mpz_mul_2exp(upper_numerator, upper_numerator, sum_lo->q2);
            }
            mpz_cdiv_q(upper_binary, upper_numerator, sum_lo->t);
        }
    }

    if (mpz_cmp(lower_binary, upper_binary) > 0) {
        mpz_clears(lower_binary, upper_binary, lower_numerator, upper_numerator,
                   sqrt_upper, rounded_lo, rounded_hi, pow5, width, NULL);
        mpz_clear(combine_scratch);
        bs_clear(&next);
        bs_clear(&extended);
        return -1;
    }

    mpz_sub(width, upper_binary, lower_binary);
    *interval_width_bits = mpz_sgn(width) == 0
        ? 0UL : (unsigned long)mpz_sizeinbase(width, 2);

    mpz_ui_pow_ui(pow5, 5UL, digits);
    int round_lo_status = 0;
    int round_hi_status = 0;
PPF_OMP_PRAGMA(omp parallel sections num_threads(2) if(workers >= 2UL))
    {
PPF_OMP_PRAGMA(omp section)
        { round_lo_status = round_dyadic_to_decimal_integer(
              rounded_lo, lower_binary, scale_bits, digits, pow5); }
PPF_OMP_PRAGMA(omp section)
        { round_hi_status = round_dyadic_to_decimal_integer(
              rounded_hi, upper_binary, scale_bits, digits, pow5); }
    }
    if (round_lo_status != 0 || round_hi_status != 0) {
        mpz_clears(lower_binary, upper_binary, lower_numerator, upper_numerator,
                   sqrt_upper, rounded_lo, rounded_hi, pow5, width, NULL);
        mpz_clear(combine_scratch);
        bs_clear(&next);
        bs_clear(&extended);
        return -1;
    }

    int decisive = mpz_cmp(rounded_lo, rounded_hi) == 0;
    int allocation_failed = 0;
    if (decisive) {
        mpz_set(rounded_decimal, rounded_lo);
        *text_out = decimal_integer_to_string(rounded_decimal, digits);
        allocation_failed = *text_out == NULL;
    }

    mpz_clears(lower_binary, upper_binary, lower_numerator, upper_numerator,
               sqrt_upper, rounded_lo, rounded_hi, pow5, width, NULL);
    mpz_clear(combine_scratch);
    bs_clear(&next);
    bs_clear(&extended);
    if (allocation_failed) return -1;
    return decisive ? 0 : 1;
}

static void chud_binary_split_reuse(
    bs_result_t *out,
    unsigned long a,
    unsigned long b,
    bs_result_t *scratch,
    unsigned long depth,
    mpz_t scratch_t,
    mpz_t gcd_scratch,
    int allow_directed_cancel
) {
    if (b - a == 1UL) {
        chud_leaf(out, a);
        return;
    }
    unsigned long m = a + (b - a) / 2UL;
    chud_binary_split_reuse(out, a, m, scratch, depth + 1UL, scratch_t,
                            gcd_scratch, allow_directed_cancel);
    chud_binary_split_reuse(&scratch[depth], m, b, scratch, depth + 1UL,
                            scratch_t, gcd_scratch, allow_directed_cancel);
    unsigned long width = b - a;
    int cancel_here = g_directed_cancel &&
        ((g_pair_phase && width == 2UL) ||
         (allow_directed_cancel &&
          ((width >= g_cancel_block1 && width < 2UL * g_cancel_block1) ||
           (width >= g_cancel_block2 && width < 2UL * g_cancel_block2))));
    if (cancel_here) {
        if (g_pair_phase && width == 2UL) {
            pair_directed_cancel(out, &scratch[depth], a, gcd_scratch);
        } else {
            directed_cancel(out, &scratch[depth], gcd_scratch);
        }
    }
    chud_combine_inplace(out, &scratch[depth], scratch_t);
}

/* Q/T-only binary splitting for the globally rightmost packet.  The right
   child is recursively Q/T-only; the left child remains full because its P
   contributes to T_L Q_R + P_L T_R. */
static void chud_binary_split_qt_reuse(
    bs_result_t *out,
    unsigned long a,
    unsigned long b,
    bs_result_t *scratch,
    unsigned long depth,
    mpz_t scratch_t,
    mpz_t gcd_scratch,
    int allow_directed_cancel
) {
    if (b - a == 1UL) {
        chud_leaf(out, a);
        mpz_set_ui(out->p, 0UL);
        return;
    }
    unsigned long m = a + (b - a) / 2UL;
    chud_binary_split_reuse(out, a, m, scratch, depth + 1UL, scratch_t,
                            gcd_scratch, allow_directed_cancel);
    chud_binary_split_qt_reuse(&scratch[depth], m, b, scratch, depth + 1UL,
                               scratch_t, gcd_scratch, allow_directed_cancel);
    unsigned long width = b - a;
    int cancel_here = g_directed_cancel &&
        ((g_pair_phase && width == 2UL) ||
         (allow_directed_cancel &&
          ((width >= g_cancel_block1 && width < 2UL * g_cancel_block1) ||
           (width >= g_cancel_block2 && width < 2UL * g_cancel_block2))));
    if (cancel_here) {
        if (g_pair_phase && width == 2UL) {
            pair_directed_cancel(out, &scratch[depth], a, gcd_scratch);
        } else {
            directed_cancel(out, &scratch[depth], gcd_scratch);
        }
    }
    chud_combine_qt_only(out, &scratch[depth], scratch_t);
}

static double packet_weight(unsigned long k) {
    if (k == 0UL) return 1.0;
    return 3.0 * log2((double)k) + log2((double)CHUD_C3_OVER_24_U64);
}

static void phase_align_boundaries(
    unsigned long *bounds,
    unsigned long terms,
    unsigned long packets
) {
    if (!g_pair_phase || !g_phase_align || packets <= 1UL ||
        terms < 2UL * packets) return;
    for (unsigned long i = 1UL; i < packets; ++i) {
        unsigned long minimum = bounds[i - 1UL] + 2UL;
        unsigned long maximum = terms - 2UL * (packets - i);
        maximum &= ~1UL;
        unsigned long candidate = bounds[i];
        if (candidate & 1UL) {
            unsigned long down = candidate - 1UL;
            unsigned long up = candidate + 1UL;
            candidate = (down >= minimum || up > maximum) ? down : up;
        }
        if (candidate < minimum) candidate = minimum;
        if (candidate > maximum) candidate = maximum;
        candidate &= ~1UL;
        if (candidate < minimum) candidate += 2UL;
        bounds[i] = candidate;
    }
}

static unsigned long *packet_boundaries(unsigned long terms, unsigned long packets) {
    unsigned long *bounds = (unsigned long *)PPF_CORE_CALLOC(packets + 1UL, sizeof(unsigned long));
    if (!bounds) return NULL;
    bounds[0] = 0UL;
    bounds[packets] = terms;
    if (packets <= 1UL) return bounds;

    if (!g_weighted_packets) {
        for (unsigned long i = 1UL; i < packets; ++i) {
            bounds[i] = (unsigned long)(((unsigned long long)terms * i) / packets);
            if (bounds[i] <= bounds[i - 1UL]) bounds[i] = bounds[i - 1UL] + 1UL;
        }
        phase_align_boundaries(bounds, terms, packets);
        return bounds;
    }

    double total = 0.0;
    for (unsigned long k = 0UL; k < terms; ++k) total += packet_weight(k);
    double per_packet = total / (double)packets;
    double cumulative = 0.0;
    unsigned long boundary = 1UL;
    for (unsigned long k = 0UL; k < terms && boundary < packets; ++k) {
        cumulative += packet_weight(k);
        unsigned long cut = k + 1UL;
        unsigned long remaining_terms = terms - cut;
        unsigned long remaining_packets = packets - boundary;
        if (cumulative >= per_packet * (double)boundary &&
            cut > bounds[boundary - 1UL] && remaining_terms >= remaining_packets) {
            bounds[boundary++] = cut;
        }
    }
    while (boundary < packets) {
        unsigned long previous = bounds[boundary - 1UL];
        unsigned long remaining_packets = packets - boundary + 1UL;
        unsigned long span = terms - previous;
        unsigned long cut = previous + span / remaining_packets;
        if (cut <= previous) cut = previous + 1UL;
        bounds[boundary++] = cut;
    }
    phase_align_boundaries(bounds, terms, packets);
    return bounds;
}


static unsigned long next_power_of_two(unsigned long x) {
    if (x <= 1UL) return 1UL;
    unsigned long value = 1UL;
    while (value < x && value <= ULONG_MAX / 2UL) value <<= 1UL;
    return value < x ? x : value;
}

static unsigned long choose_adic_packet_count(
    unsigned long terms,
    unsigned long workers
) {
    unsigned long raw = (terms + g_adic_block_terms - 1UL) / g_adic_block_terms;
    unsigned long packets = next_power_of_two(raw);
    unsigned long minimum = next_power_of_two(4UL * workers);
    unsigned long maximum = next_power_of_two(32UL * workers);
    if (terms > minimum && packets < minimum) packets = minimum;
    if (packets > maximum) packets = maximum;
    if (packets > terms) packets = terms;
    return packets ? packets : 1UL;
}



static int gauge_normalize_packets(
    bs_result_t *nodes,
    factor_meta_t *metadata,
    unsigned long packets,
    unsigned long workers,
    const factor_sieve_t *sieve,
    factor_stats_t *stats,
    unsigned long *max_gauge_bits
) {
#ifndef _OPENMP
    (void)workers;
#endif
    factor_vec_t inventory;
    factor_vec_t cancelled;
    factor_vec_t *gauges = (factor_vec_t *)PPF_CORE_CALLOC(packets, sizeof(*gauges));
    if (!gauges) return -1;
    factor_vec_init(&inventory);
    factor_vec_init(&cancelled);
    for (unsigned long i = 0UL; i < packets; ++i) factor_vec_init(&gauges[i]);
    *max_gauge_bits = 1UL;

    /* Metadata-only ordered gauge pass.  At packet i, inventory is the exact
       unmatched product of all earlier P factors.  Cancelling inventory
       against Q_i and copying the remainder gives the diagonal gauge h_i.
       The large integers are not touched in this serial pass. */
    for (unsigned long i = 0UL; i < packets; ++i) {
        if (factor_vec_cancel(&inventory, &metadata[i].q, &cancelled,
                              sieve, stats) != 0 ||
            factor_vec_copy(&gauges[i], &inventory) != 0) {
            for (unsigned long j = 0UL; j < packets; ++j) factor_vec_clear(&gauges[j]);
            PPF_CORE_FREE(gauges);
            factor_vec_clear(&inventory);
            factor_vec_clear(&cancelled);
            return -1;
        }
        double gauge_log2 = factor_vec_log2(&gauges[i], sieve);
        unsigned long gauge_bits = gauge_log2 > 0.0
            ? (unsigned long)ceil(gauge_log2) + 1UL : 1UL;
        if (gauge_bits > *max_gauge_bits) *max_gauge_bits = gauge_bits;
        if (factor_vec_merge_add(&inventory, &metadata[i].p) != 0) {
            for (unsigned long j = 0UL; j < packets; ++j) factor_vec_clear(&gauges[j]);
            PPF_CORE_FREE(gauges);
            factor_vec_clear(&inventory);
            factor_vec_clear(&cancelled);
            return -1;
        }
    }

    /* Materialize all reduced denominators and diagonal gauges independently.
       This replaces the former serial chain of huge multiplications and exact
       divisions by a parallel product forest. */
    int failed = 0;
PPF_OMP_PRAGMA(omp parallel num_threads(workers) shared(failed))
    {
        factor_product_ctx_t product_ctx;
        factor_product_ctx_init(&product_ctx);
        mpz_t gauge;
        mpz_init(gauge);
PPF_OMP_PRAGMA(omp for schedule(dynamic, 1))
        for (long index = 0L; index < (long)packets; ++index) {
            if (failed) continue;
            unsigned long i = (unsigned long)index;
            if (factor_product_build(nodes[i].q, &metadata[i].q,
                                     sieve, &product_ctx) != 0 ||
                factor_product_build(gauge, &gauges[i],
                                     sieve, &product_ctx) != 0) {
PPF_OMP_PRAGMA(omp atomic write)
                failed = 1;
                continue;
            }
            if (mpz_cmp_ui(gauge, 1UL) != 0) {
                mpz_mul(nodes[i].t, nodes[i].t, gauge);
            }
            mpz_set_ui(nodes[i].p, 1UL);
        }
        mpz_clear(gauge);
        factor_product_ctx_clear(&product_ctx);
    }

    for (unsigned long i = 0UL; i < packets; ++i) factor_vec_clear(&gauges[i]);
    PPF_CORE_FREE(gauges);
    factor_vec_clear(&inventory);
    factor_vec_clear(&cancelled);
    return failed ? -1 : 0;
}

static void affine_combine_inplace(
    bs_result_t *left,
    const bs_result_t *right,
    mpz_t scratch_t
) {
    mpz_mul_q_scaled(scratch_t, left->t, right);
    mpz_add(scratch_t, scratch_t, right->t);
    mpz_mul(left->q, left->q, right->q);
    if (g_split_q2) left->q2 += right->q2;
    mpz_swap(left->t, scratch_t);
    mpz_set_ui(left->p, 1UL);
}

static double affine_reduce_nodes(
    bs_result_t *nodes,
    unsigned long packets,
    unsigned long workers
) {
    channel_product_t *pool = (channel_product_t *)PPF_CORE_CALLOC(workers, sizeof(*pool));
    if (!pool) {
        perror("calloc affine product pool");
        abort();
    }
    for (unsigned long i = 0UL; i < workers; ++i) {
        mpz_inits(pool[i].p, pool[i].q, pool[i].tq, pool[i].pt, pool[i].cancel, NULL);
    }

    double started = now_seconds();
    unsigned long stride = 1UL, span = 0UL, tasks = 0UL, pair_count = 0UL;
    long jobs = 0L;
    int use_channels = 0, done = 0;

PPF_OMP_PRAGMA(omp parallel num_threads(workers) shared(stride, span, tasks, pair_count, jobs, use_channels, done, pool))
    {
        mpz_t scratch_t;
        mpz_init(scratch_t);
        for (;;) {
PPF_OMP_PRAGMA(omp single)
            {
                if (stride >= packets) {
                    done = 1;
                } else {
                    done = 0;
                    span = stride << 1UL;
                    tasks = (packets + span - 1UL) / span;
                    pair_count = 0UL;
                    for (unsigned long task = 0UL; task < tasks; ++task) {
                        if (task * span + stride < packets) ++pair_count;
                    }
                    use_channels = g_channel_kernel && pair_count > 0UL &&
                                   pair_count < workers &&
                                   q_full_bits(&nodes[0]) >= 32768UL;
                    jobs = (long)(2UL * pair_count);
                    if (use_channels) {
                        unsigned long index = 0UL;
                        for (unsigned long task = 0UL; task < tasks; ++task) {
                            unsigned long left = task * span;
                            unsigned long right = left + stride;
                            if (right >= packets) continue;
                            pool[index].left = left;
                            pool[index].right = right;
                            mp_size_t q_limbs = (mp_size_t)(mpz_size(nodes[left].q) +
                                                            mpz_size(nodes[right].q));
                            mp_size_t tq_limbs = (mp_size_t)(mpz_size(nodes[left].t) +
                                                             mpz_size(nodes[right].q) + 1);
                            mpz_realloc2(pool[index].q,
                                         (mp_bitcnt_t)q_limbs * GMP_NUMB_BITS);
                            mpz_realloc2(pool[index].tq,
                                         (mp_bitcnt_t)tq_limbs * GMP_NUMB_BITS);
                            ++index;
                        }
                    }
                }
            }
            if (done) break;

            if (use_channels) {
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long job = 0L; job < jobs; ++job) {
                    unsigned long slot = (unsigned long)job >> 1UL;
                    unsigned long channel = (unsigned long)job & 1UL;
                    unsigned long left = pool[slot].left;
                    unsigned long right = pool[slot].right;
                    if (channel == 0UL) {
                        mpz_mul(pool[slot].q, nodes[left].q, nodes[right].q);
                    } else {
                        mpz_mul_q_scaled(pool[slot].tq, nodes[left].t, &nodes[right]);
                    }
                }
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long i = 0L; i < (long)pair_count; ++i) {
                    unsigned long slot = (unsigned long)i;
                    unsigned long left = pool[slot].left;
                    unsigned long right = pool[slot].right;
                    mpz_add(pool[slot].tq, pool[slot].tq, nodes[right].t);
                    mpz_swap(nodes[left].q, pool[slot].q);
                    if (g_split_q2) nodes[left].q2 += nodes[right].q2;
                    mpz_swap(nodes[left].t, pool[slot].tq);
                    mpz_set_ui(nodes[left].p, 1UL);
                    mpz_set_ui(pool[slot].q, 0UL);
                    mpz_set_ui(pool[slot].tq, 0UL);
                }
            } else {
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long task = 0L; task < (long)tasks; ++task) {
                    unsigned long left = (unsigned long)task * span;
                    unsigned long right = left + stride;
                    if (right < packets) {
                        affine_combine_inplace(&nodes[left], &nodes[right], scratch_t);
                    }
                }
            }
PPF_OMP_PRAGMA(omp single)
            { stride <<= 1UL; }
        }
        mpz_clear(scratch_t);
    }

    double elapsed = now_seconds() - started;
    for (unsigned long i = 0UL; i < workers; ++i) {
        mpz_clears(pool[i].p, pool[i].q, pool[i].tq, pool[i].pt, pool[i].cancel, NULL);
    }
    PPF_CORE_FREE(pool);
    return elapsed;
}

static double factor_reduce_nodes(
    bs_result_t *nodes,
    factor_meta_t *metadata,
    unsigned long packets,
    unsigned long reduce_limit,
    unsigned long workers,
    const factor_sieve_t *sieve,
    factor_stats_t *aggregate_stats,
    double *cancel_worker_seconds
) {
    channel_product_t *pool = (channel_product_t *)PPF_CORE_CALLOC(workers, sizeof(*pool));
    factor_stats_t *thread_stats = (factor_stats_t *)PPF_CORE_CALLOC(workers, sizeof(*thread_stats));
    double *thread_cancel_seconds = (double *)PPF_CORE_CALLOC(workers, sizeof(*thread_cancel_seconds));
    if (!pool || !thread_stats || !thread_cancel_seconds) {
        perror("factor reduction allocation");
        abort();
    }
    for (unsigned long i = 0UL; i < workers; ++i) {
        mpz_inits(pool[i].p, pool[i].q, pool[i].tq, pool[i].pt, pool[i].cancel, NULL);
    }

    double started = now_seconds();
    unsigned long stride = 1UL, span = 0UL, tasks = 0UL, pair_count = 0UL;
    long jobs = 0L;
    int use_channels = 0, done = 0, parallel_divisions = 0, symbolic_rebuild = 0;

PPF_OMP_PRAGMA(omp parallel num_threads(workers) shared(stride, span, tasks, pair_count, jobs, use_channels, done, parallel_divisions, symbolic_rebuild, pool))
    {
        unsigned long tid = (unsigned long)omp_get_thread_num();
        mpz_t scratch_t;
        mpz_init(scratch_t);
        factor_product_ctx_t product_ctx;
        factor_product_ctx_init(&product_ctx);
        factor_vec_t cancelled, primitive;
        factor_vec_init(&cancelled);
        factor_vec_init(&primitive);
        mpz_t unit_quotient, prime_z;
        mpz_inits(unit_quotient, prime_z, NULL);

        for (;;) {
PPF_OMP_PRAGMA(omp single)
            {
                if (stride >= reduce_limit) {
                    done = 1;
                } else {
                    done = 0;
                    span = stride << 1UL;
                    tasks = (packets + span - 1UL) / span;
                    pair_count = 0UL;
                    for (unsigned long task = 0UL; task < tasks; ++task) {
                        if (task * span + stride < packets) ++pair_count;
                    }
                    use_channels = g_channel_kernel && pair_count > 0UL &&
                                   pair_count < workers &&
                                   q_full_bits(&nodes[0]) >= 32768UL;
                    parallel_divisions = use_channels &&
                        q_full_bits(&nodes[0]) >= PARALLEL_CANCEL_DIV_BITS;
                    unsigned long symbolic_span = packets;
                    for (unsigned long level = 1UL;
                         level < g_symbolic_top_levels && symbolic_span > 1UL;
                         ++level) {
                        symbolic_span = (symbolic_span + 1UL) >> 1UL;
                    }
                    symbolic_rebuild = sieve->complete &&
                        g_symbolic_top_levels > 0UL && use_channels &&
                        span >= symbolic_span && stride >= g_adic_cancel_min_span &&
                        q_full_bits(&nodes[0]) >= SYMBOLIC_ROOT_MIN_SUBTREE_BITS;
                    jobs = (long)(4UL * pair_count);
                    if (use_channels) {
                        unsigned long index = 0UL;
                        for (unsigned long task = 0UL; task < tasks; ++task) {
                            unsigned long left = task * span;
                            unsigned long right = left + stride;
                            if (right >= packets) continue;
                            pool[index].left = left;
                            pool[index].right = right;
                            pool[index].omit_p = g_p_elision && right + stride >= packets;
                            mpz_set_ui(pool[index].cancel, 1UL);
                            mp_size_t p_limbs = (mp_size_t)(mpz_size(nodes[left].p) +
                                                            mpz_size(nodes[right].p));
                            mp_size_t q_limbs = (mp_size_t)(mpz_size(nodes[left].q) +
                                                            mpz_size(nodes[right].q));
                            mp_size_t tq_limbs = (mp_size_t)(mpz_size(nodes[left].t) +
                                                             mpz_size(nodes[right].q));
                            mp_size_t pt_limbs = (mp_size_t)(mpz_size(nodes[left].p) +
                                                             mpz_size(nodes[right].t));
                            mp_size_t sum_limbs =
                                (tq_limbs > pt_limbs ? tq_limbs : pt_limbs) + 1;
                            if (!pool[index].omit_p) {
                                mpz_realloc2(pool[index].p,
                                             (mp_bitcnt_t)p_limbs * GMP_NUMB_BITS);
                            } else {
                                mpz_set_ui(pool[index].p, 0UL);
                            }
                            mpz_realloc2(pool[index].q,
                                         (mp_bitcnt_t)q_limbs * GMP_NUMB_BITS);
                            mpz_realloc2(pool[index].tq,
                                         (mp_bitcnt_t)sum_limbs * GMP_NUMB_BITS);
                            mpz_realloc2(pool[index].pt,
                                         (mp_bitcnt_t)pt_limbs * GMP_NUMB_BITS);
                            ++index;
                        }
                    }
                }
            }
            if (done) break;

            if (use_channels) {
                if (symbolic_rebuild) {
PPF_OMP_PRAGMA(omp for schedule(static))
                    for (long slot_i = 0L; slot_i < (long)pair_count; ++slot_i) {
                        unsigned long slot = (unsigned long)slot_i;
                        unsigned long left = pool[slot].left;
                        unsigned long right = pool[slot].right;
                        double t0 = now_seconds();
                        if (g_directed_cancel &&
                            factor_vec_cancel(&metadata[left].p, &metadata[right].q,
                                              &cancelled, sieve,
                                              &thread_stats[tid]) != 0) {
                            fprintf(stderr, "symbolic top cancellation failed\n");
                            abort();
                        }
                        thread_cancel_seconds[tid] += now_seconds() - t0;
                    }
PPF_OMP_PRAGMA(omp for schedule(static))
                    for (long rebuild_job = 0L;
                         rebuild_job < (long)(2UL * pair_count);
                         ++rebuild_job) {
                        unsigned long slot = (unsigned long)rebuild_job >> 1UL;
                        unsigned long left = pool[slot].left;
                        unsigned long right = pool[slot].right;
                        double t0 = now_seconds();
                        int failed = (((unsigned long)rebuild_job & 1UL) == 0UL)
                            ? factor_product_build(nodes[left].p, &metadata[left].p,
                                                   sieve, &product_ctx)
                            : factor_product_build(nodes[right].q, &metadata[right].q,
                                                   sieve, &product_ctx);
                        if (failed != 0) {
                            fprintf(stderr, "symbolic top product rebuild failed\n");
                            abort();
                        }
                        thread_cancel_seconds[tid] += now_seconds() - t0;
                    }
                } else if (parallel_divisions) {
PPF_OMP_PRAGMA(omp for schedule(static))
                    for (long slot_i = 0L; slot_i < (long)pair_count; ++slot_i) {
                        unsigned long slot = (unsigned long)slot_i;
                        unsigned long left = pool[slot].left;
                        unsigned long right = pool[slot].right;
                        double t0 = now_seconds();
                        if (stride >= g_adic_cancel_min_span &&
                            factor_prepare_directed_cancel(pool[slot].cancel,
                                                           &metadata[left], &metadata[right],
                                                           sieve, &product_ctx, &cancelled,
                                                           &thread_stats[tid]) != 0) {
                            fprintf(stderr, "factor-directed cancellation preparation failed\n");
                            abort();
                        }
                        thread_cancel_seconds[tid] += now_seconds() - t0;
                    }
PPF_OMP_PRAGMA(omp for schedule(static))
                    for (long div_job = 0L; div_job < (long)(2UL * pair_count); ++div_job) {
                        unsigned long slot = (unsigned long)div_job >> 1UL;
                        if (mpz_cmp_ui(pool[slot].cancel, 1UL) <= 0) continue;
                        unsigned long left = pool[slot].left;
                        unsigned long right = pool[slot].right;
                        double t0 = now_seconds();
                        if (((unsigned long)div_job & 1UL) == 0UL) {
                            mpz_divexact(nodes[left].p, nodes[left].p, pool[slot].cancel);
                        } else {
                            mpz_divexact(nodes[right].q, nodes[right].q, pool[slot].cancel);
                        }
                        thread_cancel_seconds[tid] += now_seconds() - t0;
                    }
                } else {
PPF_OMP_PRAGMA(omp for schedule(static))
                    for (long slot_i = 0L; slot_i < (long)pair_count; ++slot_i) {
                        unsigned long slot = (unsigned long)slot_i;
                        unsigned long left = pool[slot].left;
                        unsigned long right = pool[slot].right;
                        double t0 = now_seconds();
                        if (stride >= g_adic_cancel_min_span &&
                            factor_prepare_directed_cancel(pool[slot].cancel,
                                                           &metadata[left], &metadata[right],
                                                           sieve, &product_ctx, &cancelled,
                                                           &thread_stats[tid]) != 0) {
                            fprintf(stderr, "factor-directed cancellation preparation failed\n");
                            abort();
                        }
                        if (mpz_cmp_ui(pool[slot].cancel, 1UL) > 0) {
                            mpz_divexact(nodes[left].p, nodes[left].p,
                                         pool[slot].cancel);
                            mpz_divexact(nodes[right].q, nodes[right].q,
                                         pool[slot].cancel);
                        }
                        thread_cancel_seconds[tid] += now_seconds() - t0;
                    }
                }
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long job = 0L; job < jobs; ++job) {
                    unsigned long slot = (unsigned long)job >> 2UL;
                    unsigned long channel = (unsigned long)job & 3UL;
                    unsigned long left = pool[slot].left;
                    unsigned long right = pool[slot].right;
                    if (channel == 0UL) {
                        if (!pool[slot].omit_p) {
                            mpz_mul(pool[slot].p, nodes[left].p, nodes[right].p);
                        }
                    } else if (channel == 1UL) {
                        mpz_mul(pool[slot].q, nodes[left].q, nodes[right].q);
                    } else if (channel == 2UL) {
                        mpz_mul_q_scaled(pool[slot].tq, nodes[left].t, &nodes[right]);
                    } else {
                        mpz_mul(pool[slot].pt, nodes[left].p, nodes[right].t);
                    }
                }
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long i = 0L; i < (long)pair_count; ++i) {
                    unsigned long slot = (unsigned long)i;
                    unsigned long left = pool[slot].left;
                    unsigned long right = pool[slot].right;
                    mpz_add(pool[slot].tq, pool[slot].tq, pool[slot].pt);
                    if (pool[slot].omit_p) {
                        mpz_set_ui(nodes[left].p, 0UL);
                        factor_vec_clear(&metadata[left].p);
                        factor_vec_clear(&metadata[right].p);
                    } else {
                        mpz_swap(nodes[left].p, pool[slot].p);
                        if (factor_vec_merge_add(&metadata[left].p,
                                                 &metadata[right].p) != 0) {
                            fprintf(stderr, "factor P metadata merge failed\n");
                            abort();
                        }
                    }
                    mpz_swap(nodes[left].q, pool[slot].q);
                    if (g_split_q2) nodes[left].q2 += nodes[right].q2;
                    mpz_swap(nodes[left].t, pool[slot].tq);
                    if (factor_vec_merge_add(&metadata[left].q,
                                             &metadata[right].q) != 0) {
                        fprintf(stderr, "factor Q metadata merge failed\n");
                        abort();
                    }
                    int is_root = left == 0UL && right + stride >= packets;
                    if (!is_root && span >= g_unit_min_span &&
                        factor_unit_projective_normalize(
                            &nodes[left], &metadata[left], pool[slot].omit_p,
                            sieve, &product_ctx, &primitive,
                            unit_quotient, prime_z,
                            &thread_stats[tid]) != 0) {
                        fprintf(stderr, "p-adic unit normalization failed\n");
                        abort();
                    }
                    mpz_set_ui(pool[slot].p, 0UL);
                    mpz_set_ui(pool[slot].q, 0UL);
                    mpz_set_ui(pool[slot].tq, 0UL);
                    mpz_set_ui(pool[slot].pt, 0UL);
                    mpz_set_ui(pool[slot].cancel, 1UL);
                }
            } else {
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long task = 0L; task < (long)tasks; ++task) {
                    unsigned long left = (unsigned long)task * span;
                    unsigned long right = left + stride;
                    if (right >= packets) continue;
                    double t0 = now_seconds();
                    if (stride >= g_adic_cancel_min_span &&
                        factor_directed_cancel(&nodes[left], &nodes[right],
                                               &metadata[left], &metadata[right],
                                               sieve, &product_ctx, &cancelled,
                                               &thread_stats[tid]) != 0) {
                        fprintf(stderr, "factor-directed cancellation failed\n");
                        abort();
                    }
                    thread_cancel_seconds[tid] += now_seconds() - t0;
                    int omit_p = g_p_elision && right + stride >= packets;
                    if (omit_p) {
                        chud_combine_qt_only(&nodes[left], &nodes[right], scratch_t);
                        factor_vec_clear(&metadata[left].p);
                        factor_vec_clear(&metadata[right].p);
                    } else {
                        chud_combine_inplace(&nodes[left], &nodes[right], scratch_t);
                        if (factor_vec_merge_add(&metadata[left].p,
                                                 &metadata[right].p) != 0) {
                            fprintf(stderr, "factor P metadata merge failed\n");
                            abort();
                        }
                    }
                    if (factor_vec_merge_add(&metadata[left].q,
                                             &metadata[right].q) != 0) {
                        fprintf(stderr, "factor Q metadata merge failed\n");
                        abort();
                    }
                    int is_root = left == 0UL && right + stride >= packets;
                    if (!is_root && span >= g_unit_min_span &&
                        factor_unit_projective_normalize(
                            &nodes[left], &metadata[left], omit_p,
                            sieve, &product_ctx, &primitive,
                            unit_quotient, prime_z,
                            &thread_stats[tid]) != 0) {
                        fprintf(stderr, "p-adic unit normalization failed\n");
                        abort();
                    }
                }
            }
PPF_OMP_PRAGMA(omp single)
            { stride <<= 1UL; }
        }

        factor_vec_clear(&cancelled);
        factor_vec_clear(&primitive);
        factor_product_ctx_clear(&product_ctx);
        mpz_clears(scratch_t, unit_quotient, prime_z, NULL);
    }

    double elapsed = now_seconds() - started;
    memset(aggregate_stats, 0, sizeof(*aggregate_stats));
    *cancel_worker_seconds = 0.0;
    for (unsigned long i = 0UL; i < workers; ++i) {
        aggregate_stats->removed_bits += thread_stats[i].removed_bits;
        aggregate_stats->cancellation_events += thread_stats[i].cancellation_events;
        aggregate_stats->cancelled_prime_powers +=
            thread_stats[i].cancelled_prime_powers;
        *cancel_worker_seconds += thread_cancel_seconds[i];
        mpz_clears(pool[i].p, pool[i].q, pool[i].tq, pool[i].pt, pool[i].cancel, NULL);
    }
    PPF_CORE_FREE(pool);
    PPF_CORE_FREE(thread_stats);
    PPF_CORE_FREE(thread_cancel_seconds);
    return elapsed;
}


/* Fully factored upper reduction.  P and Q remain as p-adic exponent
   vectors; only the P-left and Q-right operands required by the T channel
   are materialized.  This avoids large exact divisions but deliberately
   exposes the cost of repeated factor-product materialization. */
static double factored_upper_reduce_nodes(
    bs_result_t *nodes,
    factor_meta_t *metadata,
    unsigned long packets,
    unsigned long workers,
    const factor_sieve_t *sieve,
    factor_stats_t *aggregate_stats,
    double *cancel_worker_seconds
) {
    factor_stats_t *thread_stats = (factor_stats_t *)PPF_CORE_CALLOC(workers, sizeof(*thread_stats));
    double *thread_cancel = (double *)PPF_CORE_CALLOC(workers, sizeof(*thread_cancel));
    if (!thread_stats || !thread_cancel) {
        perror("factored upper allocation");
        abort();
    }
    for (unsigned long i = 0UL; i < packets; ++i) {
        mpz_set_ui(nodes[i].p, 0UL);
        mpz_set_ui(nodes[i].q, 0UL);
    }

    double started = now_seconds();
    unsigned long stride = 1UL, span = 0UL, tasks = 0UL;
    int done = 0;

PPF_OMP_PRAGMA(omp parallel num_threads(workers) shared(stride, span, tasks, done))
    {
        unsigned long tid = (unsigned long)omp_get_thread_num();
        factor_product_ctx_t p_ctx, q_ctx;
        factor_product_ctx_init(&p_ctx);
        factor_product_ctx_init(&q_ctx);
        factor_vec_t cancelled;
        factor_vec_init(&cancelled);
        mpz_t p_value, q_value, tq, pt;
        mpz_inits(p_value, q_value, tq, pt, NULL);

        for (;;) {
PPF_OMP_PRAGMA(omp single)
            {
                if (stride >= packets) {
                    done = 1;
                } else {
                    done = 0;
                    span = stride << 1UL;
                    tasks = (packets + span - 1UL) / span;
                }
            }
            if (done) break;

PPF_OMP_PRAGMA(omp for schedule(static))
            for (long task = 0L; task < (long)tasks; ++task) {
                unsigned long left = (unsigned long)task * span;
                unsigned long right = left + stride;
                if (right >= packets) continue;
                double t0 = now_seconds();
                if (stride >= g_adic_cancel_min_span && g_directed_cancel) {
                    if (factor_vec_cancel(&metadata[left].p, &metadata[right].q,
                                          &cancelled, sieve,
                                          &thread_stats[tid]) != 0) {
                        fprintf(stderr, "factored upper cancellation failed\n");
                        abort();
                    }
                } else {
                    cancelled.size = 0U;
                }
                if (factor_product_build(p_value, &metadata[left].p,
                                         sieve, &p_ctx) != 0 ||
                    factor_product_build(q_value, &metadata[right].q,
                                         sieve, &q_ctx) != 0) {
                    fprintf(stderr, "factored upper materialization failed\n");
                    abort();
                }
                thread_cancel[tid] += now_seconds() - t0;

                mpz_mul(tq, nodes[left].t, q_value);
                if (g_split_q2 && nodes[right].q2 > 0UL) mpz_mul_2exp(tq, tq, nodes[right].q2);
                mpz_mul(pt, p_value, nodes[right].t);
                mpz_add(tq, tq, pt);
                mpz_swap(nodes[left].t, tq);
                if (g_split_q2) nodes[left].q2 += nodes[right].q2;

                int omit_p = g_p_elision && right + stride >= packets;
                if (omit_p) {
                    factor_vec_clear(&metadata[left].p);
                    factor_vec_clear(&metadata[right].p);
                } else if (factor_vec_merge_add(&metadata[left].p,
                                                &metadata[right].p) != 0) {
                    fprintf(stderr, "factored upper P merge failed\n");
                    abort();
                }
                if (factor_vec_merge_add(&metadata[left].q,
                                         &metadata[right].q) != 0) {
                    fprintf(stderr, "factored upper Q merge failed\n");
                    abort();
                }
            }
PPF_OMP_PRAGMA(omp single)
            { stride <<= 1UL; }
        }

        mpz_clears(p_value, q_value, tq, pt, NULL);
        factor_vec_clear(&cancelled);
        factor_product_ctx_clear(&p_ctx);
        factor_product_ctx_clear(&q_ctx);
    }

    factor_product_ctx_t root_ctx;
    factor_product_ctx_init(&root_ctx);
    if (factor_product_build(nodes[0].q, &metadata[0].q, sieve, &root_ctx) != 0) {
        fprintf(stderr, "factored root Q materialization failed\n");
        abort();
    }
    factor_product_ctx_clear(&root_ctx);
    mpz_set_ui(nodes[0].p, 0UL);

    memset(aggregate_stats, 0, sizeof(*aggregate_stats));
    *cancel_worker_seconds = 0.0;
    for (unsigned long i = 0UL; i < workers; ++i) {
        aggregate_stats->removed_bits += thread_stats[i].removed_bits;
        aggregate_stats->cancellation_events += thread_stats[i].cancellation_events;
        aggregate_stats->cancelled_prime_powers +=
            thread_stats[i].cancelled_prime_powers;
        *cancel_worker_seconds += thread_cancel[i];
    }
    PPF_CORE_FREE(thread_stats);
    PPF_CORE_FREE(thread_cancel);
    return now_seconds() - started;
}

static int affine_root_reconstruct(
    mpz_t out, const bs_result_t *left, const bs_result_t *right,
    const mpz_t sqrt10005, unsigned long scale_bits,
    unsigned long *tail_bits, unsigned long *attempts
);

static int compute_polyphase_chudnovsky(
    run_result_t *result,
    unsigned long digits,
    unsigned long guard_bits,
    unsigned long workers,
    unsigned long requested_packets,
    unsigned long packet_terms,
    unsigned long ratio_extra_bits,
    int strict_final
) {
    memset(result, 0, sizeof(*result));
    mpz_init(result->value);
    result->digits = digits;
    result->strict_final = strict_final;
    result->ratio_extra_bits = ratio_extra_bits;

    double total_started = now_seconds();
    unsigned long target_bits = bits_for_digits(digits);
    unsigned long scale_bits = target_bits + guard_bits;
    double guard_digits = (double)guard_bits / LOG2_10;
    unsigned long terms = (unsigned long)ceil(((double)digits + guard_digits) /
                                               CHUD_DIGITS_PER_TERM) + 1UL;

    unsigned long packets;
    if (packet_terms > 0UL) {
        packets = (terms + packet_terms - 1UL) / packet_terms;
    } else if (requested_packets > 0UL) {
        packets = requested_packets;
    } else if (g_adic_factors && !g_adic_gauge) {
        packets = choose_adic_packet_count(terms, workers);
    } else {
        packets = choose_packet_count(terms, workers);
    }
    if (packets < 1UL) packets = 1UL;
    if (packets > terms) packets = terms;

    double boundary_started = now_seconds();
    unsigned long *bounds = packet_boundaries(terms, packets);
    if (!bounds) {
        mpz_clear(result->value);
        return -1;
    }
    unsigned long max_width = 1UL;
    for (unsigned long i = 0UL; i < packets; ++i) {
        unsigned long width = bounds[i + 1UL] - bounds[i];
        if (width > max_width) max_width = width;
    }
    result->boundary_seconds = now_seconds() - boundary_started;

    factor_sieve_t factor_sieve;
    memset(&factor_sieve, 0, sizeof(factor_sieve));
    factor_meta_t *factor_metadata = NULL;
    if (g_adic_factors) {
        double sieve_started = now_seconds();
        if (factor_sieve_init(&factor_sieve, terms, g_adic_prime_bound) != 0) {
            fprintf(stderr, "failed to construct p-adic factor sieve\n");
            PPF_CORE_FREE(bounds);
            mpz_clear(result->value);
            return -1;
        }
        result->sieve_seconds = now_seconds() - sieve_started;
        result->sieve_limit = factor_sieve.limit;
        result->sieve_prime_count = (unsigned long)factor_sieve.prime_count;
        result->adic_block_terms = g_adic_block_terms;
        result->adic_prime_bound = factor_sieve.limit;
        result->adic_metadata_complete = factor_sieve.complete;
        result->adic_factors = 1;
        if (!factor_sieve.complete) {
            /* Partial prime bands are exact for directed cancellation, but
               cannot rebuild an operand solely from incomplete metadata. */
            g_symbolic_top_levels = 0UL;
            if (g_adic_gauge || g_factored_upper) {
                fprintf(stderr, "ordered/factored upper modes require complete prime metadata\n");
                factor_sieve_clear(&factor_sieve);
                PPF_CORE_FREE(bounds);
                mpz_clear(result->value);
                return -1;
            }
        }
        factor_metadata = (factor_meta_t *)PPF_CORE_CALLOC(packets, sizeof(*factor_metadata));
        if (!factor_metadata) {
            perror("calloc factor metadata");
            factor_sieve_clear(&factor_sieve);
            PPF_CORE_FREE(bounds);
            mpz_clear(result->value);
            return -1;
        }
        for (unsigned long i = 0UL; i < packets; ++i) {
            factor_meta_init(&factor_metadata[i]);
        }
    }

    bs_result_t *nodes = (bs_result_t *)PPF_CORE_CALLOC(packets, sizeof(*nodes));
    if (!nodes) {
        if (factor_metadata) {
            for (unsigned long i = 0UL; i < packets; ++i) factor_meta_clear(&factor_metadata[i]);
            PPF_CORE_FREE(factor_metadata);
            factor_sieve_clear(&factor_sieve);
        }
        PPF_CORE_FREE(bounds);
        mpz_clear(result->value);
        return -1;
    }
    for (unsigned long i = 0UL; i < packets; ++i) {
        bs_init(&nodes[i]);
        unsigned long width = bounds[i + 1UL] - bounds[i];
        double kmax = (double)(bounds[i + 1UL] > 1UL ? bounds[i + 1UL] : 1UL);
        double p_est = (double)width * (log2(72.0) + 3.0 * log2(kmax)) + 128.0;
        double q_full_est = (double)width *
            (log2((double)CHUD_C3_OVER_24_U64) + 3.0 * log2(kmax)) + 128.0;
        double packet_q2_est = g_split_q2 ? (double)q2_interval(bounds[i], bounds[i + 1UL]) : 0.0;
        double q_est = q_full_est - packet_q2_est;
        if (q_est < 256.0) q_est = 256.0;
        double t_est = (p_est > q_est ? p_est : q_est) + packet_q2_est + 128.0;
        mpz_realloc2(nodes[i].p, (mp_bitcnt_t)ceil(p_est));
        mpz_realloc2(nodes[i].q, (mp_bitcnt_t)ceil(q_est));
        mpz_realloc2(nodes[i].t, (mp_bitcnt_t)ceil(t_est));
    }

    mpz_t radicand, sqrt10005, numerator;
    mpz_inits(radicand, sqrt10005, numerator, NULL);
    mpz_set_ui(radicand, 10005UL);
    mpz_mul_2exp(radicand, radicand, 2UL * scale_bits);

    double kernel_started = now_seconds();
    double packet_started = now_seconds();
    atomic_int packet_allocation_failed;
    atomic_init(&packet_allocation_failed, 0);
PPF_OMP_PRAGMA(omp parallel num_threads(workers) shared(packet_allocation_failed))
    {
        unsigned long stack_depth = 2UL;
        bs_result_t *scratch;
        for (unsigned long width = max_width; width > 1UL;
             width = (width + 1UL) >> 1UL) {
            ++stack_depth;
        }
        scratch = (bs_result_t *)PPF_CORE_CALLOC(stack_depth,
                                                 sizeof(*scratch));
        if (!scratch)
            atomic_store_explicit(&packet_allocation_failed, 1,
                                  memory_order_release);
PPF_OMP_PRAGMA(omp barrier)
        if (atomic_load_explicit(&packet_allocation_failed,
                                 memory_order_acquire) == 0) {
            unsigned long scratch_width = max_width;
            double scratch_kmax = (double)(terms > 1UL ? terms : 1UL);
            mpz_t scratch_t, gcd_scratch;
            for (unsigned long i = 0UL; i < stack_depth; ++i) {
                double p_est;
                double q_full_est;
                double q2_est;
                double q_est;
                double t_est;
                bs_init(&scratch[i]);
                scratch_width = (scratch_width + 1UL) >> 1UL;
                if (scratch_width < 1UL) scratch_width = 1UL;
                p_est = (double)scratch_width *
                    (log2(72.0) + 3.0 * log2(scratch_kmax)) + 128.0;
                q_full_est = (double)scratch_width *
                    (log2((double)CHUD_C3_OVER_24_U64) +
                     3.0 * log2(scratch_kmax)) + 128.0;
                q2_est = g_split_q2 ? 18.0 * (double)scratch_width : 0.0;
                q_est = q_full_est - q2_est;
                if (q_est < 256.0) q_est = 256.0;
                t_est = (p_est > q_est ? p_est : q_est) + q2_est + 128.0;
                mpz_realloc2(scratch[i].p,
                             (mp_bitcnt_t)ceil(p_est));
                mpz_realloc2(scratch[i].q,
                             (mp_bitcnt_t)ceil(q_est));
                mpz_realloc2(scratch[i].t,
                             (mp_bitcnt_t)ceil(t_est));
            }
            mpz_inits(scratch_t, gcd_scratch, NULL);

PPF_OMP_PRAGMA(omp single nowait)
            { mpz_sqrt(sqrt10005, radicand); }

PPF_OMP_PRAGMA(omp for schedule(dynamic, 1))
            for (long i = 0L; i < (long)packets; ++i) {
                unsigned long a = bounds[(unsigned long)i];
                unsigned long b = bounds[(unsigned long)i + 1UL];
                if (g_p_elision && (unsigned long)i + 1UL == packets) {
                    chud_binary_split_qt_reuse(&nodes[i], a, b, scratch, 0UL,
                                               scratch_t, gcd_scratch,
                                               !g_adic_factors);
                } else {
                    chud_binary_split_reuse(&nodes[i], a, b, scratch, 0UL,
                                            scratch_t, gcd_scratch,
                                            !g_adic_factors);
                }
            }

            mpz_clears(scratch_t, gcd_scratch, NULL);
            for (unsigned long i = 0UL; i < stack_depth; ++i)
                bs_clear(&scratch[i]);
        }
        PPF_CORE_FREE(scratch);
    }
    result->packet_seconds = now_seconds() - packet_started;
    if (atomic_load_explicit(&packet_allocation_failed,
                             memory_order_acquire) != 0) {
        mpz_clears(radicand, sqrt10005, numerator, NULL);
        for (unsigned long i = 0UL; i < packets; ++i) {
            bs_clear(&nodes[i]);
            if (factor_metadata) factor_meta_clear(&factor_metadata[i]);
        }
        PPF_CORE_FREE(nodes);
        PPF_CORE_FREE(factor_metadata);
        if (g_adic_factors) factor_sieve_clear(&factor_sieve);
        PPF_CORE_FREE(bounds);
        mpz_clear(result->value);
        return -1;
    }

    if (g_adic_factors) {
        double metadata_started = now_seconds();
        int metadata_failed = 0;
        unsigned long full_factor_limit = terms <= 1UL ? 2UL : 6UL * (terms - 1UL) - 1UL;
        if (!g_channel_metadata && factor_sieve.limit < full_factor_limit) {
            fprintf(stderr, "bounded p-adic prime bands require residue-channel metadata\n");
            metadata_failed = 1;
        }
        if (!metadata_failed && g_channel_metadata) {
            metadata_failed = factor_channel_metadata_all(
                factor_metadata, bounds, packets, terms, &factor_sieve, workers) != 0;
        } else if (!metadata_failed) {
PPF_OMP_PRAGMA(omp parallel num_threads(workers) shared(metadata_failed))
            {
                factor_build_scratch_t factor_scratch;
                if (factor_build_scratch_init(&factor_scratch, &factor_sieve) != 0) {
PPF_OMP_PRAGMA(omp atomic write)
                    metadata_failed = 1;
                } else {
PPF_OMP_PRAGMA(omp for schedule(dynamic, 1))
                    for (long i = 0L; i < (long)packets; ++i) {
                        if (metadata_failed) continue;
                        unsigned long index = (unsigned long)i;
                        if (factor_packet_metadata(&factor_metadata[index],
                                                   bounds[index], bounds[index + 1UL],
                                                   &factor_sieve, &factor_scratch) != 0) {
PPF_OMP_PRAGMA(omp atomic write)
                            metadata_failed = 1;
                        }
                    }
                    factor_build_scratch_clear(&factor_scratch);
                }
            }
        }
        if (!metadata_failed && g_pair_phase && g_directed_cancel) {
            factor_stats_t *pair_stats = (factor_stats_t *)PPF_CORE_CALLOC(packets, sizeof(*pair_stats));
            if (!pair_stats) {
                metadata_failed = 1;
            } else {
PPF_OMP_PRAGMA(omp parallel for num_threads(workers) schedule(static))
                for (long i = 0L; i < (long)packets; ++i) {
                    unsigned long index = (unsigned long)i;
                    if (factor_pair_phase_adjust(&factor_metadata[index],
                                                 bounds[index], bounds[index + 1UL],
                                                 &factor_sieve, &pair_stats[index]) != 0) {
PPF_OMP_PRAGMA(omp atomic write)
                        metadata_failed = 1;
                    }
                }
                for (unsigned long i = 0UL; i < packets; ++i) {
                    result->pair_phase_removed_bits += pair_stats[i].removed_bits;
                    result->pair_phase_events += pair_stats[i].cancellation_events;
                }
                PPF_CORE_FREE(pair_stats);
            }
        }
        result->factor_metadata_seconds = now_seconds() - metadata_started;
        if (metadata_failed) {
            fprintf(stderr, "failed to build p-adic packet metadata\n");
            mpz_clears(radicand, sqrt10005, numerator, NULL);
            for (unsigned long i = 0UL; i < packets; ++i) {
                bs_clear(&nodes[i]);
                factor_meta_clear(&factor_metadata[i]);
            }
            PPF_CORE_FREE(nodes);
            PPF_CORE_FREE(factor_metadata);
            factor_sieve_clear(&factor_sieve);
            PPF_CORE_FREE(bounds);
            mpz_clear(result->value);
            return -1;
        }
        if (g_p_elision && packets > 0UL) {
            factor_vec_clear(&factor_metadata[packets - 1UL].p);
        }
    }

    if (g_adic_factors) {
        factor_stats_t factor_stats;
        memset(&factor_stats, 0, sizeof(factor_stats));
        if (g_adic_gauge) {
            double gauge_started = now_seconds();
            if (gauge_normalize_packets(nodes, factor_metadata, packets, workers,
                                        &factor_sieve, &factor_stats,
                                        &result->max_gauge_bits) != 0) {
                fprintf(stderr, "p-adic gauge normalization failed\n");
                mpz_clears(radicand, sqrt10005, numerator, NULL);
                for (unsigned long i = 0UL; i < packets; ++i) {
                    bs_clear(&nodes[i]);
                    factor_meta_clear(&factor_metadata[i]);
                }
                PPF_CORE_FREE(nodes);
                PPF_CORE_FREE(factor_metadata);
                factor_sieve_clear(&factor_sieve);
                PPF_CORE_FREE(bounds);
                mpz_clear(result->value);
                return -1;
            }
            result->gauge_seconds = now_seconds() - gauge_started;
            result->reduction_seconds = affine_reduce_nodes(nodes, packets, workers);
        } else if (g_factored_upper) {
            result->reduction_seconds = factored_upper_reduce_nodes(
                nodes, factor_metadata, packets, workers, &factor_sieve,
                &factor_stats, &result->factor_cancel_seconds);
        } else {
            result->reduction_seconds = factor_reduce_nodes(
                nodes, factor_metadata, packets, reduction_limit(packets, strict_final),
                workers, &factor_sieve,
                &factor_stats, &result->factor_cancel_seconds);
        }
        result->adic_removed_bits = factor_stats.removed_bits;
        result->adic_cancellation_events = factor_stats.cancellation_events;
        result->adic_cancelled_prime_powers = factor_stats.cancelled_prime_powers;
    } else {

    channel_product_t *pool = (channel_product_t *)PPF_CORE_CALLOC(
        workers, sizeof(*pool));
    if (!pool) {
        mpz_clears(radicand, sqrt10005, numerator, NULL);
        for (unsigned long i = 0UL; i < packets; ++i) {
            bs_clear(&nodes[i]);
            if (factor_metadata) factor_meta_clear(&factor_metadata[i]);
        }
        PPF_CORE_FREE(nodes);
        PPF_CORE_FREE(factor_metadata);
        if (g_adic_factors) factor_sieve_clear(&factor_sieve);
        PPF_CORE_FREE(bounds);
        mpz_clear(result->value);
        return -1;
    }
    for (unsigned long i = 0UL; i < workers; ++i) {
        mpz_inits(pool[i].p, pool[i].q, pool[i].tq, pool[i].pt, pool[i].cancel, NULL);
    }

    double reduction_started = now_seconds();
    unsigned long stride = 1UL, span = 0UL, tasks = 0UL, pair_count = 0UL;
    long jobs = 0L;
    int use_channels = 0, done = 0;

PPF_OMP_PRAGMA(omp parallel num_threads(workers) shared(stride, span, tasks, pair_count, jobs, use_channels, done, pool))
    {
        mpz_t scratch_t, upper_gcd;
        mpz_inits(scratch_t, upper_gcd, NULL);
        for (;;) {
PPF_OMP_PRAGMA(omp single)
            {
                if (stride >= reduction_limit(packets, strict_final)) {
                    done = 1;
                } else {
                    done = 0;
                    span = stride << 1UL;
                    tasks = (packets + span - 1UL) / span;
                    pair_count = 0UL;
                    for (unsigned long task = 0UL; task < tasks; ++task) {
                        if (task * span + stride < packets) ++pair_count;
                    }
                    use_channels = g_channel_kernel && pair_count > 0UL &&
                                   pair_count < workers &&
                                   q_full_bits(&nodes[0]) >= 32768UL;
                    jobs = (long)(4UL * pair_count);
                    if (use_channels) {
                        unsigned long index = 0UL;
                        for (unsigned long task = 0UL; task < tasks; ++task) {
                            unsigned long left = task * span;
                            unsigned long right = left + stride;
                            if (right >= packets) continue;
                            pool[index].left = left;
                            pool[index].right = right;
                            pool[index].omit_p = g_p_elision && right + stride >= packets;
                            mp_size_t p_limbs = (mp_size_t)(mpz_size(nodes[left].p) + mpz_size(nodes[right].p));
                            mp_size_t q_limbs = (mp_size_t)(mpz_size(nodes[left].q) + mpz_size(nodes[right].q));
                            mp_size_t tq_limbs = (mp_size_t)(mpz_size(nodes[left].t) + mpz_size(nodes[right].q));
                            mp_size_t pt_limbs = (mp_size_t)(mpz_size(nodes[left].p) + mpz_size(nodes[right].t));
                            mp_size_t sum_limbs = (tq_limbs > pt_limbs ? tq_limbs : pt_limbs) + 1;
                            if (!pool[index].omit_p) {
                                mpz_realloc2(pool[index].p, (mp_bitcnt_t)p_limbs * GMP_NUMB_BITS);
                            } else {
                                mpz_set_ui(pool[index].p, 0UL);
                            }
                            mpz_realloc2(pool[index].q, (mp_bitcnt_t)q_limbs * GMP_NUMB_BITS);
                            mpz_realloc2(pool[index].tq, (mp_bitcnt_t)sum_limbs * GMP_NUMB_BITS);
                            mpz_realloc2(pool[index].pt, (mp_bitcnt_t)pt_limbs * GMP_NUMB_BITS);
                            ++index;
                        }
                    }
                }
            }
            if (done) break;

            if (use_channels) {
                if (g_full_upper_gcd && g_directed_cancel) {
PPF_OMP_PRAGMA(omp for schedule(static))
                    for (long task = 0L; task < (long)tasks; ++task) {
                        unsigned long left = (unsigned long)task * span;
                        unsigned long right = left + stride;
                        if (right < packets) directed_cancel(&nodes[left], &nodes[right], upper_gcd);
                    }
                }
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long job = 0L; job < jobs; ++job) {
                    unsigned long slot = (unsigned long)job >> 2UL;
                    unsigned long channel = (unsigned long)job & 3UL;
                    unsigned long left = pool[slot].left;
                    unsigned long right = pool[slot].right;
                    if (channel == 0UL) {
                        if (!pool[slot].omit_p) {
                            mpz_mul(pool[slot].p, nodes[left].p, nodes[right].p);
                        }
                    } else if (channel == 1UL) mpz_mul(pool[slot].q, nodes[left].q, nodes[right].q);
                    else if (channel == 2UL) mpz_mul_q_scaled(pool[slot].tq, nodes[left].t, &nodes[right]);
                    else mpz_mul(pool[slot].pt, nodes[left].p, nodes[right].t);
                }
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long i = 0L; i < (long)pair_count; ++i) {
                    unsigned long slot = (unsigned long)i;
                    unsigned long left = pool[slot].left;
                    unsigned long right = pool[slot].right;
                    mpz_add(pool[slot].tq, pool[slot].tq, pool[slot].pt);
                    if (pool[slot].omit_p) {
                        mpz_set_ui(nodes[left].p, 0UL);
                    } else {
                        mpz_swap(nodes[left].p, pool[slot].p);
                    }
                    mpz_swap(nodes[left].q, pool[slot].q);
                    if (g_split_q2) nodes[left].q2 += nodes[right].q2;
                    mpz_swap(nodes[left].t, pool[slot].tq);
                    mpz_set_ui(pool[slot].p, 0UL);
                    mpz_set_ui(pool[slot].q, 0UL);
                    mpz_set_ui(pool[slot].tq, 0UL);
                    mpz_set_ui(pool[slot].pt, 0UL);
                }
            } else {
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long task = 0L; task < (long)tasks; ++task) {
                    unsigned long left = (unsigned long)task * span;
                    unsigned long right = left + stride;
                    if (right < packets) {
                        if (g_full_upper_gcd && g_directed_cancel) {
                            directed_cancel(&nodes[left], &nodes[right], upper_gcd);
                        }
                        if (g_p_elision && right + stride >= packets) {
                            chud_combine_qt_only(&nodes[left], &nodes[right], scratch_t);
                        } else {
                            chud_combine_inplace(&nodes[left], &nodes[right], scratch_t);
                        }
                    }
                }
            }
PPF_OMP_PRAGMA(omp single)
            { stride <<= 1UL; }
        }
        mpz_clears(scratch_t, upper_gcd, NULL);
    }
    result->reduction_seconds = now_seconds() - reduction_started;

    for (unsigned long i = 0UL; i < workers; ++i) {
        mpz_clears(pool[i].p, pool[i].q, pool[i].tq, pool[i].pt, pool[i].cancel, NULL);
    }
    PPF_CORE_FREE(pool);
    }

    int use_affine_root = g_affine_root && !strict_final && packets >= 2UL &&
        is_power_of_two_ul(packets) && !g_adic_gauge && !g_factored_upper;
    unsigned long affine_right = use_affine_root ? (packets >> 1UL) : 0UL;

    if (use_affine_root && g_affine_root_cancel && g_directed_cancel) {
        if (g_adic_factors) {
            factor_product_ctx_t root_product_ctx;
            factor_vec_t root_cancelled;
            factor_stats_t root_stats;
            factor_product_ctx_init(&root_product_ctx);
            factor_vec_init(&root_cancelled);
            memset(&root_stats, 0, sizeof(root_stats));
            double t0 = now_seconds();
            if (factor_directed_cancel(&nodes[0], &nodes[affine_right],
                                       &factor_metadata[0],
                                       &factor_metadata[affine_right],
                                       &factor_sieve, &root_product_ctx,
                                       &root_cancelled, &root_stats) != 0) {
                fprintf(stderr, "affine root cancellation failed\n");
                abort();
            }
            result->factor_cancel_seconds += now_seconds() - t0;
            result->adic_removed_bits += root_stats.removed_bits;
            result->adic_cancellation_events += root_stats.cancellation_events;
            result->adic_cancelled_prime_powers += root_stats.cancelled_prime_powers;
            factor_vec_clear(&root_cancelled);
            factor_product_ctx_clear(&root_product_ctx);
        } else {
            mpz_t root_gcd;
            mpz_init(root_gcd);
            directed_cancel(&nodes[0], &nodes[affine_right], root_gcd);
            mpz_clear(root_gcd);
        }
    }

    if (use_affine_root) {
        result->affine_root = 1;
        result->affine_left_q_bits = (unsigned long)q_full_bits(&nodes[0]);
        result->affine_right_q_bits =
            (unsigned long)q_full_bits(&nodes[affine_right]);
        result->root_q_odd_bits = (unsigned long)(
            mpz_sizeinbase(nodes[0].q, 2) +
            mpz_sizeinbase(nodes[affine_right].q, 2));
        result->root_q2_bits = g_split_q2
            ? nodes[0].q2 + nodes[affine_right].q2 : 0UL;
        result->root_q_bits = result->root_q_odd_bits + result->root_q2_bits;
        size_t tq_bits = mpz_sizeinbase(nodes[0].t, 2) +
                         q_full_bits(&nodes[affine_right]);
        size_t pt_bits = mpz_sizeinbase(nodes[0].p, 2) +
                         mpz_sizeinbase(nodes[affine_right].t, 2);
        result->root_t_bits = (unsigned long)(tq_bits > pt_bits ? tq_bits : pt_bits);
    } else {
        result->root_q_odd_bits = (unsigned long)mpz_sizeinbase(nodes[0].q, 2);
        result->root_q2_bits = g_split_q2 ? nodes[0].q2 : 0UL;
        result->root_q_bits = (unsigned long)q_full_bits(&nodes[0]);
        result->root_t_bits = (unsigned long)mpz_sizeinbase(nodes[0].t, 2);
    }

    double final_started = now_seconds();
    unsigned long ratio_shift = 0UL;
    unsigned long keep_bits = scale_bits + ratio_extra_bits;

    int certificate_status = 0;
    if (g_certified_decimal) {
        double certificate_started = now_seconds();
        result->certified_decimal = 1;
        certificate_status = certify_decimal_from_root(
            &result->certified_text, result->value,
            &result->certificate_interval_width_bits,
            &nodes[0], sqrt10005, scale_bits, terms, digits, workers);
        result->certificate_seconds = now_seconds() - certificate_started;
        result->certificate_decisive = certificate_status == 0;
        if (certificate_status < 0) {
            fprintf(stderr, "formal decimal certification failed internally\n");
        }
    } else if (use_affine_root) {
        if (affine_root_reconstruct(result->value, &nodes[0],
                                    &nodes[affine_right], sqrt10005,
                                    scale_bits, &result->affine_tail_bits,
                                    &result->affine_attempts) != 0) {
            fprintf(stderr, "certified affine root reconstruction failed\n");
            abort();
        }
    } else {
        mpz_mul_ui(numerator, sqrt10005, 426880UL);
        if (strict_final) {
            double mul_started = now_seconds();
            {
                int used_channels = mpz_mul_parallel_split(
                    numerator, numerator, nodes[0].q, workers);
                if (used_channels > 0) {
                    result->parallel_final_mul_used = 1;
                    result->parallel_final_mul_channels = (unsigned long)used_channels;
                }
            }
            result->final_mul_seconds += now_seconds() - mul_started;
            if (g_split_q2 && nodes[0].q2 > 0UL) {
                mpz_mul_2exp(numerator, numerator, nodes[0].q2);
            }
            double div_started = now_seconds();
            mpz_round_div(result->value, numerator, nodes[0].t);
            result->final_div_seconds += now_seconds() - div_started;
        } else {
            size_t q_bits = q_full_bits(&nodes[0]);
            size_t t_bits = mpz_sizeinbase(nodes[0].t, 2);
            size_t min_bits = q_bits < t_bits ? q_bits : t_bits;
            if (min_bits > (size_t)keep_bits) {
                ratio_shift = (unsigned long)(min_bits - keep_bits);
            }

            mpz_t q_high, t_high;
            mpz_inits(q_high, t_high, NULL);
            if (g_split_q2) {
                if (ratio_shift >= nodes[0].q2) {
                    mpz_fdiv_q_2exp(q_high, nodes[0].q,
                                    ratio_shift - nodes[0].q2);
                } else {
                    mpz_mul_2exp(q_high, nodes[0].q,
                                 nodes[0].q2 - ratio_shift);
                }
            } else if (ratio_shift > 0UL) {
                mpz_fdiv_q_2exp(q_high, nodes[0].q, ratio_shift);
            } else {
                mpz_set(q_high, nodes[0].q);
            }
            if (ratio_shift > 0UL) {
                mpz_fdiv_q_2exp(t_high, nodes[0].t, ratio_shift);
            } else {
                mpz_set(t_high, nodes[0].t);
            }
            if (mpz_sgn(t_high) <= 0 || mpz_sgn(q_high) <= 0 ||
                mpz_cmp(q_high, t_high) >= 0) {
                mpz_mul(numerator, numerator, nodes[0].q);
                if (g_split_q2 && nodes[0].q2 > 0UL) {
                    mpz_mul_2exp(numerator, numerator, nodes[0].q2);
                }
                mpz_round_div(result->value, numerator, nodes[0].t);
                ratio_shift = 0UL;
            } else {
                double mul_started = now_seconds();
                {
                    int used_channels = mpz_mul_parallel_split(
                        numerator, numerator, q_high, workers);
                    if (used_channels > 0) {
                        result->parallel_final_mul_used = 1;
                        result->parallel_final_mul_channels = (unsigned long)used_channels;
                    }
                }
                result->final_mul_seconds += now_seconds() - mul_started;
                double div_started = now_seconds();
                mpz_tdiv_q(result->value, numerator, t_high);
                result->final_div_seconds += now_seconds() - div_started;
            }
            mpz_clears(q_high, t_high, NULL);
        }
    }
    result->final_seconds = now_seconds() - final_started;
    result->kernel_seconds = now_seconds() - kernel_started;
    result->total_seconds = now_seconds() - total_started;

    result->scale_bits = scale_bits;
    result->terms = terms;
    result->packets = packets;
    result->max_packet_width = max_width;
    result->ratio_keep_bits = keep_bits;
    result->ratio_shift_bits = ratio_shift;

    mpz_clears(radicand, sqrt10005, numerator, NULL);
    for (unsigned long i = 0UL; i < packets; ++i) {
        bs_clear(&nodes[i]);
        if (factor_metadata) factor_meta_clear(&factor_metadata[i]);
    }
    PPF_CORE_FREE(nodes);
    PPF_CORE_FREE(factor_metadata);
    if (g_adic_factors) factor_sieve_clear(&factor_sieve);
    PPF_CORE_FREE(bounds);
    if (certificate_status < 0) return -1;
    if (certificate_status > 0) return 2;
    return 0;
}

static int affine_root_reconstruct(
    mpz_t out,
    const bs_result_t *left,
    const bs_result_t *right,
    const mpz_t sqrt10005,
    unsigned long scale_bits,
    unsigned long *tail_bits,
    unsigned long *attempts
) {
    if (mpz_sgn(left->p) <= 0 || mpz_sgn(left->q) <= 0 ||
        mpz_sgn(left->t) <= 0 || mpz_sgn(right->q) <= 0) {
        return -1;
    }

    mpz_t constant, u, u_num, u_den, y0, prod, numerator;
    mpz_t remainder, twice_remainder, margin, coefficient, threshold;
    mpz_inits(constant, u, u_num, u_den, y0, prod, numerator,
              remainder, twice_remainder, margin, coefficient, threshold,
              NULL);
    mpz_mul_ui(constant, sqrt10005, 426880UL);

    unsigned long p_bits = (unsigned long)mpz_sizeinbase(left->p, 2);
    unsigned long t_bits = (unsigned long)mpz_sizeinbase(left->t, 2);
    unsigned long attenuation = t_bits > p_bits ? t_bits - p_bits : 0UL;
    unsigned long b = scale_bits > attenuation
        ? scale_bits - attenuation + 160UL : 256UL;
    if (b < 256UL) b = 256UL;
    if (b > scale_bits + 256UL) b = scale_bits + 256UL;

    int ok = 0;
    unsigned long tries = 0UL;
    for (; tries < 12UL; ++tries) {
        if (g_split_q2 && b < right->q2) {
            mpz_set(u_num, right->t);
            mpz_mul_2exp(u_den, right->q, right->q2 - b);
        } else {
            unsigned long shift = g_split_q2 ? b - right->q2 : b;
            mpz_mul_2exp(u_num, right->t, shift);
            mpz_set(u_den, right->q);
        }
        mpz_fdiv_q(u, u_num, u_den);

        mpz_mul(prod, left->p, u);
        mpz_mul_2exp(y0, left->t, b);
        mpz_add(y0, y0, prod);
        if (mpz_sgn(y0) <= 0) break;

        mpz_mul(numerator, constant, left->q);
        unsigned long total_shift = b + (g_split_q2 ? left->q2 : 0UL);
        mpz_mul_2exp(numerator, numerator, total_shift);

        /* One-division interval certificate.  Write N=q*y0+r and let k be
           round_half_up(N/y0), the rounded upper endpoint.  The exact value
           lies above N/(y0+P).  It therefore rounds to the same k iff

             k=q:   y0+2r >= (2q-1)P,
             k=q+1: 2r-y0 >= (2q+1)P.

           This replaces the second full-size endpoint division by one
           multiplication and an exact comparison. */
        mpz_tdiv_qr(out, remainder, numerator, y0);
        mpz_mul_2exp(twice_remainder, remainder, 1UL);
        if (mpz_cmp(twice_remainder, y0) >= 0) {
            mpz_sub(margin, twice_remainder, y0);
            mpz_mul_2exp(coefficient, out, 1UL);
            mpz_add_ui(coefficient, coefficient, 1UL);
            mpz_add_ui(out, out, 1UL);
        } else {
            mpz_add(margin, y0, twice_remainder);
            mpz_mul_2exp(coefficient, out, 1UL);
            if (mpz_cmp_ui(coefficient, 0UL) == 0) break;
            mpz_sub_ui(coefficient, coefficient, 1UL);
        }
        mpz_mul(threshold, coefficient, left->p);
        if (mpz_cmp(margin, threshold) >= 0) {
            ok = 1;
            break;
        }
        if (b > ULONG_MAX - 128UL) break;
        b += 128UL;
    }

    if (tail_bits) *tail_bits = b;
    if (attempts) *attempts = tries + (ok ? 1UL : 0UL);
    mpz_clears(constant, u, u_num, u_den, y0, prod, numerator,
               remainder, twice_remainder, margin, coefficient, threshold,
               NULL);
    return ok ? 0 : -1;
}

static void print_usage(FILE *f) {
    fprintf(f,
        "polyphase_chudnovsky %s\n"
        "Usage:\n"
        "  polyphase_chudnovsky --digits N [options]\n"
        "  polyphase_chudnovsky polyphase-chudnovsky --digits N [options]\n\n"
        "Options:\n"
        "  --digits N              decimal digits after the point\n"
        "  --workers N             OpenMP workers (default: OpenMP maximum)\n"
        "  --guard-bits N          fixed-point guard bits (default: 80)\n"
        "  --packets N             exact packet count (default: automatic)\n"
        "  --packet-terms N        approximate terms per packet\n"
        "  --auto                  fastest validated policy: tuned classic tree (default)\n"
        "  --classic-tree          exact packet tree without p-adic metadata\n"
        "  --prime-band            bounded p-adic tree, primes p<=65535\n"
        "  --full-adic             complete factor-aware p-adic tree\n"
        "  --adic-gauge            experimental ordered p-adic gauge packets\n"
        "  --adic-tree             alias for --full-adic\n"
        "  --factored-upper        keep upper P/Q as exponent vectors\n"
        "  --no-adic-factors       alias for --classic-tree\n"
        "  --adic-block-terms N    approximate terms per factor-tree packet (default: 8000)\n"
        "  --adic-cancel-min-span N skip cancellation below this packet span (default: 2)\n"
        "  --adic-prime-bound N   track only primes p<=N; 0 means full (default: 0)\n"
        "  --unit-prime-bound N    normalize projective unit factors p<=N\n"
        "  --unit-min-span N       begin unit normalization at packet span N\n"
        "  --term-factor-metadata  factor terms instead of residue channels\n"
        "  --full-upper-gcd        cancel P-left/Q-right at every upper node\n"
        "  --no-p-elision          retain unused right-spine P products\n"
        "  --no-q2-split           materialize powers of two inside Q operands\n"
        "  --no-pair-phase         disable exact width-two phase cancellation\n"
        "  --no-phase-align        keep unaligned packet boundaries\n"
        "  --symbolic-top-levels N rebuild reduced operands in top N levels (default: 1)\n"
        "  --no-symbolic-root       alias for --symbolic-top-levels 0\n"
        "  --affine-root            certified precision-graded final affine phase\n"
        "  --no-affine-root-cancel skip p-adic cancellation at that boundary\n"
        "  --ratio-extra-bits N    leading ratio bits beyond output scale (default: 64)\n"
        "  --strict-final          use the complete exact Q/T ratio\n"
        "  --certified             emit only a formally certified nearest-even decimal\n"
        "  --equal-packets         balance packet term counts instead of bit height\n"
        "  --no-directed-cancel    disable selected P-left/Q-right cancellation\n"
        "  --no-channel-kernel     disable four-product top-tree scheduling\n"
        "  --no-parallel-final-mul disable exact three-channel final multiplication\n"
        "  --parallel-mul-min-bits N minimum smaller operand size for that path (default: 20000000)\n"
        "  --parallel-final-channels N force 3-channel Karatsuba or 4-channel packet\n"
        "  --cancel-blocks A,B     directed-cancellation widths (default: 64,256)\n"
        "  --output FILE           write the decimal expansion\n"
        "  --json                  print machine-readable metadata\n"
        "  --version               print version\n"
        "  --help                  print this help\n",
        POLYPHASE_CHUDNOVSKY_VERSION);
}

static int parse_cancel_blocks(const char *text, unsigned long *a, unsigned long *b) {
    char *copy = ppf_core_strdup(text);
    if (!copy) return -1;
    char *comma = strchr(copy, ',');
    if (!comma) {
        PPF_CORE_FREE(copy);
        return -1;
    }
    *comma = '\0';
    errno = 0;
    char *end1 = NULL, *end2 = NULL;
    unsigned long x = strtoul(copy, &end1, 10);
    unsigned long y = strtoul(comma + 1, &end2, 10);
    int ok = errno == 0 && end1 && *end1 == '\0' && end2 && *end2 == '\0' &&
             x > 1UL && y > 1UL;
    PPF_CORE_FREE(copy);
    if (!ok) return -1;
    *a = x;
    *b = y;
    return 0;
}

int main(int argc, char **argv) {
    mpz_init_set_str(g_chud_c3_over_24, CHUD_C3_OVER_24_TEXT, 10);
    omp_set_dynamic(0);

    int arg_start = 1;
    if (argc > 1 && strcmp(argv[1], "polyphase-chudnovsky") == 0) arg_start = 2;
    if (argc == 1) {
        print_usage(stderr);
        mpz_clear(g_chud_c3_over_24);
        return 2;
    }

    unsigned long digits = 1000UL;
    unsigned long workers = (unsigned long)omp_get_max_threads();
    unsigned long guard_bits = DEFAULT_GUARD_BITS;
    unsigned long packets = 0UL;
    unsigned long packet_terms = 0UL;
    unsigned long ratio_extra_bits = DEFAULT_RATIO_EXTRA_BITS;
    int strict_final = 0;
    int json = 0;
    const char *output_path = NULL;

    enum {
        OPT_STRICT_FINAL = 1000,
        OPT_CERTIFIED,
        OPT_AUTO,
        OPT_CLASSIC_TREE,
        OPT_PRIME_BAND,
        OPT_FULL_ADIC,
        OPT_EQUAL_PACKETS,
        OPT_NO_DIRECTED_CANCEL,
        OPT_NO_CHANNEL_KERNEL,
        OPT_RATIO_EXTRA_BITS,
        OPT_CANCEL_BLOCKS,
        OPT_ADIC_GAUGE,
        OPT_ADIC_TREE,
        OPT_FACTORED_UPPER,
        OPT_NO_ADIC_FACTORS,
        OPT_ADIC_BLOCK_TERMS,
        OPT_ADIC_CANCEL_MIN_SPAN,
        OPT_ADIC_PRIME_BOUND,
        OPT_UNIT_PRIME_BOUND,
        OPT_UNIT_MIN_SPAN,
        OPT_TERM_FACTOR_METADATA,
        OPT_FULL_UPPER_GCD,
        OPT_NO_P_ELISION,
        OPT_NO_Q2_SPLIT,
        OPT_NO_PAIR_PHASE,
        OPT_NO_PHASE_ALIGN,
        OPT_SYMBOLIC_TOP_LEVELS,
        OPT_NO_SYMBOLIC_ROOT,
        OPT_AFFINE_ROOT,
        OPT_NO_AFFINE_ROOT_CANCEL,
        OPT_NO_PARALLEL_FINAL_MUL,
        OPT_PARALLEL_MUL_MIN_BITS,
        OPT_PARALLEL_FINAL_CHANNELS
    };

    static struct option options[] = {
        {"digits", required_argument, NULL, 'd'},
        {"workers", required_argument, NULL, 'w'},
        {"guard-bits", required_argument, NULL, 'g'},
        {"packets", required_argument, NULL, 'p'},
        {"packet-terms", required_argument, NULL, 'P'},
        {"auto", no_argument, NULL, OPT_AUTO},
        {"classic-tree", no_argument, NULL, OPT_CLASSIC_TREE},
        {"prime-band", no_argument, NULL, OPT_PRIME_BAND},
        {"full-adic", no_argument, NULL, OPT_FULL_ADIC},
        {"adic-gauge", no_argument, NULL, OPT_ADIC_GAUGE},
        {"adic-tree", no_argument, NULL, OPT_ADIC_TREE},
        {"factored-upper", no_argument, NULL, OPT_FACTORED_UPPER},
        {"no-adic-factors", no_argument, NULL, OPT_NO_ADIC_FACTORS},
        {"adic-block-terms", required_argument, NULL, OPT_ADIC_BLOCK_TERMS},
        {"adic-cancel-min-span", required_argument, NULL, OPT_ADIC_CANCEL_MIN_SPAN},
        {"adic-prime-bound", required_argument, NULL, OPT_ADIC_PRIME_BOUND},
        {"unit-prime-bound", required_argument, NULL, OPT_UNIT_PRIME_BOUND},
        {"unit-min-span", required_argument, NULL, OPT_UNIT_MIN_SPAN},
        {"term-factor-metadata", no_argument, NULL, OPT_TERM_FACTOR_METADATA},
        {"full-upper-gcd", no_argument, NULL, OPT_FULL_UPPER_GCD},
        {"no-p-elision", no_argument, NULL, OPT_NO_P_ELISION},
        {"no-q2-split", no_argument, NULL, OPT_NO_Q2_SPLIT},
        {"no-pair-phase", no_argument, NULL, OPT_NO_PAIR_PHASE},
        {"no-phase-align", no_argument, NULL, OPT_NO_PHASE_ALIGN},
        {"symbolic-top-levels", required_argument, NULL, OPT_SYMBOLIC_TOP_LEVELS},
        {"no-symbolic-root", no_argument, NULL, OPT_NO_SYMBOLIC_ROOT},
        {"affine-root", no_argument, NULL, OPT_AFFINE_ROOT},
        {"no-affine-root-cancel", no_argument, NULL, OPT_NO_AFFINE_ROOT_CANCEL},
        {"ratio-extra-bits", required_argument, NULL, OPT_RATIO_EXTRA_BITS},
        {"strict-final", no_argument, NULL, OPT_STRICT_FINAL},
        {"certified", no_argument, NULL, OPT_CERTIFIED},
        {"equal-packets", no_argument, NULL, OPT_EQUAL_PACKETS},
        {"no-directed-cancel", no_argument, NULL, OPT_NO_DIRECTED_CANCEL},
        {"no-channel-kernel", no_argument, NULL, OPT_NO_CHANNEL_KERNEL},
        {"no-parallel-final-mul", no_argument, NULL, OPT_NO_PARALLEL_FINAL_MUL},
        {"parallel-mul-min-bits", required_argument, NULL, OPT_PARALLEL_MUL_MIN_BITS},
        {"parallel-final-channels", required_argument, NULL, OPT_PARALLEL_FINAL_CHANNELS},
        {"cancel-blocks", required_argument, NULL, OPT_CANCEL_BLOCKS},
        {"output", required_argument, NULL, 'o'},
        {"json", no_argument, NULL, 'j'},
        {"version", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    optind = arg_start;
    int c;
    while ((c = getopt_long(argc, argv, "d:w:g:p:P:o:jvh", options, NULL)) != -1) {
        switch (c) {
            case 'd': digits = strtoul(optarg, NULL, 10); break;
            case 'w': workers = strtoul(optarg, NULL, 10); break;
            case 'g': guard_bits = strtoul(optarg, NULL, 10); break;
            case 'p': packets = strtoul(optarg, NULL, 10); break;
            case 'P': packet_terms = strtoul(optarg, NULL, 10); break;
            case 'o': output_path = optarg; break;
            case 'j': json = 1; break;
            case 'v':
                printf("polyphase_chudnovsky %s\n", POLYPHASE_CHUDNOVSKY_VERSION);
                mpz_clear(g_chud_c3_over_24);
                return 0;
            case 'h':
                print_usage(stdout);
                mpz_clear(g_chud_c3_over_24);
                return 0;
            case OPT_STRICT_FINAL: strict_final = 1; break;
            case OPT_CERTIFIED: g_certified_decimal = 1; break;
            case OPT_AUTO:
                g_auto_mode = 1;
                break;
            case OPT_CLASSIC_TREE:
                g_auto_mode = 0;
                g_adic_factors = 0;
                g_adic_gauge = 0;
                g_factored_upper = 0;
                break;
            case OPT_PRIME_BAND:
                g_auto_mode = 0;
                g_adic_factors = 1;
                g_adic_gauge = 0;
                g_factored_upper = 0;
                g_adic_prime_bound = AUTO_PRIME_BAND_BOUND;
                g_symbolic_top_levels = 0UL;
                g_channel_metadata = 1;
                break;
            case OPT_FULL_ADIC:
                g_auto_mode = 0;
                g_adic_factors = 1;
                g_adic_gauge = 0;
                g_factored_upper = 0;
                g_adic_prime_bound = 0UL;
                break;
            case OPT_EQUAL_PACKETS: g_weighted_packets = 0; break;
            case OPT_NO_DIRECTED_CANCEL: g_directed_cancel = 0; break;
            case OPT_NO_CHANNEL_KERNEL: g_channel_kernel = 0; break;
            case OPT_NO_PARALLEL_FINAL_MUL: g_parallel_final_mul = 0; break;
            case OPT_PARALLEL_MUL_MIN_BITS:
                g_parallel_mul_min_bits = strtoul(optarg, NULL, 10);
                break;
            case OPT_PARALLEL_FINAL_CHANNELS:
                g_parallel_final_channels = strtoul(optarg, NULL, 10);
                if (g_parallel_final_channels != 3UL &&
                    g_parallel_final_channels != 4UL) {
                    fprintf(stderr, "--parallel-final-channels must be 3 or 4\n");
                    mpz_clear(g_chud_c3_over_24);
                    return 2;
                }
                break;
            case OPT_ADIC_GAUGE:
                g_auto_mode = 0;
                g_adic_factors = 1;
                g_adic_gauge = 1;
                g_factored_upper = 0;
                break;
            case OPT_ADIC_TREE:
                g_auto_mode = 0;
                g_adic_factors = 1;
                g_adic_gauge = 0;
                g_factored_upper = 0;
                break;
            case OPT_FACTORED_UPPER:
                g_auto_mode = 0;
                g_adic_factors = 1;
                g_adic_gauge = 0;
                g_factored_upper = 1;
                break;
            case OPT_NO_ADIC_FACTORS:
                g_auto_mode = 0;
                g_adic_factors = 0;
                g_adic_gauge = 0;
                g_factored_upper = 0;
                break;
            case OPT_ADIC_BLOCK_TERMS:
                g_adic_block_terms = strtoul(optarg, NULL, 10);
                break;
            case OPT_ADIC_CANCEL_MIN_SPAN:
                g_adic_cancel_min_span = strtoul(optarg, NULL, 10);
                break;
            case OPT_ADIC_PRIME_BOUND:
                g_auto_mode = 0;
                g_adic_factors = 1;
                g_adic_gauge = 0;
                g_factored_upper = 0;
                g_adic_prime_bound = strtoul(optarg, NULL, 10);
                break;
            case OPT_UNIT_PRIME_BOUND:
                g_unit_prime_bound = strtoul(optarg, NULL, 10);
                break;
            case OPT_UNIT_MIN_SPAN:
                g_unit_min_span = strtoul(optarg, NULL, 10);
                break;
            case OPT_TERM_FACTOR_METADATA:
                g_channel_metadata = 0;
                break;
            case OPT_FULL_UPPER_GCD:
                g_full_upper_gcd = 1;
                break;
            case OPT_NO_P_ELISION:
                g_p_elision = 0;
                break;
            case OPT_NO_Q2_SPLIT: g_split_q2 = 0; break;
            case OPT_NO_PAIR_PHASE: g_pair_phase = 0; break;
            case OPT_NO_PHASE_ALIGN: g_phase_align = 0; break;
            case OPT_SYMBOLIC_TOP_LEVELS:
                g_symbolic_top_levels = strtoul(optarg, NULL, 10);
                break;
            case OPT_NO_SYMBOLIC_ROOT: g_symbolic_top_levels = 0UL; break;
            case OPT_AFFINE_ROOT: g_affine_root = 1; break;
            case OPT_NO_AFFINE_ROOT_CANCEL: g_affine_root_cancel = 0; break;
            case OPT_RATIO_EXTRA_BITS: ratio_extra_bits = strtoul(optarg, NULL, 10); break;
            case OPT_CANCEL_BLOCKS:
                if (parse_cancel_blocks(optarg, &g_cancel_block1, &g_cancel_block2) != 0) {
                    fprintf(stderr, "invalid --cancel-blocks value: %s\n", optarg);
                    mpz_clear(g_chud_c3_over_24);
                    return 2;
                }
                break;
            default:
                print_usage(stderr);
                mpz_clear(g_chud_c3_over_24);
                return 2;
        }
    }

    if (digits == 0UL || workers == 0UL || guard_bits < 16UL ||
        ratio_extra_bits < 32UL || g_parallel_mul_min_bits < 64UL ||
        g_adic_block_terms == 0UL ||
        g_adic_cancel_min_span == 0UL || g_unit_min_span == 0UL) {
        fprintf(stderr, "digits/workers/adic block terms must be positive, guard bits >=16, ratio extra bits >=32\n");
        mpz_clear(g_chud_c3_over_24);
        return 2;
    }
    if (packets > 0UL && packet_terms > 0UL) {
        fprintf(stderr, "use either --packets or --packet-terms, not both\n");
        mpz_clear(g_chud_c3_over_24);
        return 2;
    }

    if (g_certified_decimal) {
        /* Formal certification needs the root P channel to append the first
           omitted term.  A larger default guard makes ambiguous midpoint
           cases extraordinarily rare, but the final decision is still made
           by the exact interval test rather than by this guard assumption. */
        g_p_elision = 0;
        g_affine_root = 0;
        if (guard_bits < 160UL) guard_bits = 160UL;
    }

    if (g_auto_mode) {
        /* The bounded p-adic tree remains exact, but its exact-division cost
           did not beat the tuned classic tree through 50,000,000 digits.
           The production auto policy therefore chooses the fastest validated
           path rather than extrapolating an unobserved crossover. */
        g_auto_selected_band = 0;
        g_adic_factors = 0;
        g_adic_gauge = 0;
        g_factored_upper = 0;
        g_adic_prime_bound = 0UL;
    }

    run_result_t result;
    int rc = compute_polyphase_chudnovsky(&result, digits, guard_bits, workers,
                                          packets, packet_terms, ratio_extra_bits,
                                          strict_final);
    if (rc == 2) {
        fprintf(stderr,
                "formal rounding interval was not decisive; rerun with more --guard-bits\n");
        PPF_CORE_FREE(result.certified_text);
        mpz_clear(result.value);
        mpz_clear(g_chud_c3_over_24);
        return 3;
    }
    if (rc != 0) {
        mpz_clear(g_chud_c3_over_24);
        return 1;
    }

    if (output_path) {
        char *owned_text = NULL;
        const char *text = result.certified_decimal
            ? result.certified_text
            : (owned_text = scaled_to_decimal(result.value, result.scale_bits, digits));
        if (!text) {
            perror(result.certified_decimal ? "certified decimal output" : "scaled_to_decimal");
            PPF_CORE_FREE(owned_text);
            PPF_CORE_FREE(result.certified_text);
            mpz_clear(result.value);
            mpz_clear(g_chud_c3_over_24);
            return 1;
        }
        FILE *f = fopen(output_path, "wb");
        if (!f) {
            perror(output_path);
            PPF_CORE_FREE(owned_text);
            PPF_CORE_FREE(result.certified_text);
            mpz_clear(result.value);
            mpz_clear(g_chud_c3_over_24);
            return 1;
        }
        fprintf(f, "%s\n", text);
        fclose(f);
        PPF_CORE_FREE(owned_text);
    }

    if (json || !output_path) {
        long certified_margin = (long)ratio_extra_bits - 28L;
        char ratio_error_text[64];
        if (g_certified_decimal) {
            strcpy(ratio_error_text, "not-used-exact-interval");
        } else {
            snprintf(ratio_error_text, sizeof(ratio_error_text),
                     "2^-%ld", certified_margin);
        }
        const char *adic_mode = !g_adic_factors ? "classic" :
            (g_adic_gauge ? "ordered-gauge" :
             (g_factored_upper ? "factored-upper" :
              (result.adic_metadata_complete ? "full-factor-tree" : "bounded-prime-tree")));
        const char *execution_policy = g_auto_mode
            ? (g_auto_selected_band ? "auto-prime-band" : "auto-classic")
            : adic_mode;
        printf("{\"method\":\"polyphase-chudnovsky\","
               "\"version\":\"%s\",\"digits\":%lu,\"scale_bits\":%lu,"
               "\"terms\":%lu,\"packets\":%lu,\"max_packet_width\":%lu,"
               "\"workers\":%lu,\"weighted_packets\":%s,"
               "\"directed_cancel\":%s,\"cancel_blocks\":\"%lu,%lu\","
               "\"channel_kernel\":%s,\"full_upper_gcd\":%s,"
               "\"p_elision\":%s,"
               "\"q2_split\":%s,\"pair_phase\":%s,\"phase_aligned\":%s,"
               "\"symbolic_top_levels\":%lu,"
               "\"final_mode\":\"%s\","
               "\"parallel_final_mul\":%s,\"parallel_final_mul_used\":%s,"
               "\"parallel_mul_min_bits\":%lu,\"parallel_final_mul_channels\":%lu,"
               "\"adic_factors\":%s,\"adic_mode\":\"%s\","
               "\"execution_policy\":\"%s\",\"auto_policy\":%s,"
               "\"metadata_mode\":\"%s\",\"adic_metadata_complete\":%s,"
               "\"adic_block_terms\":%lu,\"adic_cancel_min_span\":%lu,"
               "\"adic_prime_bound\":%lu,"
               "\"unit_prime_bound\":%lu,\"unit_min_span\":%lu,"
               "\"sieve_limit\":%lu,"
               "\"sieve_prime_count\":%lu,\"root_q_bits\":%lu,"
               "\"root_q_odd_bits\":%lu,\"root_q2_bits\":%lu,"
               "\"root_t_bits\":%lu,\"max_gauge_bits\":%lu,"
               "\"affine_root\":%s,\"affine_tail_bits\":%lu,"
               "\"affine_attempts\":%lu,\"affine_left_q_bits\":%lu,"
               "\"affine_right_q_bits\":%lu,"
               "\"pair_phase_removed_bits\":%.3f,\"pair_phase_events\":%llu,"
               "\"adic_removed_bits\":%.3f,\"adic_cancellation_events\":%llu,"
               "\"adic_cancelled_prime_powers\":%llu,"
               "\"ratio_extra_bits\":%lu,\"ratio_keep_bits\":%lu,"
               "\"ratio_shift_bits\":%lu,"
               "\"ratio_error_below_binary_units\":\"%s\","
               "\"formal_certified\":%s,\"rounding_mode\":\"%s\","
               "\"certificate_decisive\":%s,"
               "\"certificate_interval_width_bits\":%lu,"
               "\"certificate_seconds\":%.9f,"
               "\"boundary_seconds\":%.9f,\"sieve_seconds\":%.9f,"
               "\"factor_metadata_seconds\":%.9f,\"gauge_seconds\":%.9f,"
               "\"factor_cancel_seconds\":%.9f,\"packet_seconds\":%.9f,"
               "\"reduction_seconds\":%.9f,\"final_seconds\":%.9f,"
               "\"final_mul_seconds\":%.9f,\"final_div_seconds\":%.9f,"
               "\"kernel_seconds\":%.9f,\"total_seconds\":%.9f}\n",
               POLYPHASE_CHUDNOVSKY_VERSION, result.digits, result.scale_bits,
               result.terms, result.packets, result.max_packet_width,
               workers, g_weighted_packets ? "true" : "false",
               g_directed_cancel ? "true" : "false",
               g_cancel_block1, g_cancel_block2,
               g_channel_kernel ? "true" : "false",
               g_full_upper_gcd ? "true" : "false",
               g_p_elision ? "true" : "false",
               g_split_q2 ? "true" : "false",
               g_pair_phase ? "true" : "false",
               (g_pair_phase && g_phase_align) ? "true" : "false",
               g_symbolic_top_levels,
               g_certified_decimal ? "formal-decimal-nearest-even" :
                   (strict_final ? "strict-full-ratio" :
                    (g_affine_root ? "affine-root-certified" : "high-limb-certified")),
               g_parallel_final_mul ? "true" : "false",
               result.parallel_final_mul_used ? "true" : "false",
               g_parallel_mul_min_bits,
               result.parallel_final_mul_channels,
               g_adic_factors ? "true" : "false", adic_mode,
               execution_policy, g_auto_mode ? "true" : "false",
               g_channel_metadata ? "residue-channels" : "term-factorization",
               result.adic_metadata_complete ? "true" : "false",
               result.adic_block_terms, g_adic_cancel_min_span,
               result.adic_prime_bound, g_unit_prime_bound, g_unit_min_span, result.sieve_limit,
               result.sieve_prime_count, result.root_q_bits,
               result.root_q_odd_bits, result.root_q2_bits,
               result.root_t_bits, result.max_gauge_bits,
               result.affine_root ? "true" : "false",
               result.affine_tail_bits, result.affine_attempts,
               result.affine_left_q_bits, result.affine_right_q_bits,
               result.pair_phase_removed_bits,
               (unsigned long long)result.pair_phase_events,
               result.adic_removed_bits,
               (unsigned long long)result.adic_cancellation_events,
               (unsigned long long)result.adic_cancelled_prime_powers,
               result.ratio_extra_bits, result.ratio_keep_bits,
               result.ratio_shift_bits, ratio_error_text,
               result.certified_decimal ? "true" : "false",
               result.certified_decimal ? "nearest-even" : "fixed-guard",
               result.certificate_decisive ? "true" : "false",
               result.certificate_interval_width_bits,
               result.certificate_seconds,
               result.boundary_seconds, result.sieve_seconds,
               result.factor_metadata_seconds, result.gauge_seconds,
               result.factor_cancel_seconds, result.packet_seconds,
               result.reduction_seconds, result.final_seconds,
               result.final_mul_seconds, result.final_div_seconds,
               result.kernel_seconds, result.total_seconds);
    }

    PPF_CORE_FREE(result.certified_text);
    mpz_clear(result.value);
    mpz_clear(g_chud_c3_over_24);
    return 0;
}
