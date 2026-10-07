/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef PPF_FAST_TRACE
#include <stdio.h>
#define FX_TRACE(...) fprintf(stderr, __VA_ARGS__)
#else
#define FX_TRACE(...) ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PPF_FAST_UNUSED __attribute__((unused))
#else
#define PPF_FAST_UNUSED
#endif

#if defined(__SIZEOF_INT128__)
__extension__ typedef __int128 ppf_fast_i128_t;
__extension__ typedef unsigned __int128 ppf_fast_u128_t;
#endif

#ifndef PPF_FAST_TRANS_GUARD
#define PPF_FAST_TRANS_GUARD 48u
#endif
#ifndef PPF_FAST_TRANS_ATTEMPTS
#define PPF_FAST_TRANS_ATTEMPTS 5u
#endif
#ifndef PPF_FAST_TRANS_MAX_SHIFT
#define PPF_FAST_TRANS_MAX_SHIFT 10000000u
#endif
#ifndef PPF_FAST_LOG_AGM_THRESHOLD
#define PPF_FAST_LOG_AGM_THRESHOLD UINT64_MAX
#endif
#ifndef PPF_FAST_EXP_STOP_EXTRA
#define PPF_FAST_EXP_STOP_EXTRA 16u
#endif
#ifndef PPF_FAST_EXP_BITBURST_THRESHOLD
#define PPF_FAST_EXP_BITBURST_THRESHOLD 768u
#endif
#ifndef PPF_FAST_EXP2_DIRECT_THRESHOLD
#define PPF_FAST_EXP2_DIRECT_THRESHOLD 16384u
#endif
#ifndef PPF_FAST_EXP_BITBURST_HALVINGS
#define PPF_FAST_EXP_BITBURST_HALVINGS 1000000u
#endif
#ifndef PPF_FAST_LOG_BITBURST_THRESHOLD
#define PPF_FAST_LOG_BITBURST_THRESHOLD 75000u
#endif
#ifndef PPF_FAST_LOG_BITBURST_ROOTS
#define PPF_FAST_LOG_BITBURST_ROOTS 1000000u
#endif
#ifndef PPF_FAST_LOG_TABLE_BITS
#define PPF_FAST_LOG_TABLE_BITS 8u
#endif
#ifndef PPF_FAST_LOG_TABLE_SLOTS
#define PPF_FAST_LOG_TABLE_SLOTS 16u
#endif
#ifndef PPF_FAST_LOG_TABLE_ENABLED
#define PPF_FAST_LOG_TABLE_ENABLED 0
#endif
#ifndef PPF_FAST_LOG_TABLE_ROOTS_FIXED
#define PPF_FAST_LOG_TABLE_ROOTS_FIXED 0u
#endif
#ifndef PPF_FAST_LOG_NEWTON_THRESHOLD
#define PPF_FAST_LOG_NEWTON_THRESHOLD UINT64_MAX
#endif
#ifndef PPF_FAST_ATANH_RECT_WIDTH
#define PPF_FAST_ATANH_RECT_WIDTH 28u
#endif
#ifndef PPF_FAST_ATANH_METHOD
/* 0=measured automatic policy, 1=Horner, 2=block4, 3=block8,
   4=block16, 5=midpoint-radius recurrence.  Nonzero values are primarily
   used by the target-machine tuner and ablation tests. */
#define PPF_FAST_ATANH_METHOD 0u
#endif
#ifndef PPF_FAST_LOG_ROOT_OVERRIDE
#define PPF_FAST_LOG_ROOT_OVERRIDE 0u
#endif
#ifndef PPF_FAST_EXP_HALVING_OVERRIDE
#define PPF_FAST_EXP_HALVING_OVERRIDE 0u
#endif

/* Fixed-point certified interval.  Both endpoints represent integers scaled
   by 2^-scale.  The fast transcendental kernels keep one common scale for an
   entire evaluation, which removes the repeated ppf_t allocation,
   normalization, and general floating division from the inner series loops. */
typedef struct {
    mpz_t lo;
    mpz_t hi;
} ppf_fx_iv_t;

/* Midpoint-radius enclosure at the same fixed scale.  The represented value
   lies in [mid-rad, mid+rad] * 2^-scale.  For positive transcendental
   reconstruction this cuts each full interval product from two large
   multiplications to one, while all discarded arithmetic is absorbed into
   the exact integer radius. */
typedef struct {
    mpz_t mid;
    mpz_t rad;
} ppf_fx_ball_t;

static pthread_once_t g_fx_ln10_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_fx_ln10_mutex = PTHREAD_MUTEX_INITIALIZER;
static mpz_t g_fx_ln10_lo;
static mpz_t g_fx_ln10_hi;
static ppf_prec_t g_fx_ln10_scale;
static ppf_prec_t g_fx_ln10_work;
static int g_fx_ln10_valid;

static void fx_ln10_cache_init(void) {
    mpz_inits(g_fx_ln10_lo, g_fx_ln10_hi, NULL);
    g_fx_ln10_scale = 0;
    g_fx_ln10_work = 0;
    g_fx_ln10_valid = 0;
}

static void fx_init(ppf_fx_iv_t *x) { mpz_inits(x->lo, x->hi, NULL); }
static void fx_clear(ppf_fx_iv_t *x) { mpz_clears(x->lo, x->hi, NULL); }
static void fx_set(ppf_fx_iv_t *out, const ppf_fx_iv_t *x) {
    mpz_set(out->lo, x->lo); mpz_set(out->hi, x->hi);
}

static void ball_init(ppf_fx_ball_t *x) { mpz_inits(x->mid, x->rad, NULL); }
static void ball_clear(ppf_fx_ball_t *x) { mpz_clears(x->mid, x->rad, NULL); }
static void ball_set(ppf_fx_ball_t *out, const ppf_fx_ball_t *x) {
    mpz_set(out->mid, x->mid); mpz_set(out->rad, x->rad);
}

static void ball_from_interval(ppf_fx_ball_t *out, const ppf_fx_iv_t *x,
                               mpz_ptr scratch) {
    mpz_add(scratch, x->lo, x->hi);
    mpz_fdiv_q_2exp(out->mid, scratch, 1UL);
    mpz_sub(out->rad, out->mid, x->lo);
    mpz_sub(scratch, x->hi, out->mid);
    if (mpz_cmp(scratch, out->rad) > 0) mpz_set(out->rad, scratch);
    if (mpz_sgn(out->rad) < 0) mpz_set_ui(out->rad, 0UL);
}

static void ball_to_interval(ppf_fx_iv_t *out, const ppf_fx_ball_t *x) {
    mpz_sub(out->lo, x->mid, x->rad);
    mpz_add(out->hi, x->mid, x->rad);
}

static int ball_set_si(ppf_fx_ball_t *out, long value, ppf_prec_t scale) {
    if (scale > (ppf_prec_t)ULONG_MAX) return -1;
    mpz_set_si(out->mid, value);
    mpz_mul_2exp(out->mid, out->mid, (mp_bitcnt_t)scale);
    mpz_set_ui(out->rad, 0UL);
    return 0;
}

static void ball_add(ppf_fx_ball_t *out, const ppf_fx_ball_t *a,
                     const ppf_fx_ball_t *b) {
    mpz_add(out->mid, a->mid, b->mid);
    mpz_add(out->rad, a->rad, b->rad);
}

static void ball_mul_ui(ppf_fx_ball_t *out, const ppf_fx_ball_t *a,
                        unsigned long value) {
    mpz_mul_ui(out->mid, a->mid, value);
    mpz_mul_ui(out->rad, a->rad, value);
}

/* Positive multiplication with exact radius propagation.  p0..p3 are
   reusable scratch integers.  One ulp covers the fixed-point truncation. */
static int ball_mul_positive(ppf_fx_ball_t *out,
                             const ppf_fx_ball_t *a,
                             const ppf_fx_ball_t *b,
                             ppf_prec_t scale,
                             mpz_ptr p0, mpz_ptr p1,
                             mpz_ptr p2, mpz_ptr p3) {
    if (mpz_sgn(a->mid) < 0 || mpz_sgn(b->mid) < 0 ||
        mpz_sgn(a->rad) < 0 || mpz_sgn(b->rad) < 0 ||
        scale > (ppf_prec_t)ULONG_MAX)
        return -1;
    mpz_mul(p0, a->mid, b->mid);
    mpz_fdiv_q_2exp(p1, p0, (mp_bitcnt_t)scale); /* new midpoint */

    mpz_mul(p0, a->mid, b->rad);
    mpz_mul(p2, b->mid, a->rad);
    mpz_add(p0, p0, p2);
    mpz_mul(p2, a->rad, b->rad);
    mpz_add(p0, p0, p2);
    mpz_cdiv_q_2exp(p3, p0, (mp_bitcnt_t)scale);
    mpz_add_ui(p3, p3, 1UL);

    mpz_set(out->mid, p1);
    mpz_set(out->rad, p3);
    return 0;
}

/* Positive ball square.  This is the dominant exponential
   reconstruction operation; exploiting a==b avoids computing mid*rad
   twice. */
static int ball_square_positive(ppf_fx_ball_t *out,
                                const ppf_fx_ball_t *a,
                                ppf_prec_t scale,
                                mpz_ptr p0, mpz_ptr p1,
                                mpz_ptr p2, mpz_ptr p3) {
    if (mpz_sgn(a->mid) < 0 || mpz_sgn(a->rad) < 0 ||
        scale > (ppf_prec_t)ULONG_MAX)
        return -1;
    mpz_mul(p0, a->mid, a->mid);
    mpz_fdiv_q_2exp(p1, p0, (mp_bitcnt_t)scale);

    if (mpz_sgn(a->rad) == 0) {
        mpz_set_ui(p3, 1UL);
    } else {
        mpz_mul(p0, a->mid, a->rad);
        mpz_mul_2exp(p0, p0, 1UL);
        mpz_mul(p2, a->rad, a->rad);
        mpz_add(p0, p0, p2);
        mpz_cdiv_q_2exp(p3, p0, (mp_bitcnt_t)scale);
        mpz_add_ui(p3, p3, 1UL);
    }
    mpz_set(out->mid, p1);
    mpz_set(out->rad, p3);
    return 0;
}

static int ball_mul_general(ppf_fx_ball_t *out,
                            const ppf_fx_ball_t *a,
                            const ppf_fx_ball_t *b,
                            ppf_prec_t scale,
                            mpz_ptr p0, mpz_ptr p1,
                            mpz_ptr p2, mpz_ptr p3) {
    if (mpz_sgn(a->rad) < 0 || mpz_sgn(b->rad) < 0 ||
        scale > (ppf_prec_t)ULONG_MAX)
        return -1;
    mpz_mul(p0, a->mid, b->mid);
    mpz_fdiv_q_2exp(p1, p0, (mp_bitcnt_t)scale);

    mpz_abs(p0, a->mid);
    mpz_mul(p0, p0, b->rad);
    mpz_abs(p2, b->mid);
    mpz_mul(p2, p2, a->rad);
    mpz_add(p0, p0, p2);
    mpz_mul(p2, a->rad, b->rad);
    mpz_add(p0, p0, p2);
    mpz_cdiv_q_2exp(p3, p0, (mp_bitcnt_t)scale);
    mpz_add_ui(p3, p3, 1UL);

    mpz_set(out->mid, p1);
    mpz_set(out->rad, p3);
    return 0;
}

static int fx_mul_general_ball(ppf_fx_iv_t *out,
                               const ppf_fx_iv_t *a,
                               const ppf_fx_iv_t *b,
                               ppf_prec_t scale,
                               mpz_ptr p0, mpz_ptr p1,
                               mpz_ptr p2, mpz_ptr p3) {
    ppf_fx_ball_t ba, bb, result;
    int rc = -1;
    ball_init(&ba); ball_init(&bb); ball_init(&result);
    ball_from_interval(&ba, a, p0);
    ball_from_interval(&bb, b, p0);
    if (ball_mul_general(&result, &ba, &bb, scale, p0, p1, p2, p3) != 0)
        goto done;
    ball_to_interval(out, &result);
    rc = 0;
done:
    ball_clear(&ba); ball_clear(&bb); ball_clear(&result);
    return rc;
}

static int ball_div_ui_positive(ppf_fx_ball_t *out,
                                const ppf_fx_ball_t *a,
                                unsigned long divisor,
                                mpz_ptr scratch) {
    if (divisor == 0UL || mpz_sgn(a->mid) < 0 || mpz_sgn(a->rad) < 0)
        return -1;
    mpz_fdiv_q_ui(scratch, a->mid, divisor);
    mpz_cdiv_q_ui(out->rad, a->rad, divisor);
    mpz_add_ui(out->rad, out->rad, 1UL);
    mpz_set(out->mid, scratch);
    return 0;
}

static int ball_div_z_positive(ppf_fx_ball_t *out,
                               const ppf_fx_ball_t *a,
                               mpz_srcptr divisor,
                               mpz_ptr scratch) {
    if (mpz_sgn(divisor) <= 0 || mpz_sgn(a->mid) < 0 ||
        mpz_sgn(a->rad) < 0)
        return -1;
    mpz_fdiv_q(scratch, a->mid, divisor);
    mpz_cdiv_q(out->rad, a->rad, divisor);
    mpz_add_ui(out->rad, out->rad, 1UL);
    mpz_set(out->mid, scratch);
    return 0;
}

static int ball_scale2(ppf_fx_ball_t *out, const ppf_fx_ball_t *x,
                       long shift) {
    if (shift >= 0) {
        mpz_mul_2exp(out->mid, x->mid, (mp_bitcnt_t)(unsigned long)shift);
        mpz_mul_2exp(out->rad, x->rad, (mp_bitcnt_t)(unsigned long)shift);
    } else {
        unsigned long s = (unsigned long)(-(shift + 1L)) + 1UL;
        mpz_fdiv_q_2exp(out->mid, x->mid, (mp_bitcnt_t)s);
        mpz_cdiv_q_2exp(out->rad, x->rad, (mp_bitcnt_t)s);
        mpz_add_ui(out->rad, out->rad, 1UL);
    }
    return 0;
}

static int fx_shift_distance(ppf_exp_t exponent, ppf_prec_t scale,
                             int *left, mp_bitcnt_t *distance) {
#if defined(__SIZEOF_INT128__)
    ppf_fast_i128_t d = (ppf_fast_i128_t)exponent + (ppf_fast_i128_t)scale;
    if (d >= 0) {
        if ((ppf_fast_u128_t)d > (ppf_fast_u128_t)ULONG_MAX) return -1;
        *left = 1; *distance = (mp_bitcnt_t)d;
    } else {
        ppf_fast_u128_t u = (ppf_fast_u128_t)(-d);
        if (u > (ppf_fast_u128_t)ULONG_MAX) return -1;
        *left = 0; *distance = (mp_bitcnt_t)u;
    }
    return 0;
#else
    if (scale > (ppf_prec_t)INT64_MAX) return -1;
    if (exponent >= -(ppf_exp_t)scale) {
        uint64_t d = (uint64_t)exponent + (uint64_t)scale;
        if (d > (uint64_t)ULONG_MAX) return -1;
        *left = 1; *distance = (mp_bitcnt_t)d;
    } else {
        uint64_t d = (uint64_t)(-(exponent + (ppf_exp_t)scale));
        if (d > (uint64_t)ULONG_MAX) return -1;
        *left = 0; *distance = (mp_bitcnt_t)d;
    }
    return 0;
#endif
}

/* Outward conversion of z*2^exponent to the common fixed-point scale. */
static int fx_set_dyadic(ppf_fx_iv_t *out, mpz_srcptr z,
                         ppf_exp_t exponent, ppf_prec_t scale) {
    int left;
    mp_bitcnt_t distance;
    if (fx_shift_distance(exponent, scale, &left, &distance) != 0) return -1;
    if (left) {
        mpz_mul_2exp(out->lo, z, distance);
        mpz_set(out->hi, out->lo);
    } else {
        mpz_fdiv_q_2exp(out->lo, z, distance);
        mpz_cdiv_q_2exp(out->hi, z, distance);
    }
    return 0;
}

/* Read-only exact dyadic view of a regular ppf_t.  GMP accepts a negative
   limb count for a negative mpz value, so this avoids the full mpz_import
   performed by ppf_get_z_2exp on every transcendental call.  The view must
   never be modified or cleared. */
static int fx_ppf_mpz_view(mpz_t view, ppf_exp_t *exponent,
                           const ppf_t *x) {
    mp_size_t size;
    if (!view || !exponent || !x || !ppf_regular_p(x) ||
        x->used_limbs == 0u || x->used_limbs > (size_t)LONG_MAX ||
        ppf_exp_sub_prec(x->exponent, x->precision, exponent) != 0)
        return -1;
    size = (mp_size_t)x->used_limbs;
    if (x->sign < 0) size = -size;
    mpz_roinit_n(view, x->limbs, size);
    return 0;
}

static int fx_set_ppf(ppf_fx_iv_t *out, const ppf_t *x,
                      ppf_prec_t scale) {
    mpz_t view;
    ppf_exp_t e;
    if (fx_ppf_mpz_view(view, &e, x) != 0) return -1;
    return fx_set_dyadic(out, view, e, scale);
}

static int fx_set_constant(ppf_fx_iv_t *out, ppf_constant_id_t id,
                           ppf_prec_t scale) {
    mpz_t lo, hi;
    ppf_exp_t e;
    ppf_fx_iv_t a, b;
    ppf_prec_t request;
    int rc = -1;
    mpz_inits(lo, hi, NULL);
    fx_init(&a); fx_init(&b);
    if (scale > UINT64_MAX - 24u) goto done;
    request = scale + 24u;
    if (ppf_get_constant_interval(id, lo, hi, &e, request, 1UL) != 0)
        goto done;
    if (fx_set_dyadic(&a, lo, e, scale) != 0 ||
        fx_set_dyadic(&b, hi, e, scale) != 0)
        goto done;
    mpz_set(out->lo, a.lo);   /* floor of certified lower endpoint */
    mpz_set(out->hi, b.hi);   /* ceil of certified upper endpoint */
    rc = 0;
done:
    fx_clear(&a); fx_clear(&b);
    mpz_clears(lo, hi, NULL);
    return rc;
}

static int fx_set_si(ppf_fx_iv_t *out, long value, ppf_prec_t scale) {
    if (scale > (ppf_prec_t)ULONG_MAX) return -1;
    mpz_set_si(out->lo, value);
    mpz_mul_2exp(out->lo, out->lo, (mp_bitcnt_t)scale);
    mpz_set(out->hi, out->lo);
    return 0;
}

static void fx_add(ppf_fx_iv_t *out, const ppf_fx_iv_t *a,
                   const ppf_fx_iv_t *b) {
    mpz_add(out->lo, a->lo, b->lo);
    mpz_add(out->hi, a->hi, b->hi);
}

static void fx_sub(ppf_fx_iv_t *out, const ppf_fx_iv_t *a,
                   const ppf_fx_iv_t *b) {
    mpz_sub(out->lo, a->lo, b->hi);
    mpz_sub(out->hi, a->hi, b->lo);
}

static void fx_neg(ppf_fx_iv_t *out, const ppf_fx_iv_t *x) {
    mpz_neg(out->lo, x->hi);
    mpz_neg(out->hi, x->lo);
}

static int fx_scale2(ppf_fx_iv_t *out, const ppf_fx_iv_t *x, long shift) {
    if (shift >= 0) {
        mpz_mul_2exp(out->lo, x->lo, (mp_bitcnt_t)(unsigned long)shift);
        mpz_mul_2exp(out->hi, x->hi, (mp_bitcnt_t)(unsigned long)shift);
    } else {
        unsigned long s = (unsigned long)(-(shift + 1L)) + 1UL;
        mpz_fdiv_q_2exp(out->lo, x->lo, (mp_bitcnt_t)s);
        mpz_cdiv_q_2exp(out->hi, x->hi, (mp_bitcnt_t)s);
    }
    return 0;
}

static int fx_mul_positive_narrow(ppf_fx_iv_t *out,
                                  const ppf_fx_iv_t *a,
                                  const ppf_fx_iv_t *b,
                                  ppf_prec_t scale,
                                  mpz_ptr p0, mpz_ptr p1,
                                  mpz_ptr p2, mpz_ptr p3) {
    size_t endpoint_bits, width_bits;
    if (mpz_sgn(a->lo) < 0 || mpz_sgn(b->lo) < 0 ||
        scale > (ppf_prec_t)ULONG_MAX) return -1;

    mpz_sub(p1, a->hi, a->lo);
    mpz_sub(p2, b->hi, b->lo);
    if (mpz_sgn(p1) < 0 || mpz_sgn(p2) < 0) return -1;

    mpz_mul(p0, a->lo, b->lo);
    mpz_fdiv_q_2exp(out->lo, p0, (mp_bitcnt_t)scale);

    if (mpz_sgn(p1) == 0 && mpz_sgn(p2) == 0) {
        mpz_cdiv_q_2exp(out->hi, p0, (mp_bitcnt_t)scale);
        return 0;
    }

    endpoint_bits = mpz_sizeinbase(a->hi, 2);
    if (mpz_sizeinbase(b->hi, 2) > endpoint_bits)
        endpoint_bits = mpz_sizeinbase(b->hi, 2);
    width_bits = mpz_sgn(p1) ? mpz_sizeinbase(p1, 2) : 0u;
    if (mpz_sgn(p2) && mpz_sizeinbase(p2, 2) > width_bits)
        width_bits = mpz_sizeinbase(p2, 2);

    /* If the enclosure has become genuinely wide, two tuned full products
       are cheaper than four similar-size products. */
    if (width_bits * 3u > endpoint_bits * 2u) {
        mpz_mul(p3, a->hi, b->hi);
        mpz_cdiv_q_2exp(out->hi, p3, (mp_bitcnt_t)scale);
        return 0;
    }

    mpz_set(p3, p0);
    if (mpz_sgn(p2)) {
        mpz_addmul(p3, a->lo, p2);
    }
    if (mpz_sgn(p1)) {
        mpz_addmul(p3, b->lo, p1);
    }
    if (mpz_sgn(p1) && mpz_sgn(p2)) {
        mpz_addmul(p3, p1, p2);
    }
    mpz_cdiv_q_2exp(out->hi, p3, (mp_bitcnt_t)scale);
    return 0;
}

