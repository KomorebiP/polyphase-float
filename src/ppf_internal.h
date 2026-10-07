/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef PPF_INTERNAL_H
#define PPF_INTERNAL_H

#include "polyphase_float.h"

#ifndef PPF_BUILDING_ARITHMETIC_ROOT_TRANSACTION_WRAPPERS
#include "transactional_arithmetic_root_names.h"
#endif

#include <limits.h>
#include <stdalign.h>
#include <string.h>

#define PPF_LIMB_BITS ((unsigned)GMP_NUMB_BITS)

size_t ppf_limbs_for_precision(ppf_prec_t precision);
int ppf_precision_valid(ppf_prec_t precision);
int ppf_rounding_valid(ppf_rnd_t rnd);
void ppf_raise(ppf_context_t *ctx, ppf_flags_t flags);

static inline int ppf_allocation_error(ppf_t *rop, ppf_context_t *ctx) {
    /* Resource failure is transactional.  The caller keeps any initialized
       destination unchanged and receives an explicit sticky allocation flag. */
    (void)rop;
    ppf_raise(ctx, PPF_FLAG_ALLOC);
    return PPF_TERNARY_ERROR;
}
ppf_context_t *ppf_resolve_context(ppf_context_t *ctx);

/* Context-owned project allocations.  These helpers never intercept GMP's
   process-global allocator.  A failed project allocation raises
   PPF_FLAG_ALLOC in the resolved context. */
void *ppf_context_malloc_internal(ppf_context_t *ctx, size_t bytes);
void *ppf_context_calloc_internal(ppf_context_t *ctx, size_t count,
                                  size_t bytes);
void *ppf_context_realloc_internal(ppf_context_t *ctx, void *ptr,
                                   size_t old_bytes, size_t new_bytes);
void ppf_context_free_internal(ppf_context_t *ctx, void *ptr, size_t bytes);
uint64_t ppf_allocation_failure_epoch_internal(void);
void ppf_note_allocation_failure_internal(void);

/* Strings returned by ppf_get_str carry the allocator provenance needed by
   ppf_free_str.  The caller-visible pointer addresses only the character
   payload; the private prefix is released through the allocator that created
   it. */
char *ppf_owned_string_alloc_internal(ppf_context_t *ctx, size_t bytes);
void ppf_owned_string_free_internal(char *text);

/* Provenance-carrying temporary buffers.  These are intended for internal
   arrays whose final free site does not naturally retain the original byte
   count or context.  The private prefix records the exact allocator contract,
   so custom allocators remain balanced and deterministic failure injection can
   cover the complete project-controlled work-array path. */
void *ppf_owned_buffer_alloc_internal(ppf_context_t *ctx, size_t bytes);
void *ppf_owned_buffer_calloc_internal(ppf_context_t *ctx, size_t count,
                                       size_t bytes);
void *ppf_owned_buffer_realloc_internal(ppf_context_t *ctx, void *ptr,
                                        size_t bytes);
void ppf_owned_buffer_free_internal(void *ptr);


#define PPF_RESULT_TRANSACTION_INLINE_LIMBS 16u

typedef struct {
    ppf_t staged;
    ppf_context_t *ctx;
    uint64_t failure_epoch;
    ppf_flags_t saved_alloc_flag;
    ppf_context_t *previous_active_context;
    int active;
    int allocation_failed;
    /* Common precisions stage in caller-stack storage, avoiding one heap
       allocation on every public arithmetic call.  Larger destinations keep
       the same allocator-backed path used by earlier transactions. */
    mp_limb_t inline_limbs[PPF_RESULT_TRANSACTION_INLINE_LIMBS];
} ppf_result_transaction_t;

/* Stage a public numerical result in independently owned storage.  Finish
   commits without allocation while preserving the destination's storage
   ownership, including application-managed external significands. */
int ppf_result_transaction_begin_internal(ppf_result_transaction_t *tx,
                                          ppf_t *destination,
                                          ppf_context_t *ctx);
int ppf_result_transaction_begin_inline_internal(
    ppf_result_transaction_t *tx, ppf_t *destination, ppf_context_t *ctx);
int ppf_result_transaction_finish_internal(ppf_result_transaction_t *tx,
                                           ppf_t *destination,
                                           int result);
void ppf_result_transaction_abort_internal(ppf_result_transaction_t *tx);
int ppf_result_transaction_pair_finish_internal(
    ppf_result_transaction_t *first, ppf_t *first_destination,
    ppf_result_transaction_t *second, ppf_t *second_destination,
    int result);
int ppf_result_transaction_many_finish_internal(
    ppf_result_transaction_t transactions[], ppf_t *destinations[],
    size_t count, int result);
ppf_context_t *ppf_result_transaction_context_internal(void);

#include "transactional_impl_prototypes.h"

int ppf_exp_add(ppf_exp_t a, ppf_exp_t b, ppf_exp_t *out);
int ppf_exp_sub_prec(ppf_exp_t a, ppf_prec_t b, ppf_exp_t *out);

int ppf_store_normalized_mpz(ppf_t *x, mpz_srcptr magnitude,
                             ppf_exp_t exponent, int sign);
int ppf_set_max_finite(ppf_t *x, int sign, const ppf_context_t *ctx);
int ppf_set_min_normal(ppf_t *x, int sign, const ppf_context_t *ctx);
int ppf_same_value(const ppf_t *a, const ppf_t *b);
int ppf_compare_magnitudes(const ppf_t *a, const ppf_t *b);
int ppf_compare_exact_to_object(mpz_srcptr value, ppf_exp_t exponent,
                                const ppf_t *object);

