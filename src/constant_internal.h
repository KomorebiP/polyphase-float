/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef PPF_CONSTANT_INTERNAL_H
#define PPF_CONSTANT_INTERNAL_H

#include "ppf_internal.h"

#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

typedef struct {
    mpz_t lower;
    mpz_t upper;
    ppf_exp_t exponent;
} ppf_constant_interval_t;

typedef struct {
    pthread_mutex_t mutex;
    ppf_constant_interval_t interval;
    ppf_prec_t certified_precision;
    ppf_t rounded[6];
    ppf_prec_t rounded_precision[6];
    int rounded_ternary[6];
    unsigned char rounded_initialized[6];
    unsigned char rounded_valid[6];
    int initialized;
    int valid;
} ppf_constant_cache_t;

typedef int (*ppf_constant_builder_fn)(
    ppf_constant_interval_t *out,
    ppf_prec_t requested_precision,
    unsigned long workers,
    ppf_constant_stats_t *stats,
    ppf_context_t *ctx
);

typedef struct {
    ppf_constant_id_t id;
    const char *name;
    ppf_exp_t normal_exponent;
    ppf_exp_t required_emin;
    ppf_exp_t required_emax;
    ppf_constant_builder_fn build_interval;
    ppf_constant_cache_t *cache;
} ppf_constant_spec_t;

void ppf_constant_interval_init(ppf_constant_interval_t *x);
void ppf_constant_interval_clear(ppf_constant_interval_t *x);
void ppf_constant_interval_set(ppf_constant_interval_t *dst,
                               const ppf_constant_interval_t *src);
void ppf_constant_interval_swap(ppf_constant_interval_t *a,
                                ppf_constant_interval_t *b);
ppf_prec_t ppf_constant_interval_precision(
    const ppf_constant_interval_t *x);

void ppf_constant_cache_init(ppf_constant_cache_t *cache);
void ppf_constant_cache_reset(ppf_constant_cache_t *cache);
void ppf_constant_cache_purge(ppf_constant_cache_t *cache);

unsigned long ppf_constant_resolve_workers(unsigned long workers);
double ppf_constant_now(void);

int ppf_evaluate_constant_spec(const ppf_constant_spec_t *spec,
                               ppf_t *rop, ppf_rnd_t rnd,
                               unsigned long workers, int use_cache,
                               ppf_context_t *ctx);
int ppf_precompute_constant_spec(const ppf_constant_spec_t *spec,
                                 ppf_prec_t precision,
                                 unsigned long workers);
int ppf_export_constant_interval_spec(const ppf_constant_spec_t *spec,
                                      mpz_ptr lower, mpz_ptr upper,
                                      ppf_exp_t *exponent,
                                      ppf_prec_t precision,
                                      unsigned long workers);

const ppf_constant_spec_t *ppf_pi_constant_spec(void);
const ppf_constant_spec_t *ppf_log2_constant_spec(void);
const ppf_constant_spec_t *ppf_euler_constant_spec(void);
const ppf_constant_spec_t *ppf_catalan_constant_spec(void);

void ppf_set_last_constant_stats(const ppf_constant_stats_t *stats);

#endif
