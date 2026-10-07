/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <limits.h>
#include <math.h>
#include "ppf_openmp_compat.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define PPF_TRIG_UNUSED __attribute__((unused))
#else
#define PPF_TRIG_UNUSED
#endif

#ifndef PPF_FAST_TRIG_GUARD
#define PPF_FAST_TRIG_GUARD 112u
#endif
#ifndef PPF_FAST_TRIG_TAIL_GUARD
#define PPF_FAST_TRIG_TAIL_GUARD 40u
#endif
#ifndef PPF_FAST_ATAN_REDUCTIONS
#define PPF_FAST_ATAN_REDUCTIONS 5u
#endif
#ifndef PPF_FAST_TRIG_MAX_TERMS
#define PPF_FAST_TRIG_MAX_TERMS 1000000u
#endif
#ifndef PPF_FAST_TRIG_DENSE_BS_EXPERIMENT
#define PPF_FAST_TRIG_DENSE_BS_EXPERIMENT 0
#endif
#ifndef PPF_FAST_TRIG_TARGET_BITS
#define PPF_FAST_TRIG_TARGET_BITS 8u
#endif
#ifndef PPF_FAST_TRIG_HALVING_BIAS
#define PPF_FAST_TRIG_HALVING_BIAS 4u
#endif

/* A closed fixed-point interval [lo,hi] * 2^-scale. */
typedef struct {
    mpz_t lo;
    mpz_t hi;
} ppf_trig_fx_t;

static _Thread_local unsigned long g_ppf_trig_workers = 1UL;

void ppf_set_trig_threads(unsigned long workers) {
    g_ppf_trig_workers = workers ? workers : 1UL;
}

unsigned long ppf_get_trig_threads(void) {
    return g_ppf_trig_workers;
}

static void tfx_init(ppf_trig_fx_t *x) { mpz_inits(x->lo, x->hi, NULL); }
static void tfx_clear(ppf_trig_fx_t *x) { mpz_clears(x->lo, x->hi, NULL); }

static int tfx_exp_plus_scale(ppf_exp_t exponent, ppf_prec_t scale,
                              int *negative, uint64_t *magnitude) {
#if defined(__SIZEOF_INT128__)
    __extension__ __int128 v = (__int128)exponent + (__int128)scale;
    if (v < 0) {
        __extension__ unsigned __int128 m = (unsigned __int128)(-v);
        if (m > UINT64_MAX) return -1;
        *negative = 1;
        *magnitude = (uint64_t)m;
    } else {
        __extension__ unsigned __int128 m = (unsigned __int128)v;
        if (m > UINT64_MAX) return -1;
        *negative = 0;
        *magnitude = (uint64_t)m;
    }
    return 0;
#else
    if (exponent >= 0) {
        uint64_t e = (uint64_t)exponent;
        if (e > UINT64_MAX - scale) return -1;
        *negative = 0;
        *magnitude = e + scale;
        return 0;
    }
    {
        uint64_t m = (uint64_t)(-(exponent + 1)) + 1u;
        if (m > scale) {
            *negative = 1;
            *magnitude = m - scale;
        } else {
            *negative = 0;
            *magnitude = scale - m;
        }
    }
    return 0;
#endif
}

/* Directed conversion of an exact ppf_t dyadic to an integer at scale. */
static int tfx_from_ppf(mpz_ptr out, const ppf_t *x, ppf_prec_t scale,
                        int upper) {
    mpz_t z;
    ppf_exp_t exponent;
    int neg_shift;
    uint64_t shift;
    int rc = -1;
    if (!x || !ppf_regular_p(x)) return -1;
    mpz_init(z);
    exponent = ppf_get_z_2exp(z, x);
    if (tfx_exp_plus_scale(exponent, scale, &neg_shift, &shift) != 0 ||
        shift > (uint64_t)ULONG_MAX)
        goto done;
    if (!neg_shift) {
        mpz_mul_2exp(out, z, (mp_bitcnt_t)shift);
    } else if (upper) {
        mpz_cdiv_q_2exp(out, z, (mp_bitcnt_t)shift);
    } else {
        mpz_fdiv_q_2exp(out, z, (mp_bitcnt_t)shift);
    }
    rc = 0;
done:
    mpz_clear(z);
    return rc;
}

static void tfx_mul_positive(ppf_trig_fx_t *out,
                             const ppf_trig_fx_t *a,
                             const ppf_trig_fx_t *b,
                             ppf_prec_t scale,
                             mpz_ptr scratch) {
    if (mpz_cmp(a->lo, a->hi) == 0 && mpz_cmp(b->lo, b->hi) == 0) {
        mpz_mul(scratch, a->lo, b->lo);
        mpz_fdiv_q_2exp(out->lo, scratch, (mp_bitcnt_t)scale);
        mpz_cdiv_q_2exp(out->hi, scratch, (mp_bitcnt_t)scale);
        return;
    }
    mpz_mul(scratch, a->lo, b->lo);
    mpz_fdiv_q_2exp(out->lo, scratch, (mp_bitcnt_t)scale);
    mpz_mul(scratch, a->hi, b->hi);
    mpz_cdiv_q_2exp(out->hi, scratch, (mp_bitcnt_t)scale);
}

static void tfx_square_positive(ppf_trig_fx_t *out,
                                const ppf_trig_fx_t *a,
                                ppf_prec_t scale,
                                mpz_ptr scratch) {
    if (mpz_cmp(a->lo, a->hi) == 0) {
        mpz_mul(scratch, a->lo, a->lo);
        mpz_fdiv_q_2exp(out->lo, scratch, (mp_bitcnt_t)scale);
        mpz_cdiv_q_2exp(out->hi, scratch, (mp_bitcnt_t)scale);
        return;
    }
    mpz_mul(scratch, a->lo, a->lo);
    mpz_fdiv_q_2exp(out->lo, scratch, (mp_bitcnt_t)scale);
    mpz_mul(scratch, a->hi, a->hi);
    mpz_cdiv_q_2exp(out->hi, scratch, (mp_bitcnt_t)scale);
}

static void tfx_div_ui_positive(ppf_trig_fx_t *out,
                                const ppf_trig_fx_t *a,
                                unsigned long d) {
    mpz_fdiv_q_ui(out->lo, a->lo, d);
    mpz_cdiv_q_ui(out->hi, a->hi, d);
}

static void tfx_add_signed_term(ppf_trig_fx_t *sum,
                                const ppf_trig_fx_t *term,
                                int sign) {
    if (sign > 0) {
        mpz_add(sum->lo, sum->lo, term->lo);
        mpz_add(sum->hi, sum->hi, term->hi);
    } else {
        mpz_sub(sum->lo, sum->lo, term->hi);
        mpz_sub(sum->hi, sum->hi, term->lo);
    }
}

static unsigned tfx_ceil_log2_u64(uint64_t x) {
    unsigned n = 0;
    uint64_t v = x > 0 ? x - 1u : 0u;
    while (v) { v >>= 1; ++n; }
    return n;
}

/* Choose enough halvings to keep the Taylor term count moderate without
   making reconstruction dominate. */
static unsigned tfx_choose_halvings(mpz_srcptr magnitude,
                                    ppf_prec_t scale,
                                    ppf_prec_t work) {
    size_t bits = mpz_sgn(magnitude) == 0 ? 0u : mpz_sizeinbase(magnitude, 2);
    uint64_t range = 0;
    unsigned precision_part;
    if (bits > scale) range = (uint64_t)(bits - scale);
    /* Target |t| <= about 2^-8, then add a slowly growing precision term. */
    range += PPF_FAST_TRIG_TARGET_BITS;
    precision_part = (tfx_ceil_log2_u64(work > 0 ? work : 1u) + 1u) / 2u;
    if (range > UINT_MAX - precision_part) return UINT_MAX;
    if ((uint64_t)(unsigned)range + precision_part > UINT_MAX - PPF_FAST_TRIG_HALVING_BIAS)
        return UINT_MAX;
    return (unsigned)range + precision_part + PPF_FAST_TRIG_HALVING_BIAS;
}

static int tfx_tail_small(mpz_srcptr upper, ppf_prec_t scale,
                          ppf_prec_t work) {
    ppf_prec_t keep;
    if (scale <= work + PPF_FAST_TRIG_TAIL_GUARD) return 0;
    keep = scale - work - PPF_FAST_TRIG_TAIL_GUARD;
    if (keep > (ppf_prec_t)ULONG_MAX) return 0;
    if (mpz_sgn(upper) <= 0) return 1;
    return mpz_sizeinbase(upper, 2) <= (size_t)keep;
}

static PPF_TRIG_UNUSED int tfx_series(ppf_trig_fx_t *out,
                      const ppf_trig_fx_t *t,
                      const ppf_trig_fx_t *t2,
                      ppf_prec_t scale, ppf_prec_t work,
                      int cosine, mpz_ptr scratch) {
    ppf_trig_fx_t sum, term, prod, next;
    mpz_t tail;
    int sign = -1;
    int rc = -1;
    tfx_init(&sum); tfx_init(&term); tfx_init(&prod); tfx_init(&next);
    mpz_init(tail);
    if (cosine) {
        mpz_set_ui(sum.lo, 1UL); mpz_mul_2exp(sum.lo, sum.lo, (mp_bitcnt_t)scale);
        mpz_set(sum.hi, sum.lo);
        mpz_set(term.lo, sum.lo); mpz_set(term.hi, sum.hi);
    } else {
        mpz_set(sum.lo, t->lo); mpz_set(sum.hi, t->hi);
        mpz_set(term.lo, t->lo); mpz_set(term.hi, t->hi);
    }
    for (unsigned long k = 0; k < PPF_FAST_TRIG_MAX_TERMS; ++k) {
        unsigned long a, b, d;
        if (cosine) {
            a = 2UL * k + 1UL;
            b = a + 1UL;
        } else {
            a = 2UL * k + 2UL;
            b = a + 1UL;
        }
        if (a > ULONG_MAX / b) goto done;
        d = a * b;
        tfx_mul_positive(&prod, &term, t2, scale, scratch);
        tfx_div_ui_positive(&next, &prod, d);
        if (tfx_tail_small(next.hi, scale, work)) {
            /* Since |t| is aggressively reduced, the ratio of consecutive
               magnitudes is far below 1/2.  Twice the next term therefore
               encloses the complete alternating tail plus fixed rounding. */
            mpz_mul_2exp(tail, next.hi, 1UL);
            mpz_sub(out->lo, sum.lo, tail);
            mpz_add(out->hi, sum.hi, tail);
            rc = 0;
            goto done;
        }
        tfx_add_signed_term(&sum, &next, sign);
        mpz_set(term.lo, next.lo);
        mpz_set(term.hi, next.hi);
        sign = -sign;
    }
done:
    tfx_clear(&sum); tfx_clear(&term); tfx_clear(&prod); tfx_clear(&next);
    mpz_clear(tail);
    return rc;
}


static void tfx_div_z_positive(ppf_trig_fx_t *out,
                               const ppf_trig_fx_t *a,
                               mpz_srcptr divisor) {
    mpz_fdiv_q(out->lo, a->lo, divisor);
    mpz_cdiv_q(out->hi, a->hi, divisor);
}

/* Four-term rectangular evaluation of the sine or cosine Taylor series.
   q=t^2 is reused as q^0..q^4.  One block advances four alternating terms
   with two interval products instead of four recurrence products. */
