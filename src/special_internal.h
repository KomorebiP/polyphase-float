/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef PPF_SPECIAL_INTERNAL_H
#define PPF_SPECIAL_INTERNAL_H

#include "ppf_internal.h"
#include "constant_internal.h"

#include <stddef.h>

typedef struct {
    ppf_t lo;
    ppf_t hi;
    int initialized;
} ppf_special_iv_t;

typedef int (*ppf_special_builder_fn)(ppf_special_iv_t *out,
                                      ppf_prec_t work,
                                      void *opaque,
                                      ppf_context_t *ctx);

int ppf_special_iv_init(ppf_special_iv_t *x, ppf_prec_t precision,
                        ppf_context_t *ctx);
void ppf_special_iv_clear(ppf_special_iv_t *x);
int ppf_special_iv_set_exact(ppf_special_iv_t *out, const ppf_t *x,
                             ppf_context_t *ctx);
int ppf_special_iv_set_ui(ppf_special_iv_t *out, unsigned long x,
                          ppf_context_t *ctx);
int ppf_special_iv_set_si(ppf_special_iv_t *out, long x,
                          ppf_context_t *ctx);
int ppf_special_iv_set_q(ppf_special_iv_t *out, mpq_srcptr x,
                         ppf_context_t *ctx);
int ppf_special_iv_copy(ppf_special_iv_t *out, const ppf_special_iv_t *x,
                        ppf_context_t *ctx);
int ppf_special_iv_add(ppf_special_iv_t *out,
                       const ppf_special_iv_t *a,
                       const ppf_special_iv_t *b,
                       ppf_context_t *ctx);
int ppf_special_iv_sub(ppf_special_iv_t *out,
                       const ppf_special_iv_t *a,
                       const ppf_special_iv_t *b,
                       ppf_context_t *ctx);
int ppf_special_iv_neg(ppf_special_iv_t *out,
                       const ppf_special_iv_t *x,
                       ppf_context_t *ctx);
int ppf_special_iv_mul(ppf_special_iv_t *out,
                       const ppf_special_iv_t *a,
                       const ppf_special_iv_t *b,
                       ppf_context_t *ctx);
int ppf_special_iv_div(ppf_special_iv_t *out,
                       const ppf_special_iv_t *a,
                       const ppf_special_iv_t *b,
                       ppf_context_t *ctx);
int ppf_special_iv_scale2(ppf_special_iv_t *out,
                          const ppf_special_iv_t *x,
                          long shift, ppf_context_t *ctx);
int ppf_special_iv_sqrt(ppf_special_iv_t *out,
                        const ppf_special_iv_t *x,
                        ppf_context_t *ctx);
int ppf_special_iv_abs(ppf_special_iv_t *out,
                       const ppf_special_iv_t *x,
                       ppf_context_t *ctx);
int ppf_special_iv_exp(ppf_special_iv_t *out,
                       const ppf_special_iv_t *x,
                       ppf_context_t *ctx);
int ppf_special_iv_log_positive(ppf_special_iv_t *out,
                                const ppf_special_iv_t *x,
                                ppf_context_t *ctx);
int ppf_special_iv_pow_positive(ppf_special_iv_t *out,
                                const ppf_special_iv_t *base,
                                const ppf_special_iv_t *exponent,
                                ppf_context_t *ctx);
/* A nonnegative remainder bounded above by tail.hi widens only the upper end. */
int ppf_special_iv_add_positive_tail(ppf_special_iv_t *out,
                                     const ppf_special_iv_t *sum,
                                     const ppf_special_iv_t *tail,
                                     ppf_context_t *ctx);
/* Gamma is decreasing throughout the supported argument interval (0, 1]. */
int ppf_special_iv_gamma_decreasing(ppf_special_iv_t *out,
                                    const ppf_special_iv_t *argument,
                                    ppf_context_t *ctx);
int ppf_special_iv_to_dyadic(mpz_ptr lower, mpz_ptr upper,
                             ppf_exp_t *exponent,
                             const ppf_special_iv_t *x);
int ppf_special_evaluate(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx,
                         ppf_special_builder_fn builder, void *opaque);
ppf_prec_t ppf_special_grow(ppf_prec_t p);
int ppf_special_small(const ppf_t *x, ppf_prec_t work, unsigned guard);
int ppf_special_is_integer(const ppf_t *x);
int ppf_special_get_long_if_integer(const ppf_t *x, long *out,
                                    ppf_context_t *ctx);
int ppf_special_bernoulli(mpq_t *out, unsigned max_index);
void ppf_special_bernoulli_clear(mpq_t *values, unsigned max_index);
int ppf_special_pi_interval(ppf_special_iv_t *out, ppf_context_t *ctx);
int ppf_special_euler_interval(ppf_special_iv_t *out, ppf_prec_t work,
                               ppf_context_t *ctx);
int ppf_special_catalan_interval(ppf_special_iv_t *out, ppf_prec_t work,
                                 ppf_context_t *ctx);

const ppf_constant_spec_t *ppf_euler_constant_spec(void);
const ppf_constant_spec_t *ppf_catalan_constant_spec(void);

#endif