static int fx_div_positive(ppf_fx_iv_t *out, const ppf_fx_iv_t *a,
                           const ppf_fx_iv_t *b, ppf_prec_t scale,
                           mpz_ptr scratch) {
    if (mpz_sgn(a->lo) < 0 || mpz_sgn(b->lo) <= 0 ||
        scale > (ppf_prec_t)ULONG_MAX) return -1;
    mpz_mul_2exp(scratch, a->lo, (mp_bitcnt_t)scale);
    mpz_fdiv_q(out->lo, scratch, b->hi);
    mpz_mul_2exp(scratch, a->hi, (mp_bitcnt_t)scale);
    mpz_cdiv_q(out->hi, scratch, b->lo);
    return 0;
}

/* Divide a possibly signed interval by a strictly positive interval. */
static int fx_div_signed_positive(ppf_fx_iv_t *out,
                                  const ppf_fx_iv_t *a,
                                  const ppf_fx_iv_t *b,
                                  ppf_prec_t scale,
                                  mpz_ptr scratch) {
    if (mpz_sgn(b->lo) <= 0 || scale > (ppf_prec_t)ULONG_MAX) return -1;
    if (mpz_sgn(a->lo) >= 0)
        return fx_div_positive(out, a, b, scale, scratch);
    if (mpz_sgn(a->hi) <= 0) {
        mpz_mul_2exp(scratch, a->lo, (mp_bitcnt_t)scale);
        mpz_fdiv_q(out->lo, scratch, b->lo);
        mpz_mul_2exp(scratch, a->hi, (mp_bitcnt_t)scale);
        mpz_cdiv_q(out->hi, scratch, b->hi);
        return 0;
    }
    /* The interval crosses zero.  The smallest positive denominator gives
       both extrema. */
    mpz_mul_2exp(scratch, a->lo, (mp_bitcnt_t)scale);
    mpz_fdiv_q(out->lo, scratch, b->lo);
    mpz_mul_2exp(scratch, a->hi, (mp_bitcnt_t)scale);
    mpz_cdiv_q(out->hi, scratch, b->lo);
    return 0;
}

static int fx_div_ui_positive(ppf_fx_iv_t *out, const ppf_fx_iv_t *a,
                              unsigned long d) {
    if (d == 0 || mpz_sgn(a->lo) < 0) return -1;
    mpz_fdiv_q_ui(out->lo, a->lo, d);
    mpz_cdiv_q_ui(out->hi, a->hi, d);
    return 0;
}

static int fx_sqrt_positive_candidate(ppf_fx_iv_t *out,
                                      const ppf_fx_iv_t *a,
                                      ppf_prec_t scale,
                                      mpz_ptr radicand,
                                      mpz_ptr aux) {
    int exact;
    if (mpz_sgn(a->lo) < 0 || scale > (ppf_prec_t)ULONG_MAX) return -1;
    mpz_sub(aux, a->hi, a->lo);
    if (mpz_sgn(aux) < 0) return -1;
    mpz_mul_2exp(aux, aux, (mp_bitcnt_t)scale);
    mpz_mul_2exp(radicand, a->lo, (mp_bitcnt_t)scale);
    mpz_sqrtrem(out->lo, out->hi, radicand);
    if (mpz_sgn(aux) == 0 && mpz_sgn(out->hi) == 0) {
        mpz_set(out->hi, out->lo);
        return 0;
    }
    mpz_add(aux, aux, out->hi);
    mpz_mul_2exp(radicand, out->lo, 1UL);
    mpz_add_ui(radicand, radicand, 1UL);
    if (mpz_cmp(aux, radicand) <= 0) {
        mpz_add_ui(out->hi, out->lo, 1UL);
        return 0;
    }
    mpz_mul_2exp(radicand, a->hi, (mp_bitcnt_t)scale);
    exact = mpz_root(out->hi, radicand, 2UL);
    if (!exact) mpz_add_ui(out->hi, out->hi, 1UL);
    return 0;
}

static int fx_reciprocal_positive(ppf_fx_iv_t *out, const ppf_fx_iv_t *a,
                                  ppf_prec_t scale, mpz_ptr numerator) {
    if (mpz_sgn(a->lo) <= 0 || scale > (ppf_prec_t)(ULONG_MAX / 2UL)) return -1;
    mpz_set_ui(numerator, 1UL);
    mpz_mul_2exp(numerator, numerator, (mp_bitcnt_t)(2u * scale));
    mpz_fdiv_q(out->lo, numerator, a->hi);
    mpz_cdiv_q(out->hi, numerator, a->lo);
    return 0;
}

static int fx_mul_si(ppf_fx_iv_t *out, const ppf_fx_iv_t *a, long n) {
    if (n >= 0) {
        mpz_mul_si(out->lo, a->lo, n);
        mpz_mul_si(out->hi, a->hi, n);
    } else {
        mpz_mul_si(out->lo, a->hi, n);
        mpz_mul_si(out->hi, a->lo, n);
    }
    return 0;
}

/* Construct the exact dyadic value 1+x in a temporary ppf_t.  This is used
   by log1p so cancellation is resolved before the transcendental kernel,
   rather than by adding two rounded interval endpoints. */
static int fx_exact_one_plus(ppf_t *out, const ppf_t *x,
                             ppf_context_t *ctx) {
    mpz_t z, numerator;
    ppf_exp_t e, common;
    mp_bitcnt_t shift;
    ppf_prec_t precision;
    int rc = -1;
    mpz_inits(z, numerator, NULL);
    if (!ppf_regular_p(x) || ppf_get_exact_dyadic(z, &e, x) != 0)
        goto done;
    common = e < 0 ? e : 0;
    if (e >= 0) {
        if ((uint64_t)e > (uint64_t)ULONG_MAX) goto done;
        mpz_mul_2exp(numerator, z, (mp_bitcnt_t)e);
        mpz_add_ui(numerator, numerator, 1UL);
    } else {
        if (e == INT64_MIN) goto done;
        if ((uint64_t)(-e) > (uint64_t)ULONG_MAX) goto done;
        shift = (mp_bitcnt_t)(-e);
        mpz_set_ui(numerator, 1UL);
        mpz_mul_2exp(numerator, numerator, shift);
        mpz_add(numerator, numerator, z);
    }
    if (mpz_sgn(numerator) <= 0) goto done;
    /* Remove powers of two so the temporary precision is minimal. */
    shift = mpz_scan1(numerator, 0UL);
    if (shift != (mp_bitcnt_t)-1 && shift > 0) {
        mpz_fdiv_q_2exp(numerator, numerator, shift);
        if (common > PPF_EMAX_MAX - (ppf_exp_t)shift) goto done;
        common += (ppf_exp_t)shift;
    }
    precision = (ppf_prec_t)mpz_sizeinbase(numerator, 2);
    if (precision < 2u) precision = 2u;
    if (ppf_init2_ctx(out, precision, ctx) != 0) goto done;
    if (ppf_set_z_2exp(out, numerator, common, PPF_RNDN, ctx) ==
        PPF_TERNARY_ERROR) {
        ppf_clear(out);
        memset(out, 0, sizeof(*out));
        goto done;
    }
    rc = 0;
done:
    mpz_clears(z, numerator, NULL);
    return rc;
}

static unsigned long fx_ceil_sqrt(ppf_prec_t p) {
    long double v = sqrtl((long double)p);
    unsigned long s;
    if (v > (long double)ULONG_MAX) return ULONG_MAX;
    s = (unsigned long)v;
    while ((long double)s * (long double)s < (long double)p) ++s;
    return s;
}

static unsigned long fx_log_root_count(ppf_prec_t work) {
#ifdef PPF_FAST_LOG_ROOTS_FIXED
    (void)work;
    return (unsigned long)PPF_FAST_LOG_ROOTS_FIXED;
#else
    /* Measured balance for the fixed-point rectangular atanh evaluator.
       More roots reduce the series length, but beyond these bands the integer
       square-root traffic costs more than the saved product/division work. */
    if (work < 2000u) return 12UL;
    if (work < 7500u) return 26UL;
    if (work < 15000u) return 32UL;
    if (work < 30000u) return 36UL;
    if (work < 75000u) return 40UL;
    return 48UL;
#endif
}

static unsigned long fx_log_bitburst_root_count(ppf_prec_t work) {
    if ((unsigned long)PPF_FAST_LOG_BITBURST_ROOTS != 1000000UL)
        return (unsigned long)PPF_FAST_LOG_BITBURST_ROOTS;
    return work < 90000u ? 6UL : 16UL;
}

static int fx_log_dispatch(ppf_fx_iv_t *out, const ppf_t *x,
                           ppf_prec_t work, ppf_prec_t scale);

static int fx_atanh_series_horner(ppf_fx_iv_t *out,
                                  const ppf_fx_iv_t *z,
                                  ppf_prec_t work, ppf_prec_t scale);
static int fx_atanh_series_block4(ppf_fx_iv_t *out,
                                  const ppf_fx_iv_t *z,
                                  ppf_prec_t work, ppf_prec_t scale);
static int fx_atanh_series_block8(ppf_fx_iv_t *out,
                                  const ppf_fx_iv_t *z,
                                  ppf_prec_t work, ppf_prec_t scale);
static int fx_atanh_series_block16(ppf_fx_iv_t *out,
                                   const ppf_fx_iv_t *z,
                                   ppf_prec_t work, ppf_prec_t scale);
static int fx_atanh_series_ball(ppf_fx_iv_t *out,
                                const ppf_fx_iv_t *z,
                                ppf_prec_t work, ppf_prec_t scale);
static int fx_atanh_series_bs_endpoints(ppf_fx_iv_t *out,
                                         const ppf_fx_iv_t *z,
                                         ppf_prec_t work,
                                         ppf_prec_t scale);

static unsigned long fx_exp_halving_count(ppf_prec_t work) {
#ifdef PPF_FAST_EXP_HALVINGS_FIXED
    (void)work;
    return (unsigned long)PPF_FAST_EXP_HALVINGS_FIXED;
#else
    /* Blocked Taylor evaluation shifts the optimum toward fewer
       reconstruction squarings. */
    return fx_ceil_sqrt(work / 5u + 1u);
#endif
}


static int fx_atanh_series_dispatch(ppf_fx_iv_t *out,
                                     const ppf_fx_iv_t *z,
                                     ppf_prec_t work,
                                     ppf_prec_t scale) {
#if PPF_FAST_ATANH_METHOD == 1
    return fx_atanh_series_horner(out, z, work, scale);
#elif PPF_FAST_ATANH_METHOD == 2
    return fx_atanh_series_block4(out, z, work, scale);
#elif PPF_FAST_ATANH_METHOD == 3
    return fx_atanh_series_block8(out, z, work, scale);
#elif PPF_FAST_ATANH_METHOD == 4
    return fx_atanh_series_block16(out, z, work, scale);
#elif PPF_FAST_ATANH_METHOD == 5
    return fx_atanh_series_ball(out, z, work, scale);
#elif PPF_FAST_ATANH_METHOD == 6
    return fx_atanh_series_bs_endpoints(out, z, work, scale);
#else
    if (work < 2000u && fx_atanh_series_block8(out, z, work, scale) == 0)
        return 0;
    if (fx_atanh_series_horner(out, z, work, scale) == 0) return 0;
    if (work >= 12000u &&
        fx_atanh_series_block16(out, z, work, scale) == 0)
        return 0;
    if (fx_atanh_series_block8(out, z, work, scale) == 0) return 0;
    if (fx_atanh_series_block4(out, z, work, scale) == 0) return 0;
    return fx_atanh_series_ball(out, z, work, scale);
#endif
}

/* Repeated square roots for a value known mathematically to lie in [1,2].
   On that interval sqrt is 1/2-Lipschitz.  A midpoint-radius input with
   radius R fixed-point units therefore maps to a radius at most ceil(R/2)+1,
   where the extra unit covers flooring the midpoint square root.  This uses
   one integer square root per reduction step rather than two endpoint roots. */
static int fx_repeated_sqrt_unit_ball(ppf_fx_iv_t *out,
                                      const ppf_fx_iv_t *input,
                                      unsigned long roots,
                                      ppf_prec_t scale) {
    ppf_fx_ball_t value, next;
    mpz_t scratch, one;
    int rc = -1;
    ball_init(&value); ball_init(&next);
    mpz_inits(scratch, one, NULL);
    if (scale > (ppf_prec_t)ULONG_MAX) goto done;
    ball_from_interval(&value, input, scratch);
    if (mpz_sgn(value.mid) <= 0) goto done;
    for (unsigned long i = 0; i < roots; ++i) {
        mpz_mul_2exp(scratch, value.mid, (mp_bitcnt_t)scale);
        mpz_sqrt(next.mid, scratch);
        mpz_cdiv_q_2exp(next.rad, value.rad, 1UL);
        mpz_add_ui(next.rad, next.rad, 1UL);
        ball_set(&value, &next);
    }
    ball_to_interval(out, &value);
    /* The exact mathematical sequence never drops below one. */
    mpz_set_ui(one, 1UL);
    mpz_mul_2exp(one, one, (mp_bitcnt_t)scale);
    if (mpz_cmp(out->lo, one) < 0) mpz_set(out->lo, one);
    rc = 0;
done:
    ball_clear(&value); ball_clear(&next);
    mpz_clears(scratch, one, NULL);
    return rc;
}


/* Choose the number of Taylor terms from a rigorous power-of-two upper
   bound.  If 0 <= x < 2^-gain, the exponential tail after N terms is below
   2*x^N and the reduced atanh/log tail (after the final factor two) is below
   4*x^N. */
static int fx_series_term_count(mpz_srcptr upper, ppf_prec_t scale,
                                unsigned long stop_bits,
                                unsigned tail_extra,
                                unsigned long *terms) {
    size_t bits;
    ppf_prec_t gain;
#if defined(__SIZEOF_INT128__)
    ppf_fast_u128_t target, n;
#endif
    if (!terms || mpz_sgn(upper) < 0) return -1;
    if (mpz_sgn(upper) == 0) { *terms = 1UL; return 0; }
    bits = mpz_sizeinbase(upper, 2);
    if ((ppf_prec_t)bits >= scale) return -1;
    gain = scale - (ppf_prec_t)bits;
    if (gain == 0u) return -1;
#if defined(__SIZEOF_INT128__)
    target = (ppf_fast_u128_t)scale + (ppf_fast_u128_t)tail_extra;
    if (target > (ppf_fast_u128_t)stop_bits) target -= stop_bits;
    else target = 1u;
    n = (target + gain - 1u) / gain;
    if (n < 2u) n = 2u;
    if (n > (ppf_fast_u128_t)ULONG_MAX) return -1;
    *terms = (unsigned long)n;
#else
    if (scale > (ppf_prec_t)ULONG_MAX) return -1;
    {
        unsigned long target = (unsigned long)scale;
        if (target > ULONG_MAX - tail_extra) return -1;
        target += tail_extra;
        target = target > stop_bits ? target - stop_bits : 1UL;
        *terms = (target + (unsigned long)gain - 1UL) /
                 (unsigned long)gain;
        if (*terms < 2UL) *terms = 2UL;
    }
#endif
    return 0;
}

static unsigned fx_rectangular_block_width(unsigned long terms) {
    long double r = sqrtl((long double)terms);
    unsigned m = (unsigned)r;
    while ((unsigned long)m * (unsigned long)m < terms) ++m;
    if (m < 4u) m = 4u;
    if (m > 32u) m = 32u;
    return m;
}

static void fx_set_tail_power_bound(mpz_ptr tail, ppf_prec_t scale,
                                    unsigned tail_extra,
                                    ppf_prec_t gain,
                                    unsigned long terms) {
#if defined(__SIZEOF_INT128__)
    ppf_fast_i128_t e = (ppf_fast_i128_t)scale + tail_extra -
                        (ppf_fast_i128_t)gain * terms;
    if (e >= 0 && e <= (ppf_fast_i128_t)ULONG_MAX) {
        mpz_set_ui(tail, 1UL);
        mpz_mul_2exp(tail, tail, (mp_bitcnt_t)(unsigned long)e);
    } else {
        /* The positive tail is nonzero but below one fixed-point ulp. */
        mpz_set_ui(tail, 1UL);
    }
#else
    long double e = (long double)scale + tail_extra -
                    (long double)gain * terms;
    if (e >= 0.0L && e <= (long double)ULONG_MAX) {
        mpz_set_ui(tail, 1UL);
        mpz_mul_2exp(tail, tail, (mp_bitcnt_t)(unsigned long)e);
    } else mpz_set_ui(tail, 1UL);
#endif
}

/* Backward rectangular Horner evaluation of 2*atanh(z). */
static int fx_atanh_series_horner(ppf_fx_iv_t *out, const ppf_fx_iv_t *z,
                                  ppf_prec_t work, ppf_prec_t scale) {
    ppf_fx_ball_t zb,q,h,temp,quotient,scaled,result;
    ppf_fx_ball_t *powers=NULL,*blocks=NULL,*ratios=NULL;
    mpz_t p0,p1,p2,p3,scratch,upper,tail;
    unsigned long terms=0,block_count;
    unsigned m=0;
    int rc=-1;
    ball_init(&zb);ball_init(&q);ball_init(&h);ball_init(&temp);
    ball_init(&quotient);ball_init(&scaled);ball_init(&result);
    mpz_inits(p0,p1,p2,p3,scratch,upper,tail,NULL);
    ball_from_interval(&zb,z,scratch);
    if(mpz_sgn(zb.mid)<0 ||
       ball_square_positive(&q,&zb,scale,p0,p1,p2,p3)!=0) goto done;
    mpz_add(upper,q.mid,q.rad);
    if(fx_series_term_count(upper,scale,32u,2u,&terms)!=0) goto done;
    m=fx_rectangular_block_width(terms);
    block_count=(terms+m-1UL)/m;
    powers=(ppf_fx_ball_t*)ppf_owned_buffer_calloc_internal(
        ppf_result_transaction_context_internal(), (size_t)m+1u, sizeof(*powers));
    blocks=(ppf_fx_ball_t*)ppf_owned_buffer_calloc_internal(
        ppf_result_transaction_context_internal(), block_count, sizeof(*blocks));
    ratios=block_count>1UL?(ppf_fx_ball_t*)
        ppf_owned_buffer_calloc_internal(ppf_result_transaction_context_internal(),
                                         block_count-1UL,sizeof(*ratios)):NULL;
    if(!powers||!blocks||(block_count>1UL&&!ratios))goto done;
    for(unsigned j=0;j<=m;++j)ball_init(&powers[j]);
    for(unsigned long b=0;b<block_count;++b)ball_init(&blocks[b]);
    for(unsigned long b=0;b+1UL<block_count;++b)ball_init(&ratios[b]);
    if(ball_set_si(&powers[0],1L,scale)!=0)goto done;
    ball_set(&powers[1],&q);
    for(unsigned j=2;j<=m;++j)
        if(ball_mul_positive(&powers[j],&powers[j-1],&q,scale,p0,p1,p2,p3)!=0)
            goto done;
    for(unsigned long b=0;b<block_count;++b){
        unsigned long k=b*(unsigned long)m;
        unsigned len=(unsigned)((terms-k)<m?(terms-k):m);
        unsigned long numerator=2UL*k+1UL;
        if(ball_set_si(&blocks[b],0L,scale)!=0)goto done;
        for(unsigned j=0;j<len;++j){
            unsigned long denominator=2UL*(k+j)+1UL;
            if(numerator>ULONG_MAX/1UL)goto done;
            ball_mul_ui(&scaled,&powers[j],numerator);
            if(ball_div_ui_positive(&quotient,&scaled,denominator,scratch)!=0)goto done;
            ball_add(&blocks[b],&blocks[b],&quotient);
        }
        if(b+1UL<block_count){
            unsigned long denominator=2UL*(k+m)+1UL;
            if(len!=m)goto done;
            ball_mul_ui(&scaled,&powers[m],numerator);
            if(ball_div_ui_positive(&ratios[b],&scaled,denominator,scratch)!=0)goto done;
        }
    }
    ball_set(&h,&blocks[block_count-1UL]);
    for(unsigned long b=block_count-1UL;b-- >0UL;){
        if(ball_mul_positive(&temp,&ratios[b],&h,scale,p0,p1,p2,p3)!=0)goto done;
        ball_add(&h,&blocks[b],&temp);
    }
    if(ball_mul_positive(&result,&zb,&h,scale,p0,p1,p2,p3)!=0)goto done;
    mpz_sub(out->lo,result.mid,result.rad);
    mpz_add(out->hi,result.mid,result.rad);
    mpz_mul_2exp(out->lo,out->lo,1UL);
    mpz_mul_2exp(out->hi,out->hi,1UL);
    {
        size_t bits=mpz_sizeinbase(upper,2);
        ppf_prec_t gain=scale-(ppf_prec_t)bits;
        fx_set_tail_power_bound(tail,scale,2u,gain,terms);
    }
    mpz_add(out->hi,out->hi,tail);
    rc=0;
done:
    if(powers){for(unsigned j=0;j<=m;++j)ball_clear(&powers[j]);ppf_owned_buffer_free_internal(powers);}
    if(blocks){for(unsigned long b=0;b<block_count;++b)ball_clear(&blocks[b]);ppf_owned_buffer_free_internal(blocks);}
    if(ratios){for(unsigned long b=0;b+1UL<block_count;++b)ball_clear(&ratios[b]);ppf_owned_buffer_free_internal(ratios);}
    ball_clear(&zb);ball_clear(&q);ball_clear(&h);ball_clear(&temp);
    ball_clear(&quotient);ball_clear(&scaled);ball_clear(&result);
    mpz_clears(p0,p1,p2,p3,scratch,upper,tail,NULL);
    (void)work;
    return rc;
}