static PPF_TRIG_UNUSED int tfx_series_block4(ppf_trig_fx_t *out,
                             const ppf_trig_fx_t *t,
                             const ppf_trig_fx_t *q,
                             ppf_prec_t scale, ppf_prec_t work,
                             int cosine, mpz_ptr scratch) {
    ppf_trig_fx_t powers[5], term, sum, block, quotient;
    ppf_trig_fx_t contribution, ratio, update;
    mpz_t denominator, factor, tail;
    unsigned long k = 0UL;
    int rc = -1;
    for (unsigned i = 0; i < 5; ++i) tfx_init(&powers[i]);
    tfx_init(&term); tfx_init(&sum); tfx_init(&block); tfx_init(&quotient);
    tfx_init(&contribution); tfx_init(&ratio); tfx_init(&update);
    mpz_inits(denominator, factor, tail, NULL);

    mpz_set_ui(powers[0].lo, 1UL);
    mpz_mul_2exp(powers[0].lo, powers[0].lo, (mp_bitcnt_t)scale);
    mpz_set(powers[0].hi, powers[0].lo);
    mpz_set(powers[1].lo, q->lo); mpz_set(powers[1].hi, q->hi);
    for (unsigned j = 2; j <= 4; ++j)
        tfx_mul_positive(&powers[j], &powers[j - 1], q, scale, scratch);

    if (cosine) {
        mpz_set(term.lo, powers[0].lo); mpz_set(term.hi, powers[0].hi);
    } else {
        mpz_set(term.lo, t->lo); mpz_set(term.hi, t->hi);
    }
    mpz_set_ui(sum.lo, 0UL); mpz_set_ui(sum.hi, 0UL);

    while (k < PPF_FAST_TRIG_MAX_TERMS) {
        mpz_set_ui(block.lo, 0UL); mpz_set_ui(block.hi, 0UL);
        mpz_set_ui(denominator, 1UL);
        for (unsigned j = 0; j < 4; ++j) {
            tfx_div_z_positive(&quotient, &powers[j], denominator);
            tfx_add_signed_term(&block, &quotient, (j & 1u) ? -1 : 1);
            {
                unsigned long a = cosine ? 2UL * (k + j) + 1UL
                                         : 2UL * (k + j) + 2UL;
                unsigned long b = a + 1UL;
                if (a > ULONG_MAX / b) goto done;
                mpz_set_ui(factor, a * b);
                mpz_mul(denominator, denominator, factor);
            }
        }
        if (mpz_sgn(block.lo) <= 0) goto done;
        tfx_mul_positive(&contribution, &term, &block, scale, scratch);
        mpz_add(sum.lo, sum.lo, contribution.lo);
        mpz_add(sum.hi, sum.hi, contribution.hi);

        tfx_div_z_positive(&ratio, &powers[4], denominator);
        tfx_mul_positive(&update, &term, &ratio, scale, scratch);
        k += 4UL;
        if (tfx_tail_small(update.hi, scale, work)) {
            mpz_mul_2exp(tail, update.hi, 1UL);
            mpz_sub(out->lo, sum.lo, tail);
            mpz_add(out->hi, sum.hi, tail);
            rc = 0;
            goto done;
        }
        mpz_set(term.lo, update.lo); mpz_set(term.hi, update.hi);
    }
done:
    for (unsigned i = 0; i < 5; ++i) tfx_clear(&powers[i]);
    tfx_clear(&term); tfx_clear(&sum); tfx_clear(&block); tfx_clear(&quotient);
    tfx_clear(&contribution); tfx_clear(&ratio); tfx_clear(&update);
    mpz_clears(denominator, factor, tail, NULL);
    return rc;
}


/* Eight-term variant.  It trades a slightly larger power table for half as
   many term/update products as the four-term schedule. */
static PPF_TRIG_UNUSED int tfx_series_block8(ppf_trig_fx_t *out,
                             const ppf_trig_fx_t *t,
                             const ppf_trig_fx_t *q,
                             ppf_prec_t scale, ppf_prec_t work,
                             int cosine, mpz_ptr scratch) {
    ppf_trig_fx_t powers[9], term, sum, block, quotient;
    ppf_trig_fx_t contribution, ratio, update;
    mpz_t denominator, factor, tail;
    unsigned long k = 0UL;
    int rc = -1;
    for (unsigned i = 0; i < 9; ++i) tfx_init(&powers[i]);
    tfx_init(&term); tfx_init(&sum); tfx_init(&block); tfx_init(&quotient);
    tfx_init(&contribution); tfx_init(&ratio); tfx_init(&update);
    mpz_inits(denominator, factor, tail, NULL);

    mpz_set_ui(powers[0].lo, 1UL);
    mpz_mul_2exp(powers[0].lo, powers[0].lo, (mp_bitcnt_t)scale);
    mpz_set(powers[0].hi, powers[0].lo);
    mpz_set(powers[1].lo, q->lo); mpz_set(powers[1].hi, q->hi);
    for (unsigned j = 2; j <= 8; ++j)
        tfx_mul_positive(&powers[j], &powers[j - 1], q, scale, scratch);

    if (cosine) {
        mpz_set(term.lo, powers[0].lo); mpz_set(term.hi, powers[0].hi);
    } else {
        mpz_set(term.lo, t->lo); mpz_set(term.hi, t->hi);
    }
    mpz_set_ui(sum.lo, 0UL); mpz_set_ui(sum.hi, 0UL);

    while (k < PPF_FAST_TRIG_MAX_TERMS) {
        mpz_set_ui(block.lo, 0UL); mpz_set_ui(block.hi, 0UL);
        mpz_set_ui(denominator, 1UL);
        for (unsigned j = 0; j < 8; ++j) {
            tfx_div_z_positive(&quotient, &powers[j], denominator);
            tfx_add_signed_term(&block, &quotient, (j & 1u) ? -1 : 1);
            {
                unsigned long a = cosine ? 2UL * (k + j) + 1UL
                                         : 2UL * (k + j) + 2UL;
                unsigned long b = a + 1UL;
                if (a > ULONG_MAX / b) goto done;
                mpz_set_ui(factor, a * b);
                mpz_mul(denominator, denominator, factor);
            }
        }
        if (mpz_sgn(block.lo) <= 0) goto done;
        tfx_mul_positive(&contribution, &term, &block, scale, scratch);
        mpz_add(sum.lo, sum.lo, contribution.lo);
        mpz_add(sum.hi, sum.hi, contribution.hi);

        tfx_div_z_positive(&ratio, &powers[8], denominator);
        tfx_mul_positive(&update, &term, &ratio, scale, scratch);
        k += 8UL;
        if (tfx_tail_small(update.hi, scale, work)) {
            mpz_mul_2exp(tail, update.hi, 1UL);
            mpz_sub(out->lo, sum.lo, tail);
            mpz_add(out->hi, sum.hi, tail);
            rc = 0;
            goto done;
        }
        mpz_set(term.lo, update.lo); mpz_set(term.hi, update.hi);
    }
done:
    for (unsigned i = 0; i < 9; ++i) tfx_clear(&powers[i]);
    tfx_clear(&term); tfx_clear(&sum); tfx_clear(&block); tfx_clear(&quotient);
    tfx_clear(&contribution); tfx_clear(&ratio); tfx_clear(&update);
    mpz_clears(denominator, factor, tail, NULL);
    return rc;
}


/* Shared four-term evaluation of
       P(q)=sum (-1)^k q^k/(2k+1)!
   and cos(t)=sum (-1)^k (2k+1)q^k/(2k+1)!.
   Then sin(t)=t*P(q).  The sine and cosine recurrences therefore share the
   power table, coefficient divisions, and term update. */
static int tfx_sincos_series_pair_block4(ppf_trig_fx_t *sine,
                                         ppf_trig_fx_t *cosine,
                                         const ppf_trig_fx_t *t,
                                         const ppf_trig_fx_t *q,
                                         ppf_prec_t scale,
                                         ppf_prec_t work,
                                         mpz_ptr scratch) {
    ppf_trig_fx_t powers[5], term, psum, csum, pblock, cblock;
    ppf_trig_fx_t quotient, weighted, pcontrib, ccontrib, ratio, update;
    mpz_t denominator, factor, ptail, ctail;
    unsigned long k = 0UL;
    int rc = -1;
    for (unsigned i = 0; i < 5; ++i) tfx_init(&powers[i]);
    tfx_init(&term); tfx_init(&psum); tfx_init(&csum);
    tfx_init(&pblock); tfx_init(&cblock); tfx_init(&quotient);
    tfx_init(&weighted); tfx_init(&pcontrib); tfx_init(&ccontrib);
    tfx_init(&ratio); tfx_init(&update);
    mpz_inits(denominator, factor, ptail, ctail, NULL);

    mpz_set_ui(powers[0].lo, 1UL);
    mpz_mul_2exp(powers[0].lo, powers[0].lo, (mp_bitcnt_t)scale);
    mpz_set(powers[0].hi, powers[0].lo);
    mpz_set(powers[1].lo, q->lo); mpz_set(powers[1].hi, q->hi);
    for (unsigned j = 2; j <= 4; ++j)
        tfx_mul_positive(&powers[j], &powers[j - 1], q, scale, scratch);
    mpz_set(term.lo, powers[0].lo); mpz_set(term.hi, powers[0].hi);
    mpz_set_ui(psum.lo, 0UL); mpz_set_ui(psum.hi, 0UL);
    mpz_set_ui(csum.lo, 0UL); mpz_set_ui(csum.hi, 0UL);

    while (k < PPF_FAST_TRIG_MAX_TERMS) {
        mpz_set_ui(pblock.lo, 0UL); mpz_set_ui(pblock.hi, 0UL);
        mpz_set_ui(cblock.lo, 0UL); mpz_set_ui(cblock.hi, 0UL);
        mpz_set_ui(denominator, 1UL);
        for (unsigned j = 0; j < 4; ++j) {
            unsigned long weight = 2UL * (k + j) + 1UL;
            tfx_div_z_positive(&quotient, &powers[j], denominator);
            tfx_add_signed_term(&pblock, &quotient, (j & 1u) ? -1 : 1);
            mpz_mul_ui(weighted.lo, quotient.lo, weight);
            mpz_mul_ui(weighted.hi, quotient.hi, weight);
            tfx_add_signed_term(&cblock, &weighted, (j & 1u) ? -1 : 1);
            {
                unsigned long a = 2UL * (k + j) + 2UL;
                unsigned long b = a + 1UL;
                if (a > ULONG_MAX / b) goto done;
                mpz_set_ui(factor, a * b);
                mpz_mul(denominator, denominator, factor);
            }
        }
        if (mpz_sgn(pblock.lo) <= 0 || mpz_sgn(cblock.lo) <= 0) goto done;
        tfx_mul_positive(&pcontrib, &term, &pblock, scale, scratch);
        tfx_mul_positive(&ccontrib, &term, &cblock, scale, scratch);
        mpz_add(psum.lo, psum.lo, pcontrib.lo);
        mpz_add(psum.hi, psum.hi, pcontrib.hi);
        mpz_add(csum.lo, csum.lo, ccontrib.lo);
        mpz_add(csum.hi, csum.hi, ccontrib.hi);

        tfx_div_z_positive(&ratio, &powers[4], denominator);
        tfx_mul_positive(&update, &term, &ratio, scale, scratch);
        k += 4UL;
        if (tfx_tail_small(update.hi, scale, work)) {
            mpz_mul_2exp(ptail, update.hi, 1UL);
            mpz_mul_ui(ctail, update.hi, 2UL * k + 1UL);
            mpz_mul_2exp(ctail, ctail, 1UL);
            mpz_sub(pblock.lo, psum.lo, ptail);
            mpz_add(pblock.hi, psum.hi, ptail);
            mpz_sub(cosine->lo, csum.lo, ctail);
            mpz_add(cosine->hi, csum.hi, ctail);
            if (mpz_sgn(pblock.lo) <= 0 || mpz_sgn(cosine->lo) <= 0)
                goto done;
            tfx_mul_positive(sine, t, &pblock, scale, scratch);
            rc = 0;
            goto done;
        }
        mpz_set(term.lo, update.lo); mpz_set(term.hi, update.hi);
    }
done:
    for (unsigned i = 0; i < 5; ++i) tfx_clear(&powers[i]);
    tfx_clear(&term); tfx_clear(&psum); tfx_clear(&csum);
    tfx_clear(&pblock); tfx_clear(&cblock); tfx_clear(&quotient);
    tfx_clear(&weighted); tfx_clear(&pcontrib); tfx_clear(&ccontrib);
    tfx_clear(&ratio); tfx_clear(&update);
    mpz_clears(denominator, factor, ptail, ctail, NULL);
    return rc;
}

static void tfx_clamp_unit(ppf_trig_fx_t *x, mpz_srcptr one,
                           int nonnegative) {
    if (nonnegative && mpz_sgn(x->lo) < 0) mpz_set_ui(x->lo, 0UL);
    if (mpz_cmp(x->hi, one) > 0) mpz_set(x->hi, one);
    if (mpz_cmp(x->lo, one) > 0) mpz_set(x->lo, one);
}

/* Evaluate sin/cos for a nonnegative exact fixed-point argument by Taylor at
   a reduced argument and exact double-angle reconstruction. */
static void tfx_interval_to_ball(mpz_ptr mid, mpz_ptr rad,
                                 const ppf_trig_fx_t *x,
                                 mpz_ptr scratch) {
    mpz_add(scratch, x->lo, x->hi);
    mpz_fdiv_q_2exp(mid, scratch, 1UL);
    mpz_sub(rad, mid, x->lo);
    mpz_sub(scratch, x->hi, mid);
    if (mpz_cmp(scratch, rad) > 0) mpz_set(rad, scratch);
    if (mpz_sgn(rad) < 0) mpz_set_ui(rad, 0UL);
}



/* Fixed-point midpoint-radius arithmetic.  A ball (m,r) represents
      [m-r,m+r] * 2^-scale.
   Midpoint products use one full multiplication; all uncertainty is carried
   by an explicit integer radius. */
typedef struct {
    mpz_t mid;
    mpz_t rad;
} ppf_trig_ball_t;

static void tball_init(ppf_trig_ball_t *x) { mpz_inits(x->mid, x->rad, NULL); }
static void tball_clear(ppf_trig_ball_t *x) { mpz_clears(x->mid, x->rad, NULL); }

static void tball_set_exact(ppf_trig_ball_t *x, mpz_srcptr value) {
    mpz_set(x->mid, value);
    mpz_set_ui(x->rad, 0UL);
}