void ppf_zero_unused_bits(ppf_t *x);


/* Round sign * magnitude * 2^base_exponent directly into an initialized
   independent floating object.  external_sticky means the exact magnitude
   is strictly larger than magnitude by less than one unit at base_exponent. */
int ppf_round_limbs_2exp(ppf_t *rop,
                         mp_srcptr magnitude,
                         size_t used,
                         int sign,
                         ppf_exp_t base_exponent,
                         int external_sticky,
                         ppf_rnd_t rnd,
                         ppf_context_t *ctx);

/* Exact finite value extraction used by the arithmetic layer. */
int ppf_get_exact_dyadic(mpz_ptr significand, ppf_exp_t *exponent,
                         const ppf_t *op);

/* Exact direct-limb rank-5 Toom-3 product graph.  Returns the number of
   product tasks used, or zero after falling back to a full GMP product. */
unsigned long ppf_toom3_limb_product_internal(
    mp_ptr out, mp_srcptr a, mp_srcptr b, mp_size_t n,
    unsigned long workers, int square);
void ppf_toom3_limb_cleanup_thread(void);
int ppf_product_plan_internal(
    ppf_product_plan_t *plan, const ppf_t *a, const ppf_t *b,
    ppf_product_operation_t operation, unsigned long workers,
    ppf_product_algorithm_t requested);
void ppf_context_release_pool_internal(ppf_context_t *ctx);
/* Register heap-backed thread-local state for automatic POSIX thread-exit
   cleanup.  This is idempotent and does not allocate through a numerical
   context. */
void ppf_thread_state_touch(void);
void ppf_transcendental_cache_purge(void);

int ppf_window_product_round_internal(
    ppf_t *rop, const ppf_t *a, const ppf_t *b,
    ppf_rnd_t rnd, unsigned long workers, int square,
    int *ternary_out, ppf_context_t *ctx);

/* Certified fixed-point transcendental fast paths.  Return nonzero only when
   the result and ternary direction have been decided; otherwise callers keep
   the existing native interval evaluator as a correctness fallback. */
int ppf_fast_log_internal(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                          ppf_context_t *ctx, int *ternary_out);
int ppf_fast_log_base_internal(ppf_t *rop, const ppf_t *x, int base,
                               ppf_rnd_t rnd, ppf_context_t *ctx,
                               int *ternary_out);
int ppf_fast_log1p_internal(ppf_t *rop, const ppf_t *x, int base,
                            ppf_rnd_t rnd, ppf_context_t *ctx,
                            int *ternary_out);
int ppf_fast_exp_interval_internal(ppf_t *lo, ppf_t *hi,
                                   const ppf_t *x, ppf_prec_t work,
                                   ppf_context_t *ctx);
int ppf_fast_exp_internal(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                          ppf_context_t *ctx, int *ternary_out);
int ppf_fast_exp_scaled_internal(ppf_t *rop, const ppf_t *x, int base,
                                 int minus_one, ppf_rnd_t rnd,
                                 ppf_context_t *ctx, int *ternary_out);
int ppf_fast_pow_internal(ppf_t *rop, const ppf_t *base,
                          const ppf_t *exponent, int result_sign,
                          ppf_rnd_t rnd, ppf_context_t *ctx,
                          int *ternary_out);

/* Fixed-point shared sine/cosine kernel for an already reduced interval. */
int ppf_fast_sincos_reduced_internal(ppf_t *sin_lo, ppf_t *sin_hi,
                                     ppf_t *cos_lo, ppf_t *cos_hi,
                                     const ppf_t *r_lo, const ppf_t *r_hi,
                                     ppf_prec_t work, ppf_context_t *ctx);
int ppf_fast_atan_internal(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                           ppf_context_t *ctx, int *ternary_out);
int ppf_fast_atan_halley_internal(ppf_t *rop, const ppf_t *x,
                                  ppf_rnd_t rnd, ppf_context_t *ctx,
                                  int *ternary_out);
enum {
    PPF_ATAN_Q_POS = 0,
    PPF_ATAN_Q_NEG = 1,
    PPF_ATAN_Q_PI_MINUS = 2,
    PPF_ATAN_Q_NEG_PI_PLUS = 3
};

int ppf_fast_atan_ratio_internal(ppf_t *rop,
                                 const ppf_t *numerator,
                                 const ppf_t *denominator,
                                 int quadrant, ppf_rnd_t rnd,
                                 ppf_context_t *ctx, int *ternary_out);
int ppf_fast_atan_positive_internal(ppf_t *out_lo, ppf_t *out_hi,
                                    const ppf_t *x_lo, const ppf_t *x_hi,
                                    ppf_prec_t work, ppf_context_t *ctx);
int ppf_fast_atan_reduced_internal(ppf_t *out_lo, ppf_t *out_hi,
                                   const ppf_t *x_lo, const ppf_t *x_hi,
                                   ppf_prec_t work, ppf_context_t *ctx);
int ppf_fast_sinhcosh_internal(ppf_t *sinh_lo, ppf_t *sinh_hi,
                               ppf_t *cosh_lo, ppf_t *cosh_hi,
                               const ppf_t *x_lo, const ppf_t *x_hi,
                               ppf_prec_t work, ppf_context_t *ctx);
int ppf_fast_hyperbolic_from_exp_internal(ppf_t *out_lo,
                                          ppf_t *out_hi,
                                          const ppf_t *u_lo,
                                          const ppf_t *u_hi,
                                          int fn, int negative,
                                          ppf_prec_t work,
                                          ppf_context_t *ctx);


#endif