/* Four-term rectangular block for the reduced exponential series.
   Reusing t^0..t^4 cuts four term-by-term full products to two block
   products; coefficient work is exact small-integer division. */
static int fx_exp_series_block4(ppf_fx_iv_t *out, const ppf_fx_iv_t *t,
                                ppf_prec_t work, ppf_prec_t scale,
                                unsigned long stop_bits) {
    ppf_fx_ball_t tb, term, sum, block, contribution, quotient, update;
    ppf_fx_ball_t powers[5];
    mpz_t p0, p1, p2, p3, scratch, upper, tail;
    unsigned long k = 1UL, max_steps;
    int rc = -1;
    ball_init(&tb); ball_init(&term); ball_init(&sum); ball_init(&block);
    ball_init(&contribution); ball_init(&quotient); ball_init(&update);
    for (unsigned i = 0; i < 5; ++i) ball_init(&powers[i]);
    mpz_inits(p0, p1, p2, p3, scratch, upper, tail, NULL);
    ball_from_interval(&tb, t, scratch);
    if (mpz_sgn(tb.mid) < 0 || ball_set_si(&powers[0], 1L, scale) != 0 ||
        ball_set_si(&sum, 1L, scale) != 0)
        goto done;
    ball_set(&powers[1], &tb);
    for (unsigned j = 2; j <= 4; ++j) {
        if (ball_mul_positive(&powers[j], &powers[j - 1], &tb, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
    }
    ball_set(&term, &tb);
    max_steps = fx_ceil_sqrt(work) * 16UL + 1024UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    while (k <= max_steps) {
        unsigned long denominator = 1UL;
        if (ball_set_si(&block, 0L, scale) != 0) goto done;
        for (unsigned j = 0; j < 4; ++j) {
            if (j != 0) {
                unsigned long factor = k + j;
                if (denominator > ULONG_MAX / factor) goto done;
                denominator *= factor;
            }
            if (ball_div_ui_positive(&quotient, &powers[j], denominator,
                                     scratch) != 0)
                goto done;
            ball_add(&block, &block, &quotient);
        }
        if (ball_mul_positive(&contribution, &term, &block, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_add(&sum, &sum, &contribution);
        {
            unsigned long factor = k + 4UL;
            if (denominator > ULONG_MAX / factor) goto done;
            denominator *= factor;
        }
        if (ball_div_ui_positive(&quotient, &powers[4], denominator,
                                 scratch) != 0 ||
            ball_mul_positive(&update, &term, &quotient, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_set(&term, &update);
        k += 4UL;
        mpz_add(upper, term.mid, term.rad);
        if (mpz_sgn(upper) == 0 || mpz_sizeinbase(upper, 2) <= stop_bits) {
            mpz_sub(out->lo, sum.mid, sum.rad);
            mpz_add(out->hi, sum.mid, sum.rad);
            mpz_mul_2exp(tail, upper, 1UL);
            mpz_add(out->hi, out->hi, tail);
            rc = 0;
            goto done;
        }
    }
done:
    ball_clear(&tb); ball_clear(&term); ball_clear(&sum); ball_clear(&block);
    ball_clear(&contribution); ball_clear(&quotient); ball_clear(&update);
    for (unsigned i = 0; i < 5; ++i) ball_clear(&powers[i]);
    mpz_clears(p0, p1, p2, p3, scratch, upper, tail, NULL);
    return rc;
}

/* Four-term rectangular block for 2*atanh(z). */
static int fx_atanh_series_block4(ppf_fx_iv_t *out, const ppf_fx_iv_t *z,
                                  ppf_prec_t work, ppf_prec_t scale) {
    ppf_fx_ball_t zb, q, term, sum, block, contribution, quotient, scaled;
    ppf_fx_ball_t update, powers[5];
    mpz_t p0, p1, p2, p3, scratch, upper, tail;
    unsigned long k = 0UL, max_steps;
    int rc = -1;
    ball_init(&zb); ball_init(&q); ball_init(&term); ball_init(&sum);
    ball_init(&block); ball_init(&contribution); ball_init(&quotient);
    ball_init(&scaled); ball_init(&update);
    for (unsigned i = 0; i < 5; ++i) ball_init(&powers[i]);
    mpz_inits(p0, p1, p2, p3, scratch, upper, tail, NULL);
    ball_from_interval(&zb, z, scratch);
    if (mpz_sgn(zb.mid) < 0 || ball_set_si(&powers[0], 1L, scale) != 0 ||
        ball_set_si(&sum, 0L, scale) != 0 ||
        ball_square_positive(&q, &zb, scale, p0, p1, p2, p3) != 0)
        goto done;
    ball_set(&powers[1], &q);
    for (unsigned j = 2; j <= 4; ++j) {
        if (ball_mul_positive(&powers[j], &powers[j - 1], &q, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
    }
    ball_set(&term, &zb);
    max_steps = fx_ceil_sqrt(work) * 16UL + 1024UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    while (k <= max_steps) {
        unsigned long numerator = 2UL * k + 1UL;
        if (ball_set_si(&block, 0L, scale) != 0) goto done;
        for (unsigned j = 0; j < 4; ++j) {
            unsigned long denominator = 2UL * (k + j) + 1UL;
            ball_mul_ui(&scaled, &powers[j], numerator);
            if (ball_div_ui_positive(&quotient, &scaled, denominator,
                                     scratch) != 0)
                goto done;
            ball_add(&block, &block, &quotient);
        }
        if (ball_mul_positive(&contribution, &term, &block, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_add(&sum, &sum, &contribution);
        ball_mul_ui(&scaled, &powers[4], numerator);
        if (ball_div_ui_positive(&quotient, &scaled,
                                 2UL * (k + 4UL) + 1UL, scratch) != 0 ||
            ball_mul_positive(&update, &term, &quotient, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_set(&term, &update);
        k += 4UL;
        mpz_add(upper, term.mid, term.rad);
        if (mpz_sgn(upper) == 0 || mpz_sizeinbase(upper, 2) <= 32u) {
            mpz_sub(out->lo, sum.mid, sum.rad);
            mpz_add(out->hi, sum.mid, sum.rad);
            mpz_mul_2exp(tail, upper, 1UL);
            mpz_add(out->hi, out->hi, tail);
            mpz_mul_2exp(out->lo, out->lo, 1UL);
            mpz_mul_2exp(out->hi, out->hi, 1UL);
            rc = 0;
            goto done;
        }
    }
done:
    ball_clear(&zb); ball_clear(&q); ball_clear(&term); ball_clear(&sum);
    ball_clear(&block); ball_clear(&contribution); ball_clear(&quotient);
    ball_clear(&scaled); ball_clear(&update);
    for (unsigned i = 0; i < 5; ++i) ball_clear(&powers[i]);
    mpz_clears(p0, p1, p2, p3, scratch, upper, tail, NULL);
    return rc;
}

/* Eight-term variants.  They are attempted first and return failure if the
   rising factorial no longer fits an unsigned long, allowing the four-term
   path to take over at very large series lengths. */
static int fx_exp_series_block8(ppf_fx_iv_t *out, const ppf_fx_iv_t *t,
                                ppf_prec_t work, ppf_prec_t scale,
                                unsigned long stop_bits) {
    ppf_fx_ball_t tb, term, sum, block, contribution, quotient, update;
    ppf_fx_ball_t powers[9];
    mpz_t p0, p1, p2, p3, scratch, upper, tail;
    unsigned long k = 1UL, max_steps;
    int rc = -1;
    ball_init(&tb); ball_init(&term); ball_init(&sum); ball_init(&block);
    ball_init(&contribution); ball_init(&quotient); ball_init(&update);
    for (unsigned i = 0; i < 9; ++i) ball_init(&powers[i]);
    mpz_inits(p0, p1, p2, p3, scratch, upper, tail, NULL);
    ball_from_interval(&tb, t, scratch);
    if (mpz_sgn(tb.mid) < 0 || ball_set_si(&powers[0], 1L, scale) != 0 ||
        ball_set_si(&sum, 1L, scale) != 0)
        goto done;
    ball_set(&powers[1], &tb);
    for (unsigned j = 2; j <= 8; ++j) {
        if (ball_mul_positive(&powers[j], &powers[j - 1], &tb, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
    }
    ball_set(&term, &tb);
    max_steps = fx_ceil_sqrt(work) * 16UL + 1024UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    while (k <= max_steps) {
        mpz_t denominator, coefficient;
        mpz_inits(denominator, coefficient, NULL);
        mpz_set_ui(denominator, 1UL);
        for (unsigned j = 1; j <= 8; ++j) {
            if (k > ULONG_MAX - (unsigned long)j) {
                mpz_clears(denominator, coefficient, NULL); goto done;
            }
            mpz_mul_ui(denominator, denominator, k + (unsigned long)j);
        }
        if (ball_set_si(&block, 0L, scale) != 0) {
            mpz_clears(denominator, coefficient, NULL); goto done;
        }
        mpz_set(coefficient, denominator);
        for (unsigned j = 0; j < 8; ++j) {
            mpz_addmul(block.mid, powers[j].mid, coefficient);
            mpz_addmul(block.rad, powers[j].rad, coefficient);
            if (j + 1u < 8u)
                mpz_divexact_ui(coefficient, coefficient,
                                k + (unsigned long)j + 1UL);
        }
        if (ball_div_z_positive(&block, &block, denominator, scratch) != 0 ||
            ball_mul_positive(&contribution, &term, &block, scale,
                              p0, p1, p2, p3) != 0) {
            mpz_clears(denominator, coefficient, NULL); goto done;
        }
        ball_add(&sum, &sum, &contribution);
        if (ball_div_z_positive(&quotient, &powers[8], denominator,
                                scratch) != 0 ||
            ball_mul_positive(&update, &term, &quotient, scale,
                              p0, p1, p2, p3) != 0) {
            mpz_clears(denominator, coefficient, NULL); goto done;
        }
        ball_set(&term, &update);
        mpz_clears(denominator, coefficient, NULL);
        k += 8UL;
        mpz_add(upper, term.mid, term.rad);
        if (mpz_sgn(upper) == 0 || mpz_sizeinbase(upper, 2) <= stop_bits) {
            mpz_sub(out->lo, sum.mid, sum.rad);
            mpz_add(out->hi, sum.mid, sum.rad);
            mpz_mul_2exp(tail, upper, 1UL);
            mpz_add(out->hi, out->hi, tail);
            rc = 0;
            goto done;
        }
    }
done:
    ball_clear(&tb); ball_clear(&term); ball_clear(&sum); ball_clear(&block);
    ball_clear(&contribution); ball_clear(&quotient); ball_clear(&update);
    for (unsigned i = 0; i < 9; ++i) ball_clear(&powers[i]);
    mpz_clears(p0, p1, p2, p3, scratch, upper, tail, NULL);
    return rc;
}

static int fx_atanh_series_block8(ppf_fx_iv_t *out, const ppf_fx_iv_t *z,
                                  ppf_prec_t work, ppf_prec_t scale) {
    ppf_fx_ball_t zb, q, term, sum, block, contribution, quotient, scaled;
    ppf_fx_ball_t update, powers[9];
    mpz_t p0, p1, p2, p3, scratch, upper, tail;
    unsigned long k = 0UL, max_steps;
    int rc = -1;
    ball_init(&zb); ball_init(&q); ball_init(&term); ball_init(&sum);
    ball_init(&block); ball_init(&contribution); ball_init(&quotient);
    ball_init(&scaled); ball_init(&update);
    for (unsigned i = 0; i < 9; ++i) ball_init(&powers[i]);
    mpz_inits(p0, p1, p2, p3, scratch, upper, tail, NULL);
    ball_from_interval(&zb, z, scratch);
    if (mpz_sgn(zb.mid) < 0 || ball_set_si(&powers[0], 1L, scale) != 0 ||
        ball_set_si(&sum, 0L, scale) != 0 ||
        ball_square_positive(&q, &zb, scale, p0, p1, p2, p3) != 0)
        goto done;
    ball_set(&powers[1], &q);
    for (unsigned j = 2; j <= 8; ++j) {
        if (ball_mul_positive(&powers[j], &powers[j - 1], &q, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
    }
    ball_set(&term, &zb);
    max_steps = fx_ceil_sqrt(work) * 16UL + 1024UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    while (k <= max_steps) {
        unsigned long numerator = 2UL * k + 1UL;
        if (ball_set_si(&block, 0L, scale) != 0) goto done;
        for (unsigned j = 0; j < 8; ++j) {
            unsigned long denominator = 2UL * (k + j) + 1UL;
            ball_mul_ui(&scaled, &powers[j], numerator);
            if (ball_div_ui_positive(&quotient, &scaled, denominator,
                                     scratch) != 0)
                goto done;
            ball_add(&block, &block, &quotient);
        }
        if (ball_mul_positive(&contribution, &term, &block, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_add(&sum, &sum, &contribution);
        ball_mul_ui(&scaled, &powers[8], numerator);
        if (ball_div_ui_positive(&quotient, &scaled,
                                 2UL * (k + 8UL) + 1UL, scratch) != 0 ||
            ball_mul_positive(&update, &term, &quotient, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_set(&term, &update);
        k += 8UL;
        mpz_add(upper, term.mid, term.rad);
        if (mpz_sgn(upper) == 0 || mpz_sizeinbase(upper, 2) <= 32u) {
            mpz_sub(out->lo, sum.mid, sum.rad);
            mpz_add(out->hi, sum.mid, sum.rad);
            mpz_mul_2exp(tail, upper, 1UL);
            mpz_add(out->hi, out->hi, tail);
            mpz_mul_2exp(out->lo, out->lo, 1UL);
            mpz_mul_2exp(out->hi, out->hi, 1UL);
            rc = 0;
            goto done;
        }
    }
done:
    ball_clear(&zb); ball_clear(&q); ball_clear(&term); ball_clear(&sum);
    ball_clear(&block); ball_clear(&contribution); ball_clear(&quotient);
    ball_clear(&scaled); ball_clear(&update);
    for (unsigned i = 0; i < 9; ++i) ball_clear(&powers[i]);
    mpz_clears(p0, p1, p2, p3, scratch, upper, tail, NULL);
    return rc;
}


/* Sixteen-term rectangular block for larger precisions. */
static int fx_exp_series_block16(ppf_fx_iv_t *out, const ppf_fx_iv_t *t,
                                 ppf_prec_t work, ppf_prec_t scale,
                                 unsigned long stop_bits) {
    ppf_fx_ball_t tb, term, sum, block, contribution, quotient, update;
    ppf_fx_ball_t powers[17];
    mpz_t p0, p1, p2, p3, scratch, upper, tail, denominator;
    unsigned long k = 1UL, max_steps;
    int rc = -1;
    ball_init(&tb); ball_init(&term); ball_init(&sum); ball_init(&block);
    ball_init(&contribution); ball_init(&quotient); ball_init(&update);
    for (unsigned i = 0; i < 17; ++i) ball_init(&powers[i]);
    mpz_inits(p0, p1, p2, p3, scratch, upper, tail, denominator, NULL);
    ball_from_interval(&tb, t, scratch);
    if (mpz_sgn(tb.mid) < 0 || ball_set_si(&powers[0], 1L, scale) != 0 ||
        ball_set_si(&sum, 1L, scale) != 0)
        goto done;
    ball_set(&powers[1], &tb);
    for (unsigned j = 2; j <= 16; ++j) {
        if (ball_mul_positive(&powers[j], &powers[j - 1], &tb, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
    }
    ball_set(&term, &tb);
    max_steps = fx_ceil_sqrt(work) * 20UL + 2048UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    while (k <= max_steps) {
        mpz_t coefficient;
        mpz_init(coefficient);
        mpz_set_ui(denominator,1UL);
        for(unsigned j=1;j<=16;++j){
            if(k>ULONG_MAX-(unsigned long)j){mpz_clear(coefficient);goto done;}
            mpz_mul_ui(denominator,denominator,k+(unsigned long)j);
        }
        if(ball_set_si(&block,0L,scale)!=0){mpz_clear(coefficient);goto done;}
        mpz_set(coefficient,denominator);
        for(unsigned j=0;j<16;++j){
            mpz_addmul(block.mid,powers[j].mid,coefficient);
            mpz_addmul(block.rad,powers[j].rad,coefficient);
            if(j+1u<16u)
                mpz_divexact_ui(coefficient,coefficient,
                                k+(unsigned long)j+1UL);
        }
        if(ball_div_z_positive(&block,&block,denominator,scratch)!=0 ||
           ball_mul_positive(&contribution,&term,&block,scale,p0,p1,p2,p3)!=0){
            mpz_clear(coefficient);goto done;
        }
        ball_add(&sum,&sum,&contribution);
        if(ball_div_z_positive(&quotient,&powers[16],denominator,scratch)!=0 ||
           ball_mul_positive(&update,&term,&quotient,scale,p0,p1,p2,p3)!=0){
            mpz_clear(coefficient);goto done;
        }
        ball_set(&term,&update);
        mpz_clear(coefficient);
        k += 16UL;
        mpz_add(upper, term.mid, term.rad);
        if (mpz_sgn(upper) == 0 || mpz_sizeinbase(upper, 2) <= stop_bits) {
            mpz_sub(out->lo, sum.mid, sum.rad);
            mpz_add(out->hi, sum.mid, sum.rad);
            mpz_mul_2exp(tail, upper, 1UL);
            mpz_add(out->hi, out->hi, tail);
            rc = 0;
            goto done;
        }
    }
done:
    ball_clear(&tb); ball_clear(&term); ball_clear(&sum); ball_clear(&block);
    ball_clear(&contribution); ball_clear(&quotient); ball_clear(&update);
    for (unsigned i = 0; i < 17; ++i) ball_clear(&powers[i]);
    mpz_clears(p0, p1, p2, p3, scratch, upper, tail, denominator, NULL);
    return rc;
}

static int fx_atanh_series_block16(ppf_fx_iv_t *out, const ppf_fx_iv_t *z,
                                   ppf_prec_t work, ppf_prec_t scale) {
    ppf_fx_ball_t zb, q, term, sum, block, contribution, quotient, scaled;
    ppf_fx_ball_t update, powers[17];
    mpz_t p0, p1, p2, p3, scratch, upper, tail;
    unsigned long k = 0UL, max_steps;
    int rc = -1;
    ball_init(&zb); ball_init(&q); ball_init(&term); ball_init(&sum);
    ball_init(&block); ball_init(&contribution); ball_init(&quotient);
    ball_init(&scaled); ball_init(&update);
    for (unsigned i = 0; i < 17; ++i) ball_init(&powers[i]);
    mpz_inits(p0, p1, p2, p3, scratch, upper, tail, NULL);
    ball_from_interval(&zb, z, scratch);
    if (mpz_sgn(zb.mid) < 0 || ball_set_si(&powers[0], 1L, scale) != 0 ||
        ball_set_si(&sum, 0L, scale) != 0 ||
        ball_square_positive(&q, &zb, scale, p0, p1, p2, p3) != 0)
        goto done;
    ball_set(&powers[1], &q);
    for (unsigned j = 2; j <= 16; ++j) {
        if (ball_mul_positive(&powers[j], &powers[j - 1], &q, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
    }
    ball_set(&term, &zb);
    max_steps = fx_ceil_sqrt(work) * 20UL + 2048UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    while (k <= max_steps) {
        unsigned long numerator = 2UL * k + 1UL;
        if (ball_set_si(&block, 0L, scale) != 0) goto done;
        for (unsigned j = 0; j < 16; ++j) {
            unsigned long denominator = 2UL * (k + j) + 1UL;
            ball_mul_ui(&scaled, &powers[j], numerator);
            if (ball_div_ui_positive(&quotient, &scaled, denominator,
                                     scratch) != 0)
                goto done;
            ball_add(&block, &block, &quotient);
        }
        if (ball_mul_positive(&contribution, &term, &block, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_add(&sum, &sum, &contribution);
        ball_mul_ui(&scaled, &powers[16], numerator);
        if (ball_div_ui_positive(&quotient, &scaled,
                                 2UL * (k + 16UL) + 1UL, scratch) != 0 ||
            ball_mul_positive(&update, &term, &quotient, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_set(&term, &update);
        k += 16UL;
        mpz_add(upper, term.mid, term.rad);
        if (mpz_sgn(upper) == 0 || mpz_sizeinbase(upper, 2) <= 32u) {
            mpz_sub(out->lo, sum.mid, sum.rad);
            mpz_add(out->hi, sum.mid, sum.rad);
            mpz_mul_2exp(tail, upper, 1UL);
            mpz_add(out->hi, out->hi, tail);
            mpz_mul_2exp(out->lo, out->lo, 1UL);
            mpz_mul_2exp(out->hi, out->hi, 1UL);
            rc = 0;
            goto done;
        }
    }
done:
    ball_clear(&zb); ball_clear(&q); ball_clear(&term); ball_clear(&sum);
    ball_clear(&block); ball_clear(&contribution); ball_clear(&quotient);
    ball_clear(&scaled); ball_clear(&update);
    for (unsigned i = 0; i < 17; ++i) ball_clear(&powers[i]);
    mpz_clears(p0, p1, p2, p3, scratch, upper, tail, NULL);
    return rc;
}

/* Midpoint-radius atanh series.  The square-root reduced z is
   nonnegative and small, and the remaining tail is bounded by twice the next
   contribution. */
static int fx_atanh_series_ball(ppf_fx_iv_t *out,
                                const ppf_fx_iv_t *z,
                                ppf_prec_t work,
                                ppf_prec_t scale) {
    ppf_fx_ball_t zb, z2, power, contrib, sum, temp, next;
    mpz_t p0, p1, p2, p3, scratch, upper, tail;
    unsigned long max_steps;
    int rc = -1;
    ball_init(&zb); ball_init(&z2); ball_init(&power);
    ball_init(&contrib); ball_init(&sum); ball_init(&temp); ball_init(&next);
    mpz_inits(p0, p1, p2, p3, scratch, upper, tail, NULL);
    ball_from_interval(&zb, z, scratch);
    if (mpz_sgn(zb.mid) < 0 ||
        ball_mul_positive(&z2, &zb, &zb, scale, p0, p1, p2, p3) != 0)
        goto done;
    ball_set(&power, &zb);
    ball_set(&sum, &zb);
    max_steps = fx_ceil_sqrt(work) * 16UL + 1024UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    for (unsigned long k = 1UL; k <= max_steps; ++k) {
        if (ball_mul_positive(&temp, &power, &z2, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_set(&power, &temp);
        if (ball_div_ui_positive(&contrib, &power, 2UL * k + 1UL,
                                 scratch) != 0)
            goto done;
        ball_add(&sum, &sum, &contrib);
        mpz_add(upper, contrib.mid, contrib.rad);
        if (mpz_sizeinbase(upper, 2) <= 36u) {
            if (ball_mul_positive(&temp, &power, &z2, scale,
                                  p0, p1, p2, p3) != 0 ||
                ball_div_ui_positive(&next, &temp,
                                     2UL * (k + 1UL) + 1UL,
                                     scratch) != 0)
                goto done;
            mpz_add(upper, next.mid, next.rad);
            if (mpz_sgn(upper) == 0 || mpz_sizeinbase(upper, 2) <= 32u) {
                mpz_sub(out->lo, sum.mid, sum.rad);
                mpz_add(out->hi, sum.mid, sum.rad);
                mpz_mul_2exp(tail, upper, 1UL);
                mpz_add(out->hi, out->hi, tail);
                mpz_mul_2exp(out->lo, out->lo, 1UL);
                mpz_mul_2exp(out->hi, out->hi, 1UL);
                rc = 0;
                goto done;
            }
        }
    }
done:
    ball_clear(&zb); ball_clear(&z2); ball_clear(&power);
    ball_clear(&contrib); ball_clear(&sum); ball_clear(&temp); ball_clear(&next);
    mpz_clears(p0, p1, p2, p3, scratch, upper, tail, NULL);
    return rc;
}


/* Exact exponential binary-splitting builders. */
/* Exact binary-splitting packet shared by the atanh and exponential
   fixed-point evaluators.  Keeping powers of two as a separate shift avoids
   multiplying integers containing long runs of zero limbs. */
typedef struct {
    mpz_t p;
    mpz_t q;
    mpz_t t;
    ppf_prec_t shift;
} ppf_fx_exp_bs_t;

static void fx_exp_bs_init(ppf_fx_exp_bs_t *x) {
    mpz_inits(x->p, x->q, x->t, NULL);
    x->shift = 0u;
}
static void fx_exp_bs_clear(ppf_fx_exp_bs_t *x) {
    mpz_clears(x->p, x->q, x->t, NULL);
    x->shift = 0u;
}
static int fx_exp_bs_combine(ppf_fx_exp_bs_t *out,
                             const ppf_fx_exp_bs_t *left,
                             const ppf_fx_exp_bs_t *right,
                             mpz_ptr u, mpz_ptr v);

static int fx_exp_interval(ppf_fx_iv_t *out, const ppf_fx_iv_t *x,
                           ppf_prec_t work, ppf_prec_t scale);

static int PPF_FAST_UNUSED fx_atanh_bs_build(ppf_fx_exp_bs_t *out, mpz_srcptr a2,
                             ppf_prec_t denominator_bits,
                             unsigned long begin, unsigned long end) {
    if (begin >= end || denominator_bits > (ppf_prec_t)ULONG_MAX)
        return -1;
    if (end - begin <= 16UL) {
        unsigned long oddp = 2UL * begin + 1UL;
        unsigned long oddq = oddp + 2UL;
        mpz_mul_ui(out->p, a2, oddp);
        mpz_set_ui(out->q, oddq);
        mpz_set_ui(out->t, oddq);
        mpz_mul_2exp(out->t, out->t,
                     (mp_bitcnt_t)(unsigned long)denominator_bits);
        out->shift = denominator_bits;
        {
            mpz_t leafp;
            mpz_init(leafp);
            for (unsigned long k = begin + 1UL; k < end; ++k) {
                unsigned long q = 2UL * k + 3UL;
                unsigned long p_odd = 2UL * k + 1UL;
                if (out->shift > UINT64_MAX - denominator_bits) {
                    mpz_clear(leafp);
                    return -1;
                }
                mpz_mul_ui(leafp, a2, p_odd);
                mpz_add(out->t, out->t, out->p);
                mpz_mul_ui(out->t, out->t, q);
                mpz_mul_2exp(out->t, out->t,
                             (mp_bitcnt_t)(unsigned long)denominator_bits);
                mpz_mul(out->p, out->p, leafp);
                mpz_mul_ui(out->q, out->q, q);
                out->shift += denominator_bits;
            }
            mpz_clear(leafp);
        }
        return 0;
    }
    {
        unsigned long mid = begin + (end - begin) / 2UL;
        ppf_fx_exp_bs_t left, right;
        mpz_t u,v;
        int rc=-1;
        fx_exp_bs_init(&left);fx_exp_bs_init(&right);mpz_inits(u,v,NULL);
        if(fx_atanh_bs_build(&left,a2,denominator_bits,begin,mid)!=0||
           fx_atanh_bs_build(&right,a2,denominator_bits,mid,end)!=0)goto done;
        if(left.shift>UINT64_MAX-right.shift)goto done;
        mpz_mul(u,left.t,right.q);
        mpz_mul_2exp(u,u,(mp_bitcnt_t)(unsigned long)right.shift);
        mpz_mul(v,left.p,right.t);
        mpz_add(out->t,u,v);
        mpz_mul(out->p,left.p,right.p);
        mpz_mul(out->q,left.q,right.q);
        out->shift=left.shift+right.shift;
        rc=0;
done:
        mpz_clears(u,v,NULL);fx_exp_bs_clear(&left);fx_exp_bs_clear(&right);
        return rc;
    }
}


#if 1
/* Allocation-stable atanh binary splitting.  Leaf packets are built once,
   then reduced level by level through the same exact packet composition used
   by the exponential evaluator. */
static int fx_atanh_bs_build_iterative(ppf_fx_exp_bs_t *out, mpz_srcptr a2,
                                       ppf_prec_t denominator_bits,
                                       unsigned long begin,
                                       unsigned long end) {
    enum { LEAF_SPAN = 16 };
    ppf_fx_exp_bs_t *level0 = NULL, *level1 = NULL, *src, *dst, *swap;
    mpz_t u, v, leafp;
    unsigned long term_count, count, cap1, active;
    int scratch_initialized = 0, rc = -1;
    if (begin >= end || denominator_bits > (ppf_prec_t)ULONG_MAX)
        return -1;
    term_count = end - begin;
    count = (term_count + LEAF_SPAN - 1UL) / LEAF_SPAN;
    cap1 = (count + 1UL) / 2UL;
    if (count > SIZE_MAX / sizeof(*level0) ||
        cap1 > SIZE_MAX / sizeof(*level1))
        return -1;
    level0 = (ppf_fx_exp_bs_t *)ppf_owned_buffer_calloc_internal(
        ppf_result_transaction_context_internal(), (size_t)count, sizeof(*level0));
    level1 = (ppf_fx_exp_bs_t *)ppf_owned_buffer_calloc_internal(
        ppf_result_transaction_context_internal(), (size_t)cap1, sizeof(*level1));
    if (!level0 || !level1) goto done;
    for (unsigned long i = 0; i < count; ++i) fx_exp_bs_init(&level0[i]);
    for (unsigned long i = 0; i < cap1; ++i) fx_exp_bs_init(&level1[i]);
    mpz_inits(u, v, leafp, NULL);
    scratch_initialized = 1;
    for (unsigned long i = 0; i < count; ++i) {
        unsigned long first = begin + i * LEAF_SPAN;
        unsigned long last = first + LEAF_SPAN;
        unsigned long oddp, oddq;
        if (last > end) last = end;
        if (first > (ULONG_MAX - 3UL) / 2UL) goto done;
        oddp = 2UL * first + 1UL;
        oddq = oddp + 2UL;
        mpz_mul_ui(level0[i].p, a2, oddp);
        mpz_set_ui(level0[i].q, oddq);
        mpz_set_ui(level0[i].t, oddq);
        mpz_mul_2exp(level0[i].t, level0[i].t,
                     (mp_bitcnt_t)(unsigned long)denominator_bits);
        level0[i].shift = denominator_bits;
        for (unsigned long k = first + 1UL; k < last; ++k) {
            unsigned long p_odd, q;
            if (k > (ULONG_MAX - 3UL) / 2UL ||
                level0[i].shift > UINT64_MAX - denominator_bits)
                goto done;
            p_odd = 2UL * k + 1UL;
            q = p_odd + 2UL;
            mpz_mul_ui(leafp, a2, p_odd);
            mpz_add(level0[i].t, level0[i].t, level0[i].p);
            mpz_mul_ui(level0[i].t, level0[i].t, q);
            mpz_mul_2exp(level0[i].t, level0[i].t,
                         (mp_bitcnt_t)(unsigned long)denominator_bits);
            mpz_mul(level0[i].p, level0[i].p, leafp);
            mpz_mul_ui(level0[i].q, level0[i].q, q);
            level0[i].shift += denominator_bits;
        }
    }
    src = level0;
    dst = level1;
    active = count;
    while (active > 1UL) {
        unsigned long next_count = (active + 1UL) / 2UL;
        for (unsigned long j = 0; j < active / 2UL; ++j) {
            if (fx_exp_bs_combine(&dst[j], &src[2UL*j],
                                  &src[2UL*j + 1UL], u, v) != 0)
                goto done;
        }
        if (active & 1UL) {
            unsigned long j = next_count - 1UL;
            const ppf_fx_exp_bs_t *last = &src[active - 1UL];
            mpz_set(dst[j].p, last->p);
            mpz_set(dst[j].q, last->q);
            mpz_set(dst[j].t, last->t);
            dst[j].shift = last->shift;
        }
        swap = src;
        src = dst;
        dst = swap;
        active = next_count;
    }
    mpz_set(out->p, src[0].p);
    mpz_set(out->q, src[0].q);
    mpz_set(out->t, src[0].t);
    out->shift = src[0].shift;
    rc = 0;
done:
    if (scratch_initialized) mpz_clears(u, v, leafp, NULL);
    if (level0) {
        for (unsigned long i = 0; i < count; ++i) fx_exp_bs_clear(&level0[i]);
        ppf_owned_buffer_free_internal(level0);
    }
    if (level1) {
        for (unsigned long i = 0; i < cap1; ++i) fx_exp_bs_clear(&level1[i]);
        ppf_owned_buffer_free_internal(level1);
    }
    return rc;
}

#endif


static int fx_shifted_rational_to_fixed(ppf_fx_iv_t *out,
                                        mpz_srcptr numerator,
                                        mpz_srcptr denominator,
                                        ppf_prec_t shift,
                                        ppf_prec_t scale) {
    mpz_t q,r,temp,r2;
    int rc=-1;
    mpz_inits(q,r,temp,r2,NULL);
    if(mpz_sgn(numerator)<0||mpz_sgn(denominator)<=0)goto done;
    if(scale>=shift){
        ppf_prec_t d=scale-shift;
        if(d>(ppf_prec_t)ULONG_MAX)goto done;
        mpz_mul_2exp(temp,numerator,(mp_bitcnt_t)(unsigned long)d);
        mpz_fdiv_qr(q,r,temp,denominator);
        mpz_set(out->lo,q);mpz_set(out->hi,q);
        if(mpz_sgn(r)!=0)mpz_add_ui(out->hi,out->hi,1UL);
    }else{
        ppf_prec_t d=shift-scale;
        if(d>(ppf_prec_t)ULONG_MAX)goto done;
        mpz_fdiv_qr(q,r,numerator,denominator);
        mpz_fdiv_q_2exp(out->lo,q,(mp_bitcnt_t)(unsigned long)d);
        mpz_set(out->hi,out->lo);
        mpz_fdiv_r_2exp(r2,q,(mp_bitcnt_t)(unsigned long)d);
        if(mpz_sgn(r2)!=0||mpz_sgn(r)!=0)mpz_add_ui(out->hi,out->hi,1UL);
    }
    rc=0;
done:
    mpz_clears(q,r,temp,r2,NULL);return rc;
}

static int fx_atanh_dyadic_chunk(ppf_fx_iv_t *out, mpz_srcptr input_a,
                                 ppf_prec_t denominator_bits,
                                 ppf_prec_t scale) {
    ppf_fx_exp_bs_t bs;
    mpz_t a,a2,numerator,tail;
    size_t bits;
    ppf_prec_t gain,total_shift;
    unsigned long terms;
    int rc=-1;
    fx_exp_bs_init(&bs);mpz_inits(a,a2,numerator,tail,NULL);
    if(mpz_sgn(input_a)<=0||denominator_bits>(ppf_prec_t)ULONG_MAX)goto done;
    mpz_set(a,input_a);
    {
        mp_bitcnt_t zeros=mpz_scan1(a,0UL);
        if(zeros!=(mp_bitcnt_t)-1 && zeros>0){
            if((ppf_prec_t)zeros>=denominator_bits)goto done;
            mpz_fdiv_q_2exp(a,a,zeros);
            denominator_bits-=(ppf_prec_t)zeros;
        }
    }
    bits=mpz_sizeinbase(a,2);
    if((ppf_prec_t)bits>=denominator_bits)goto done;
    gain=denominator_bits-(ppf_prec_t)bits;
#if defined(__SIZEOF_INT128__)
    {
        ppf_fast_u128_t target=(ppf_fast_u128_t)scale+24u;
        ppf_fast_u128_t nn=(target+2u*gain-1u)/(2u*gain);
        if(nn<1u)nn=1u;
        if(nn>(ppf_fast_u128_t)ULONG_MAX)goto done;
        terms=(unsigned long)nn;
    }
#else
    if(scale>(ppf_prec_t)ULONG_MAX-24u)goto done;
    terms=((unsigned long)scale+24UL+2UL*(unsigned long)gain-1UL)/
          (2UL*(unsigned long)gain);
    if(terms<1UL)terms=1UL;
#endif
    mpz_mul(a2,a,a);
    if(fx_atanh_bs_build_iterative(&bs,a2,2u*denominator_bits,0UL,terms)!=0)goto done;
    mpz_mul(numerator,bs.t,a);
    if(bs.shift>UINT64_MAX-denominator_bits)goto done;
    total_shift=bs.shift+denominator_bits;
    if(fx_shifted_rational_to_fixed(out,numerator,bs.q,total_shift,scale)!=0)
        goto done;
#if defined(__SIZEOF_INT128__)
    {
        ppf_fast_i128_t e=(ppf_fast_i128_t)scale+1-
            (ppf_fast_i128_t)gain*(2u*(ppf_fast_u128_t)terms+1u);
        if(e>=0&&e<=(ppf_fast_i128_t)ULONG_MAX){
            mpz_set_ui(tail,1UL);mpz_mul_2exp(tail,tail,(mp_bitcnt_t)(unsigned long)e);
        }else mpz_set_ui(tail,1UL);
    }
#else
    mpz_set_ui(tail,1UL);
#endif
    mpz_add(out->hi,out->hi,tail);
    rc=0;
done:
    fx_exp_bs_clear(&bs);mpz_clears(a,a2,numerator,tail,NULL);return rc;
}

static int PPF_FAST_UNUSED fx_atanh_series_bs_endpoints(ppf_fx_iv_t *out,
                                         const ppf_fx_iv_t *z,
                                         ppf_prec_t work,
                                         ppf_prec_t scale) {
    ppf_fx_iv_t lo, hi;
    int rc = -1;
    (void)work;
    fx_init(&lo); fx_init(&hi);
    if (mpz_sgn(z->lo) < 0 ||
        fx_atanh_dyadic_chunk(&lo, z->lo, scale, scale) != 0 ||
        fx_atanh_dyadic_chunk(&hi, z->hi, scale, scale) != 0)
        goto done;
    mpz_mul_2exp(out->lo, lo.lo, 1UL);
    mpz_mul_2exp(out->hi, hi.hi, 1UL);
    rc = 0;
done:
    fx_clear(&lo); fx_clear(&hi);
    return rc;
}

static int fx_atanh_bitburst(ppf_fx_iv_t *out,const ppf_fx_iv_t *input,
                             ppf_prec_t work,ppf_prec_t scale){
    ppf_fx_iv_t residual,aiv,chunk,sum,num,prod,den,temp,one;
    mpz_t prefix,a,p0,p1,p2,p3,scratch,upper_corr,onez,qhi;
    ppf_prec_t boundary=8u,shift;
    int rc=-1;
    fx_init(&residual);fx_init(&aiv);fx_init(&chunk);fx_init(&sum);
    fx_init(&num);fx_init(&prod);fx_init(&den);fx_init(&temp);fx_init(&one);
    mpz_inits(prefix,a,p0,p1,p2,p3,scratch,upper_corr,onez,qhi,NULL);
    if(scale>(ppf_prec_t)ULONG_MAX||fx_set_si(&sum,0L,scale)!=0||
       fx_set_si(&one,1L,scale)!=0)goto done;
    fx_set(&residual,input);
    while(boundary<scale){
        if(boundary>scale/2u)boundary=scale;
        shift=scale-boundary;
        if(shift>(ppf_prec_t)ULONG_MAX)goto done;
        mpz_fdiv_q_2exp(prefix,residual.lo,(mp_bitcnt_t)(unsigned long)shift);
        mpz_set(a,prefix);
        if(mpz_sgn(a)!=0){
            if(fx_set_dyadic(&aiv,a,-(ppf_exp_t)boundary,scale)!=0)goto done;
#ifdef PPF_FAST_LOG_BITBURST_CHUNK_HORNER
            if(fx_atanh_series_horner(&chunk,&aiv,work,scale)!=0)goto done;
#else
            if(fx_atanh_dyadic_chunk(&chunk,a,boundary,scale)!=0)goto done;
#endif
            fx_add(&sum,&sum,&chunk);
            fx_sub(&num,&residual,&aiv);
            if(fx_mul_positive_narrow(&prod,&aiv,&residual,scale,p0,p1,p2,p3)!=0)
                goto done;
            fx_sub(&den,&one,&prod);
            if(fx_div_positive(&temp,&num,&den,scale,scratch)!=0)goto done;
            fx_set(&residual,&temp);
        }
        if(boundary==scale)break;
        boundary=boundary>scale/2u?scale:boundary*2u;
    }
    /* Complete the remaining positive residual with
       r <= atanh(r) <= r/(1-r^2). */
    mpz_add(sum.lo,sum.lo,residual.lo);
    mpz_mul(qhi,residual.hi,residual.hi);
    mpz_cdiv_q_2exp(qhi,qhi,(mp_bitcnt_t)(unsigned long)scale);
    mpz_set_ui(onez,1UL);mpz_mul_2exp(onez,onez,(mp_bitcnt_t)(unsigned long)scale);
    if(mpz_cmp(qhi,onez)>=0)goto done;
    mpz_sub(scratch,onez,qhi);
    mpz_mul(upper_corr,residual.hi,onez);
    mpz_cdiv_q(upper_corr,upper_corr,scratch);
    mpz_add(sum.hi,sum.hi,upper_corr);
    mpz_mul_2exp(out->lo,sum.lo,1UL);
    mpz_mul_2exp(out->hi,sum.hi,1UL);
    rc=0;
done:
    fx_clear(&residual);fx_clear(&aiv);fx_clear(&chunk);fx_clear(&sum);
    fx_clear(&num);fx_clear(&prod);fx_clear(&den);fx_clear(&temp);fx_clear(&one);
    mpz_clears(prefix,a,p0,p1,p2,p3,scratch,upper_corr,onez,qhi,NULL);
    (void)work;return rc;
}


static int fx_rescale_interval(ppf_fx_iv_t *out, const ppf_fx_iv_t *x,
                               ppf_prec_t old_scale,
                               ppf_prec_t new_scale);

typedef struct {
    pthread_mutex_t mutex;
    unsigned long key;
    ppf_prec_t scale;
    int valid;
    mpz_t lo;
    mpz_t hi;
} ppf_fx_log_table_slot_t;

static ppf_fx_log_table_slot_t
    g_fx_log_table[PPF_FAST_LOG_TABLE_SLOTS];
static pthread_once_t g_fx_log_table_once = PTHREAD_ONCE_INIT;

static void fx_log_table_initialize(void) {
    for (unsigned i = 0; i < PPF_FAST_LOG_TABLE_SLOTS; ++i) {
        (void)pthread_mutex_init(&g_fx_log_table[i].mutex, NULL);
        mpz_init(g_fx_log_table[i].lo);
        mpz_init(g_fx_log_table[i].hi);
        g_fx_log_table[i].key = 0UL;
        g_fx_log_table[i].scale = 0u;
        g_fx_log_table[i].valid = 0;
    }
}

static int fx_log_table_correction_build(ppf_fx_iv_t *out,
                                         unsigned long key,
                                         ppf_prec_t work,
                                         ppf_prec_t scale) {
    ppf_fx_iv_t z;
    mpz_t numerator, denominator, scratch;
    unsigned long radix = 1UL << PPF_FAST_LOG_TABLE_BITS;
    int rc = -1;
    fx_init(&z);
    mpz_inits(numerator, denominator, scratch, NULL);
    if (key < radix / 2UL || key > radix ||
        scale > (ppf_prec_t)ULONG_MAX)
        goto done;
    if (key == radix) {
        mpz_set_ui(out->lo, 0UL); mpz_set_ui(out->hi, 0UL);
        rc = 0; goto done;
    }
    if (key == radix / 2UL) {
        rc = fx_set_constant(out, PPF_CONSTANT_LOG2, scale);
        goto done;
    }
    /* log(radix/key) = 2*atanh((radix-key)/(radix+key)). */
    mpz_set_ui(numerator, radix - key);
    mpz_mul_2exp(numerator, numerator, (mp_bitcnt_t)(unsigned long)scale);
    mpz_set_ui(denominator, radix + key);
    mpz_fdiv_q(z.lo, numerator, denominator);
    mpz_cdiv_q(z.hi, numerator, denominator);
    if (work >= 50000u) {
        if (fx_atanh_bitburst(out, &z, work, scale) != 0) goto done;
    } else if (fx_atanh_series_dispatch(out, &z, work, scale) != 0) {
        goto done;
    }
    rc = 0;
done:
    mpz_clears(numerator, denominator, scratch, NULL);
    fx_clear(&z);
    return rc;
}

static int fx_log_table_correction(ppf_fx_iv_t *out,
                                   unsigned long key,
                                   ppf_prec_t work,
                                   ppf_prec_t scale) {
    unsigned slot_index;
    ppf_fx_log_table_slot_t *slot;
    ppf_fx_iv_t built;
    ppf_prec_t build_scale;
    int rc = -1;
    if (!PPF_FAST_LOG_TABLE_ENABLED || PPF_FAST_LOG_TABLE_SLOTS == 0u)
        return -1;
    if (pthread_once(&g_fx_log_table_once, fx_log_table_initialize) != 0)
        return -1;
    slot_index = (unsigned)(key % PPF_FAST_LOG_TABLE_SLOTS);
    slot = &g_fx_log_table[slot_index];
    if (pthread_mutex_lock(&slot->mutex) != 0) return -1;
    if (slot->valid && slot->key == key && slot->scale >= scale) {
        ppf_fx_iv_t cached;
        fx_init(&cached);
        mpz_set(cached.lo, slot->lo); mpz_set(cached.hi, slot->hi);
        rc = fx_rescale_interval(out, &cached, slot->scale, scale);
        fx_clear(&cached);
        (void)pthread_mutex_unlock(&slot->mutex);
        return rc;
    }
    fx_init(&built);
    build_scale = scale;
    if (build_scale <= UINT64_MAX - 32u) build_scale += 32u;
    if (fx_log_table_correction_build(&built, key, work,
                                      build_scale) == 0) {
        slot->key = key;
        slot->scale = build_scale;
        slot->valid = 1;
        mpz_set(slot->lo, built.lo); mpz_set(slot->hi, built.hi);
        rc = fx_rescale_interval(out, &built, build_scale, scale);
    }
    fx_clear(&built);
    (void)pthread_mutex_unlock(&slot->mutex);
    return rc;
}

static unsigned long fx_log_table_root_count(ppf_prec_t work) {
#if PPF_FAST_LOG_TABLE_ROOTS_FIXED > 0
    (void)work;
    return (unsigned long)PPF_FAST_LOG_TABLE_ROOTS_FIXED;
#else
    if (work < 2000u) return 4UL;
    if (work < 7500u) return 16UL;
    if (work < 75000u) return 28UL;
    return 16UL; /* high precision enters the bit-burst branch */
#endif
}

/* Replace m in [1,2) by y=m*j/2^B in [1,1+2^(1-B)] and return
   log(2^B/j).  j is selected with a ceiling so the reduced interval stays
   on the positive atanh side. */
static int fx_log_table_reduce(ppf_fx_iv_t *reduced,
                               ppf_fx_iv_t *correction,
                               const ppf_fx_iv_t *m,
                               ppf_prec_t work,
                               ppf_prec_t scale,
                               mpz_ptr scratch) {
    mpz_t numerator, jz, product;
    unsigned long radix = 1UL << PPF_FAST_LOG_TABLE_BITS;
    unsigned long j;
    int rc = -1;
    mpz_inits(numerator, jz, product, NULL);
    if (!PPF_FAST_LOG_TABLE_ENABLED || mpz_sgn(m->lo) <= 0 ||
        scale > (ppf_prec_t)ULONG_MAX)
        goto done;
    mpz_set_ui(numerator, radix);
    mpz_mul_2exp(numerator, numerator, (mp_bitcnt_t)(unsigned long)scale);
    mpz_cdiv_q(jz, numerator, m->lo);
    if (!mpz_fits_ulong_p(jz)) goto done;
    j = mpz_get_ui(jz);
    if (j < radix / 2UL) j = radix / 2UL;
    if (j > radix) j = radix;
    mpz_mul_ui(product, m->lo, j);
    mpz_fdiv_q_2exp(reduced->lo, product, PPF_FAST_LOG_TABLE_BITS);
    mpz_mul_ui(product, m->hi, j);
    mpz_cdiv_q_2exp(reduced->hi, product, PPF_FAST_LOG_TABLE_BITS);
    if (fx_log_table_correction(correction, j, work, scale) != 0)
        goto done;
    rc = 0;
done:
    mpz_clears(numerator, jz, product, NULL);
    (void)scratch;
    return rc;
}


static void fx_point_midpoint(ppf_fx_iv_t *out, const ppf_fx_iv_t *x,
                              mpz_ptr scratch) {
    mpz_add(scratch, x->lo, x->hi);
    mpz_fdiv_q_2exp(scratch, scratch, 1UL);
    mpz_set(out->lo, scratch);
    mpz_set(out->hi, scratch);
}

static int fx_rescale_interval(ppf_fx_iv_t *out, const ppf_fx_iv_t *x,
                               ppf_prec_t old_scale,
                               ppf_prec_t new_scale) {
    if (old_scale == new_scale) { fx_set(out, x); return 0; }
    if (old_scale < new_scale) {
        ppf_prec_t d = new_scale - old_scale;
        if (d > (ppf_prec_t)ULONG_MAX) return -1;
        mpz_mul_2exp(out->lo, x->lo, (mp_bitcnt_t)(unsigned long)d);
        mpz_mul_2exp(out->hi, x->hi, (mp_bitcnt_t)(unsigned long)d);
    } else {
        ppf_prec_t d = old_scale - new_scale;
        if (d > (ppf_prec_t)ULONG_MAX) return -1;
        mpz_fdiv_q_2exp(out->lo, x->lo, (mp_bitcnt_t)(unsigned long)d);
        mpz_cdiv_q_2exp(out->hi, x->hi, (mp_bitcnt_t)(unsigned long)d);
    }
    return 0;
}

/* Precision-doubling Newton inversion of exp, followed by a rigorous residual
   enclosure.  For z=x/exp(y) and u=z-1,

       u/(1+u) <= log(1+u) <= u,

   so the final exp interval certifies log(x) without another logarithm. */
static int fx_log_newton(ppf_fx_iv_t *out, const ppf_t *x,
                         ppf_prec_t work, ppf_prec_t target_scale) {
    ppf_context_t ctx;
    ppf_t initial;
    ppf_fx_iv_t y, y2, xiv, eiv, ratio, one, delta, candidate;
    mpz_t scratch, numerator, corr_lo, corr_hi;
    ppf_prec_t q = 96u, current_scale, next_scale, target_work;
    long e2 = 0;
    long double fraction, approximation;
    int rc = -1, ctx_ok = 0, initial_ok = 0;
    memset(&ctx, 0, sizeof(ctx)); memset(&initial, 0, sizeof(initial));
    fx_init(&y); fx_init(&y2); fx_init(&xiv); fx_init(&eiv);
    fx_init(&ratio); fx_init(&one); fx_init(&delta); fx_init(&candidate);
    mpz_inits(scratch, numerator, corr_lo, corr_hi, NULL);
    if (ppf_context_init(&ctx) != 0) goto done;
    ctx_ok = 1;
    if (ppf_context_set_range(&ctx, PPF_EMIN_MIN, PPF_EMAX_MAX) != 0)
        goto done;
    if (ppf_init2_ctx(&initial, q, &ctx) != 0) goto done;
    initial_ok = 1;
    fraction = ppf_get_ld_2exp(&e2, x, PPF_RNDN, &ctx);
    if (!(fraction > 0.0L) || !isfinite(fraction)) goto done;
    approximation = logl(fraction) + (long double)e2 * logl(2.0L);
    if (!isfinite(approximation) ||
        ppf_set_ld(&initial, approximation, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        goto done;
    current_scale = q + 96u;
    if (fx_set_ppf(&y, &initial, current_scale) != 0) goto done;
    fx_point_midpoint(&y, &y, scratch);
    target_work = work + 32u;
    if (target_work < work) goto done;
    while (q < target_work) {
        ppf_prec_t next = q > target_work / 2u ? target_work : q * 2u;
        next_scale = next + 96u;
        if (next_scale < next ||
            fx_rescale_interval(&y2, &y, current_scale, next_scale) != 0)
            goto done;
        fx_point_midpoint(&y, &y2, scratch);
        if (fx_set_ppf(&xiv, x, next_scale) != 0 ||
            fx_exp_interval(&eiv, &y, next, next_scale) != 0 ||
            fx_div_positive(&ratio, &xiv, &eiv, next_scale, scratch) != 0 ||
            fx_set_si(&one, 1L, next_scale) != 0)
            goto done;
        fx_sub(&delta, &ratio, &one);
        fx_add(&candidate, &y, &delta);
        fx_point_midpoint(&y, &candidate, scratch);
        q = next;
        current_scale = next_scale;
    }
    if (fx_rescale_interval(&y2, &y, current_scale, target_scale) != 0)
        goto done;
    fx_point_midpoint(&y, &y2, scratch);
    if (fx_set_ppf(&xiv, x, target_scale) != 0 ||
        fx_exp_interval(&eiv, &y, work + 32u, target_scale) != 0 ||
        fx_div_positive(&ratio, &xiv, &eiv, target_scale, scratch) != 0 ||
        fx_set_si(&one, 1L, target_scale) != 0 ||
        mpz_sgn(ratio.lo) <= 0)
        goto done;
    mpz_sub(numerator, ratio.lo, one.lo);
    mpz_mul(numerator, numerator, one.lo);
    mpz_fdiv_q(corr_lo, numerator, ratio.lo);
    mpz_sub(corr_hi, ratio.hi, one.hi);
    mpz_add(out->lo, y.lo, corr_lo);
    mpz_add(out->hi, y.hi, corr_hi);
    rc = 0;
done:
    if (initial_ok) ppf_clear(&initial);
    if (ctx_ok) ppf_context_clear(&ctx);
    fx_clear(&y); fx_clear(&y2); fx_clear(&xiv); fx_clear(&eiv);
    fx_clear(&ratio); fx_clear(&one); fx_clear(&delta); fx_clear(&candidate);
    mpz_clears(scratch, numerator, corr_lo, corr_hi, NULL);
    return rc;
}

/* Fixed-point atanh series after square-root argument reduction. */
static int fx_log_exact(ppf_fx_iv_t *out, const ppf_t *x,
                        ppf_prec_t work, ppf_prec_t scale) {
    ppf_fx_iv_t m, root, one, num, den, z, z2, power, contrib, sum;
    ppf_fx_iv_t next_power, next_contrib, tail, core, ln2, nterm, tmp;
    ppf_fx_iv_t table_reduced, table_correction;
    mpz_t sig_view, scratch, p0, p1, p2, p3;
    ppf_exp_t exact_e, n;
    unsigned long roots, max_steps;
    int rc = -1, table_used = 0;
    fx_init(&m); fx_init(&root); fx_init(&one); fx_init(&num); fx_init(&den);
    fx_init(&z); fx_init(&z2); fx_init(&power); fx_init(&contrib); fx_init(&sum);
    fx_init(&next_power); fx_init(&next_contrib); fx_init(&tail); fx_init(&core);
    fx_init(&ln2); fx_init(&nterm); fx_init(&tmp);
    fx_init(&table_reduced); fx_init(&table_correction);
    mpz_inits(scratch, p0, p1, p2, p3, NULL);
    if (!ppf_regular_p(x) || x->sign <= 0) goto done;
    if (work >= (ppf_prec_t)PPF_FAST_LOG_NEWTON_THRESHOLD &&
        fx_log_newton(out, x, work, scale) == 0) {
        rc = 0;
        goto done;
    }
    if (fx_ppf_mpz_view(sig_view, &exact_e, x) != 0) goto done;
    n = x->exponent - 1;
    if (fx_set_dyadic(&m, sig_view, exact_e - n, scale) != 0 ||
        fx_set_si(&one, 1L, scale) != 0) goto done;

    if (fx_log_table_reduce(&table_reduced, &table_correction, &m,
                            work, scale, scratch) == 0) {
        fx_set(&m, &table_reduced);
        table_used = 1;
    }

    if (work >= (ppf_prec_t)PPF_FAST_LOG_BITBURST_THRESHOLD) {
        unsigned long burst_roots = fx_log_bitburst_root_count(work);
        const ppf_fx_iv_t *burst_arg = &m;
        if (burst_roots > 0UL) {
            if (burst_roots > 4096UL ||
                fx_repeated_sqrt_unit_ball(&root, &m, burst_roots, scale) != 0)
                goto done;
            burst_arg = &root;
        }
        fx_sub(&num, burst_arg, &one);
        fx_add(&den, burst_arg, &one);
        if (fx_div_positive(&z, &num, &den, scale, scratch) == 0 &&
            fx_atanh_bitburst(&core, &z, work, scale) == 0) {
            if (burst_roots > (unsigned long)LONG_MAX) goto done;
            if (burst_roots > 0UL)
                fx_scale2(&tmp, &core, (long)burst_roots);
            else
                fx_set(&tmp, &core);
            if (table_used) {
                fx_add(&core, &tmp, &table_correction);
                fx_set(&tmp, &core);
            }
            if (n != 0) {
                if (n < LONG_MIN || n > LONG_MAX ||
                    fx_set_constant(&ln2, PPF_CONSTANT_LOG2, scale) != 0)
                    goto done;
                fx_mul_si(&nterm, &ln2, (long)n);
                fx_add(out, &tmp, &nterm);
            } else fx_set(out, &tmp);
            rc = 0;
            goto done;
        }
    }

    roots = table_used ? fx_log_table_root_count(work)
                       : fx_log_root_count(work);
    if (roots < 3UL) roots = 3UL;
    if (roots > 4096UL) roots = 4096UL;
    if (fx_repeated_sqrt_unit_ball(&root, &m, roots, scale) != 0)
        goto done;
    fx_set(&m, &root);
    fx_sub(&num, &m, &one);
    fx_add(&den, &m, &one);
    if (fx_div_positive(&z, &num, &den, scale, scratch) != 0) goto done;
    if (fx_atanh_series_dispatch(&core, &z, work, scale) == 0) {
        if (roots > (unsigned long)LONG_MAX) goto done;
        fx_scale2(&tmp, &core, (long)roots);
        if (table_used) {
            fx_add(&core, &tmp, &table_correction);
            fx_set(&tmp, &core);
        }
        if (n != 0) {
            if (n < LONG_MIN || n > LONG_MAX ||
                fx_set_constant(&ln2, PPF_CONSTANT_LOG2, scale) != 0)
                goto done;
            fx_mul_si(&nterm, &ln2, (long)n);
            fx_add(out, &tmp, &nterm);
        } else {
            fx_set(out, &tmp);
        }
        rc = 0;
        goto done;
    }
    if (fx_mul_positive_narrow(&z2, &z, &z, scale, p0, p1, p2, p3) != 0)
        goto done;
    fx_set(&power, &z);
    fx_set(&sum, &z);
    max_steps = fx_ceil_sqrt(work) * 12UL + 512UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    for (unsigned long k = 1UL; k <= max_steps; ++k) {
        if (fx_mul_positive_narrow(&power, &power, &z2, scale, p0, p1, p2, p3) != 0 ||
            fx_div_ui_positive(&contrib, &power, 2UL * k + 1UL) != 0)
            goto done;
        fx_add(&sum, &sum, &contrib);
        /* Only construct the next-term tail once the current term is close
           to the fixed-point ulp. */
        if (mpz_sizeinbase(contrib.hi, 2) <= 8u) {
            if (fx_mul_positive_narrow(&next_power, &power, &z2, scale, p0, p1, p2, p3) != 0 ||
                fx_div_ui_positive(&next_contrib, &next_power,
                                   2UL * (k + 1UL) + 1UL) != 0)
                goto done;
            if (mpz_cmp_ui(next_contrib.hi, 1UL) <= 0) {
                mpz_mul_2exp(tail.lo, next_contrib.lo, 1UL);
                mpz_mul_2exp(tail.hi, next_contrib.hi, 1UL);
                mpz_set(core.lo, sum.lo);
                mpz_add(core.hi, sum.hi, tail.hi);
                mpz_mul_2exp(core.lo, core.lo, 1UL); /* 2*atanh */
                mpz_mul_2exp(core.hi, core.hi, 1UL);
                if (roots > (unsigned long)LONG_MAX) goto done;
                fx_scale2(&tmp, &core, (long)roots);
                if (n != 0) {
                    if (n < LONG_MIN || n > LONG_MAX ||
                        fx_set_constant(&ln2, PPF_CONSTANT_LOG2, scale) != 0)
                        goto done;
                    fx_mul_si(&nterm, &ln2, (long)n);
                    fx_add(out, &tmp, &nterm);
                } else {
                    fx_set(out, &tmp);
                }
                rc = 0;
                goto done;
            }
        }
    }
done:
    mpz_clears(scratch, p0, p1, p2, p3, NULL);
    fx_clear(&m); fx_clear(&root); fx_clear(&one); fx_clear(&num); fx_clear(&den);
    fx_clear(&z); fx_clear(&z2); fx_clear(&power); fx_clear(&contrib); fx_clear(&sum);
    fx_clear(&next_power); fx_clear(&next_contrib); fx_clear(&tail); fx_clear(&core);
    fx_clear(&ln2); fx_clear(&nterm); fx_clear(&tmp);
    fx_clear(&table_reduced); fx_clear(&table_correction);
    return rc;
}

/* Midpoint-radius AGM.  This removes the repeated endpoint products from
   the reference interval AGM.  Each iteration performs one product and,
   for the narrow balls produced here, normally one integer square root.
   The exact arithmetic-geometric mean remains between the lower enclosure
   of b_n and the upper enclosure of a_n. */
static int fx_agm_ball_interval(ppf_fx_iv_t *out,
                                const ppf_fx_iv_t *a0,
                                const ppf_fx_iv_t *b0,
                                ppf_prec_t work,
                                ppf_prec_t scale) {
    ppf_fx_ball_t a, b, next_a, product, next_b;
    ppf_fx_iv_t product_iv, sqrt_iv;
    mpz_t p0, p1, p2, p3, aux, gap;
    unsigned max_iterations = 0u;
    int rc = -1;

    ball_init(&a); ball_init(&b); ball_init(&next_a);
    ball_init(&product); ball_init(&next_b);
    fx_init(&product_iv); fx_init(&sqrt_iv);
    mpz_inits(p0, p1, p2, p3, aux, gap, NULL);

    ball_from_interval(&a, a0, p0);
    ball_from_interval(&b, b0, p0);
    if (mpz_sgn(a.mid) <= 0 || mpz_sgn(b.mid) <= 0) goto done;

    for (ppf_prec_t t = work ? work : 1u; t != 0; t >>= 1u)
        ++max_iterations;
    max_iterations += 12u;

    for (unsigned iteration = 0u; iteration < max_iterations; ++iteration) {
        ball_add(&next_a, &a, &b);
        if (ball_scale2(&next_a, &next_a, -1L) != 0 ||
            ball_mul_positive(&product, &a, &b, scale,
                              p0, p1, p2, p3) != 0)
            goto done;
        ball_to_interval(&product_iv, &product);
        if (fx_sqrt_positive_candidate(&sqrt_iv, &product_iv, scale,
                                       p0, aux) != 0)
            goto done;
        ball_from_interval(&next_b, &sqrt_iv, p0);
        ball_set(&a, &next_a);
        ball_set(&b, &next_b);

        mpz_add(p0, a.mid, a.rad);
        mpz_sub(p1, b.mid, b.rad);
        mpz_sub(gap, p0, p1);
        if (mpz_sgn(gap) < 0) goto done;
        if (scale > work + 20u) {
            ppf_prec_t allowed_bits = scale - work - 20u;
            if ((ppf_prec_t)mpz_sizeinbase(gap, 2) <= allowed_bits) {
                mpz_set(out->lo, p1);
                mpz_set(out->hi, p0);
                rc = 0;
                goto done;
            }
        }
    }

done:
    ball_clear(&a); ball_clear(&b); ball_clear(&next_a);
    ball_clear(&product); ball_clear(&next_b);
    fx_clear(&product_iv); fx_clear(&sqrt_iv);
    mpz_clears(p0, p1, p2, p3, aux, gap, NULL);
    return rc;
}

/* Fixed-point AGM logarithm.  For s=x*2^m with EXP(s)=ceil((work+2)/2),

       F = pi / (2 AGM(1,4/s))

   satisfies F*(1-4/s^2) < log(s) < F.  The power-of-two bound
   4/s^2 <= 2^(4-2*EXP(s)) avoids a separate large square and division.
   AGM is enclosed by its monotone arithmetic and geometric means. */
static int fx_log_agm(ppf_fx_iv_t *out, const ppf_t *x,
                      ppf_prec_t work, ppf_prec_t scale) {
    ppf_fx_iv_t x_fixed, reciprocal_x, b, a;
    ppf_fx_iv_t agm, two_agm, pi, f, epsilon, one, one_minus;
    ppf_fx_iv_t lower_product, log_s, ln2, correction;
    mpz_t scratch, one_z, p0, p1, p2, p3;
    ppf_exp_t target_exponent, m, epsilon_exponent;
    int rc = -1;

    fx_init(&x_fixed); fx_init(&reciprocal_x); fx_init(&b); fx_init(&a);
    fx_init(&agm); fx_init(&two_agm); fx_init(&pi); fx_init(&f);
    fx_init(&epsilon); fx_init(&one); fx_init(&one_minus);
    fx_init(&lower_product); fx_init(&log_s); fx_init(&ln2);
    fx_init(&correction);
    mpz_inits(scratch, one_z, p0, p1, p2, p3, NULL);

    if (!ppf_regular_p(x) || x->sign <= 0 ||
        scale > (ppf_prec_t)ULONG_MAX ||
        fx_set_ppf(&x_fixed, x, scale) != 0 ||
        fx_reciprocal_positive(&reciprocal_x, &x_fixed,
                               scale, scratch) != 0 ||
        fx_set_si(&a, 1L, scale) != 0)
        goto done;

    target_exponent = (ppf_exp_t)((work + 3u) / 2u);
    if (target_exponent < 4) target_exponent = 4;
    m = target_exponent - x->exponent;
    if (m < LONG_MIN + 2 || m > LONG_MAX - 2) goto done;
    /* b = 4/(x*2^m) = (1/x)*2^(2-m). */
    if (fx_scale2(&b, &reciprocal_x, 2L - (long)m) != 0)
        goto done;

    if (fx_agm_ball_interval(&agm, &a, &b, work, scale) != 0)
        goto done;
    fx_scale2(&two_agm, &agm, 1L);
    if (fx_set_constant(&pi, PPF_CONSTANT_PI, scale) != 0 ||
        fx_div_positive(&f, &pi, &two_agm, scale, scratch) != 0)
        goto done;

    epsilon_exponent = 4 - 2 * target_exponent;
    mpz_set_ui(one_z, 1UL);
    if (fx_set_dyadic(&epsilon, one_z, epsilon_exponent, scale) != 0 ||
        fx_set_si(&one, 1L, scale) != 0)
        goto done;
    fx_sub(&one_minus, &one, &epsilon);
    if (fx_mul_positive_narrow(&lower_product, &f, &one_minus,
                               scale, p0, p1, p2, p3) != 0)
        goto done;
    mpz_set(log_s.lo, lower_product.lo);
    mpz_set(log_s.hi, f.hi);

    if (m != 0) {
        if (fx_set_constant(&ln2, PPF_CONSTANT_LOG2, scale) != 0 ||
            fx_mul_si(&correction, &ln2, (long)m) != 0)
            goto done;
        fx_sub(out, &log_s, &correction);
    } else {
        fx_set(out, &log_s);
    }
    rc = 0;

done:
    mpz_clears(scratch, one_z, p0, p1, p2, p3, NULL);
    fx_clear(&x_fixed); fx_clear(&reciprocal_x); fx_clear(&b); fx_clear(&a);
    fx_clear(&agm); fx_clear(&two_agm); fx_clear(&pi); fx_clear(&f);
    fx_clear(&epsilon); fx_clear(&one); fx_clear(&one_minus);
    fx_clear(&lower_product); fx_clear(&log_s); fx_clear(&ln2);
    fx_clear(&correction);
    return rc;
}

static int fx_log_dispatch(ppf_fx_iv_t *out, const ppf_t *x,
                           ppf_prec_t work, ppf_prec_t scale) {
    if (work >= (ppf_prec_t)PPF_FAST_LOG_AGM_THRESHOLD &&
        fx_log_agm(out, x, work, scale) == 0)
        return 0;
    return fx_log_exact(out, x, work, scale);
}

static int fx_set_ln10(ppf_fx_iv_t *out, ppf_prec_t work,
                       ppf_prec_t scale, ppf_context_t *ctx) {
    ppf_fx_iv_t built;
    ppf_t ten;
    ppf_prec_t shift;
    int rc = -1;
    fx_init(&built);
    memset(&ten, 0, sizeof(ten));
    pthread_once(&g_fx_ln10_once, fx_ln10_cache_init);
    pthread_mutex_lock(&g_fx_ln10_mutex);
    if (!g_fx_ln10_valid || g_fx_ln10_scale < scale ||
        g_fx_ln10_work < work) {
        if (ppf_init2_ctx(&ten, 4u, ctx) != 0 ||
            ppf_set_ui(&ten, 10UL, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            fx_log_dispatch(&built, &ten, work, scale) != 0)
            goto done;
        mpz_set(g_fx_ln10_lo, built.lo);
        mpz_set(g_fx_ln10_hi, built.hi);
        g_fx_ln10_scale = scale;
        g_fx_ln10_work = work;
        g_fx_ln10_valid = 1;
    }
    shift = g_fx_ln10_scale - scale;
    if (shift > (ppf_prec_t)ULONG_MAX) goto done;
    if (shift == 0u) {
        mpz_set(out->lo, g_fx_ln10_lo);
        mpz_set(out->hi, g_fx_ln10_hi);
    } else {
        mpz_fdiv_q_2exp(out->lo, g_fx_ln10_lo, (mp_bitcnt_t)shift);
        mpz_cdiv_q_2exp(out->hi, g_fx_ln10_hi, (mp_bitcnt_t)shift);
    }
    rc = 0;
done:
    if (ten.limbs) ppf_clear(&ten);
    pthread_mutex_unlock(&g_fx_ln10_mutex);
    fx_clear(&built);
    return rc;
}

static int fx_exp_series_ball(ppf_fx_iv_t *out, const ppf_fx_iv_t *t,
                               ppf_prec_t work, ppf_prec_t scale,
                               unsigned long stop_bits) {
    ppf_fx_ball_t tb, term, sum, next, temp;
    mpz_t p0, p1, p2, p3, scratch, upper, tail;
    unsigned long max_steps;
    int rc = -1;
    ball_init(&tb); ball_init(&term); ball_init(&sum);
    ball_init(&next); ball_init(&temp);
    mpz_inits(p0, p1, p2, p3, scratch, upper, tail, NULL);
    ball_from_interval(&tb, t, scratch);
    if (mpz_sgn(tb.mid) < 0 || ball_set_si(&term, 1L, scale) != 0)
        goto done;
    ball_set(&sum, &term);
    max_steps = fx_ceil_sqrt(work) * 16UL + 1024UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    for (unsigned long k = 1UL; k <= max_steps; ++k) {
        if (ball_mul_positive(&temp, &term, &tb, scale,
                              p0, p1, p2, p3) != 0 ||
            ball_div_ui_positive(&term, &temp, k, scratch) != 0)
            goto done;
        ball_add(&sum, &sum, &term);
        mpz_add(upper, term.mid, term.rad);
        if (mpz_sizeinbase(upper, 2) <= stop_bits + 2UL) {
            if (ball_mul_positive(&temp, &term, &tb, scale,
                                  p0, p1, p2, p3) != 0 ||
                ball_div_ui_positive(&next, &temp, k + 1UL, scratch) != 0)
                goto done;
            mpz_add(upper, next.mid, next.rad);
            if (mpz_sgn(upper) == 0 ||
                mpz_sizeinbase(upper, 2) <= stop_bits) {
                mpz_sub(out->lo, sum.mid, sum.rad);
                mpz_add(out->hi, sum.mid, sum.rad);
                /* For the reduced nonnegative argument the remaining
                   positive tail is below twice the next term. */
                mpz_mul_2exp(tail, upper, 1UL);
                mpz_add(out->hi, out->hi, tail);
                rc = 0;
                goto done;
            }
        }
    }
done:
    ball_clear(&tb); ball_clear(&term); ball_clear(&sum);
    ball_clear(&next); ball_clear(&temp);
    mpz_clears(p0, p1, p2, p3, scratch, upper, tail, NULL);
    return rc;
}

static int fx_exp_series(ppf_fx_iv_t *out, const ppf_fx_iv_t *t,
                         ppf_prec_t work, ppf_prec_t scale) {
    ppf_fx_iv_t term, sum, next, tail, tmp;
    mpz_t scratch, p0, p1, p2, p3;
    unsigned long max_steps;
    int rc = -1;
    fx_init(&term); fx_init(&sum); fx_init(&next); fx_init(&tail); fx_init(&tmp);
    mpz_inits(scratch, p0, p1, p2, p3, NULL);
    if (fx_set_si(&term, 1L, scale) != 0) goto done;
    fx_set(&sum, &term);
    max_steps = fx_ceil_sqrt(work) * 12UL + 512UL;
    if (max_steps > 10000000UL) max_steps = 10000000UL;
    for (unsigned long k = 1UL; k <= max_steps; ++k) {
        if (fx_mul_positive_narrow(&tmp, &term, t, scale, p0, p1, p2, p3) != 0 ||
            fx_div_ui_positive(&term, &tmp, k) != 0)
            goto done;
        fx_add(&sum, &sum, &term);
        if (mpz_sizeinbase(term.hi, 2) <= 8u) {
            if (fx_mul_positive_narrow(&tmp, &term, t, scale, p0, p1, p2, p3) != 0 ||
                fx_div_ui_positive(&next, &tmp, k + 1UL) != 0)
                goto done;
            if (mpz_cmp_ui(next.hi, 1UL) <= 0) {
                mpz_set(out->lo, sum.lo);
                mpz_mul_2exp(tail.hi, next.hi, 1UL);
                mpz_add(out->hi, sum.hi, tail.hi);
                rc = 0;
                goto done;
            }
        }
    }
done:
    mpz_clears(scratch, p0, p1, p2, p3, NULL);
    fx_clear(&term); fx_clear(&sum); fx_clear(&next); fx_clear(&tail); fx_clear(&tmp);
    return rc;
}


#ifdef PPF_FAST_KEEP_REFERENCE_BS
static int fx_exp_bs_build(ppf_fx_exp_bs_t *out, mpz_srcptr a,
                           ppf_prec_t denominator_bits,
                           unsigned long begin, unsigned long end) {
    if (begin >= end || denominator_bits > (ppf_prec_t)ULONG_MAX)
        return -1;
    if (end - begin <= 16UL) {
        mpz_set(out->p, a);
        mpz_set_ui(out->q, begin);
        mpz_set_ui(out->t, begin);
        mpz_mul_2exp(out->t, out->t,
                     (mp_bitcnt_t)(unsigned long)denominator_bits);
        out->shift = denominator_bits;
        for (unsigned long k = begin + 1UL; k < end; ++k) {
            if (out->shift > UINT64_MAX - denominator_bits) return -1;
            /* Compose with the leaf 1 + a/(k*2^b):
               T <- (T+P)*k*2^b. */
            mpz_add(out->t, out->t, out->p);
            mpz_mul_ui(out->t, out->t, k);
            mpz_mul_2exp(out->t, out->t,
                         (mp_bitcnt_t)(unsigned long)denominator_bits);
            mpz_mul(out->p, out->p, a);
            mpz_mul_ui(out->q, out->q, k);
            out->shift += denominator_bits;
        }
        return 0;
    }
    {
        unsigned long mid = begin + (end - begin) / 2UL;
        ppf_fx_exp_bs_t left, right;
        mpz_t u, v;
        int rc = -1;
        fx_exp_bs_init(&left); fx_exp_bs_init(&right);
        mpz_inits(u, v, NULL);
        if (fx_exp_bs_build(&left, a, denominator_bits, begin, mid) != 0 ||
            fx_exp_bs_build(&right, a, denominator_bits, mid, end) != 0)
            goto done;
        if (left.shift > UINT64_MAX - right.shift) goto done;
        mpz_mul(u, left.t, right.q);
        if (right.shift > (ppf_prec_t)ULONG_MAX) goto done;
        mpz_mul_2exp(u, u, (mp_bitcnt_t)(unsigned long)right.shift);
        mpz_mul(v, left.p, right.t);
        mpz_add(out->t, u, v);
        mpz_mul(out->p, left.p, right.p);
        mpz_mul(out->q, left.q, right.q);
        out->shift = left.shift + right.shift;
        rc = 0;
done:
        mpz_clears(u, v, NULL);
        fx_exp_bs_clear(&left); fx_exp_bs_clear(&right);
        return rc;
    }
}

#endif


static int fx_exp_bs_combine(ppf_fx_exp_bs_t *out,
                             const ppf_fx_exp_bs_t *left,
                             const ppf_fx_exp_bs_t *right,
                             mpz_ptr u, mpz_ptr v) {
    if (left->shift > UINT64_MAX - right->shift ||
        right->shift > (ppf_prec_t)ULONG_MAX)
        return -1;
    mpz_mul(u, left->t, right->q);
    mpz_mul_2exp(u, u, (mp_bitcnt_t)(unsigned long)right->shift);
    mpz_mul(v, left->p, right->t);
    mpz_add(out->t, u, v);
    mpz_mul(out->p, left->p, right->p);
    mpz_mul(out->q, left->q, right->q);
    out->shift = left->shift + right->shift;
    return 0;
}

/* Iterative binary splitting.  Two reusable packet levels and one scratch
   pair replace per-node packet and scratch initialization in the recursive
   reference builder. */
static int fx_exp_bs_build_iterative(ppf_fx_exp_bs_t *out, mpz_srcptr a,
                                     ppf_prec_t denominator_bits,
                                     unsigned long begin,
                                     unsigned long end) {
    enum { LEAF_SPAN = 16 };
    ppf_fx_exp_bs_t *level0 = NULL, *level1 = NULL, *src, *dst, *swap;
    mpz_t u, v;
    unsigned long term_count, count, cap1, active;
    int scratch_initialized = 0, rc = -1;
    if (begin >= end || denominator_bits > (ppf_prec_t)ULONG_MAX)
        return -1;
    term_count = end - begin;
    count = (term_count + LEAF_SPAN - 1UL) / LEAF_SPAN;
    cap1 = (count + 1UL) / 2UL;
    if (count > SIZE_MAX / sizeof(*level0) ||
        cap1 > SIZE_MAX / sizeof(*level1))
        return -1;
    level0 = (ppf_fx_exp_bs_t *)ppf_owned_buffer_calloc_internal(
        ppf_result_transaction_context_internal(), (size_t)count, sizeof(*level0));
    level1 = (ppf_fx_exp_bs_t *)ppf_owned_buffer_calloc_internal(
        ppf_result_transaction_context_internal(), (size_t)cap1, sizeof(*level1));
    if (!level0 || !level1) goto done;
    for (unsigned long i = 0; i < count; ++i) fx_exp_bs_init(&level0[i]);
    for (unsigned long i = 0; i < cap1; ++i) fx_exp_bs_init(&level1[i]);
    mpz_inits(u, v, NULL);
    scratch_initialized = 1;
    for (unsigned long i = 0; i < count; ++i) {
        unsigned long first = begin + i * LEAF_SPAN;
        unsigned long last = first + LEAF_SPAN;
        if (last > end) last = end;
        mpz_set(level0[i].p, a);
        mpz_set_ui(level0[i].q, first);
        mpz_set_ui(level0[i].t, first);
        mpz_mul_2exp(level0[i].t, level0[i].t,
                     (mp_bitcnt_t)(unsigned long)denominator_bits);
        level0[i].shift = denominator_bits;
        for (unsigned long k = first + 1UL; k < last; ++k) {
            if (level0[i].shift > UINT64_MAX - denominator_bits) goto done;
            mpz_add(level0[i].t, level0[i].t, level0[i].p);
            mpz_mul_ui(level0[i].t, level0[i].t, k);
            mpz_mul_2exp(level0[i].t, level0[i].t,
                         (mp_bitcnt_t)(unsigned long)denominator_bits);
            mpz_mul(level0[i].p, level0[i].p, a);
            mpz_mul_ui(level0[i].q, level0[i].q, k);
            level0[i].shift += denominator_bits;
        }
    }
    src = level0;
    dst = level1;
    active = count;
    while (active > 1UL) {
        unsigned long next_count = (active + 1UL) / 2UL;
        for (unsigned long j = 0; j < active / 2UL; ++j) {
            if (fx_exp_bs_combine(&dst[j], &src[2UL*j],
                                  &src[2UL*j + 1UL], u, v) != 0)
                goto done;
        }
        if (active & 1UL) {
            unsigned long j = next_count - 1UL;
            const ppf_fx_exp_bs_t *last = &src[active - 1UL];
            mpz_set(dst[j].p, last->p);
            mpz_set(dst[j].q, last->q);
            mpz_set(dst[j].t, last->t);
            dst[j].shift = last->shift;
        }
        swap = src;
        src = dst;
        dst = swap;
        active = next_count;
    }
    mpz_set(out->p, src[0].p);
    mpz_set(out->q, src[0].q);
    mpz_set(out->t, src[0].t);
    out->shift = src[0].shift;
    rc = 0;
done:
    if (scratch_initialized) mpz_clears(u, v, NULL);
    if (level0) {
        for (unsigned long i = 0; i < count; ++i) fx_exp_bs_clear(&level0[i]);
        ppf_owned_buffer_free_internal(level0);
    }
    if (level1) {
        for (unsigned long i = 0; i < cap1; ++i) fx_exp_bs_clear(&level1[i]);
        ppf_owned_buffer_free_internal(level1);
    }
    return rc;
}


/* Convert T/(Q*2^shift) outward to the fixed scale without ever materializing
   Q*2^shift. */
static int fx_exp_bs_to_fixed(ppf_fx_iv_t *out,
                              const ppf_fx_exp_bs_t *x,
                              ppf_prec_t scale) {
    mpz_t quotient, remainder, temp, rem2;
    int inexact;
    int rc = -1;
    mpz_inits(quotient, remainder, temp, rem2, NULL);
    if (mpz_sgn(x->q) <= 0) goto done;
    if (scale >= x->shift) {
        ppf_prec_t d = scale - x->shift;
        if (d > (ppf_prec_t)ULONG_MAX) goto done;
        mpz_mul_2exp(temp, x->t, (mp_bitcnt_t)(unsigned long)d);
        mpz_fdiv_qr(quotient, remainder, temp, x->q);
        mpz_set(out->lo, quotient);
        mpz_set(out->hi, quotient);
        if (mpz_sgn(remainder) != 0) mpz_add_ui(out->hi, out->hi, 1UL);
    } else {
        ppf_prec_t d = x->shift - scale;
        if (d > (ppf_prec_t)ULONG_MAX) goto done;
        mpz_fdiv_qr(quotient, remainder, x->t, x->q);
        mpz_fdiv_q_2exp(out->lo, quotient,
                         (mp_bitcnt_t)(unsigned long)d);
        mpz_set(out->hi, out->lo);
        mpz_fdiv_r_2exp(rem2, quotient,
                         (mp_bitcnt_t)(unsigned long)d);
        inexact = mpz_sgn(rem2) != 0 || mpz_sgn(remainder) != 0;
        if (inexact) mpz_add_ui(out->hi, out->hi, 1UL);
    }
    rc = 0;
done:
    mpz_clears(quotient, remainder, temp, rem2, NULL);
    return rc;
}


static int fx_exp_factorial_term_count(ppf_prec_t scale, ppf_prec_t gain,
                                       unsigned long *terms) {
    mpz_t factorial, previous;
    ppf_fast_u128_t target;
    unsigned long low, high, n;
    int rc = -1;
    if (!terms || gain == 0u) return -1;
    target = (ppf_fast_u128_t)scale + 24u;
    {
        ppf_fast_u128_t hh = (target + gain - 1u) / gain;
        if (hh < 2u) hh = 2u;
        if (hh > (ppf_fast_u128_t)ULONG_MAX) return -1;
        high = (unsigned long)hh;
    }
    low = 2UL;
    /* A floating estimate only selects a candidate.  The exact factorial
       bit length below verifies and adjusts it, so no correctness depends on
       libm rounding. */
    while (low < high) {
        unsigned long mid = low + (high - low) / 2UL;
        long double bound = (long double)gain * (long double)mid +
            lgammal((long double)mid + 1.0L) / logl(2.0L);
        if (bound >= (long double)scale + 24.0L) high = mid;
        else low = mid + 1UL;
    }
    n = low;
    mpz_inits(factorial, previous, NULL);
    mpz_fac_ui(factorial, n);
    for (;;) {
        size_t fb = mpz_sizeinbase(factorial, 2);
        ppf_fast_u128_t proven = (ppf_fast_u128_t)gain * n + (fb - 1u);
        if (proven >= target) break;
        if (n == ULONG_MAX) goto done;
        ++n;
        mpz_mul_ui(factorial, factorial, n);
    }
    while (n > 2UL) {
        size_t fb;
        ppf_fast_u128_t proven;
        mpz_fdiv_q_ui(previous, factorial, n);
        fb = mpz_sizeinbase(previous, 2);
        proven = (ppf_fast_u128_t)gain * (n - 1UL) + (fb - 1u);
        if (proven < target) break;
        mpz_set(factorial, previous);
        --n;
    }
    *terms = n;
    rc = 0;
done:
    mpz_clears(factorial, previous, NULL);
    return rc;
}

static void fx_set_exp_factorial_tail(mpz_ptr tail, ppf_prec_t scale,
                                      ppf_prec_t gain, unsigned long terms,
                                      mpz_srcptr factorial) {
#if defined(__SIZEOF_INT128__)
    ppf_fast_i128_t e = (ppf_fast_i128_t)scale + 1 -
                        (ppf_fast_i128_t)gain * terms;
    if (e >= 0 && e <= (ppf_fast_i128_t)ULONG_MAX) {
        mpz_set_ui(tail, 1UL);
        mpz_mul_2exp(tail, tail, (mp_bitcnt_t)(unsigned long)e);
        mpz_cdiv_q(tail, tail, factorial);
        if (mpz_sgn(tail) == 0) mpz_set_ui(tail, 1UL);
    } else mpz_set_ui(tail, 1UL);
#else
    fx_set_tail_power_bound(tail, scale, 1u, gain, terms);
    if (mpz_cmp_ui(tail, 1UL) > 0) {
        mpz_cdiv_q(tail, tail, factorial);
        if (mpz_sgn(tail) == 0) mpz_set_ui(tail, 1UL);
    }
#endif
}

static int fx_exp_dyadic_chunk(ppf_fx_iv_t *out, mpz_srcptr numerator,
                               ppf_prec_t denominator_bits,
                               ppf_prec_t scale) {
    ppf_fx_exp_bs_t bs;
    mpz_t upper_tail;
    size_t nbits;
    ppf_prec_t gain;
    unsigned long terms;
    int rc = -1;
    fx_exp_bs_init(&bs);
    mpz_init(upper_tail);
    if (mpz_sgn(numerator) < 0 || denominator_bits > (ppf_prec_t)ULONG_MAX)
        goto done;
    if (mpz_sgn(numerator) == 0) {
        if (fx_set_si(out, 1L, scale) != 0) goto done;
        rc = 0; goto done;
    }
    nbits = mpz_sizeinbase(numerator, 2);
    if ((ppf_prec_t)nbits >= denominator_bits) goto done;
    gain = denominator_bits - (ppf_prec_t)nbits;
    if (fx_exp_factorial_term_count(scale, gain, &terms) != 0)
        goto done;
    if (terms == 1UL) {
        if (fx_set_si(out, 1L, scale) != 0) goto done;
    } else {
        if (terms == ULONG_MAX ||
            fx_exp_bs_build_iterative(&bs, numerator, denominator_bits,
                                      1UL, terms + 1UL) != 0 ||
            fx_exp_bs_to_fixed(out, &bs, scale) != 0)
            goto done;
    }
    fx_set_exp_factorial_tail(upper_tail, scale, gain, terms, bs.q);
    mpz_add(out->hi, out->hi, upper_tail);
    rc = 0;
done:
    mpz_clear(upper_tail);
    fx_exp_bs_clear(&bs);
    return rc;
}

/* Bit-burst evaluation of exp(X/2^denominator_bits).  Binary chunks double
   their starting bit position, keeping each exact binary-splitting packet at
   O(scale) bits. */
static int fx_exp_dyadic_bitburst(ppf_fx_iv_t *out, mpz_srcptr x,
                                  ppf_prec_t denominator_bits,
                                  ppf_prec_t scale,
                                  unsigned long first_bits) {
    ppf_fx_iv_t value, chunk, temp;
    mpz_t prefix, numerator, p0, p1, p2, p3;
    ppf_prec_t previous, next, shift, width;
    int rc = -1;
    fx_init(&value); fx_init(&chunk); fx_init(&temp);
    mpz_inits(prefix, numerator, p0, p1, p2, p3, NULL);
    if (mpz_sgn(x) < 0 || denominator_bits > (ppf_prec_t)ULONG_MAX ||
        scale > (ppf_prec_t)ULONG_MAX ||
        fx_set_si(&value, 1L, scale) != 0)
        goto done;
    previous = first_bits;
    if (previous >= denominator_bits) previous = denominator_bits - 1u;
    while (previous < denominator_bits) {
        next = previous > denominator_bits / 2u
            ? denominator_bits : previous * 2u;
        if (next <= previous) next = denominator_bits;
        shift = denominator_bits - next;
        width = next - previous;
        if (shift > (ppf_prec_t)ULONG_MAX || width > (ppf_prec_t)ULONG_MAX)
            goto done;
        mpz_fdiv_q_2exp(prefix, x, (mp_bitcnt_t)(unsigned long)shift);
        mpz_fdiv_r_2exp(numerator, prefix,
                        (mp_bitcnt_t)(unsigned long)width);
        if (mpz_sgn(numerator) != 0) {
            if (fx_exp_dyadic_chunk(&chunk, numerator, next, scale) != 0 ||
                fx_mul_positive_narrow(&temp, &value, &chunk, scale,
                                       p0,p1,p2,p3) != 0)
                goto done;
            fx_set(&value, &temp);
        }
        previous = next;
    }
    fx_set(out, &value);
    rc = 0;
done:
    fx_clear(&value); fx_clear(&chunk); fx_clear(&temp);
    mpz_clears(prefix, numerator, p0,p1,p2,p3,NULL);
    return rc;
}

static int fx_exp_reconstruct_bitburst(ppf_fx_iv_t *out,
                                       const ppf_fx_iv_t *remainder,
                                       long binary_shift,
                                       ppf_prec_t work,
                                       ppf_prec_t scale,
                                       unsigned long halvings) {
    ppf_fx_ball_t input_ball, value_ball, temp_ball;
    ppf_fx_iv_t seed;
    mpz_t scratch, radius, one, factor, product, p0,p1,p2,p3;
    ppf_prec_t denominator_bits;
    int rc = -1;
    ball_init(&input_ball); ball_init(&value_ball); ball_init(&temp_ball);
    fx_init(&seed);
    mpz_inits(scratch,radius,one,factor,product,p0,p1,p2,p3,NULL);
    if (scale > (ppf_prec_t)ULONG_MAX ||
        scale > UINT64_MAX - halvings)
        goto done;
    denominator_bits = scale + halvings;
    ball_from_interval(&input_ball, remainder, scratch);
    if (mpz_sgn(input_ball.mid) < 0) goto done;
    if (fx_exp_dyadic_bitburst(&seed, input_ball.mid,
                               denominator_bits, scale, halvings) != 0)
        goto done;
    /* The input radius is also divided by 2^halvings. */
    mpz_cdiv_q_2exp(radius, input_ball.rad, halvings);
    mpz_set_ui(one,1UL);
    mpz_mul_2exp(one,one,(mp_bitcnt_t)(unsigned long)scale);
    if (mpz_cmp(radius,one)>=0) goto done;
    mpz_sub(factor,one,radius);
    /* exp(m-r) >= exp(m)(1-r). */
    mpz_mul(product,seed.lo,factor);
    mpz_fdiv_q_2exp(seed.lo,product,(mp_bitcnt_t)(unsigned long)scale);
    /* exp(m+r) <= exp(m)/(1-r). */
    mpz_mul(product,seed.hi,one);
    mpz_cdiv_q(seed.hi,product,factor);
    ball_from_interval(&value_ball,&seed,scratch);
    for(unsigned long i=0;i<halvings;++i){
        if(ball_square_positive(&temp_ball,&value_ball,scale,p0,p1,p2,p3)!=0)
            goto done;
        ball_set(&value_ball,&temp_ball);
    }
    if(binary_shift!=0 && ball_scale2(&value_ball,&value_ball,binary_shift)!=0)
        goto done;
    ball_to_interval(out,&value_ball);
    rc=0;
done:
    ball_clear(&input_ball);ball_clear(&value_ball);ball_clear(&temp_ball);
    fx_clear(&seed);
    mpz_clears(scratch,radius,one,factor,product,p0,p1,p2,p3,NULL);
    (void)work;
    return rc;
}

static int fx_exp_remainder_sparse(const ppf_fx_iv_t *remainder,
                                   ppf_prec_t work) {
    mpz_t midpoint;
    mp_bitcnt_t ones;
    int sparse;
    if (mpz_sgn(remainder->lo) < 0) return 0;
    mpz_init(midpoint);
    mpz_add(midpoint, remainder->lo, remainder->hi);
    mpz_fdiv_q_2exp(midpoint, midpoint, 1UL);
    ones = mpz_popcount(midpoint);
    /* Dyadic and low-Hamming-weight inputs are the ideal bit-burst case:
       most doubling packets vanish.  Dense inputs below the high-precision
       crossover are better served by the rectangular Taylor kernel. */
    sparse = ones != (mp_bitcnt_t)ULONG_MAX &&
             ones <= (mp_bitcnt_t)(32u + work / 1024u);
    mpz_clear(midpoint);
    return sparse;
}

static int fx_exp_reconstruct_ball(ppf_fx_iv_t *out,
                                    const ppf_fx_iv_t *remainder,
                                    long binary_shift,
                                    ppf_prec_t work,
                                    ppf_prec_t scale) {
    ppf_fx_iv_t tiny, seed;
    ppf_fx_ball_t value, temp;
    mpz_t scratch, p0, p1, p2, p3;
    unsigned long halvings;
    int rc = -1;
    fx_init(&tiny); fx_init(&seed);
    ball_init(&value); ball_init(&temp);
    mpz_inits(scratch, p0, p1, p2, p3, NULL);
    if (work >= (ppf_prec_t)PPF_FAST_EXP_BITBURST_THRESHOLD) {
        int sparse = fx_exp_remainder_sparse(remainder, work);
        if (work >= 9000u || sparse) {
            unsigned long burst_halvings = sparse
                ? (work < 2000u ? 6UL : 4UL)
                : 16UL;
            if (fx_exp_reconstruct_bitburst(out, remainder, binary_shift,
                                            work, scale, burst_halvings) == 0) {
                rc = 0;
                goto done;
            }
        }
    }
    halvings = fx_exp_halving_count(work);
    if (halvings < 4UL || halvings > (unsigned long)LONG_MAX) goto done;
    fx_scale2(&tiny, remainder, -(long)halvings);
    if (!((work >= 12000u &&
           fx_exp_series_block16(&seed, &tiny, work, scale,
                                 halvings + (unsigned long)PPF_FAST_EXP_STOP_EXTRA) == 0) ||
          fx_exp_series_block8(&seed, &tiny, work, scale,
                             halvings + (unsigned long)PPF_FAST_EXP_STOP_EXTRA) == 0) &&
        fx_exp_series_block4(&seed, &tiny, work, scale,
                             halvings + (unsigned long)PPF_FAST_EXP_STOP_EXTRA) != 0 &&
        fx_exp_series_ball(&seed, &tiny, work, scale,
                           halvings + (unsigned long)PPF_FAST_EXP_STOP_EXTRA) != 0)
        goto done;
    ball_from_interval(&value, &seed, scratch);
    for (unsigned long i = 0; i < halvings; ++i) {
        if (ball_square_positive(&temp, &value, scale,
                                 p0, p1, p2, p3) != 0)
            goto done;
        ball_set(&value, &temp);
    }
    if (binary_shift != 0 && ball_scale2(&value, &value, binary_shift) != 0)
        goto done;
    ball_to_interval(out, &value);
    rc = 0;
done:
    fx_clear(&tiny); fx_clear(&seed);
    ball_clear(&value); ball_clear(&temp);
    mpz_clears(scratch, p0, p1, p2, p3, NULL);
    return rc;
}

static int fx_exp_nonnegative_ball(ppf_fx_iv_t *out,
                                    const ppf_fx_iv_t *x,
                                    ppf_prec_t work,
                                    ppf_prec_t scale) {
    ppf_fx_iv_t ln2, rem;
    mpz_t k, product;
    long k_long;
    int rc = -1;
    fx_init(&ln2); fx_init(&rem);
    mpz_inits(k, product, NULL);
    if (mpz_sgn(x->lo) < 0 ||
        fx_set_constant(&ln2, PPF_CONSTANT_LOG2, scale) != 0)
        goto done;
    mpz_fdiv_q(k, x->lo, ln2.hi);
    if (!mpz_fits_slong_p(k)) goto done;
    k_long = mpz_get_si(k);
    if (k_long < 0 || (unsigned long)k_long > PPF_FAST_TRANS_MAX_SHIFT)
        goto done;
    mpz_mul_si(product, ln2.hi, k_long);
    mpz_sub(rem.lo, x->lo, product);
    mpz_mul_si(product, ln2.lo, k_long);
    mpz_sub(rem.hi, x->hi, product);
    if (mpz_sgn(rem.lo) < 0) mpz_set_ui(rem.lo, 0UL);
    rc = fx_exp_reconstruct_ball(out, &rem, k_long, work, scale);
done:
    fx_clear(&ln2); fx_clear(&rem);
    mpz_clears(k, product, NULL);
    return rc;
}

/* Direct 2^x reduction.  It splits x=k+f exactly at the fixed-point
   integer lattice, then computes exp(f*log(2))*2^k without dividing by
   log(2) only to recover k again. */
static int fx_exp2_interval_fast(ppf_fx_iv_t *out,
                                 const ppf_fx_iv_t *x,
                                 ppf_prec_t work,
                                 ppf_prec_t scale) {
    ppf_fx_iv_t fractional, ln2, natural_remainder;
    mpz_t k, k_fixed, p0, p1, p2, p3;
    long k_long;
    int rc = -1;
    fx_init(&fractional); fx_init(&ln2); fx_init(&natural_remainder);
    mpz_inits(k, k_fixed, p0, p1, p2, p3, NULL);
    if (scale > (ppf_prec_t)ULONG_MAX) goto done;
    mpz_fdiv_q_2exp(k, x->lo, (mp_bitcnt_t)scale);
    if (!mpz_fits_slong_p(k)) goto done;
    k_long = mpz_get_si(k);
    if ((k_long < 0 ? (unsigned long)(-(k_long + 1L)) + 1UL
                    : (unsigned long)k_long) > PPF_FAST_TRANS_MAX_SHIFT)
        goto done;
    mpz_mul_2exp(k_fixed, k, (mp_bitcnt_t)scale);
    mpz_sub(fractional.lo, x->lo, k_fixed);
    mpz_sub(fractional.hi, x->hi, k_fixed);
    if (mpz_sgn(fractional.lo) < 0) mpz_set_ui(fractional.lo, 0UL);
    if (fx_set_constant(&ln2, PPF_CONSTANT_LOG2, scale) != 0 ||
        fx_mul_positive_narrow(&natural_remainder, &fractional, &ln2,
                               scale, p0, p1, p2, p3) != 0)
        goto done;
    rc = fx_exp_reconstruct_ball(out, &natural_remainder,
                                 k_long, work, scale);
done:
    fx_clear(&fractional); fx_clear(&ln2); fx_clear(&natural_remainder);
    mpz_clears(k, k_fixed, p0, p1, p2, p3, NULL);
    return rc;
}

static int fx_exp_nonnegative(ppf_fx_iv_t *out, const ppf_fx_iv_t *x,
                              ppf_prec_t work, ppf_prec_t scale) {
    if (fx_exp_nonnegative_ball(out, x, work, scale) == 0) return 0;
    ppf_fx_iv_t ln2, rem, tiny, value, tmp;
    mpz_t k, scratch, product, p0, p1, p2, p3;
    unsigned long halvings;
    long k_long;
    int rc = -1;
    fx_init(&ln2); fx_init(&rem); fx_init(&tiny); fx_init(&value); fx_init(&tmp);
    mpz_inits(k, scratch, product, p0, p1, p2, p3, NULL);
    if (mpz_sgn(x->lo) < 0 || fx_set_constant(&ln2, PPF_CONSTANT_LOG2, scale) != 0)
        goto done;
    mpz_fdiv_q(k, x->lo, ln2.hi);
    if (!mpz_fits_slong_p(k)) goto done;
    k_long = mpz_get_si(k);
    if (k_long < 0 || (unsigned long)k_long > PPF_FAST_TRANS_MAX_SHIFT)
        goto done;
    mpz_mul_si(product, ln2.hi, k_long);
    mpz_sub(rem.lo, x->lo, product);
    mpz_mul_si(product, ln2.lo, k_long);
    mpz_sub(rem.hi, x->hi, product);
    if (mpz_sgn(rem.lo) < 0) mpz_set_ui(rem.lo, 0UL);

    halvings = fx_exp_halving_count(work);
    if (halvings < 4UL) halvings = 4UL;
    if (halvings > (unsigned long)LONG_MAX) goto done;
    fx_scale2(&tiny, &rem, -(long)halvings);
    if (fx_exp_series(&value, &tiny, work, scale) != 0) goto done;
    for (unsigned long i = 0; i < halvings; ++i) {
        if (fx_mul_positive_narrow(&tmp, &value, &value, scale, p0, p1, p2, p3) != 0)
            goto done;
        fx_set(&value, &tmp);
    }
    fx_scale2(out, &value, k_long);
    rc = 0;
done:
    mpz_clears(k, scratch, product, p0, p1, p2, p3, NULL);
    fx_clear(&ln2); fx_clear(&rem); fx_clear(&tiny); fx_clear(&value); fx_clear(&tmp);
    return rc;
}

static int fx_exp_negative_range_ball(ppf_fx_iv_t *out,
                                      const ppf_fx_iv_t *x,
                                      ppf_prec_t work,
                                      ppf_prec_t scale) {
    ppf_fx_iv_t ln2, quotient, k_interval, product, remainder;
    mpz_t k, scratch;
    long k_long;
    int rc = -1;
    fx_init(&ln2); fx_init(&quotient); fx_init(&k_interval);
    fx_init(&product); fx_init(&remainder);
    mpz_inits(k, scratch, NULL);
    if (mpz_sgn(x->hi) > 0 ||
        fx_set_constant(&ln2, PPF_CONSTANT_LOG2, scale) != 0 ||
        fx_div_signed_positive(&quotient, x, &ln2, scale, scratch) != 0)
        goto done;
    if (scale > (ppf_prec_t)ULONG_MAX) goto done;
    mpz_fdiv_q_2exp(k, quotient.lo, (mp_bitcnt_t)scale);
    if (!mpz_fits_slong_p(k)) goto done;
    k_long = mpz_get_si(k);
    if ((unsigned long)(-(k_long + 1L)) + 1UL > PPF_FAST_TRANS_MAX_SHIFT)
        goto done;
    if (fx_set_si(&k_interval, k_long, scale) != 0) goto done;
    fx_mul_si(&product, &ln2, k_long);
    fx_sub(&remainder, x, &product);
    if (mpz_sgn(remainder.lo) < 0) mpz_set_ui(remainder.lo, 0UL);
    rc = fx_exp_reconstruct_ball(out, &remainder, k_long, work, scale);
done:
    fx_clear(&ln2); fx_clear(&quotient); fx_clear(&k_interval);
    fx_clear(&product); fx_clear(&remainder);
    mpz_clears(k, scratch, NULL);
    return rc;
}

static int fx_exp_interval(ppf_fx_iv_t *out, const ppf_fx_iv_t *x,
                           ppf_prec_t work, ppf_prec_t scale) {
    ppf_fx_iv_t ax, positive, reciprocal, left, right;
    mpz_t scratch;
    int rc = -1;
    fx_init(&ax); fx_init(&positive); fx_init(&reciprocal); fx_init(&left); fx_init(&right);
    mpz_init(scratch);
    if (mpz_sgn(x->lo) >= 0) {
        rc = fx_exp_nonnegative(out, x, work, scale);
        goto done;
    }
    if (mpz_sgn(x->hi) <= 0) {
        if (fx_exp_negative_range_ball(out, x, work, scale) == 0) {
            rc = 0;
            goto done;
        }
        fx_neg(&ax, x);
        if (fx_exp_nonnegative(&positive, &ax, work, scale) != 0 ||
            fx_reciprocal_positive(&reciprocal, &positive, scale, scratch) != 0)
            goto done;
        fx_set(out, &reciprocal);
        rc = 0;
        goto done;
    }
    /* Rare sign-crossing interval: monotonic endpoint evaluation. */
    mpz_set(left.lo, x->lo); mpz_set(left.hi, x->lo);
    mpz_set(right.lo, x->hi); mpz_set(right.hi, x->hi);
    if (fx_exp_interval(&left, &left, work, scale) != 0 ||
        fx_exp_interval(&right, &right, work, scale) != 0)
        goto done;
    mpz_set(out->lo, left.lo); mpz_set(out->hi, right.hi);
    rc = 0;
done:
    mpz_clear(scratch);
    fx_clear(&ax); fx_clear(&positive); fx_clear(&reciprocal); fx_clear(&left); fx_clear(&right);
    return rc;
}

static int fx_commit_interval(ppf_t *rop, const ppf_fx_iv_t *iv,
                              ppf_prec_t scale, ppf_rnd_t rnd,
                              ppf_context_t *ctx, int *ternary) {
    ppf_context_t local;
    ppf_t temp;
    ppf_interval_status_t status;
    int rc = 0;
    memset(&local, 0, sizeof(local)); memset(&temp, 0, sizeof(temp));
    if (scale > (ppf_prec_t)INT64_MAX || ppf_context_clone(&local, ctx) != 0)
        return 0;
    local.flags = 0;
    if (ppf_init2_ctx(&temp, rop->precision, &local) != 0) goto done;
    status = ppf_set_dyadic_interval(&temp, iv->lo, iv->hi,
                                     -(ppf_exp_t)scale, rnd, ternary, &local);
    if (status != PPF_INTERVAL_DECIDED) goto done;
    ppf_swap(rop, &temp);
    ctx->flags |= local.flags;
    rc = 1;
done:
    if (temp.limbs) ppf_clear(&temp);
    ppf_context_clear(&local);
    return rc;
}

static ppf_prec_t fx_work_grow(ppf_prec_t p) {
    ppf_prec_t add = p / 3u + 48u;
    return p > UINT64_MAX - add ? p : p + add;
}

int ppf_fast_log_internal(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                          ppf_context_t *ctx, int *ternary_out) {
    ppf_fx_iv_t iv;
    ppf_prec_t work;
    int ok = 0, ternary = 0;
    fx_init(&iv);
    if (!rop || !x || !ctx || !ppf_regular_p(x) || x->sign <= 0)
        goto done;
    work = rop->precision + PPF_FAST_TRANS_GUARD;
    if (work < rop->precision) goto done;
    for (unsigned attempt = 0; attempt < PPF_FAST_TRANS_ATTEMPTS; ++attempt) {
        unsigned long roots = fx_log_root_count(work);
        ppf_prec_t scale;
        if (roots < 3UL) roots = 3UL;
        if (work > UINT64_MAX - (ppf_prec_t)roots - 64u) break;
        scale = work + (ppf_prec_t)roots + 64u;
        if (fx_log_dispatch(&iv, x, work, scale) == 0 &&
            fx_commit_interval(rop, &iv, scale, rnd, ctx, &ternary)) {
            FX_TRACE("fast_log attempt=%u work=%llu scale=%llu decided\n",
                     attempt, (unsigned long long)work,
                     (unsigned long long)scale);
            ok = 1; break;
        }
        FX_TRACE("fast_log attempt=%u work=%llu scale=%llu retry\n",
                 attempt, (unsigned long long)work,
                 (unsigned long long)scale);
        { ppf_prec_t next = fx_work_grow(work); if (next <= work) break; work = next; }
    }
    if (ok && ternary_out) *ternary_out = ternary;
done:
    fx_clear(&iv);
    return ok;
}

static int fx_apply_log_base(ppf_fx_iv_t *out,
                             const ppf_fx_iv_t *natural_log,
                             int base, ppf_prec_t work, ppf_prec_t scale,
                             ppf_context_t *ctx) {
    ppf_fx_iv_t denominator;
    mpz_t scratch;
    int rc = -1;
    if (base == 0) {
        fx_set(out, natural_log);
        return 0;
    }
    if (base != 2 && base != 10) return -1;
    fx_init(&denominator);
    mpz_init(scratch);
    if ((base == 2 &&
         fx_set_constant(&denominator, PPF_CONSTANT_LOG2, scale) != 0) ||
        (base == 10 && fx_set_ln10(&denominator, work, scale, ctx) != 0))
        goto done;
    if (fx_div_signed_positive(out, natural_log, &denominator,
                               scale, scratch) != 0)
        goto done;
    rc = 0;
done:
    mpz_clear(scratch);
    fx_clear(&denominator);
    return rc;
}

int ppf_fast_log_base_internal(ppf_t *rop, const ppf_t *x, int base,
                               ppf_rnd_t rnd, ppf_context_t *ctx,
                               int *ternary_out) {
    ppf_fx_iv_t natural_log, converted;
    ppf_prec_t work;
    int ok = 0, ternary = 0;
    fx_init(&natural_log);
    fx_init(&converted);
    if (!rop || !x || !ctx || !ppf_regular_p(x) || x->sign <= 0 ||
        (base != 2 && base != 10))
        goto done;
    work = rop->precision + PPF_FAST_TRANS_GUARD;
    if (work < rop->precision) goto done;
    for (unsigned attempt = 0; attempt < PPF_FAST_TRANS_ATTEMPTS; ++attempt) {
        unsigned long roots = fx_ceil_sqrt(work / 2u + 1u);
        ppf_prec_t scale;
        if (roots < 3UL) roots = 3UL;
        if (work > UINT64_MAX - (ppf_prec_t)roots - 80u) break;
        scale = work + (ppf_prec_t)roots + 80u;
        if (fx_log_dispatch(&natural_log, x, work, scale) == 0 &&
            fx_apply_log_base(&converted, &natural_log, base, work, scale, ctx) == 0 &&
            fx_commit_interval(rop, &converted, scale, rnd, ctx, &ternary)) {
            ok = 1;
            break;
        }
        { ppf_prec_t next = fx_work_grow(work); if (next <= work) break; work = next; }
    }
    if (ok && ternary_out) *ternary_out = ternary;
done:
    fx_clear(&natural_log);
    fx_clear(&converted);
    return ok;
}

int ppf_fast_log1p_internal(ppf_t *rop, const ppf_t *x, int base,
                            ppf_rnd_t rnd, ppf_context_t *ctx,
                            int *ternary_out) {
    ppf_fx_iv_t natural_log, converted;
    ppf_t one_plus;
    ppf_prec_t work;
    int ok = 0, ternary = 0;
    fx_init(&natural_log);
    fx_init(&converted);
    memset(&one_plus, 0, sizeof(one_plus));
    if (!rop || !x || !ctx || !ppf_regular_p(x) ||
        (base != 0 && base != 2 && base != 10) ||
        fx_exact_one_plus(&one_plus, x, ctx) != 0)
        goto done;
    work = rop->precision + PPF_FAST_TRANS_GUARD;
    if (work < rop->precision) goto done;
    for (unsigned attempt = 0; attempt < PPF_FAST_TRANS_ATTEMPTS; ++attempt) {
        unsigned long roots = fx_ceil_sqrt(work / 2u + 1u);
        ppf_prec_t scale;
        if (roots < 3UL) roots = 3UL;
        if (work > UINT64_MAX - (ppf_prec_t)roots - 80u) break;
        scale = work + (ppf_prec_t)roots + 80u;
        if (fx_log_dispatch(&natural_log, &one_plus, work, scale) == 0 &&
            fx_apply_log_base(&converted, &natural_log, base, work, scale, ctx) == 0 &&
            fx_commit_interval(rop, &converted, scale, rnd, ctx, &ternary)) {
            FX_TRACE("fast_log1p attempt=%u work=%llu scale=%llu decided\n",
                     attempt, (unsigned long long)work,
                     (unsigned long long)scale);
            ok = 1;
            break;
        }
        FX_TRACE("fast_log1p attempt=%u work=%llu scale=%llu retry\n",
                 attempt, (unsigned long long)work,
                 (unsigned long long)scale);
        { ppf_prec_t next = fx_work_grow(work); if (next <= work) break; work = next; }
    }
    if (ok && ternary_out) *ternary_out = ternary;
done:
    if (one_plus.limbs) ppf_clear(&one_plus);
    fx_clear(&natural_log);
    fx_clear(&converted);
    return ok;
}



int ppf_fast_exp_interval_internal(ppf_t *lo, ppf_t *hi,
                                   const ppf_t *x, ppf_prec_t requested_work,
                                   ppf_context_t *ctx) {
    ppf_fx_iv_t input, output;
    ppf_prec_t work;
    int ok = 0;
    fx_init(&input); fx_init(&output);
    if (!lo || !hi || !x || !ctx || !ppf_regular_p(x)) goto done;
    work = requested_work + PPF_FAST_TRANS_GUARD;
    if (work < requested_work) goto done;
    for (unsigned attempt = 0; attempt < PPF_FAST_TRANS_ATTEMPTS; ++attempt) {
        unsigned long h = fx_exp_halving_count(work);
        ppf_prec_t scale;
        ppf_exp_t exponent;
        if (work > UINT64_MAX - 2u * (ppf_prec_t)h - 64u) break;
        scale = work + 2u * (ppf_prec_t)h + 64u;
        if (scale > (ppf_prec_t)INT64_MAX) break;
        exponent = -(ppf_exp_t)scale;
        if (fx_set_ppf(&input, x, scale) == 0 &&
            fx_exp_interval(&output, &input, work, scale) == 0 &&
            ppf_set_z_2exp(lo, output.lo, exponent, PPF_RNDD, ctx) != PPF_TERNARY_ERROR &&
            ppf_set_z_2exp(hi, output.hi, exponent, PPF_RNDU, ctx) != PPF_TERNARY_ERROR) {
            ok = 1;
            break;
        }
        { ppf_prec_t next = fx_work_grow(work); if (next <= work) break; work = next; }
    }
done:
    fx_clear(&input); fx_clear(&output);
    return ok;
}
int ppf_fast_exp_internal(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                          ppf_context_t *ctx, int *ternary_out) {
    ppf_fx_iv_t input, output;
    ppf_prec_t work;
    int ok = 0, ternary = 0;
    fx_init(&input); fx_init(&output);
    if (!rop || !x || !ctx || !ppf_regular_p(x)) goto done;
    work = rop->precision + PPF_FAST_TRANS_GUARD;
    if (work < rop->precision) goto done;
    for (unsigned attempt = 0; attempt < PPF_FAST_TRANS_ATTEMPTS; ++attempt) {
        unsigned long h = fx_exp_halving_count(work);
        ppf_prec_t scale;
        if (work > UINT64_MAX - 2u * (ppf_prec_t)h - 64u) break;
        scale = work + 2u * (ppf_prec_t)h + 64u;
        if (fx_set_ppf(&input, x, scale) == 0 &&
            fx_exp_interval(&output, &input, work, scale) == 0 &&
            fx_commit_interval(rop, &output, scale, rnd, ctx, &ternary)) {
            FX_TRACE("fast_exp attempt=%u work=%llu scale=%llu decided\n",
                     attempt, (unsigned long long)work,
                     (unsigned long long)scale);
            ok = 1; break;
        }
        FX_TRACE("fast_exp attempt=%u work=%llu scale=%llu retry\n",
                 attempt, (unsigned long long)work,
                 (unsigned long long)scale);
        { ppf_prec_t next = fx_work_grow(work); if (next <= work) break; work = next; }
    }
    if (ok && ternary_out) *ternary_out = ternary;
done:
    fx_clear(&input); fx_clear(&output);
    return ok;
}

int ppf_fast_exp_scaled_internal(ppf_t *rop, const ppf_t *x, int base,
                                 int minus_one, ppf_rnd_t rnd,
                                 ppf_context_t *ctx, int *ternary_out) {
    ppf_fx_iv_t input, scaled, factor, output;
    mpz_t p0, p1, p2, p3, one;
    ppf_prec_t work;
    int ok = 0, ternary = 0;
    fx_init(&input);
    fx_init(&scaled);
    fx_init(&factor);
    fx_init(&output);
    mpz_inits(p0, p1, p2, p3, one, NULL);
    if (!rop || !x || !ctx || !ppf_regular_p(x) ||
        (base != 0 && base != 2 && base != 10))
        goto done;
    work = rop->precision + PPF_FAST_TRANS_GUARD;
    if (work < rop->precision) goto done;
    for (unsigned attempt = 0; attempt < PPF_FAST_TRANS_ATTEMPTS; ++attempt) {
        unsigned long h = fx_exp_halving_count(work);
        ppf_prec_t scale;
        if (work > UINT64_MAX - 2u * (ppf_prec_t)h - 80u) break;
        scale = work + 2u * (ppf_prec_t)h + 80u;
        if (fx_set_ppf(&input, x, scale) != 0) goto grow;
        if (base == 2 && work < (ppf_prec_t)PPF_FAST_EXP2_DIRECT_THRESHOLD) {
            if (fx_exp2_interval_fast(&output, &input, work, scale) != 0)
                goto grow;
        } else {
            if (base == 2 || base == 10) {
                if ((base == 2 &&
                     fx_set_constant(&factor, PPF_CONSTANT_LOG2, scale) != 0) ||
                    (base == 10 && fx_set_ln10(&factor, work, scale, ctx) != 0) ||
                    fx_mul_general_ball(&scaled, &input, &factor, scale,
                                        p0, p1, p2, p3) != 0)
                    goto grow;
            } else {
                fx_set(&scaled, &input);
            }
            if (fx_exp_interval(&output, &scaled, work, scale) != 0)
                goto grow;
        }
        if (minus_one) {
            if (scale > (ppf_prec_t)ULONG_MAX) goto grow;
            mpz_set_ui(one, 1UL);
            mpz_mul_2exp(one, one, (mp_bitcnt_t)scale);
            mpz_sub(output.lo, output.lo, one);
            mpz_sub(output.hi, output.hi, one);
        }
        if (fx_commit_interval(rop, &output, scale, rnd, ctx, &ternary)) {
            FX_TRACE("fast_exp_scaled base=%d minus=%d attempt=%u work=%llu scale=%llu decided\n",
                     base, minus_one, attempt,
                     (unsigned long long)work,
                     (unsigned long long)scale);
            ok = 1;
            break;
        }
grow:
        FX_TRACE("fast_exp_scaled base=%d minus=%d attempt=%u work=%llu scale=%llu retry\n",
                 base, minus_one, attempt,
                 (unsigned long long)work,
                 (unsigned long long)scale);
        { ppf_prec_t next = fx_work_grow(work); if (next <= work) break; work = next; }
    }
    if (ok && ternary_out) *ternary_out = ternary;
done:
    mpz_clears(p0, p1, p2, p3, one, NULL);
    fx_clear(&input);
    fx_clear(&scaled);
    fx_clear(&factor);
    fx_clear(&output);
    return ok;
}

int ppf_fast_pow_internal(ppf_t *rop, const ppf_t *base,
                          const ppf_t *exponent, int result_sign,
                          ppf_rnd_t rnd, ppf_context_t *ctx,
                          int *ternary_out) {
    ppf_fx_iv_t logv, y, product, expv, signedv;
    mpz_t p0,p1,p2,p3;
    ppf_t magnitude;
    ppf_prec_t work;
    int ok = 0, ternary = 0;
    fx_init(&logv); fx_init(&y); fx_init(&product); fx_init(&expv); fx_init(&signedv);
    mpz_inits(p0,p1,p2,p3,NULL);
    memset(&magnitude,0,sizeof(magnitude));
    if (!rop || !base || !exponent || !ctx || !ppf_regular_p(base) ||
        !ppf_regular_p(exponent) || ppf_zero_p(base)) goto done;
    if (ppf_init2_ctx(&magnitude, base->precision, ctx) != 0) goto done;
    if (ppf_abs(&magnitude, base, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) goto done;
    work = rop->precision + PPF_FAST_TRANS_GUARD;
    if (work < rop->precision) goto done;
    for (unsigned attempt=0; attempt<PPF_FAST_TRANS_ATTEMPTS; ++attempt) {
        unsigned long r = fx_log_root_count(work);
        unsigned long h = fx_exp_halving_count(work);
        ppf_prec_t scale;
        if (work > UINT64_MAX - (ppf_prec_t)r - 2u*(ppf_prec_t)h - 96u) break;
        scale = work + (ppf_prec_t)r + 2u*(ppf_prec_t)h + 96u;
        if (fx_log_dispatch(&logv, &magnitude, work, scale) == 0 &&
            fx_set_ppf(&y, exponent, scale) == 0 &&
            fx_mul_general_ball(&product, &logv, &y, scale, p0,p1,p2,p3) == 0 &&
            fx_exp_interval(&expv, &product, work, scale) == 0) {
            if (result_sign < 0) fx_neg(&signedv, &expv);
            else fx_set(&signedv, &expv);
            if (fx_commit_interval(rop, &signedv, scale, rnd, ctx, &ternary)) {
                FX_TRACE("fast_pow attempt=%u work=%llu scale=%llu decided\n",
                         attempt, (unsigned long long)work,
                         (unsigned long long)scale);
                ok=1; break;
            }
        }
        FX_TRACE("fast_pow attempt=%u work=%llu scale=%llu retry\n",
                 attempt, (unsigned long long)work,
                 (unsigned long long)scale);
        { ppf_prec_t next=fx_work_grow(work); if(next<=work)break; work=next; }
    }
    if(ok && ternary_out)*ternary_out=ternary;
done:
    if(magnitude.limbs)ppf_clear(&magnitude);
    mpz_clears(p0,p1,p2,p3,NULL);
    fx_clear(&logv);fx_clear(&y);fx_clear(&product);fx_clear(&expv);fx_clear(&signedv);
    return ok;
}

void ppf_transcendental_cache_purge(void) {
    pthread_once(&g_fx_ln10_once, fx_ln10_cache_init);
    pthread_mutex_lock(&g_fx_ln10_mutex);
    mpz_set_ui(g_fx_ln10_lo, 0UL);
    mpz_set_ui(g_fx_ln10_hi, 0UL);
    mpz_realloc2(g_fx_ln10_lo, 1);
    mpz_realloc2(g_fx_ln10_hi, 1);
    g_fx_ln10_scale = 0;
    g_fx_ln10_work = 0;
    g_fx_ln10_valid = 0;
    pthread_mutex_unlock(&g_fx_ln10_mutex);

    pthread_once(&g_fx_log_table_once, fx_log_table_initialize);
    for (unsigned i = 0; i < PPF_FAST_LOG_TABLE_SLOTS; ++i) {
        pthread_mutex_lock(&g_fx_log_table[i].mutex);
        mpz_set_ui(g_fx_log_table[i].lo, 0UL);
        mpz_set_ui(g_fx_log_table[i].hi, 0UL);
        mpz_realloc2(g_fx_log_table[i].lo, 1);
        mpz_realloc2(g_fx_log_table[i].hi, 1);
        g_fx_log_table[i].key = 0UL;
        g_fx_log_table[i].scale = 0;
        g_fx_log_table[i].valid = 0;
        pthread_mutex_unlock(&g_fx_log_table[i].mutex);
    }
}