static void tball_mul_positive(ppf_trig_ball_t *out,
                               const ppf_trig_ball_t *a,
                               const ppf_trig_ball_t *b,
                               ppf_prec_t scale,
                               mpz_ptr p0, mpz_ptr p1, mpz_ptr p2) {
    /* midpoint = floor(a.mid*b.mid/2^scale) */
    mpz_mul(p0, a->mid, b->mid);
    mpz_fdiv_q_2exp(out->mid, p0, (mp_bitcnt_t)scale);
    /* radius numerator = |a.mid| b.rad + |b.mid| a.rad + a.rad b.rad. */
    mpz_set_ui(p0, 0UL);
    if (mpz_sgn(b->rad) != 0) {
        mpz_abs(p1, a->mid);
        mpz_mul(p1, p1, b->rad);
        mpz_add(p0, p0, p1);
    }
    if (mpz_sgn(a->rad) != 0) {
        mpz_abs(p1, b->mid);
        mpz_mul(p1, p1, a->rad);
        mpz_add(p0, p0, p1);
    }
    if (mpz_sgn(a->rad) != 0 && mpz_sgn(b->rad) != 0) {
        mpz_mul(p2, a->rad, b->rad);
        mpz_add(p0, p0, p2);
    }
    mpz_cdiv_q_2exp(out->rad, p0, (mp_bitcnt_t)scale);
    mpz_add_ui(out->rad, out->rad, 1UL); /* midpoint truncation */
}

static void tball_div_z_positive(ppf_trig_ball_t *out,
                                 const ppf_trig_ball_t *a,
                                 mpz_srcptr d) {
    mpz_fdiv_q(out->mid, a->mid, d);
    mpz_cdiv_q(out->rad, a->rad, d);
    mpz_add_ui(out->rad, out->rad, 1UL);
}

static void tball_add_signed(ppf_trig_ball_t *sum,
                             const ppf_trig_ball_t *term,
                             int sign) {
    if (sign > 0) mpz_add(sum->mid, sum->mid, term->mid);
    else mpz_sub(sum->mid, sum->mid, term->mid);
    mpz_add(sum->rad, sum->rad, term->rad);
}

static void tball_mul_ui(ppf_trig_ball_t *out,
                         const ppf_trig_ball_t *a,
                         unsigned long value) {
    mpz_mul_ui(out->mid, a->mid, value);
    mpz_mul_ui(out->rad, a->rad, value);
}

static void tball_to_interval(ppf_trig_fx_t *out,
                              const ppf_trig_ball_t *ball) {
    mpz_sub(out->lo, ball->mid, ball->rad);
    mpz_add(out->hi, ball->mid, ball->rad);
}

/* Eight-term rectangular splitting for sin(t)/t and cos(t), sharing q powers
   and coefficient divisions. */
static int tball_sincos_series_block8(ppf_trig_fx_t *sine,
                                      ppf_trig_fx_t *cosine,
                                      mpz_srcptr tvalue,
                                      ppf_prec_t scale,
                                      ppf_prec_t work) {
    enum { MAX_BLOCK = 36 };
    mp_bitcnt_t input_trailing = mpz_scan1(tvalue, 0UL);
    size_t input_odd_bits = input_trailing == (mp_bitcnt_t)-1
        ? 0u : mpz_sizeinbase(tvalue, 2) - (size_t)input_trailing;
    int dense_input = input_odd_bits * 2u > (size_t)work;
    unsigned block;
    if (work < 3000u) block = 8u;
    else if (work < 8000u) block = 10u;
    else if (work < 35000u) block = 20u;
    else if (work < 75000u) block = dense_input ? 24u : 32u;
    else block = dense_input ? 36u : 28u;
    ppf_trig_ball_t t, q, powers[MAX_BLOCK + 1], term, psum, csum;
    ppf_trig_ball_t pblock, cblock, quotient, weighted;
    ppf_trig_ball_t pcontrib, ccontrib, ratio, update, pfinal, sfinal;
    mpz_t denominator, factor, upper, p0, p1, p2;
    unsigned long k = 0UL;
    int rc = -1;
    tball_init(&t); tball_init(&q);
    for (unsigned i = 0; i <= MAX_BLOCK; ++i) tball_init(&powers[i]);
    tball_init(&term); tball_init(&psum); tball_init(&csum);
    tball_init(&pblock); tball_init(&cblock); tball_init(&quotient);
    tball_init(&weighted); tball_init(&pcontrib); tball_init(&ccontrib);
    tball_init(&ratio); tball_init(&update); tball_init(&pfinal);
    tball_init(&sfinal);
    mpz_inits(denominator, factor, upper, p0, p1, p2, NULL);
    tball_set_exact(&t, tvalue);
    tball_mul_positive(&q, &t, &t, scale, p0, p1, p2);
    mpz_set_ui(powers[0].mid, 1UL);
    mpz_mul_2exp(powers[0].mid, powers[0].mid, (mp_bitcnt_t)scale);
    mpz_set_ui(powers[0].rad, 0UL);
    for (unsigned j = 1; j <= block; ++j)
        tball_mul_positive(&powers[j], &powers[j - 1], &q,
                           scale, p0, p1, p2);
    tball_set_exact(&term, powers[0].mid);
    mpz_set_ui(psum.mid, 0UL); mpz_set_ui(psum.rad, 0UL);
    mpz_set_ui(csum.mid, 0UL); mpz_set_ui(csum.rad, 0UL);
    while (k < PPF_FAST_TRIG_MAX_TERMS) {
        mpz_set_ui(pblock.mid, 0UL); mpz_set_ui(pblock.rad, 0UL);
        mpz_set_ui(cblock.mid, 0UL); mpz_set_ui(cblock.rad, 0UL);
        mpz_set_ui(denominator, 1UL);
        for (unsigned j = 0; j < block; ++j) {
            unsigned long weight = 2UL * (k + j) + 1UL;
            tball_div_z_positive(&quotient, &powers[j], denominator);
            tball_add_signed(&pblock, &quotient, (j & 1u) ? -1 : 1);
            tball_mul_ui(&weighted, &quotient, weight);
            tball_add_signed(&cblock, &weighted, (j & 1u) ? -1 : 1);
            {
                unsigned long aa = 2UL * (k + j) + 2UL;
                unsigned long bb = aa + 1UL;
                if (aa > ULONG_MAX / bb) goto done;
                mpz_set_ui(factor, aa * bb);
                mpz_mul(denominator, denominator, factor);
            }
        }
        tball_mul_positive(&pcontrib, &term, &pblock,
                           scale, p0, p1, p2);
        tball_mul_positive(&ccontrib, &term, &cblock,
                           scale, p0, p1, p2);
        tball_add_signed(&psum, &pcontrib, 1);
        tball_add_signed(&csum, &ccontrib, 1);
        tball_div_z_positive(&ratio, &powers[block], denominator);
        tball_mul_positive(&update, &term, &ratio,
                           scale, p0, p1, p2);
        k += block;
        mpz_add(upper, update.mid, update.rad);
        if (tfx_tail_small(upper, scale, work)) {
            /* Twice the first omitted term encloses the remaining tail. */
            mpz_set(pfinal.mid, psum.mid);
            mpz_set(pfinal.rad, psum.rad);
            mpz_mul_2exp(upper, upper, 1UL);
            mpz_add(pfinal.rad, pfinal.rad, upper);
            mpz_set(cosine->lo, csum.mid);
            mpz_sub(cosine->lo, cosine->lo, csum.rad);
            mpz_mul_ui(upper, upper, 2UL * k + 1UL);
            mpz_sub(cosine->lo, cosine->lo, upper);
            mpz_set(cosine->hi, csum.mid);
            mpz_add(cosine->hi, cosine->hi, csum.rad);
            mpz_add(cosine->hi, cosine->hi, upper);
            tball_mul_positive(&sfinal, &t, &pfinal,
                               scale, p0, p1, p2);
            tball_to_interval(sine, &sfinal);
            rc = 0;
            goto done;
        }
        mpz_set(term.mid, update.mid);
        mpz_set(term.rad, update.rad);
    }
done:
    tball_clear(&t); tball_clear(&q);
    for (unsigned i = 0; i <= MAX_BLOCK; ++i) tball_clear(&powers[i]);
    tball_clear(&term); tball_clear(&psum); tball_clear(&csum);
    tball_clear(&pblock); tball_clear(&cblock); tball_clear(&quotient);
    tball_clear(&weighted); tball_clear(&pcontrib); tball_clear(&ccontrib);
    tball_clear(&ratio); tball_clear(&update); tball_clear(&pfinal);
    tball_clear(&sfinal);
    mpz_clears(denominator, factor, upper, p0, p1, p2, NULL);
    return rc;
}

/* Exact paired binary splitting for low-complexity dyadic arguments.  The
   sine and cosine series have the same numerator ratio -a^2; one product tree
   therefore serves both denominator sequences. */
typedef struct {
    /* One product/denominator tree serves both series. */
    mpz_t p;
    mpz_t q;
    mpz_t t;
    mpz_t w;
} ppf_trig_bs_t;

static void tbs_init(ppf_trig_bs_t *x) {
    mpz_inits(x->p, x->q, x->t, x->w, NULL);
}
static void tbs_clear(ppf_trig_bs_t *x) {
    mpz_clears(x->p, x->q, x->t, x->w, NULL);
}

typedef struct {
    ppf_trig_bs_t left;
    ppf_trig_bs_t right;
    mpz_t scratch;
} ppf_trig_bs_workspace_t;

static int tbs_build_workspace(ppf_trig_bs_t *out, mpz_srcptr a2,
                               mp_bitcnt_t denominator_bits, int alternating,
                               unsigned long begin, unsigned long end,
                               ppf_trig_bs_workspace_t *workspace,
                               size_t depth, size_t depth_count) {
    if (end <= begin) return -1;
    if (end - begin == 1UL) {
        unsigned long q1 = 2UL * begin + 2UL;
        unsigned long q2 = q1 + 1UL;
        unsigned long weight = 2UL * begin + 1UL;
        if (q1 > ULONG_MAX / q2) return -1;
        if (alternating) mpz_neg(out->p, a2);
        else mpz_set(out->p, a2);
        mpz_set_ui(out->q, q1 * q2);
        mpz_mul_2exp(out->q, out->q, denominator_bits);
        mpz_set(out->t, out->q);
        mpz_mul_ui(out->w, out->q, weight);
        return 0;
    }
    if (depth >= depth_count) return -1;
    {
        unsigned long mid = begin + (end - begin) / 2UL;
        ppf_trig_bs_t *left = &workspace[depth].left;
        ppf_trig_bs_t *right = &workspace[depth].right;
        mpz_ptr scratch = workspace[depth].scratch;
        if (tbs_build_workspace(left, a2, denominator_bits, alternating,
                                begin, mid, workspace, depth + 1u,
                                depth_count) != 0 ||
            tbs_build_workspace(right, a2, denominator_bits, alternating,
                                mid, end, workspace, depth + 1u,
                                depth_count) != 0)
            return -1;

        mpz_mul(scratch, left->t, right->q);
        mpz_addmul(scratch, left->p, right->t);
        mpz_swap(out->t, scratch);

        mpz_mul(scratch, left->w, right->q);
        mpz_addmul(scratch, left->p, right->w);
        mpz_swap(out->w, scratch);

        mpz_mul(out->q, left->q, right->q);
        mpz_mul(out->p, left->p, right->p);
        return 0;
    }
}

static size_t tbs_workspace_depth(unsigned long length) {
    size_t depth_count = 1u;
    unsigned long value = length;
    while (value > 1UL) {
        value = (value + 1UL) / 2UL;
        ++depth_count;
    }
    return depth_count;
}

static void tbs_workspace_array_init(ppf_trig_bs_workspace_t *workspace,
                                     size_t count) {
    for (size_t i = 0; i < count; ++i) {
        tbs_init(&workspace[i].left);
        tbs_init(&workspace[i].right);
        mpz_init(workspace[i].scratch);
    }
}

static void tbs_workspace_array_clear(ppf_trig_bs_workspace_t *workspace,
                                      size_t count) {
    if (!workspace) return;
    for (size_t i = 0; i < count; ++i) {
        tbs_clear(&workspace[i].left);
        tbs_clear(&workspace[i].right);
        mpz_clear(workspace[i].scratch);
    }
}

static int tbs_build_serial(ppf_trig_bs_t *out, mpz_srcptr a2,
                            mp_bitcnt_t denominator_bits, int alternating,
                            unsigned long begin, unsigned long end) {
    ppf_trig_bs_workspace_t *workspace;
    size_t depth_count;
    int rc;
    if (end <= begin) return -1;
    depth_count = tbs_workspace_depth(end - begin);
    workspace = (ppf_trig_bs_workspace_t *)
        ppf_owned_buffer_calloc_internal(
            ppf_result_transaction_context_internal(), depth_count,
            sizeof(*workspace));
    if (!workspace) return -1;
    tbs_workspace_array_init(workspace, depth_count);
    rc = tbs_build_workspace(out, a2, denominator_bits, alternating,
                             begin, end, workspace, 0u, depth_count);
    tbs_workspace_array_clear(workspace, depth_count);
    ppf_owned_buffer_free_internal(workspace);
    return rc;
}


static void tbs_combine_inplace(ppf_trig_bs_t *left,
                                const ppf_trig_bs_t *right,
                                mpz_ptr scratch) {
    mpz_mul(scratch, left->t, right->q);
    mpz_addmul(scratch, left->p, right->t);
    mpz_swap(left->t, scratch);
    mpz_mul(scratch, left->w, right->q);
    mpz_addmul(scratch, left->p, right->w);
    mpz_swap(left->w, scratch);
    mpz_mul(left->q, left->q, right->q);
    mpz_mul(left->p, left->p, right->p);
}

static int tbs_build_parallel(ppf_trig_bs_t *out, mpz_srcptr a2,
                              mp_bitcnt_t denominator_bits, int alternating,
                              unsigned long begin, unsigned long end,
                              unsigned long workers) {
    ppf_trig_bs_t *nodes = NULL;
    mpz_t *scratch = NULL;
    ppf_trig_bs_workspace_t *workspaces = NULL;
    unsigned long terms = end - begin, packets, packet_width;
    size_t depth_count = 0u, workspace_count = 0u;
    int initialized = 0, failed = 0, rc = -1;
    if (terms < 24UL || workers < 2UL)
        return tbs_build_serial(out, a2, denominator_bits,
                                alternating, begin, end);
    packets = workers;
    if (packets > terms / 8UL) packets = terms / 8UL;
    if (packets < 2UL)
        return tbs_build_serial(out, a2, denominator_bits,
                                alternating, begin, end);
    packet_width = terms / packets + (terms % packets != 0UL);
    depth_count = tbs_workspace_depth(packet_width);
    if (depth_count == 0u || packets > SIZE_MAX / depth_count) return -1;
    workspace_count = (size_t)packets * depth_count;
    nodes = (ppf_trig_bs_t *)ppf_owned_buffer_calloc_internal(
        ppf_result_transaction_context_internal(), (size_t)packets,
        sizeof(*nodes));
    scratch = (mpz_t *)ppf_owned_buffer_calloc_internal(
        ppf_result_transaction_context_internal(), (size_t)packets,
        sizeof(mpz_t));
    workspaces = (ppf_trig_bs_workspace_t *)
        ppf_owned_buffer_calloc_internal(
            ppf_result_transaction_context_internal(), workspace_count,
            sizeof(*workspaces));
    if (!nodes || !scratch || !workspaces) goto done;
    for (unsigned long i = 0; i < packets; ++i) {
        tbs_init(&nodes[i]);
        mpz_init(scratch[i]);
    }
    tbs_workspace_array_init(workspaces, workspace_count);
    initialized = 1;
PPF_OMP_PRAGMA(omp parallel num_threads(workers) shared(nodes, scratch, workspaces, failed))
    {
PPF_OMP_PRAGMA(omp for schedule(static) reduction(|:failed))
        for (long i = 0; i < (long)packets; ++i) {
            unsigned long a = begin + (terms * (unsigned long)i) / packets;
            unsigned long b = begin + (terms * ((unsigned long)i + 1UL)) / packets;
            ppf_trig_bs_workspace_t *workspace =
                &workspaces[(size_t)i * depth_count];
            if (tbs_build_workspace(&nodes[i], a2, denominator_bits,
                                    alternating, a, b, workspace, 0u,
                                    depth_count) != 0)
                failed |= 1;
        }
        if (!failed) {
            for (unsigned long local_stride = 1UL; local_stride < packets;) {
                unsigned long step = local_stride * 2UL;
                unsigned long pairs = (packets + step - 1UL) / step;
PPF_OMP_PRAGMA(omp for schedule(static))
                for (long pair = 0; pair < (long)pairs; ++pair) {
                    unsigned long i = (unsigned long)pair * step;
                    if (i + local_stride < packets)
                        tbs_combine_inplace(&nodes[i], &nodes[i + local_stride],
                                            scratch[i]);
                }
                if (local_stride > ULONG_MAX / 2UL) break;
                local_stride = step;
            }
        }
    }
    if (failed) goto done;
    mpz_swap(out->p, nodes[0].p);
    mpz_swap(out->q, nodes[0].q);
    mpz_swap(out->t, nodes[0].t);
    mpz_swap(out->w, nodes[0].w);
    rc = 0;
done:
    if (initialized) {
        tbs_workspace_array_clear(workspaces, workspace_count);
        for (unsigned long i = 0; i < packets; ++i) {
            tbs_clear(&nodes[i]);
            mpz_clear(scratch[i]);
        }
    }
    ppf_owned_buffer_free_internal(workspaces);
    ppf_owned_buffer_free_internal(nodes);
    ppf_owned_buffer_free_internal(scratch);
    return rc;
}

static int tbs_build(ppf_trig_bs_t *out, mpz_srcptr a2,
                     mp_bitcnt_t denominator_bits, int alternating,
                     unsigned long begin, unsigned long end) {
    unsigned long workers = g_ppf_trig_workers;
    if (workers > 1UL && !omp_in_parallel())
        return tbs_build_parallel(out, a2, denominator_bits, alternating,
                                  begin, end, workers);
    return tbs_build_serial(out, a2, denominator_bits,
                            alternating, begin, end);
}

static void tbs_fdiv_q(mpz_ptr out, mpz_srcptr numerator,
                       const ppf_trig_bs_t *root) {
    mpz_fdiv_q(out, numerator, root->q);
}

static void tbs_cdiv_q(mpz_ptr out, mpz_srcptr numerator,
                       const ppf_trig_bs_t *root) {
    mpz_cdiv_q(out, numerator, root->q);
}

static unsigned long tbs_term_count(size_t numerator_bits,
                                    uint64_t denominator_bits,
                                    ppf_prec_t work) {
    const long double inv_ln2 = 1.44269504088896340736L;
    long double logt = (long double)numerator_bits -
                       (long double)denominator_bits;
    long double target = -(long double)work - 128.0L;
    unsigned long n = 1UL;
    if (logt >= 0.0L) return 0UL;
    for (; n < 1000000UL; ++n) {
        long double ls = (long double)(2UL * n + 1UL) * logt -
                         lgammal((long double)(2UL * n + 2UL)) * inv_ln2;
        long double lc = (long double)(2UL * n) * logt -
                         lgammal((long double)(2UL * n + 1UL)) * inv_ln2;
        if (ls < target && lc < target) return n;
    }
    return 0UL;
}

static int tbs_sincos_point(ppf_trig_fx_t *s, ppf_trig_fx_t *c,
                            mpz_srcptr argument, ppf_prec_t work,
                            ppf_prec_t scale, unsigned halvings) {
    mpz_t tint, a, a2, snum, cnum, stail, ctail, lower, upper, ap;
    ppf_trig_bs_t root;
    mp_bitcnt_t trailing;
    uint64_t d;
    size_t abits;
    unsigned long terms;
    int rc = 0;
    mpz_inits(tint, a, a2, snum, cnum, stail, ctail, lower, upper, ap, NULL);
    tbs_init(&root);
    if (scale > (ppf_prec_t)ULONG_MAX || halvings > (unsigned)ULONG_MAX)
        goto done;
    mpz_fdiv_q_2exp(tint, argument, halvings);
    if (mpz_sgn(tint) <= 0) goto done;
    trailing = mpz_scan1(tint, 0UL);
    if (trailing == (mp_bitcnt_t)-1 || trailing > scale) goto done;
    mpz_fdiv_q_2exp(a, tint, trailing);
    d = (uint64_t)scale - (uint64_t)trailing;
    if (d > (uint64_t)ULONG_MAX / 2u) goto done;
    abits = mpz_sizeinbase(a, 2);
    /* Exact splitting is valuable only when the normalized numerator is far
       smaller than the requested output precision. */
    if (!PPF_FAST_TRIG_DENSE_BS_EXPERIMENT &&
        abits > 1024u && abits * 8u > (size_t)work) goto done;
    terms = tbs_term_count(abits, d, work);
    if (terms == 0UL || terms > 4096UL) goto done;
    mpz_mul(a2, a, a);
    if (tbs_build(&root, a2, (mp_bitcnt_t)(2u * d), 1, 0UL, terms) != 0)
        goto done;

    /* sin partial and its first omitted term, both scaled by 2^scale. */
    mpz_mul(snum, a, root.t);
    mpz_mul(ap, a, root.p);
    mpz_abs(stail, ap);
    mpz_mul_2exp(stail, stail, 1UL);
    if (trailing != 0UL) {
        mpz_mul_2exp(snum, snum, trailing);
        mpz_mul_2exp(stail, stail, trailing);
    }
    mpz_sub(lower, snum, stail);
    mpz_add(upper, snum, stail);
    tbs_fdiv_q(s->lo, lower, &root);
    tbs_cdiv_q(s->hi, upper, &root);

    /* cos partial and first omitted term at the same output scale. */
    mpz_mul_2exp(cnum, root.w, (mp_bitcnt_t)scale);
    mpz_abs(ctail, root.p);
    mpz_mul_ui(ctail, ctail, 2UL * terms + 1UL);
    mpz_mul_2exp(ctail, ctail, (mp_bitcnt_t)scale + 1UL);
    mpz_sub(lower, cnum, ctail);
    mpz_add(upper, cnum, ctail);
    tbs_fdiv_q(c->lo, lower, &root);
    tbs_cdiv_q(c->hi, upper, &root);
    if (mpz_cmp(s->lo, s->hi) > 0 || mpz_cmp(c->lo, c->hi) > 0) goto done;
    rc = 1;
done:
    tbs_clear(&root);
    mpz_clears(tint, a, a2, snum, cnum, stail, ctail, lower, upper, ap, NULL);
    return rc;
}

/* Evaluate sin/cos for a nonnegative exact fixed-point argument.  The Taylor
   stage produces narrow endpoint intervals.  Double-angle reconstruction is
   then performed as a specialized unit-ball update.  Since 0<=sin,cos<=1 on
   every intermediate angle, radius propagation needs only shifts/additions
   plus products of the tiny radii; each reconstruction level therefore uses
   two full-size products instead of four endpoint products. */
static int tfx_sincos_point(ppf_trig_fx_t *s, ppf_trig_fx_t *c,
                            mpz_srcptr argument, ppf_prec_t work,
                            ppf_prec_t scale, unsigned halvings) {
    ppf_trig_fx_t t, t2;
    mpz_t scratch, one;
    mpz_t sm, sr, cm, cr, nsm, nsr, ncm, ncr;
    mpz_t p0, p1, cross;
    int rc = -1;
    tfx_init(&t); tfx_init(&t2);
    mpz_inits(scratch, one, sm, sr, cm, cr, nsm, nsr, ncm, ncr,
              p0, p1, cross, NULL);
    mpz_set_ui(one, 1UL);
    mpz_mul_2exp(one, one, (mp_bitcnt_t)scale);
    if (halvings > (unsigned)ULONG_MAX) goto done;
    if (PPF_FAST_TRIG_DENSE_BS_EXPERIMENT &&
        tbs_sincos_point(s, c, argument, work, scale, 0u)) {
        rc = 0;
        goto done;
    }
    if (mpz_scan1(argument, 0) < (mp_bitcnt_t)halvings) goto done;
    mpz_fdiv_q_2exp(t.lo, argument, halvings);
    mpz_set(t.hi, t.lo);
    if (!tbs_sincos_point(s, c, argument, work, scale, halvings) &&
        tball_sincos_series_block8(s, c, t.lo, scale, work) != 0) {
        tfx_square_positive(&t2, &t, scale, scratch);
        if (tfx_sincos_series_pair_block4(s, c, &t, &t2,
                                           scale, work, scratch) != 0)
            goto done;
    }
    tfx_clamp_unit(s, one, 1);
    tfx_clamp_unit(c, one, 1);
    tfx_interval_to_ball(sm, sr, s, scratch);
    tfx_interval_to_ball(cm, cr, c, scratch);

    for (unsigned i = 0; i < halvings; ++i) {
        /* Midpoints: sin(2x)=2sc and cos(2x)=1-2s^2. */
        mpz_mul(p0, sm, cm);
        mpz_mul_2exp(p0, p0, 1UL);
        mpz_fdiv_q_2exp(nsm, p0, (mp_bitcnt_t)scale);

        mpz_mul(p1, sm, sm);
        mpz_mul_2exp(p1, p1, 1UL);
        mpz_fdiv_q_2exp(p1, p1, (mp_bitcnt_t)scale);
        mpz_sub(ncm, one, p1);

        /* |2sc-2 sm cm| <= 2(rs+rc)+2 rs rc at the common scale. */
        mpz_add(nsr, sr, cr);
        mpz_mul_2exp(nsr, nsr, 1UL);
        if (mpz_sgn(sr) != 0 && mpz_sgn(cr) != 0) {
            mpz_mul(cross, sr, cr);
            mpz_mul_2exp(cross, cross, 1UL);
            mpz_cdiv_q_2exp(cross, cross, (mp_bitcnt_t)scale);
            mpz_add(nsr, nsr, cross);
        }
        mpz_add_ui(nsr, nsr, 1UL); /* fixed-point midpoint truncation */

        /* |(1-2s^2)-(1-2 sm^2)| <= 4rs+2rs^2. */
        mpz_mul_2exp(ncr, sr, 2UL);
        if (mpz_sgn(sr) != 0) {
            mpz_mul(cross, sr, sr);
            mpz_mul_2exp(cross, cross, 1UL);
            mpz_cdiv_q_2exp(cross, cross, (mp_bitcnt_t)scale);
            mpz_add(ncr, ncr, cross);
        }
        mpz_add_ui(ncr, ncr, 1UL);

        mpz_set(sm, nsm); mpz_set(sr, nsr);
        mpz_set(cm, ncm); mpz_set(cr, ncr);
    }

    mpz_sub(s->lo, sm, sr); mpz_add(s->hi, sm, sr);
    mpz_sub(c->lo, cm, cr); mpz_add(c->hi, cm, cr);
    {
        mpz_t minus_one;
        mpz_init(minus_one);
        mpz_neg(minus_one, one);
        if (mpz_cmp(s->lo, minus_one) < 0) mpz_set(s->lo, minus_one);
        if (mpz_cmp(s->hi, one) > 0) mpz_set(s->hi, one);
        if (mpz_cmp(c->lo, minus_one) < 0) mpz_set(c->lo, minus_one);
        if (mpz_cmp(c->hi, one) > 0) mpz_set(c->hi, one);
        mpz_clear(minus_one);
    }
    rc = 0;
done:
    tfx_clear(&t); tfx_clear(&t2);
    mpz_clears(scratch, one, sm, sr, cm, cr, nsm, nsr, ncm, ncr,
               p0, p1, cross, NULL);
    return rc;
}

static int tfx_store_interval(ppf_t *lo, ppf_t *hi,
                              mpz_srcptr lower, mpz_srcptr upper,
                              ppf_prec_t scale, ppf_context_t *ctx) {
    ppf_exp_t exponent;
    if (scale > (ppf_prec_t)INT64_MAX) return -1;
    exponent = -(ppf_exp_t)scale;
    if (ppf_set_z_2exp(lo, lower, exponent, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_z_2exp(hi, upper, exponent, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

int ppf_fast_sincos_reduced_internal(ppf_t *sin_lo, ppf_t *sin_hi,
                                     ppf_t *cos_lo, ppf_t *cos_hi,
                                     const ppf_t *r_lo,
                                     const ppf_t *r_hi,
                                     ppf_prec_t work,
                                     ppf_context_t *ctx) {
    ppf_trig_fx_t s, c;
    mpz_t lower, upper, anchor, magnitude, width, one;
    ppf_prec_t scale;
    unsigned halvings;
    int anchor_negative;
    int rc = 0;
    if (!sin_lo || !sin_hi || !cos_lo || !cos_hi || !r_lo || !r_hi || !ctx ||
        !ppf_regular_p(r_lo) || !ppf_regular_p(r_hi) ||
        ppf_cmp(r_lo, r_hi, ctx) > 0)
        return 0;
    tfx_init(&s); tfx_init(&c);
    mpz_inits(lower, upper, anchor, magnitude, width, one, NULL);
    if (work > UINT64_MAX - PPF_FAST_TRIG_GUARD - 128u) goto done;
    /* Reserve room for both Taylor rounding and double-angle reconstruction. */
    scale = work + PPF_FAST_TRIG_GUARD + 64u;
    if (tfx_from_ppf(lower, r_lo, scale, 0) != 0 ||
        tfx_from_ppf(upper, r_hi, scale, 1) != 0 ||
        mpz_cmp(lower, upper) > 0)
        goto done;
    mpz_sub(width, upper, lower);
    mpz_set(anchor, lower);
    anchor_negative = mpz_sgn(anchor) < 0;
    mpz_abs(magnitude, anchor);
    halvings = tfx_choose_halvings(magnitude, scale, work);
    if (halvings == UINT_MAX || scale > UINT64_MAX - halvings) goto done;
    /* Re-express the exact anchor with enough trailing zero bits that the
       halving phase is an exact shift. */
    scale += halvings;
    if (tfx_from_ppf(lower, r_lo, scale, 0) != 0 ||
        tfx_from_ppf(upper, r_hi, scale, 1) != 0)
        goto done;
    mpz_sub(width, upper, lower);
    mpz_set(anchor, lower);
    anchor_negative = mpz_sgn(anchor) < 0;
    mpz_abs(magnitude, anchor);
    if (tfx_sincos_point(&s, &c, magnitude, work, scale, halvings) != 0)
        goto done;
    if (anchor_negative) {
        mpz_neg(one, s.hi);
        mpz_neg(s.hi, s.lo);
        mpz_set(s.lo, one);
    }
    /* sin is increasing on the reduced interval.  The derivative bounds for
       both sin and cos are at most one, so the narrow reduction uncertainty
       can be added directly in fixed-point ulps. */
    mpz_add(s.hi, s.hi, width);
    mpz_sub(c.lo, c.lo, width);
    mpz_add(c.hi, c.hi, width);
    mpz_set_ui(one, 1UL);
    mpz_mul_2exp(one, one, (mp_bitcnt_t)scale);
    if (mpz_cmp(s.lo, one) > 0) mpz_set(s.lo, one);
    if (mpz_cmp(s.hi, one) > 0) mpz_set(s.hi, one);
    mpz_neg(magnitude, one);
    if (mpz_cmp(s.lo, magnitude) < 0) mpz_set(s.lo, magnitude);
    if (mpz_cmp(s.hi, magnitude) < 0) mpz_set(s.hi, magnitude);
    if (mpz_cmp(c.lo, magnitude) < 0) mpz_set(c.lo, magnitude);
    if (mpz_cmp(c.hi, one) > 0) mpz_set(c.hi, one);
    if (tfx_store_interval(sin_lo, sin_hi, s.lo, s.hi, scale, ctx) != 0 ||
        tfx_store_interval(cos_lo, cos_hi, c.lo, c.hi, scale, ctx) != 0)
        goto done;
    rc = 1;
done:
    tfx_clear(&s); tfx_clear(&c);
    mpz_clears(lower, upper, anchor, magnitude, width, one, NULL);
    return rc;
}


/* Positive companion for sinh/cosh. */
static int tbs_sinhcosh_point(ppf_trig_fx_t *s, ppf_trig_fx_t *c,
                              mpz_srcptr argument, ppf_prec_t work,
                              ppf_prec_t scale, unsigned halvings) {
    mpz_t tint, a, a2, snum, cnum, stail, ctail, upper, ap;
    ppf_trig_bs_t root;
    mp_bitcnt_t trailing;
    uint64_t d;
    size_t abits;
    unsigned long terms;
    int rc = 0;
    mpz_inits(tint, a, a2, snum, cnum, stail, ctail, upper, ap, NULL);
    tbs_init(&root);
    if (scale > (ppf_prec_t)ULONG_MAX || halvings > (unsigned)ULONG_MAX)
        goto done;
    mpz_fdiv_q_2exp(tint, argument, halvings);
    if (mpz_sgn(tint) <= 0) goto done;
    trailing = mpz_scan1(tint, 0UL);
    if (trailing == (mp_bitcnt_t)-1 || trailing > scale) goto done;
    mpz_fdiv_q_2exp(a, tint, trailing);
    d = (uint64_t)scale - (uint64_t)trailing;
    if (d > (uint64_t)ULONG_MAX / 2u) goto done;
    abits = mpz_sizeinbase(a, 2);
    if (abits > 1024u && abits * 8u > (size_t)work) goto done;
    terms = tbs_term_count(abits, d, work);
    if (terms == 0UL || terms > 4096UL) goto done;
    mpz_mul(a2, a, a);
    if (tbs_build(&root, a2, (mp_bitcnt_t)(2u * d), 0, 0UL, terms) != 0)
        goto done;

    mpz_mul(snum, a, root.t);
    mpz_mul(ap, a, root.p);
    mpz_abs(stail, ap);
    mpz_mul_2exp(stail, stail, 1UL);
    if (trailing != 0UL) {
        mpz_mul_2exp(snum, snum, trailing);
        mpz_mul_2exp(stail, stail, trailing);
    }
    tbs_fdiv_q(s->lo, snum, &root);
    mpz_add(upper, snum, stail);
    tbs_cdiv_q(s->hi, upper, &root);

    mpz_mul_2exp(cnum, root.w, (mp_bitcnt_t)scale);
    mpz_abs(ctail, root.p);
    mpz_mul_ui(ctail, ctail, 2UL * terms + 1UL);
    mpz_mul_2exp(ctail, ctail, (mp_bitcnt_t)scale + 1UL);
    tbs_fdiv_q(c->lo, cnum, &root);
    mpz_add(upper, cnum, ctail);
    tbs_cdiv_q(c->hi, upper, &root);
    if (mpz_cmp(s->lo, s->hi) > 0 || mpz_cmp(c->lo, c->hi) > 0) goto done;
    rc = 1;
done:
    tbs_clear(&root);
    mpz_clears(tint, a, a2, snum, cnum, stail, ctail, upper, ap, NULL);
    return rc;
}

static int tfx_sinhcosh_series_pair_block4(ppf_trig_fx_t *sinhv,
                                           ppf_trig_fx_t *coshv,
                                           const ppf_trig_fx_t *t,
                                           const ppf_trig_fx_t *q,
                                           ppf_prec_t scale,
                                           ppf_prec_t work,
                                           mpz_ptr scratch) {
    ppf_trig_fx_t powers[5], term, psum, csum, pblock, cblock;
    ppf_trig_fx_t quotient, weighted, pcontrib, ccontrib, ratio, update;
    mpz_t denominator, factor, ptail, ctail;
    unsigned long k = 0UL;
    int rc = -1;
    for (unsigned i = 0; i < 5; ++i) tfx_init(&powers[i]);
    tfx_init(&term); tfx_init(&psum); tfx_init(&csum);
    tfx_init(&pblock); tfx_init(&cblock); tfx_init(&quotient);
    tfx_init(&weighted); tfx_init(&pcontrib); tfx_init(&ccontrib);
    tfx_init(&ratio); tfx_init(&update);
    mpz_inits(denominator, factor, ptail, ctail, NULL);

    mpz_set_ui(powers[0].lo, 1UL);
    mpz_mul_2exp(powers[0].lo, powers[0].lo, (mp_bitcnt_t)scale);
    mpz_set(powers[0].hi, powers[0].lo);
    mpz_set(powers[1].lo, q->lo); mpz_set(powers[1].hi, q->hi);
    for (unsigned j = 2; j <= 4; ++j)
        tfx_mul_positive(&powers[j], &powers[j - 1], q, scale, scratch);
    mpz_set(term.lo, powers[0].lo); mpz_set(term.hi, powers[0].hi);
    mpz_set_ui(psum.lo, 0UL); mpz_set_ui(psum.hi, 0UL);
    mpz_set_ui(csum.lo, 0UL); mpz_set_ui(csum.hi, 0UL);

    while (k < PPF_FAST_TRIG_MAX_TERMS) {
        mpz_set_ui(pblock.lo, 0UL); mpz_set_ui(pblock.hi, 0UL);
        mpz_set_ui(cblock.lo, 0UL); mpz_set_ui(cblock.hi, 0UL);
        mpz_set_ui(denominator, 1UL);
        for (unsigned j = 0; j < 4; ++j) {
            unsigned long weight = 2UL * (k + j) + 1UL;
            tfx_div_z_positive(&quotient, &powers[j], denominator);
            mpz_add(pblock.lo, pblock.lo, quotient.lo);
            mpz_add(pblock.hi, pblock.hi, quotient.hi);
            mpz_mul_ui(weighted.lo, quotient.lo, weight);
            mpz_mul_ui(weighted.hi, quotient.hi, weight);
            mpz_add(cblock.lo, cblock.lo, weighted.lo);
            mpz_add(cblock.hi, cblock.hi, weighted.hi);
            {
                unsigned long aa = 2UL * (k + j) + 2UL;
                unsigned long bb = aa + 1UL;
                if (aa > ULONG_MAX / bb) goto done;
                mpz_set_ui(factor, aa * bb);
                mpz_mul(denominator, denominator, factor);
            }
        }
        tfx_mul_positive(&pcontrib, &term, &pblock, scale, scratch);
        tfx_mul_positive(&ccontrib, &term, &cblock, scale, scratch);
        mpz_add(psum.lo, psum.lo, pcontrib.lo);
        mpz_add(psum.hi, psum.hi, pcontrib.hi);
        mpz_add(csum.lo, csum.lo, ccontrib.lo);
        mpz_add(csum.hi, csum.hi, ccontrib.hi);
        tfx_div_z_positive(&ratio, &powers[4], denominator);
        tfx_mul_positive(&update, &term, &ratio, scale, scratch);
        k += 4UL;
        if (tfx_tail_small(update.hi, scale, work)) {
            mpz_mul_2exp(ptail, update.hi, 1UL);
            mpz_mul_ui(ctail, update.hi, 2UL * k + 1UL);
            mpz_mul_2exp(ctail, ctail, 1UL);
            mpz_set(pblock.lo, psum.lo);
            mpz_add(pblock.hi, psum.hi, ptail);
            mpz_set(coshv->lo, csum.lo);
            mpz_add(coshv->hi, csum.hi, ctail);
            tfx_mul_positive(sinhv, t, &pblock, scale, scratch);
            rc = 0;
            goto done;
        }
        mpz_set(term.lo, update.lo); mpz_set(term.hi, update.hi);
    }
done:
    for (unsigned i = 0; i < 5; ++i) tfx_clear(&powers[i]);
    tfx_clear(&term); tfx_clear(&psum); tfx_clear(&csum);
    tfx_clear(&pblock); tfx_clear(&cblock); tfx_clear(&quotient);
    tfx_clear(&weighted); tfx_clear(&pcontrib); tfx_clear(&ccontrib);
    tfx_clear(&ratio); tfx_clear(&update);
    mpz_clears(denominator, factor, ptail, ctail, NULL);
    return rc;
}

static int tfx_sinhcosh_point(ppf_trig_fx_t *s, ppf_trig_fx_t *c,
                              mpz_srcptr argument, ppf_prec_t work,
                              ppf_prec_t scale, unsigned halvings) {
    ppf_trig_fx_t t, q;
    mpz_t scratch, one, sm, sr, cm, cr, nsm, nsr, ncm, ncr, p0, p1, cross;
    int rc = -1;
    tfx_init(&t); tfx_init(&q);
    mpz_inits(scratch, one, sm, sr, cm, cr, nsm, nsr, ncm, ncr,
              p0, p1, cross, NULL);
    if (halvings > (unsigned)ULONG_MAX ||
        mpz_scan1(argument, 0) < (mp_bitcnt_t)halvings)
        goto done;
    mpz_fdiv_q_2exp(t.lo, argument, halvings);
    mpz_set(t.hi, t.lo);
    if (!tbs_sinhcosh_point(s, c, argument, work, scale, halvings)) {
        tfx_square_positive(&q, &t, scale, scratch);
        if (tfx_sinhcosh_series_pair_block4(s, c, &t, &q,
                                             scale, work, scratch) != 0)
            goto done;
    }
    tfx_interval_to_ball(sm, sr, s, scratch);
    tfx_interval_to_ball(cm, cr, c, scratch);
    mpz_set_ui(one, 1UL);
    mpz_mul_2exp(one, one, (mp_bitcnt_t)scale);
    for (unsigned i = 0; i < halvings; ++i) {
        mpz_mul(p0, sm, cm);
        mpz_mul_2exp(p0, p0, 1UL);
        mpz_fdiv_q_2exp(nsm, p0, (mp_bitcnt_t)scale);
        mpz_mul(p1, sm, sm);
        mpz_mul_2exp(p1, p1, 1UL);
        mpz_fdiv_q_2exp(p1, p1, (mp_bitcnt_t)scale);
        mpz_add(ncm, one, p1);
        mpz_add(nsr, sr, cr);
        mpz_mul_2exp(nsr, nsr, 1UL);
        if (mpz_sgn(sr) != 0 && mpz_sgn(cr) != 0) {
            mpz_mul(cross, sr, cr);
            mpz_mul_2exp(cross, cross, 1UL);
            mpz_cdiv_q_2exp(cross, cross, (mp_bitcnt_t)scale);
            mpz_add(nsr, nsr, cross);
        }
        mpz_add_ui(nsr, nsr, 1UL);
        mpz_mul_2exp(ncr, sr, 2UL);
        if (mpz_sgn(sr) != 0) {
            mpz_mul(cross, sr, sr);
            mpz_mul_2exp(cross, cross, 1UL);
            mpz_cdiv_q_2exp(cross, cross, (mp_bitcnt_t)scale);
            mpz_add(ncr, ncr, cross);
        }
        mpz_add_ui(ncr, ncr, 1UL);
        mpz_set(sm, nsm); mpz_set(sr, nsr);
        mpz_set(cm, ncm); mpz_set(cr, ncr);
    }
    mpz_sub(s->lo, sm, sr); mpz_add(s->hi, sm, sr);
    mpz_sub(c->lo, cm, cr); mpz_add(c->hi, cm, cr);
    rc = 0;
done:
    tfx_clear(&t); tfx_clear(&q);
    mpz_clears(scratch, one, sm, sr, cm, cr, nsm, nsr, ncm, ncr,
               p0, p1, cross, NULL);
    return rc;
}

int ppf_fast_sinhcosh_internal(ppf_t *sinh_lo, ppf_t *sinh_hi,
                               ppf_t *cosh_lo, ppf_t *cosh_hi,
                               const ppf_t *x_lo, const ppf_t *x_hi,
                               ppf_prec_t work, ppf_context_t *ctx) {
    ppf_trig_fx_t s, c;
    mpz_t lower, upper, width, magnitude;
    ppf_prec_t scale;
    unsigned halvings;
    int rc = 0;
    if (!sinh_lo || !sinh_hi || !cosh_lo || !cosh_hi || !x_lo || !x_hi ||
        !ctx || !ppf_regular_p(x_lo) || !ppf_regular_p(x_hi) ||
        ppf_sgn(x_lo, ctx) < 0 || ppf_cmp(x_lo, x_hi, ctx) > 0)
        return 0;
    tfx_init(&s); tfx_init(&c);
    mpz_inits(lower, upper, width, magnitude, NULL);
    if (work > UINT64_MAX - PPF_FAST_TRIG_GUARD - 128u) goto done;
    scale = work + PPF_FAST_TRIG_GUARD + 64u;
    if (tfx_from_ppf(lower, x_lo, scale, 0) != 0 ||
        tfx_from_ppf(upper, x_hi, scale, 1) != 0 ||
        mpz_cmp(lower, upper) > 0)
        goto done;
    mpz_sub(width, upper, lower);
    /* The fast direct kernel currently requires an exact dyadic anchor.
       Nonzero-width source intervals keep the established exp-based path. */
    if (mpz_sgn(width) != 0) goto done;
    mpz_set(magnitude, lower);
    halvings = tfx_choose_halvings(magnitude, scale, work);
    if (halvings == UINT_MAX || scale > UINT64_MAX - halvings) goto done;
    scale += halvings;
    if (tfx_from_ppf(magnitude, x_lo, scale, 0) != 0 ||
        tfx_sinhcosh_point(&s, &c, magnitude, work, scale, halvings) != 0)
        goto done;
    if (tfx_store_interval(sinh_lo, sinh_hi, s.lo, s.hi, scale, ctx) != 0 ||
        tfx_store_interval(cosh_lo, cosh_hi, c.lo, c.hi, scale, ctx) != 0)
        goto done;
    rc = 1;
done:
    tfx_clear(&s); tfx_clear(&c);
    mpz_clears(lower, upper, width, magnitude, NULL);
    return rc;
}

static void tfx_mul_ui_positive(ppf_trig_fx_t *out,
                                const ppf_trig_fx_t *a,
                                unsigned long value) {
    mpz_mul_ui(out->lo, a->lo, value);
    mpz_mul_ui(out->hi, a->hi, value);
}



typedef struct { mpz_t p, q, t; } ppf_atan_bs_t;
static void tabs_init(ppf_atan_bs_t *x) { mpz_inits(x->p, x->q, x->t, NULL); }
static void tabs_clear(ppf_atan_bs_t *x) { mpz_clears(x->p, x->q, x->t, NULL); }
static int tabs_build(ppf_atan_bs_t *out, mpz_srcptr a2,
                      mp_bitcnt_t denominator_bits,
                      unsigned long begin, unsigned long end) {
    if (end <= begin) return -1;
    if (end - begin == 1UL) {
        unsigned long num = 2UL * begin + 1UL;
        unsigned long den = num + 2UL;
        mpz_mul_ui(out->p, a2, num); mpz_neg(out->p, out->p);
        mpz_set_ui(out->q, den); mpz_mul_2exp(out->q, out->q, denominator_bits);
        mpz_set(out->t, out->q);
        return 0;
    }
    {
        unsigned long mid = begin + (end - begin) / 2UL;
        ppf_atan_bs_t l, r; mpz_t u, v; int rc=-1;
        tabs_init(&l); tabs_init(&r); mpz_inits(u,v,NULL);
        if (tabs_build(&l,a2,denominator_bits,begin,mid)!=0 ||
            tabs_build(&r,a2,denominator_bits,mid,end)!=0) goto done;
        mpz_mul(u,l.t,r.q); mpz_mul(v,l.p,r.t); mpz_add(out->t,u,v);
        mpz_mul(out->q,l.q,r.q); mpz_mul(out->p,l.p,r.p); rc=0;
done:
        mpz_clears(u,v,NULL); tabs_clear(&l); tabs_clear(&r); return rc;
    }
}
static unsigned long tabs_term_count(size_t abits, uint64_t d, ppf_prec_t work) {
    long double logt=(long double)abits-(long double)d;
    long double target=-(long double)work-128.0L;
    if(logt>=0.0L)return 0UL;
    for(unsigned long n=1;n<1000000UL;++n){
        long double l=(long double)(2UL*n+1UL)*logt-
                      log2l((long double)(2UL*n+1UL));
        if(l<target)return n;
    }
    return 0UL;
}
static int tabs_atan_exact(ppf_trig_fx_t *out, mpz_srcptr value,
                           ppf_prec_t work, ppf_prec_t scale) {
    mpz_t a,a2,num,tail,lo,hi,ap; ppf_atan_bs_t root;
    mp_bitcnt_t trailing; uint64_t d; size_t abits; unsigned long terms; int rc=0;
    mpz_inits(a,a2,num,tail,lo,hi,ap,NULL); tabs_init(&root);
    if(mpz_sgn(value)<=0 || scale>(ppf_prec_t)ULONG_MAX)goto done;
    trailing=mpz_scan1(value,0UL); if(trailing==(mp_bitcnt_t)-1||trailing>scale)goto done;
    mpz_fdiv_q_2exp(a,value,trailing); d=(uint64_t)scale-(uint64_t)trailing;
    abits=mpz_sizeinbase(a,2); if(abits>1024u && abits*8u>(size_t)work)goto done;
    terms=tabs_term_count(abits,d,work); if(terms==0UL||terms>16384UL)goto done;
    mpz_mul(a2,a,a); if(tabs_build(&root,a2,(mp_bitcnt_t)(2u*d),0UL,terms)!=0)goto done;
    mpz_mul(num,a,root.t); mpz_mul(ap,a,root.p); mpz_abs(tail,ap); mpz_mul_2exp(tail,tail,1UL);
    if(trailing){mpz_mul_2exp(num,num,trailing);mpz_mul_2exp(tail,tail,trailing);}
    mpz_sub(lo,num,tail);mpz_add(hi,num,tail);
    mpz_fdiv_q(out->lo,lo,root.q);mpz_cdiv_q(out->hi,hi,root.q);rc=1;
done:
    tabs_clear(&root);mpz_clears(a,a2,num,tail,lo,hi,ap,NULL);return rc;
}

static int tfx_atan_point_block4(ppf_trig_fx_t *out,
                                  mpz_srcptr argument,
                                  ppf_prec_t work,
                                  ppf_prec_t scale) {
    enum { MAX_BLOCK = 32 };
    unsigned block_size = work < 12000u ? 4u : (work < 80000u ? 16u : 32u);
    ppf_trig_fx_t t, q, powers[MAX_BLOCK + 1], term, sum, block, quotient;
    ppf_trig_fx_t contribution, update;
    mpz_t scratch, tail;
    unsigned long k = 0UL;
    int rc = -1;
    tfx_init(&t); tfx_init(&q);
    for (unsigned i = 0; i <= MAX_BLOCK; ++i) tfx_init(&powers[i]);
    tfx_init(&term); tfx_init(&sum); tfx_init(&block); tfx_init(&quotient);
    tfx_init(&contribution); tfx_init(&update);
    mpz_inits(scratch, tail, NULL);
    mpz_set(t.lo, argument); mpz_set(t.hi, argument);
    tfx_square_positive(&q, &t, scale, scratch);
    mpz_set_ui(powers[0].lo, 1UL);
    mpz_mul_2exp(powers[0].lo, powers[0].lo, (mp_bitcnt_t)scale);
    mpz_set(powers[0].hi, powers[0].lo);
    mpz_set(powers[1].lo, q.lo); mpz_set(powers[1].hi, q.hi);
    for (unsigned j = 2; j <= block_size; ++j)
        tfx_mul_positive(&powers[j], &powers[j - 1], &q, scale, scratch);
    mpz_set(term.lo, powers[0].lo); mpz_set(term.hi, powers[0].hi);
    mpz_set_ui(sum.lo, 0UL); mpz_set_ui(sum.hi, 0UL);
    while (k < PPF_FAST_TRIG_MAX_TERMS) {
        mpz_set_ui(block.lo, 0UL); mpz_set_ui(block.hi, 0UL);
        for (unsigned j = 0; j < block_size; ++j) {
            unsigned long den = 2UL * (k + j) + 1UL;
            tfx_div_ui_positive(&quotient, &powers[j], den);
            tfx_add_signed_term(&block, &quotient, (j & 1u) ? -1 : 1);
        }
        if (mpz_sgn(block.lo) <= 0) goto done;
        tfx_mul_positive(&contribution, &term, &block, scale, scratch);
        mpz_add(sum.lo, sum.lo, contribution.lo);
        mpz_add(sum.hi, sum.hi, contribution.hi);
        tfx_mul_positive(&update, &term, &powers[block_size], scale, scratch);
        k += block_size;
        tfx_div_ui_positive(&quotient, &update, 2UL * k + 1UL);
        if (tfx_tail_small(quotient.hi, scale, work)) {
            mpz_mul_2exp(tail, quotient.hi, 1UL);
            mpz_sub(block.lo, sum.lo, tail);
            mpz_add(block.hi, sum.hi, tail);
            tfx_mul_positive(out, &t, &block, scale, scratch);
            rc = 0;
            goto done;
        }
        mpz_set(term.lo, update.lo); mpz_set(term.hi, update.hi);
    }
done:
    tfx_clear(&t); tfx_clear(&q);
    for (unsigned i = 0; i <= MAX_BLOCK; ++i) tfx_clear(&powers[i]);
    tfx_clear(&term); tfx_clear(&sum); tfx_clear(&block); tfx_clear(&quotient);
    tfx_clear(&contribution); tfx_clear(&update);
    mpz_clears(scratch, tail, NULL);
    return rc;
}

static int tfx_atan_point(ppf_trig_fx_t *out,
                          mpz_srcptr argument,
                          ppf_prec_t work,
                          ppf_prec_t scale) {
    if (tfx_atan_point_block4(out, argument, work, scale) == 0) return 0;
    ppf_trig_fx_t t, t2, sum, term, prod, scaled, next;
    mpz_t scratch, tail;
    int sign = -1;
    int rc = -1;
    tfx_init(&t); tfx_init(&t2); tfx_init(&sum); tfx_init(&term);
    tfx_init(&prod); tfx_init(&scaled); tfx_init(&next);
    mpz_inits(scratch, tail, NULL);
    mpz_set(t.lo, argument); mpz_set(t.hi, argument);
    mpz_set(sum.lo, argument); mpz_set(sum.hi, argument);
    mpz_set(term.lo, argument); mpz_set(term.hi, argument);
    tfx_square_positive(&t2, &t, scale, scratch);
    for (unsigned long k = 0; k < PPF_FAST_TRIG_MAX_TERMS; ++k) {
        unsigned long num = 2UL * k + 1UL;
        unsigned long den = num + 2UL;
        tfx_mul_positive(&prod, &term, &t2, scale, scratch);
        tfx_mul_ui_positive(&scaled, &prod, num);
        tfx_div_ui_positive(&next, &scaled, den);
        if (tfx_tail_small(next.hi, scale, work)) {
            mpz_mul_2exp(tail, next.hi, 1UL);
            mpz_sub(out->lo, sum.lo, tail);
            mpz_add(out->hi, sum.hi, tail);
            rc = 0;
            goto done;
        }
        tfx_add_signed_term(&sum, &next, sign);
        mpz_set(term.lo, next.lo);
        mpz_set(term.hi, next.hi);
        sign = -sign;
    }
done:
    tfx_clear(&t); tfx_clear(&t2); tfx_clear(&sum); tfx_clear(&term);
    tfx_clear(&prod); tfx_clear(&scaled); tfx_clear(&next);
    mpz_clears(scratch, tail, NULL);
    return rc;
}


/* Full fixed-point half-angle reduction for atan on 0<=x<=1. */
int ppf_fast_atan_positive_internal(ppf_t *out_lo, ppf_t *out_hi,
                                    const ppf_t *x_lo, const ppf_t *x_hi,
                                    ppf_prec_t work, ppf_context_t *ctx) {
    ppf_trig_fx_t t, q, a;
    mpz_t one, radicand, root, rem, denlo, denhi, scratch, width;
    ppf_prec_t scale;
    unsigned reductions = 0u, target;
    int rc = 0;
    if (!out_lo || !out_hi || !x_lo || !x_hi || !ctx ||
        !ppf_regular_p(x_lo) || !ppf_regular_p(x_hi) ||
        ppf_sgn(x_lo, ctx) < 0 || ppf_cmp(x_lo, x_hi, ctx) > 0 ||
        ppf_cmp_ui(x_hi, 1UL, ctx) > 0)
        return 0;
    tfx_init(&t); tfx_init(&q); tfx_init(&a);
    mpz_inits(one, radicand, root, rem, denlo, denhi, scratch, width, NULL);
    if (work > UINT64_MAX - PPF_FAST_TRIG_GUARD - 128u) goto done;
    scale = work + PPF_FAST_TRIG_GUARD + 80u;
    if (scale > (ppf_prec_t)ULONG_MAX ||
        tfx_from_ppf(t.lo, x_lo, scale, 0) != 0 ||
        tfx_from_ppf(t.hi, x_hi, scale, 1) != 0 ||
        mpz_cmp(t.lo, t.hi) > 0)
        goto done;
    mpz_set_ui(one, 1UL);
    mpz_mul_2exp(one, one, (mp_bitcnt_t)scale);
    if (mpz_cmp(t.lo, t.hi) == 0 && tabs_atan_exact(&a, t.lo, work, scale)) {
        if (tfx_store_interval(out_lo, out_hi, a.lo, a.hi, scale, ctx) != 0)
            goto done;
        rc = 1;
        goto done;
    }
    /* Each step approximately halves the argument.  Stop with enough bits of
       contraction that the blocked atan series has a small term count. */
    if (PPF_FAST_ATAN_REDUCTIONS != 0u) {
        target = PPF_FAST_ATAN_REDUCTIONS;
    } else {
        target = 6u + (tfx_ceil_log2_u64(work > 0u ? work : 1u) + 1u) / 2u;
        if (target < 8u) target = 8u;
        if (target > 32u) target = 32u;
    }
    for (reductions = 0u; reductions < target; ++reductions) {
        /* q=t^2. */
        mpz_mul(scratch, t.lo, t.lo);
        mpz_fdiv_q_2exp(q.lo, scratch, (mp_bitcnt_t)scale);
        mpz_mul(scratch, t.hi, t.hi);
        mpz_cdiv_q_2exp(q.hi, scratch, (mp_bitcnt_t)scale);

        /* sqrt(1+q) at the same fixed scale. */
        mpz_add(radicand, one, q.lo);
        mpz_mul_2exp(radicand, radicand, (mp_bitcnt_t)scale);
        mpz_sqrt(root, radicand);
        mpz_add(denlo, one, root);

        mpz_add(radicand, one, q.hi);
        mpz_mul_2exp(radicand, radicand, (mp_bitcnt_t)scale);
        mpz_sqrtrem(root, rem, radicand);
        if (mpz_sgn(rem) != 0) mpz_add_ui(root, root, 1UL);
        mpz_add(denhi, one, root);

        /* t/(1+sqrt(1+t^2)), directed. */
        mpz_mul_2exp(scratch, t.lo, (mp_bitcnt_t)scale);
        mpz_fdiv_q(t.lo, scratch, denhi);
        mpz_mul_2exp(scratch, t.hi, (mp_bitcnt_t)scale);
        mpz_cdiv_q(t.hi, scratch, denlo);
        if (mpz_cmp(t.lo, t.hi) > 0) goto done;
    }
    mpz_sub(width, t.hi, t.lo);
    if (tfx_atan_point(&a, t.lo, work, scale) != 0) goto done;
    mpz_add(a.hi, a.hi, width);
    mpz_mul_2exp(a.lo, a.lo, reductions);
    mpz_mul_2exp(a.hi, a.hi, reductions);
    if (tfx_store_interval(out_lo, out_hi, a.lo, a.hi, scale, ctx) != 0)
        goto done;
    rc = 1;
done:
    tfx_clear(&t); tfx_clear(&q); tfx_clear(&a);
    mpz_clears(one, radicand, root, rem, denlo, denhi, scratch, width, NULL);
    return rc;
}

int ppf_fast_atan_reduced_internal(ppf_t *out_lo, ppf_t *out_hi,
                                   const ppf_t *x_lo, const ppf_t *x_hi,
                                   ppf_prec_t work, ppf_context_t *ctx) {
    ppf_trig_fx_t a;
    mpz_t lower, upper, width;
    ppf_prec_t scale;
    int rc = 0;
    if (!out_lo || !out_hi || !x_lo || !x_hi || !ctx ||
        !ppf_regular_p(x_lo) || !ppf_regular_p(x_hi) ||
        ppf_sgn(x_lo, ctx) < 0 || ppf_cmp(x_lo, x_hi, ctx) > 0)
        return 0;
    tfx_init(&a);
    mpz_inits(lower, upper, width, NULL);
    if (work > UINT64_MAX - PPF_FAST_TRIG_GUARD - 64u) goto done;
    scale = work + PPF_FAST_TRIG_GUARD + 64u;
    if (tfx_from_ppf(lower, x_lo, scale, 0) != 0 ||
        tfx_from_ppf(upper, x_hi, scale, 1) != 0 ||
        mpz_cmp(lower, upper) > 0)
        goto done;
    mpz_sub(width, upper, lower);
    if (tfx_atan_point(&a, lower, work, scale) != 0) goto done;
    /* atan is increasing and |atan'| <= 1. */
    mpz_add(a.hi, a.hi, width);
    if (tfx_store_interval(out_lo, out_hi, a.lo, a.hi, scale, ctx) != 0)
        goto done;
    rc = 1;
done:
    tfx_clear(&a);
    mpz_clears(lower, upper, width, NULL);
    return rc;
}

static void tfx_set_one(ppf_trig_fx_t *one, ppf_prec_t scale) {
    mpz_set_ui(one->lo, 1UL);
    mpz_mul_2exp(one->lo, one->lo, (mp_bitcnt_t)scale);
    mpz_set(one->hi, one->lo);
}

static int tfx_div_positive(ppf_trig_fx_t *out,
                            const ppf_trig_fx_t *num,
                            const ppf_trig_fx_t *den,
                            ppf_prec_t scale,
                            mpz_ptr scratch) {
    if (mpz_sgn(num->lo) < 0 || mpz_sgn(den->lo) <= 0 ||
        scale > (ppf_prec_t)ULONG_MAX)
        return -1;
    mpz_mul_2exp(scratch, num->lo, (mp_bitcnt_t)scale);
    mpz_fdiv_q(out->lo, scratch, den->hi);
    mpz_mul_2exp(scratch, num->hi, (mp_bitcnt_t)scale);
    mpz_cdiv_q(out->hi, scratch, den->lo);
    return 0;
}

/* fn: 0=sinh,1=cosh,2=tanh,3=sech,4=csch,5=coth. */
int ppf_fast_hyperbolic_from_exp_internal(ppf_t *out_lo, ppf_t *out_hi,
                                          const ppf_t *u_lo,
                                          const ppf_t *u_hi,
                                          int fn, int negative,
                                          ppf_prec_t work,
                                          ppf_context_t *ctx) {
    ppf_trig_fx_t u, u2, one, two_u, one_minus, one_plus, result;
    mpz_t scratch, tmp;
    ppf_prec_t scale;
    int rc = 0;
    if (!out_lo || !out_hi || !u_lo || !u_hi || !ctx || fn < 0 || fn > 5 ||
        !ppf_regular_p(u_lo) || !ppf_regular_p(u_hi) ||
        ppf_sgn(u_lo, ctx) <= 0 || ppf_cmp(u_lo, u_hi, ctx) > 0)
        return 0;
    tfx_init(&u); tfx_init(&u2); tfx_init(&one); tfx_init(&two_u);
    tfx_init(&one_minus); tfx_init(&one_plus); tfx_init(&result);
    mpz_inits(scratch, tmp, NULL);
    if (work > UINT64_MAX - PPF_FAST_TRIG_GUARD - 64u) goto done;
    scale = work + PPF_FAST_TRIG_GUARD + 64u;
    if (tfx_from_ppf(u.lo, u_lo, scale, 0) != 0 ||
        tfx_from_ppf(u.hi, u_hi, scale, 1) != 0 ||
        mpz_sgn(u.lo) <= 0)
        goto done;
    tfx_square_positive(&u2, &u, scale, scratch);
    tfx_set_one(&one, scale);
    mpz_mul_2exp(two_u.lo, u.lo, 1UL);
    mpz_mul_2exp(two_u.hi, u.hi, 1UL);
    mpz_sub(one_minus.lo, one.lo, u2.hi);
    mpz_sub(one_minus.hi, one.hi, u2.lo);
    mpz_add(one_plus.lo, one.lo, u2.lo);
    mpz_add(one_plus.hi, one.hi, u2.hi);
    switch (fn) {
        case 0: /* sinh */
            if (tfx_div_positive(&result, &one_minus, &two_u, scale, scratch) != 0)
                goto done;
            break;
        case 1: /* cosh */
            if (tfx_div_positive(&result, &one_plus, &two_u, scale, scratch) != 0)
                goto done;
            break;
        case 2: /* tanh */
            if (tfx_div_positive(&result, &one_minus, &one_plus, scale, scratch) != 0)
                goto done;
            break;
        case 3: /* sech */
            if (tfx_div_positive(&result, &two_u, &one_plus, scale, scratch) != 0)
                goto done;
            break;
        case 4: /* csch */
            if (tfx_div_positive(&result, &two_u, &one_minus, scale, scratch) != 0)
                goto done;
            break;
        case 5: /* coth */
            if (tfx_div_positive(&result, &one_plus, &one_minus, scale, scratch) != 0)
                goto done;
            break;
    }
    if (negative && (fn == 0 || fn == 2 || fn == 4 || fn == 5)) {
        mpz_neg(tmp, result.hi);
        mpz_neg(result.hi, result.lo);
        mpz_set(result.lo, tmp);
    }
    if (tfx_store_interval(out_lo, out_hi, result.lo, result.hi, scale, ctx) != 0)
        goto done;
    rc = 1;
done:
    tfx_clear(&u); tfx_clear(&u2); tfx_clear(&one); tfx_clear(&two_u);
    tfx_clear(&one_minus); tfx_clear(&one_plus); tfx_clear(&result);
    mpz_clears(scratch, tmp, NULL);
    return rc;
}


static PPF_TRIG_UNUSED void tfx_add_interval(ppf_trig_fx_t *out,
                             const ppf_trig_fx_t *a,
                             const ppf_trig_fx_t *b) {
    mpz_add(out->lo, a->lo, b->lo);
    mpz_add(out->hi, a->hi, b->hi);
}

static void tfx_neg_interval(ppf_trig_fx_t *out,
                             const ppf_trig_fx_t *a,
                             mpz_ptr scratch) {
    mpz_neg(scratch, a->hi);
    mpz_neg(out->hi, a->lo);
    mpz_set(out->lo, scratch);
}

static int tfx_scale2_interval(ppf_trig_fx_t *out,
                               const ppf_trig_fx_t *a,
                               long shift) {
    if (shift >= 0) {
        mpz_mul_2exp(out->lo, a->lo, (mp_bitcnt_t)(unsigned long)shift);
        mpz_mul_2exp(out->hi, a->hi, (mp_bitcnt_t)(unsigned long)shift);
    } else {
        unsigned long r = (unsigned long)(-(shift + 1L)) + 1UL;
        mpz_fdiv_q_2exp(out->lo, a->lo, (mp_bitcnt_t)r);
        mpz_cdiv_q_2exp(out->hi, a->hi, (mp_bitcnt_t)r);
    }
    return 0;
}

static PPF_TRIG_UNUSED int tfx_sqrt_positive(ppf_trig_fx_t *out,
                              const ppf_trig_fx_t *a,
                              ppf_prec_t scale,
                              mpz_ptr scratch) {
    if (mpz_sgn(a->lo) < 0 || scale > (ppf_prec_t)ULONG_MAX) return -1;
    mpz_mul_2exp(scratch, a->lo, (mp_bitcnt_t)scale);
    mpz_sqrt(out->lo, scratch);
    mpz_mul_2exp(scratch, a->hi, (mp_bitcnt_t)scale);
    {
        int exact = mpz_perfect_square_p(scratch);
        mpz_sqrt(out->hi, scratch);
        if (!exact) mpz_add_ui(out->hi, out->hi, 1UL);
    }
    return 0;
}

static int tfx_dyadic_to_scale(mpz_ptr out, mpz_srcptr value,
                                ppf_exp_t exponent, ppf_prec_t scale,
                                int upper) {
    int neg_shift;
    uint64_t shift;
    if (tfx_exp_plus_scale(exponent, scale, &neg_shift, &shift) != 0 ||
        shift > (uint64_t)ULONG_MAX) return -1;
    if (!neg_shift) mpz_mul_2exp(out, value, (mp_bitcnt_t)shift);
    else if (upper) mpz_cdiv_q_2exp(out, value, (mp_bitcnt_t)shift);
    else mpz_fdiv_q_2exp(out, value, (mp_bitcnt_t)shift);
    return 0;
}

static int tfx_set_pi_interval(ppf_trig_fx_t *out, ppf_prec_t scale,
                               ppf_prec_t work) {
    mpz_t lo, hi;
    ppf_exp_t exponent;
    int rc = -1;
    mpz_inits(lo, hi, NULL);
    if (ppf_get_constant_interval(PPF_CONSTANT_PI, lo, hi, &exponent,
                                  work + 32u, 1UL) != 0)
        goto done;
    if (tfx_dyadic_to_scale(out->lo, lo, exponent, scale, 0) != 0 ||
        tfx_dyadic_to_scale(out->hi, hi, exponent, scale, 1) != 0)
        goto done;
    rc = 0;
done:
    mpz_clears(lo, hi, NULL);
    return rc;
}


static int tfx_atan_half_angle(ppf_trig_fx_t *out,
                                const ppf_trig_fx_t *t,
                                ppf_prec_t scale,
                                mpz_ptr m, mpz_ptr r,
                                mpz_ptr qmid, mpz_ptr qrad,
                                mpz_ptr smid, mpz_ptr srad,
                                mpz_ptr denlo, mpz_ptr denhi,
                                mpz_ptr p0, mpz_ptr p1) {
    if (mpz_sgn(t->lo) < 0 || scale > (ppf_prec_t)ULONG_MAX) return -1;
    /* Midpoint-radius for t. */
    mpz_add(p0, t->lo, t->hi);
    mpz_fdiv_q_2exp(m, p0, 1UL);
    mpz_sub(r, m, t->lo);
    mpz_sub(p0, t->hi, m);
    if (mpz_cmp(p0, r) > 0) mpz_set(r, p0);

    /* t^2: one full midpoint square and an exact radius bound. */
    mpz_mul(p0, m, m);
    mpz_fdiv_q_2exp(qmid, p0, (mp_bitcnt_t)scale);
    mpz_mul(p0, m, r);
    mpz_mul_2exp(p0, p0, 1UL);
    mpz_mul(p1, r, r);
    mpz_add(p0, p0, p1);
    mpz_cdiv_q_2exp(qrad, p0, (mp_bitcnt_t)scale);
    mpz_add_ui(qrad, qrad, 1UL);

    /* sqrt(1+t^2).  Since the argument is >=1, the fixed-point derivative
       is <=1/2, so ceil(qrad/2)+1 is a rigorous propagated radius. */
    mpz_set_ui(p0, 1UL);
    mpz_mul_2exp(p0, p0, (mp_bitcnt_t)scale);
    mpz_add(qmid, qmid, p0);
    mpz_mul_2exp(p1, qmid, (mp_bitcnt_t)scale);
    mpz_sqrt(smid, p1);
    mpz_cdiv_q_2exp(srad, qrad, 1UL);
    mpz_add_ui(srad, srad, 1UL);

    mpz_add(denlo, p0, smid);
    mpz_sub(denlo, denlo, srad);
    mpz_add(denhi, p0, smid);
    mpz_add(denhi, denhi, srad);
    if (mpz_sgn(denlo) <= 0) return -1;
    mpz_mul_2exp(p0, t->lo, (mp_bitcnt_t)scale);
    mpz_fdiv_q(out->lo, p0, denhi);
    mpz_mul_2exp(p0, t->hi, (mp_bitcnt_t)scale);
    mpz_cdiv_q(out->hi, p0, denlo);
    return 0;
}

static unsigned tfx_atan_full_reductions(ppf_prec_t work) {
    long double root = sqrtl((long double)(work ? work : 1u));
    unsigned r = (unsigned)(root * 0.25L + 0.999999L);
    if (r < 4u) r = 4u;
    if (r > 64u) r = 64u;
    return r;
}

int ppf_fast_atan_internal(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                           ppf_context_t *ctx, int *ternary_out) {
    if (ppf_fast_atan_halley_internal(rop, x, rnd, ctx, ternary_out))
        return 1;
    ppf_trig_fx_t t, ax, one, t2, den, root, temp, value, pi, halfpi;
    mpz_t scratch, width;
    mpz_t hm, hr, hqmid, hqrad, hsmid, hsrad, hdenlo, hdenhi, hp0, hp1;
    ppf_prec_t work;
    int ok = 0, ternary = 0;
    if (!rop || !x || !ctx || !ppf_regular_p(x) || ppf_zero_p(x)) return 0;
    tfx_init(&t); tfx_init(&ax); tfx_init(&one); tfx_init(&t2);
    tfx_init(&den); tfx_init(&root); tfx_init(&temp); tfx_init(&value);
    tfx_init(&pi); tfx_init(&halfpi);
    mpz_inits(scratch, width, hm, hr, hqmid, hqrad, hsmid, hsrad,
              hdenlo, hdenhi, hp0, hp1, NULL);
    work = rop->precision + 72u;
    if (work < rop->precision) goto done;
    for (unsigned attempt = 0; attempt < 5u; ++attempt) {
        unsigned reductions = tfx_atan_full_reductions(work);
        ppf_prec_t scale;
        int neg, reciprocal = 0;
        if (work > UINT64_MAX - reductions - 144u) break;
        scale = work + reductions + 144u;
        if (scale > (ppf_prec_t)INT64_MAX || scale > (ppf_prec_t)ULONG_MAX)
            break;
        if (tfx_from_ppf(t.lo, x, scale, 0) != 0 ||
            tfx_from_ppf(t.hi, x, scale, 1) != 0)
            break;
        neg = mpz_sgn(t.hi) < 0;
        if (neg) { tfx_neg_interval(&ax, &t, scratch); mpz_set(t.lo, ax.lo); mpz_set(t.hi, ax.hi); }
        mpz_set_ui(one.lo, 1UL); mpz_mul_2exp(one.lo, one.lo, (mp_bitcnt_t)scale);
        mpz_set(one.hi, one.lo);
        if (mpz_cmp(t.lo, one.lo) > 0) {
            if (tfx_div_positive(&temp, &one, &t, scale, scratch) != 0) break;
            mpz_set(t.lo, temp.lo); mpz_set(t.hi, temp.hi);
            reciprocal = 1;
        }
        for (unsigned i = 0; i < reductions; ++i) {
            if (tfx_atan_half_angle(&temp, &t, scale, hm, hr, hqmid, hqrad,
                                    hsmid, hsrad, hdenlo, hdenhi, hp0, hp1) != 0)
                goto next_attempt;
            mpz_set(t.lo, temp.lo); mpz_set(t.hi, temp.hi);
        }
        mpz_sub(width, t.hi, t.lo);
        if (tfx_atan_point(&value, t.lo, work, scale) != 0) goto next_attempt;
        mpz_add(value.hi, value.hi, width); /* atan is 1-Lipschitz */
        if (reductions > (unsigned)LONG_MAX) goto next_attempt;
        tfx_scale2_interval(&temp, &value, (long)reductions);
        mpz_set(value.lo, temp.lo); mpz_set(value.hi, temp.hi);
        if (reciprocal) {
            if (tfx_set_pi_interval(&pi, scale, work) != 0) goto next_attempt;
            tfx_scale2_interval(&halfpi, &pi, -1L);
            mpz_sub(temp.lo, halfpi.lo, value.hi);
            mpz_sub(temp.hi, halfpi.hi, value.lo);
            mpz_set(value.lo, temp.lo); mpz_set(value.hi, temp.hi);
        }
        if (neg) {
            tfx_neg_interval(&temp, &value, scratch);
            mpz_set(value.lo, temp.lo); mpz_set(value.hi, temp.hi);
        }
        {
            ppf_interval_status_t status = ppf_set_dyadic_interval(
                rop, value.lo, value.hi, -(ppf_exp_t)scale,
                rnd, &ternary, ctx);
            if (status == PPF_INTERVAL_DECIDED) {
                if (ternary_out) *ternary_out = ternary;
                ok = 1;
                goto done;
            }
        }
next_attempt:
        {
            ppf_prec_t next = work + work / 8u + 64u;
            if (next <= work) break;
            work = next;
        }
    }
done:
    tfx_clear(&t); tfx_clear(&ax); tfx_clear(&one); tfx_clear(&t2);
    tfx_clear(&den); tfx_clear(&root); tfx_clear(&temp); tfx_clear(&value);
    tfx_clear(&pi); tfx_clear(&halfpi);
    mpz_clears(scratch, width, hm, hr, hqmid, hqrad, hsmid, hsrad,
               hdenlo, hdenhi, hp0, hp1, NULL);
    return ok;
}
