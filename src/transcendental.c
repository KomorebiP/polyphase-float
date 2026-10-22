/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include "transactional_transcendental_names.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__SIZEOF_INT128__)
__extension__ typedef __int128 ppf_trans_i128_t;
#endif

#ifndef PPF_TRANS_INITIAL_GUARD
#define PPF_TRANS_INITIAL_GUARD 80u
#endif
#ifndef PPF_TRANS_MAX_ATTEMPTS
#define PPF_TRANS_MAX_ATTEMPTS 12u
#endif
#ifndef PPF_TRANS_MAX_STEPS
#define PPF_TRANS_MAX_STEPS 10000000u
#endif
#ifndef PPF_TRANS_MAX_ALIGNMENT_BITS
#define PPF_TRANS_MAX_ALIGNMENT_BITS 1000000u
#endif
#ifndef PPF_TRANS_EXACT_POWER_FACTOR
#define PPF_TRANS_EXACT_POWER_FACTOR 8u
#endif
#ifndef PPF_TRANS_EXACT_POWER_SLACK
#define PPF_TRANS_EXACT_POWER_SLACK 65536u
#endif

typedef struct {
    ppf_t lo;
    ppf_t hi;
    int initialized;
} ppf_iv_t;

static int trans_error(ppf_t *rop, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, 1);
    if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

static int trans_nan(ppf_t *rop, int sign, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, sign);
    if (ctx) ppf_raise(ctx, PPF_FLAG_NAN);
    return 0;
}

static int trans_divzero_inf(ppf_t *rop, int sign, ppf_context_t *ctx) {
    if (rop) ppf_set_inf(rop, sign);
    if (ctx) ppf_raise(ctx, PPF_FLAG_DIVBY0);
    return 0;
}

/* Exponent bounds are 64-bit even on targets whose long is narrower. */
static int trans_cmp_exp(const ppf_t *x, ppf_exp_t bound, ppf_context_t *ctx) {
    uint64_t magnitude = bound < 0 ? (uint64_t)(-(bound + 1)) + 1u
                                   : (uint64_t)bound;
    mpz_t integer;
    int comparison;
    mpz_init(integer);
    mpz_import(integer, 1, 1, sizeof(magnitude), 0, 0, &magnitude);
    if (bound < 0) mpz_neg(integer, integer);
    comparison = ppf_cmp_z(x, integer, ctx);
    mpz_clear(integer);
    return comparison;
}

static int trans_overflow_positive(ppf_t *rop, ppf_rnd_t rnd,
                                   ppf_context_t *ctx) {
    int t;
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (rnd == PPF_RNDD || rnd == PPF_RNDZ) {
        if (ppf_set_max_finite(rop, 1, ctx) != 0)
            return trans_error(rop, ctx);
        t = -1;
    } else {
        ppf_set_inf(rop, 1);
        t = 1;
    }
    ppf_raise(ctx, PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT);
    return t;
}

static int trans_underflow_positive(ppf_t *rop, ppf_rnd_t rnd,
                                    ppf_context_t *ctx) {
    int t;
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (rnd == PPF_RNDU || rnd == PPF_RNDA) {
        if (ppf_set_min_normal(rop, 1, ctx) != 0)
            return trans_error(rop, ctx);
        t = 1;
    } else {
        ppf_set_zero(rop, 1);
        t = -1;
    }
    ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
    return t;
}

static int iv_init(ppf_iv_t *x, ppf_prec_t precision,
                   ppf_context_t *ctx) {
    memset(x, 0, sizeof(*x));
    if (ppf_init2_ctx(&x->lo, precision, ctx) != 0) return -1;
    if (ppf_init2_ctx(&x->hi, precision, ctx) != 0) {
        ppf_clear(&x->lo);
        return -1;
    }
    x->initialized = 1;
    return 0;
}

static void iv_clear(ppf_iv_t *x) {
    if (!x || !x->initialized) return;
    ppf_clear(&x->lo);
    ppf_clear(&x->hi);
    memset(x, 0, sizeof(*x));
}

static int iv_set_exact(ppf_iv_t *out, const ppf_t *x,
                        ppf_context_t *ctx) {
    if (ppf_set(&out->lo, x, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&out->hi, x, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_set_ui(ppf_iv_t *out, unsigned long value,
                     ppf_context_t *ctx) {
    if (ppf_set_ui(&out->lo, value, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&out->hi, value, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_set_si(ppf_iv_t *out, long value, ppf_context_t *ctx) {
    if (ppf_set_si(&out->lo, value, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_si(&out->hi, value, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_copy(ppf_iv_t *out, const ppf_iv_t *x,
                   ppf_context_t *ctx) {
    if (ppf_set(&out->lo, &x->lo, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&out->hi, &x->hi, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_add(ppf_iv_t *out, const ppf_iv_t *a, const ppf_iv_t *b,
                  ppf_context_t *ctx) {
    if (ppf_add(&out->lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_add(&out->hi, &a->hi, &b->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_sub(ppf_iv_t *out, const ppf_iv_t *a, const ppf_iv_t *b,
                  ppf_context_t *ctx) {
    if (ppf_sub(&out->lo, &a->lo, &b->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_sub(&out->hi, &a->hi, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_neg(ppf_iv_t *out, const ppf_iv_t *x,
                  ppf_context_t *ctx) {
    if (ppf_neg(&out->lo, &x->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_neg(&out->hi, &x->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_min_assign(ppf_t *dst, const ppf_t *x, ppf_context_t *ctx) {
    if (ppf_cmp(x, dst, ctx) < 0)
        return ppf_set(dst, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
    return 0;
}

static int iv_max_assign(ppf_t *dst, const ppf_t *x, ppf_context_t *ctx) {
    if (ppf_cmp(x, dst, ctx) > 0)
        return ppf_set(dst, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
    return 0;
}

static int iv_mul(ppf_iv_t *out, const ppf_iv_t *a, const ppf_iv_t *b,
                  ppf_context_t *ctx) {
    ppf_t lo, hi, t;
    int rc = -1;
    memset(&lo, 0, sizeof(lo));
    memset(&hi, 0, sizeof(hi));
    memset(&t, 0, sizeof(t));
    if (ppf_init2_ctx(&lo, out->lo.precision, ctx) != 0 ||
        ppf_init2_ctx(&hi, out->lo.precision, ctx) != 0 ||
        ppf_init2_ctx(&t, out->lo.precision, ctx) != 0) {
        if (lo.limbs) ppf_clear(&lo);
        if (hi.limbs) ppf_clear(&hi);
        if (t.limbs) ppf_clear(&t);
        return -1;
    }
    if (ppf_mul(&lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_mul(&hi, &a->lo, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        goto done;
#define PPF_IV_MUL_CAND(X,Y) do { \
    if (ppf_mul(&t, (X), (Y), PPF_RNDD, ctx) == PPF_TERNARY_ERROR) goto done; \
    if (iv_min_assign(&lo, &t, ctx) != 0) goto done; \
    if (ppf_mul(&t, (X), (Y), PPF_RNDU, ctx) == PPF_TERNARY_ERROR) goto done; \
    if (iv_max_assign(&hi, &t, ctx) != 0) goto done; \
} while (0)
    PPF_IV_MUL_CAND(&a->lo, &b->hi);
    PPF_IV_MUL_CAND(&a->hi, &b->lo);
    PPF_IV_MUL_CAND(&a->hi, &b->hi);
#undef PPF_IV_MUL_CAND
    ppf_swap(&out->lo, &lo);
    ppf_swap(&out->hi, &hi);
    rc = 0;
done:
    ppf_clear(&lo);
    ppf_clear(&hi);
    ppf_clear(&t);
    return rc;
}

static int iv_mul_positive(ppf_iv_t *out, const ppf_iv_t *a,
                           const ppf_iv_t *b, ppf_context_t *ctx) {
    if (ppf_mul(&out->lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_mul(&out->hi, &a->hi, &b->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_div_positive(ppf_iv_t *out, const ppf_iv_t *a,
                           const ppf_iv_t *b, ppf_context_t *ctx) {
    int slo, shi;
    if (ppf_sgn(&b->lo, ctx) <= 0) return -1;
    slo = ppf_sgn(&a->lo, ctx);
    shi = ppf_sgn(&a->hi, ctx);
    if (slo >= 0) {
        if (ppf_div(&out->lo, &a->lo, &b->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(&out->hi, &a->hi, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
    } else if (shi <= 0) {
        if (ppf_div(&out->lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(&out->hi, &a->hi, &b->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
    } else {
        if (ppf_div(&out->lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(&out->hi, &a->hi, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
    }
    return 0;
}

static int iv_reciprocal_positive(ppf_iv_t *out, const ppf_iv_t *x,
                                  ppf_context_t *ctx) {
    ppf_t one;
    int rc = -1;
    if (ppf_sgn(&x->lo, ctx) <= 0) return -1;
    if (ppf_init2_ctx(&one, out->lo.precision, ctx) != 0) return -1;
    if (ppf_set_ui(&one, 1UL, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) goto done;
    if (ppf_div(&out->lo, &one, &x->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_div(&out->hi, &one, &x->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        goto done;
    rc = 0;
done:
    ppf_clear(&one);
    return rc;
}

static int iv_scale2(ppf_iv_t *out, const ppf_iv_t *x, long shift,
                     ppf_context_t *ctx) {
    if (ppf_mul_2si(&out->lo, &x->lo, shift, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_mul_2si(&out->hi, &x->hi, shift, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_sqrt(ppf_iv_t *out, const ppf_iv_t *x,
                   ppf_context_t *ctx) {
    if (ppf_sgn(&x->lo, ctx) < 0) return -1;
    if (ppf_sqrt(&out->lo, &x->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_sqrt(&out->hi, &x->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int iv_to_dyadic(mpz_ptr lower, mpz_ptr upper, ppf_exp_t *exponent,
                        const ppf_iv_t *x) {
    mpz_t zl, zu;
    ppf_exp_t el, eu, common;
    uint64_t dl, du;
    if (!lower || !upper || !exponent || !x ||
        !ppf_number_p(&x->lo) || !ppf_number_p(&x->hi) ||
        ppf_cmp(&x->lo, &x->hi, NULL) > 0) return -1;
    mpz_inits(zl, zu, NULL);
    el = ppf_get_z_2exp(zl, &x->lo);
    eu = ppf_get_z_2exp(zu, &x->hi);
    /* Zero has no required binary exponent.  Aligning a finite endpoint with
       an artificial exponent zero can request an astronomically large GMP
       integer.  Nonfinite endpoints must never enter this representation. */
    common = mpz_sgn(zl) == 0 ? eu : mpz_sgn(zu) == 0 ? el : el < eu ? el : eu;
    dl = mpz_sgn(zl) == 0 ? 0 : (uint64_t)el - (uint64_t)common;
    du = mpz_sgn(zu) == 0 ? 0 : (uint64_t)eu - (uint64_t)common;
    if (dl > (uint64_t)ULONG_MAX || du > (uint64_t)ULONG_MAX ||
        dl > PPF_TRANS_MAX_ALIGNMENT_BITS || du > PPF_TRANS_MAX_ALIGNMENT_BITS) {
        mpz_clears(zl, zu, NULL);
        return -1;
    }
    if (dl) mpz_mul_2exp(zl, zl, (mp_bitcnt_t)dl);
    if (du) mpz_mul_2exp(zu, zu, (mp_bitcnt_t)du);
    mpz_set(lower, zl);
    mpz_set(upper, zu);
    *exponent = common;
    mpz_clears(zl, zu, NULL);
    return 0;
}

static ppf_prec_t trans_grow(ppf_prec_t p) {
    ppf_prec_t extra = p / 2u + 64u;
    if (p > UINT64_MAX - extra) return p;
    return p + extra;
}

static unsigned long ceil_sqrt_prec(ppf_prec_t p) {
    long double v = sqrtl((long double)p);
    unsigned long s;
    if (v > (long double)ULONG_MAX) return ULONG_MAX;
    s = (unsigned long)v;
    while ((long double)s * (long double)s < (long double)p) ++s;
    return s;
}

static int term_small_enough(const ppf_t *term, ppf_prec_t work,
                             unsigned extra) {
    ppf_exp_t limit;
    if (ppf_zero_p(term)) return 1;
    if (!ppf_regular_p(term)) return 0;
    if (work > (ppf_prec_t)INT64_MAX - extra) return 0;
    limit = -(ppf_exp_t)(work + extra);
    return term->exponent <= limit;
}

static int series_exp_small(ppf_iv_t *out, const ppf_iv_t *t,
                            ppf_prec_t work, ppf_context_t *ctx) {
    ppf_iv_t sum, term, next, denom, tail, tmp;
    unsigned long max_steps, k;
    int rc = -1;
    memset(&sum, 0, sizeof(sum));
    memset(&term, 0, sizeof(term));
    memset(&next, 0, sizeof(next));
    memset(&denom, 0, sizeof(denom));
    memset(&tail, 0, sizeof(tail));
    memset(&tmp, 0, sizeof(tmp));
    if (iv_init(&sum, work, ctx) || iv_init(&term, work, ctx) ||
        iv_init(&next, work, ctx) || iv_init(&denom, work, ctx) ||
        iv_init(&tail, work, ctx) || iv_init(&tmp, work, ctx)) goto done;
    if (iv_set_ui(&sum, 1UL, ctx) || iv_set_ui(&term, 1UL, ctx)) goto done;
    max_steps = ceil_sqrt_prec(work) * 8UL + 256UL;
    if (max_steps > PPF_TRANS_MAX_STEPS) max_steps = PPF_TRANS_MAX_STEPS;
    for (k = 1UL; k <= max_steps; ++k) {
        if (iv_mul_positive(&tmp, &term, t, ctx) != 0) goto done;
        if (iv_set_ui(&denom, k, ctx) != 0) goto done;
        if (iv_div_positive(&term, &tmp, &denom, ctx) != 0) goto done;
        if (iv_add(&tmp, &sum, &term, ctx) != 0) goto done;
        if (iv_copy(&sum, &tmp, ctx) != 0) goto done;
        /* The remainder following term_k is bounded by twice term_{k+1}
           once t/(k+1) <= 1/2; t is range-reduced below one. */
        if (iv_mul_positive(&tmp, &term, t, ctx) != 0) goto done;
        if (iv_set_ui(&denom, k + 1UL, ctx) != 0) goto done;
        if (iv_div_positive(&next, &tmp, &denom, ctx) != 0) goto done;
        if (k >= 2UL && term_small_enough(&next.hi, work, 24u)) {
            if (iv_scale2(&tail, &next, 1L, ctx) != 0) goto done;
            if (ppf_set(&out->lo, &sum.lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR)
                goto done;
            if (ppf_add(&out->hi, &sum.hi, &tail.hi,
                        PPF_RNDU, ctx) == PPF_TERNARY_ERROR) goto done;
            rc = 0;
            goto done;
        }
    }
done:
    iv_clear(&sum); iv_clear(&term); iv_clear(&next); iv_clear(&denom);
    iv_clear(&tail); iv_clear(&tmp);
    return rc;
}

static int exp_nonnegative_interval(ppf_iv_t *out, const ppf_iv_t *x,
                                    ppf_prec_t work, ppf_context_t *ctx) {
    ppf_iv_t ln2, quotient, k_iv, remainder, tiny, value, tmp;
    mpz_t k;
    long k_long;
    unsigned long halvings;
    int rc = -1;
    memset(&ln2, 0, sizeof(ln2)); memset(&quotient, 0, sizeof(quotient));
    memset(&k_iv, 0, sizeof(k_iv)); memset(&remainder, 0, sizeof(remainder));
    memset(&tiny, 0, sizeof(tiny)); memset(&value, 0, sizeof(value));
    memset(&tmp, 0, sizeof(tmp));
    mpz_init(k);
    if (iv_init(&ln2, work, ctx) || iv_init(&quotient, work, ctx) ||
        iv_init(&k_iv, work, ctx) || iv_init(&remainder, work, ctx) ||
        iv_init(&tiny, work, ctx) || iv_init(&value, work, ctx) ||
        iv_init(&tmp, work, ctx)) goto done;
    if (ppf_const_log2(&ln2.lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_const_log2(&ln2.hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR) goto done;
    if (iv_div_positive(&quotient, x, &ln2, ctx) != 0) goto done;
    /* The reduction index must fit long. Check its floating value before
       materializing an integer whose size could follow a huge input exponent. */
    if (!ppf_number_p(&quotient.lo) ||
        ppf_cmp_si(&quotient.lo, LONG_MAX, ctx) > 0) goto done;
    if (ppf_get_z(k, &quotient.lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR) goto done;
    if (!mpz_fits_slong_p(k)) goto done;
    k_long = mpz_get_si(k);
    if (iv_set_si(&k_iv, k_long, ctx) != 0) goto done;
    if (iv_mul(&tmp, &k_iv, &ln2, ctx) != 0) goto done;
    if (iv_sub(&remainder, x, &tmp, ctx) != 0) goto done;
    /* floor(x/upper(log2)) ensures a nonnegative exact remainder.  Tiny
       negative lower endpoints can only be directed-rounding noise. */
    if (ppf_sgn(&remainder.lo, ctx) < 0) ppf_set_zero(&remainder.lo, 1);
    halvings = ceil_sqrt_prec(work);
    if (halvings < 4UL) halvings = 4UL;
    if (halvings > (unsigned long)LONG_MAX) goto done;
    if (iv_scale2(&tiny, &remainder, -(long)halvings, ctx) != 0) goto done;
    if (series_exp_small(&value, &tiny, work, ctx) != 0) goto done;
    for (unsigned long i = 0; i < halvings; ++i) {
        if (iv_mul_positive(&tmp, &value, &value, ctx) != 0) goto done;
        if (iv_copy(&value, &tmp, ctx) != 0) goto done;
    }
    if (iv_scale2(out, &value, k_long, ctx) != 0) goto done;
    rc = 0;
done:
    mpz_clear(k);
    iv_clear(&ln2); iv_clear(&quotient); iv_clear(&k_iv); iv_clear(&remainder);
    iv_clear(&tiny); iv_clear(&value); iv_clear(&tmp);
    return rc;
}

static int exp_exact_interval(ppf_iv_t *out, const ppf_t *x,
                              ppf_prec_t work, ppf_context_t *ctx) {
    ppf_iv_t ax, positive, temp;
    int negative = ppf_signbit(x) && !ppf_zero_p(x), rc = -1;
    memset(&ax, 0, sizeof(ax)); memset(&positive, 0, sizeof(positive));
    memset(&temp, 0, sizeof(temp));
    if (iv_init(&ax, work, ctx) || iv_init(&positive, work, ctx) ||
        iv_init(&temp, work, ctx)) goto done;
    if (iv_set_exact(&ax, x, ctx) != 0) goto done;
    if (negative) {
        if (iv_neg(&temp, &ax, ctx) != 0 || iv_copy(&ax, &temp, ctx) != 0)
            goto done;
    }
    if (exp_nonnegative_interval(&positive, &ax, work, ctx) != 0) goto done;
    if (negative) {
        if (iv_reciprocal_positive(out, &positive, ctx) != 0) goto done;
    } else if (iv_copy(out, &positive, ctx) != 0) goto done;
    rc = 0;
done:
    iv_clear(&ax); iv_clear(&positive); iv_clear(&temp);
    return rc;
}

static int atanh_log_series(ppf_iv_t *out, const ppf_iv_t *z,
                            ppf_prec_t work, ppf_context_t *ctx) {
    ppf_iv_t z2, term, sum, tmp, denom, next, tail;
    unsigned long max_steps;
    int rc = -1;
    memset(&z2,0,sizeof(z2)); memset(&term,0,sizeof(term));
    memset(&sum,0,sizeof(sum)); memset(&tmp,0,sizeof(tmp));
    memset(&denom,0,sizeof(denom)); memset(&next,0,sizeof(next));
    memset(&tail,0,sizeof(tail));
    if (iv_init(&z2,work,ctx)||iv_init(&term,work,ctx)||
        iv_init(&sum,work,ctx)||iv_init(&tmp,work,ctx)||
        iv_init(&denom,work,ctx)||iv_init(&next,work,ctx)||
        iv_init(&tail,work,ctx)) goto done;
    if (iv_mul_positive(&z2,z,z,ctx)!=0 || iv_copy(&term,z,ctx)!=0 ||
        iv_copy(&sum,z,ctx)!=0) goto done;
    max_steps=ceil_sqrt_prec(work)*8UL+256UL;
    if(max_steps>PPF_TRANS_MAX_STEPS) max_steps=PPF_TRANS_MAX_STEPS;
    for(unsigned long k=1UL;k<=max_steps;++k){
        if(iv_mul_positive(&tmp,&term,&z2,ctx)!=0) goto done;
        if(iv_copy(&term,&tmp,ctx)!=0) goto done;
        if(iv_set_ui(&denom,2UL*k+1UL,ctx)!=0) goto done;
        if(iv_div_positive(&tmp,&term,&denom,ctx)!=0) goto done;
        if(iv_add(&next,&sum,&tmp,ctx)!=0 || iv_copy(&sum,&next,ctx)!=0)
            goto done;
        if(iv_mul_positive(&tmp,&term,&z2,ctx)!=0) goto done;
        if(iv_set_ui(&denom,2UL*(k+1UL)+1UL,ctx)!=0) goto done;
        if(iv_div_positive(&next,&tmp,&denom,ctx)!=0) goto done;
        if(term_small_enough(&next.hi,work,24u)){
            /* z^2 is far below one after the square-root reduction, so
               twice the next term is a conservative geometric tail. */
            if(iv_scale2(&tail,&next,1L,ctx)!=0) goto done;
            if(ppf_mul_2si(&out->lo,&sum.lo,1L,PPF_RNDD,ctx)==PPF_TERNARY_ERROR)
                goto done;
            if(ppf_add(&tmp.hi,&sum.hi,&tail.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR ||
               ppf_mul_2si(&out->hi,&tmp.hi,1L,PPF_RNDU,ctx)==PPF_TERNARY_ERROR)
                goto done;
            rc=0; goto done;
        }
    }
done:
    iv_clear(&z2);iv_clear(&term);iv_clear(&sum);iv_clear(&tmp);
    iv_clear(&denom);iv_clear(&next);iv_clear(&tail);
    return rc;
}

static int log_positive_interval(ppf_iv_t *out, const ppf_t *x,
                                 ppf_prec_t work, ppf_context_t *ctx) {
    ppf_iv_t m, temp, root, one, numerator, denominator, z, core, ln2, n_iv;
    mpz_t sig;
    ppf_exp_t exact_e, n;
    unsigned long roots;
    int rc=-1;
    memset(&m,0,sizeof(m));memset(&temp,0,sizeof(temp));memset(&root,0,sizeof(root));
    memset(&one,0,sizeof(one));memset(&numerator,0,sizeof(numerator));
    memset(&denominator,0,sizeof(denominator));memset(&z,0,sizeof(z));
    memset(&core,0,sizeof(core));memset(&ln2,0,sizeof(ln2));memset(&n_iv,0,sizeof(n_iv));
    mpz_init(sig);
    if(ppf_get_exact_dyadic(sig,&exact_e,x)!=0) goto done;
    n=x->exponent-1;
    if(iv_init(&m,work,ctx)||iv_init(&temp,work,ctx)||iv_init(&root,work,ctx)||
       iv_init(&one,work,ctx)||iv_init(&numerator,work,ctx)||
       iv_init(&denominator,work,ctx)||iv_init(&z,work,ctx)||
       iv_init(&core,work,ctx)||iv_init(&ln2,work,ctx)||iv_init(&n_iv,work,ctx)) goto done;
    /* m=x/2^n lies in [1,2). */
    if(ppf_set_z_2exp(&m.lo,sig,exact_e-n,PPF_RNDD,ctx)==PPF_TERNARY_ERROR ||
       ppf_set_z_2exp(&m.hi,sig,exact_e-n,PPF_RNDU,ctx)==PPF_TERNARY_ERROR) goto done;
    roots=ceil_sqrt_prec(work/2u+1u);
    if(roots<4UL) roots=4UL;
    if(roots>4096UL) roots=4096UL;
    for(unsigned long i=0;i<roots;++i){
        if(iv_sqrt(&root,&m,ctx)!=0 || iv_copy(&m,&root,ctx)!=0) goto done;
    }
    if(iv_set_ui(&one,1UL,ctx)!=0 || iv_sub(&numerator,&m,&one,ctx)!=0 ||
       iv_add(&denominator,&m,&one,ctx)!=0 ||
       iv_div_positive(&z,&numerator,&denominator,ctx)!=0) goto done;
    if(atanh_log_series(&core,&z,work,ctx)!=0) goto done;
    if(roots>(unsigned long)LONG_MAX || iv_scale2(&temp,&core,(long)roots,ctx)!=0)
        goto done;
    if(n!=0){
        if(ppf_const_log2(&ln2.lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR ||
           ppf_const_log2(&ln2.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR) goto done;
        if(n<LONG_MIN || n>LONG_MAX) goto done;
        if(iv_set_si(&n_iv,(long)n,ctx)!=0 || iv_mul(&core,&ln2,&n_iv,ctx)!=0 ||
           iv_add(out,&temp,&core,ctx)!=0) goto done;
    } else if(iv_copy(out,&temp,ctx)!=0) goto done;
    rc=0;
done:
    mpz_clear(sig);
    iv_clear(&m);iv_clear(&temp);iv_clear(&root);iv_clear(&one);
    iv_clear(&numerator);iv_clear(&denominator);iv_clear(&z);iv_clear(&core);
    iv_clear(&ln2);iv_clear(&n_iv);
    return rc;
}


static int exp_interval_input(ppf_iv_t *out, const ppf_iv_t *arg,
                              ppf_prec_t work, ppf_context_t *ctx) {
    ppf_iv_t lower, upper;
    int rc = -1;
    memset(&lower, 0, sizeof(lower));
    memset(&upper, 0, sizeof(upper));
    if (iv_init(&lower, work, ctx) != 0 || iv_init(&upper, work, ctx) != 0)
        goto done;
    if (exp_exact_interval(&lower, &arg->lo, work, ctx) != 0 ||
        exp_exact_interval(&upper, &arg->hi, work, ctx) != 0)
        goto done;
    if (ppf_set(&out->lo, &lower.lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&out->hi, &upper.hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        goto done;
    rc = 0;
done:
    iv_clear(&lower);
    iv_clear(&upper);
    return rc;
}

static int build_ln10_interval(ppf_iv_t *out, ppf_prec_t work,
                               ppf_context_t *ctx) {
    ppf_t ten;
    int rc = -1;
    memset(&ten, 0, sizeof(ten));
    if (ppf_init2_ctx(&ten, work, ctx) != 0) return -1;
    if (ppf_set_ui(&ten, 10UL, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
        goto done;
    rc = log_positive_interval(out, &ten, work, ctx);
done:
    ppf_clear(&ten);
    return rc;
}

typedef struct {
    const ppf_t *x;
    int base;
} log_base_arg_t;

static int build_log_base_interval(ppf_iv_t *out, ppf_prec_t work,
                                   void *opaque, ppf_context_t *ctx) {
    log_base_arg_t *a = (log_base_arg_t *)opaque;
    ppf_iv_t numerator, denominator;
    int rc = -1;
    memset(&numerator, 0, sizeof(numerator));
    memset(&denominator, 0, sizeof(denominator));
    if (iv_init(&numerator, work, ctx) != 0 ||
        iv_init(&denominator, work, ctx) != 0)
        goto done;
    if (log_positive_interval(&numerator, a->x, work, ctx) != 0)
        goto done;
    if (a->base == 2) {
        if (ppf_const_log2(&denominator.lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_const_log2(&denominator.hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            goto done;
    } else if (build_ln10_interval(&denominator, work, ctx) != 0) {
        goto done;
    }
    rc = iv_div_positive(out, &numerator, &denominator, ctx);
done:
    iv_clear(&numerator);
    iv_clear(&denominator);
    return rc;
}

typedef struct {
    const ppf_t *x;
    int which;
    int minus_one;
} exp_scaled_arg_t;

static int iv_subtract_one_admitted(const ppf_iv_t *value) {
    const ppf_t *endpoints[] = {&value->lo, &value->hi};
    for (unsigned i = 0; i < 2; ++i) {
        if (!ppf_number_p(endpoints[i])) return 0;
        if (ppf_regular_p(endpoints[i]) &&
            (endpoints[i]->exponent > 1 + (ppf_exp_t)PPF_TRANS_MAX_ALIGNMENT_BITS ||
             endpoints[i]->exponent < 1 - (ppf_exp_t)PPF_TRANS_MAX_ALIGNMENT_BITS))
            return 0;
    }
    return 1;
}

static int build_exp_scaled_interval(ppf_iv_t *out, ppf_prec_t work,
                                     void *opaque, ppf_context_t *ctx) {
    exp_scaled_arg_t *a = (exp_scaled_arg_t *)opaque;
    ppf_iv_t input, factor, product, value, one;
    int rc = -1;
    memset(&input, 0, sizeof(input)); memset(&factor, 0, sizeof(factor));
    memset(&product, 0, sizeof(product)); memset(&value, 0, sizeof(value));
    memset(&one, 0, sizeof(one));
    if (iv_init(&input, work, ctx) != 0 || iv_init(&factor, work, ctx) != 0 ||
        iv_init(&product, work, ctx) != 0 || iv_init(&value, work, ctx) != 0 ||
        iv_init(&one, work, ctx) != 0)
        goto done;
    if (iv_set_exact(&input, a->x, ctx) != 0) goto done;
    if (a->which == 2) {
        if (ppf_const_log2(&factor.lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_const_log2(&factor.hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            goto done;
    } else if (build_ln10_interval(&factor, work, ctx) != 0) {
        goto done;
    }
    if (iv_mul(&product, &input, &factor, ctx) != 0 ||
        exp_interval_input(&value, &product, work, ctx) != 0)
        goto done;
    if (a->minus_one) {
        /* Subtracting one must not expand a huge dyadic alignment before the
           final certificate admission check. Both operands have work bits. */
        if (!iv_subtract_one_admitted(&value)) goto done;
        if (iv_set_ui(&one, 1UL, ctx) != 0 ||
            iv_sub(out, &value, &one, ctx) != 0)
            goto done;
    } else if (iv_copy(out, &value, ctx) != 0) {
        goto done;
    }
    rc = 0;
done:
    iv_clear(&input); iv_clear(&factor); iv_clear(&product);
    iv_clear(&value); iv_clear(&one);
    return rc;
}

typedef struct {
    const ppf_t *x;
    const ppf_t *y;
    int sign;
} power_arg_t;

static int build_general_power_interval(ppf_iv_t *out, ppf_prec_t work,
                                        void *opaque, ppf_context_t *ctx) {
    power_arg_t *a = (power_arg_t *)opaque;
    ppf_t magnitude;
    ppf_iv_t logarithm, exponent, yinterval, value, negated;
    int rc = -1;
    memset(&magnitude, 0, sizeof(magnitude));
    memset(&logarithm, 0, sizeof(logarithm));
    memset(&exponent, 0, sizeof(exponent));
    memset(&yinterval, 0, sizeof(yinterval));
    memset(&value, 0, sizeof(value));
    memset(&negated, 0, sizeof(negated));
    if (ppf_init2_ctx(&magnitude, work, ctx) != 0 ||
        iv_init(&logarithm, work, ctx) != 0 ||
        iv_init(&exponent, work, ctx) != 0 ||
        iv_init(&yinterval, work, ctx) != 0 ||
        iv_init(&value, work, ctx) != 0 ||
        iv_init(&negated, work, ctx) != 0)
        goto done;
    if (ppf_abs(&magnitude, a->x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        log_positive_interval(&logarithm, &magnitude, work, ctx) != 0 ||
        iv_set_exact(&yinterval, a->y, ctx) != 0 ||
        iv_mul(&exponent, &logarithm, &yinterval, ctx) != 0 ||
        exp_interval_input(&value, &exponent, work, ctx) != 0)
        goto done;
    if (a->sign < 0) {
        if (iv_neg(&negated, &value, ctx) != 0 || iv_copy(out, &negated, ctx) != 0)
            goto done;
    } else if (iv_copy(out, &value, ctx) != 0) {
        goto done;
    }
    rc = 0;
done:
    if (magnitude.limbs) ppf_clear(&magnitude);
    iv_clear(&logarithm); iv_clear(&exponent); iv_clear(&yinterval);
    iv_clear(&value); iv_clear(&negated);
    return rc;
}


typedef struct {
    const ppf_t *x;
    int base;
} log1p_arg_t;

static int log_interval_input(ppf_iv_t *out, const ppf_iv_t *arg,
                              ppf_prec_t work, ppf_context_t *ctx) {
    ppf_iv_t lower, upper;
    int rc = -1;
    memset(&lower, 0, sizeof(lower));
    memset(&upper, 0, sizeof(upper));
    if (ppf_sgn(&arg->lo, ctx) <= 0 ||
        iv_init(&lower, work, ctx) != 0 || iv_init(&upper, work, ctx) != 0)
        goto done;
    if (log_positive_interval(&lower, &arg->lo, work, ctx) != 0 ||
        log_positive_interval(&upper, &arg->hi, work, ctx) != 0)
        goto done;
    if (ppf_set(&out->lo, &lower.lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&out->hi, &upper.hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        goto done;
    rc = 0;
done:
    iv_clear(&lower); iv_clear(&upper); return rc;
}

static int build_log1p_interval(ppf_iv_t *out, ppf_prec_t work,
                                void *opaque, ppf_context_t *ctx) {
    log1p_arg_t *a = (log1p_arg_t *)opaque;
    ppf_iv_t xinterval, one, sum, logarithm, denominator;
    int rc = -1;
    memset(&xinterval, 0, sizeof(xinterval)); memset(&one, 0, sizeof(one));
    memset(&sum, 0, sizeof(sum)); memset(&logarithm, 0, sizeof(logarithm));
    memset(&denominator, 0, sizeof(denominator));
    if (iv_init(&xinterval, work, ctx) != 0 || iv_init(&one, work, ctx) != 0 ||
        iv_init(&sum, work, ctx) != 0 || iv_init(&logarithm, work, ctx) != 0 ||
        iv_init(&denominator, work, ctx) != 0)
        goto done;
    if (iv_set_exact(&xinterval, a->x, ctx) != 0 || iv_set_ui(&one, 1UL, ctx) != 0 ||
        iv_add(&sum, &xinterval, &one, ctx) != 0 ||
        log_interval_input(&logarithm, &sum, work, ctx) != 0)
        goto done;
    if (a->base == 0) {
        rc = iv_copy(out, &logarithm, ctx);
    } else {
        if (a->base == 2) {
            if (ppf_const_log2(&denominator.lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
                ppf_const_log2(&denominator.hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
                goto done;
        } else if (build_ln10_interval(&denominator, work, ctx) != 0) {
            goto done;
        }
        rc = iv_div_positive(out, &logarithm, &denominator, ctx);
    }
done:
    iv_clear(&xinterval); iv_clear(&one); iv_clear(&sum);
    iv_clear(&logarithm); iv_clear(&denominator);
    return rc;
}

typedef int (*trans_builder_fn)(ppf_iv_t *, ppf_prec_t, void *, ppf_context_t *);

static int trans_evaluate(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx,
                          trans_builder_fn builder, void *opaque) {
    ppf_context_t wide;
    ppf_iv_t interval;
    mpz_t lower,upper;
    ppf_exp_t exponent;
    ppf_prec_t work;
    int ternary=0,rc=PPF_TERNARY_ERROR;
    ppf_interval_status_t status;
    ctx=ppf_resolve_context(ctx);
    if(!rop||!ctx||!builder) return trans_error(rop,ctx);
    if(rnd==PPF_RND_DEFAULT) rnd=ctx->default_rounding;
    if(!ppf_rounding_valid(rnd)) return trans_error(rop,ctx);
    if(ppf_context_clone(&wide,ctx)!=0) return trans_error(rop,ctx);
    wide.emin=PPF_EMIN_MIN; wide.emax=PPF_EMAX_MAX; wide.flags=0;
    memset(&interval,0,sizeof(interval));
    mpz_inits(lower,upper,NULL);
    work=rop->precision+PPF_TRANS_INITIAL_GUARD;
    if(work<rop->precision) goto done;
    for(unsigned attempt=0;attempt<PPF_TRANS_MAX_ATTEMPTS;++attempt){
        if(iv_init(&interval,work,&wide)!=0) goto done;
        if(builder(&interval,work,opaque,&wide)==0 &&
           iv_to_dyadic(lower,upper,&exponent,&interval)==0){
            status=ppf_set_dyadic_interval(rop,lower,upper,exponent,rnd,&ternary,ctx);
            iv_clear(&interval);
            if(status==PPF_INTERVAL_DECIDED){rc=ternary;goto done;}
        } else iv_clear(&interval);
        { ppf_prec_t next=trans_grow(work); if(next<=work) break; work=next; }
    }
done:
    iv_clear(&interval);
    mpz_clears(lower,upper,NULL);
    ppf_context_clear(&wide);
    if(rc==PPF_TERNARY_ERROR) return trans_error(rop,ctx);
    return rc;
}

typedef struct { const ppf_t *x; } unary_arg_t;
static int build_log(ppf_iv_t *out,ppf_prec_t work,void *opaque,ppf_context_t *ctx){
    unary_arg_t *a=(unary_arg_t*)opaque; return log_positive_interval(out,a->x,work,ctx);
}
static int build_exp(ppf_iv_t *out,ppf_prec_t work,void *opaque,ppf_context_t *ctx){
    unary_arg_t *a=(unary_arg_t*)opaque; return exp_exact_interval(out,a->x,work,ctx);
}

int ppf_log(ppf_t *rop,const ppf_t *x,ppf_rnd_t rnd,ppf_context_t *ctx){
    unary_arg_t a={x};
    ctx=ppf_resolve_context(ctx);
    if(!rop||!x||!ctx) return trans_error(rop,ctx);
    if(ppf_nan_p(x)) return trans_nan(rop,x->sign,ctx);
    if(ppf_zero_p(x)) return trans_divzero_inf(rop,-1,ctx);
    if(x->sign<0) return trans_nan(rop,1,ctx);
    if(ppf_inf_p(x)){ppf_set_inf(rop,1);return 0;}
    if(ppf_cmp_ui(x,1UL,ctx)==0){ppf_set_zero(rop,1);return 0;}
    { int fast_ternary = 0;
      if (ppf_fast_log_internal(rop, x, rnd, ctx, &fast_ternary))
          return fast_ternary; }
    return trans_evaluate(rop,rnd,ctx,build_log,&a);
}

static int log_base_transform(ppf_t *rop, const ppf_t *x, int base,
                              ppf_rnd_t rnd, ppf_context_t *ctx) {
    log_base_arg_t arg;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !x || !ctx) return trans_error(rop, ctx);
    if (ppf_nan_p(x)) return trans_nan(rop, x->sign, ctx);
    if (ppf_zero_p(x)) return trans_divzero_inf(rop, -1, ctx);
    if (x->sign < 0) return trans_nan(rop, 1, ctx);
    if (ppf_inf_p(x)) { ppf_set_inf(rop, 1); return 0; }
    if (ppf_cmp_ui(x, 1UL, ctx) == 0) { ppf_set_zero(rop, 1); return 0; }
    if (base == 10 && ppf_regular_p(x) && ppf_integer_p(x)) {
        mpz_t integer, quotient, remainder;
        unsigned long exponent10 = 0UL;
        mpz_inits(integer, quotient, remainder, NULL);
        if (ppf_get_z(integer, x, PPF_RNDZ, ctx) != PPF_TERNARY_ERROR &&
            mpz_sgn(integer) > 0) {
            while (mpz_cmp_ui(integer, 1UL) > 0) {
                mpz_tdiv_qr_ui(quotient, remainder, integer, 10UL);
                if (mpz_sgn(remainder) != 0) break;
                mpz_set(integer, quotient);
                ++exponent10;
            }
            if (mpz_cmp_ui(integer, 1UL) == 0) {
                mpz_clears(integer, quotient, remainder, NULL);
                return ppf_set_ui(rop, exponent10, rnd, ctx);
            }
        }
        mpz_clears(integer, quotient, remainder, NULL);
    }
    if (base == 2 && ppf_regular_p(x)) {
        size_t n = x->used_limbs;
        unsigned ones = 0;
        for (size_t i = 0; i < n; ++i) {
            mp_limb_t v = x->limbs[i];
            while (v) {
                v &= v - 1u;
                if (++ones > 1u) break;
            }
            if (ones > 1u) break;
        }
        if (ones == 1u && x->exponent - 1 >= LONG_MIN &&
            x->exponent - 1 <= LONG_MAX)
            return ppf_set_si(rop, (long)(x->exponent - 1), rnd, ctx);
    }
    { int fast_ternary = 0;
      if (ppf_fast_log_base_internal(rop, x, base, rnd, ctx,
                                     &fast_ternary))
          return fast_ternary; }
    arg.x = x;
    arg.base = base;
    return trans_evaluate(rop, rnd, ctx, build_log_base_interval, &arg);
}

int ppf_log2(ppf_t *r,const ppf_t *x,ppf_rnd_t q,ppf_context_t *c){return log_base_transform(r,x,2,q,c);}
int ppf_log10(ppf_t *r,const ppf_t *x,ppf_rnd_t q,ppf_context_t *c){return log_base_transform(r,x,10,q,c);}
int ppf_log_ui(ppf_t *r, unsigned long x, ppf_rnd_t q, ppf_context_t *c) {
    ppf_t t;
    ppf_prec_t bits = 2u;
    int z;
    c = ppf_resolve_context(c);
    if (!c || !r) return trans_error(r, c);
    if (x != 0UL) {
        unsigned long v = x;
        bits = 0;
        while (v) { ++bits; v >>= 1; }
        if (bits < 2u) bits = 2u;
    }
    memset(&t, 0, sizeof(t));
    if (ppf_init2_ctx(&t, bits, c) != 0) return trans_error(r, c);
    (void)ppf_set_ui(&t, x, PPF_RNDN, c);
    z = ppf_log(r, &t, q, c);
    ppf_clear(&t);
    return z;
}


/* Form the exact dyadic value 1+x as numerator*2^exponent when the
   alignment is practical.  This is used only to recognize exact algebraic
   cases; the general correctly-rounded path remains interval based. */
static int exact_one_plus_dyadic(mpz_t numerator, ppf_exp_t *exponent,
                                 const ppf_t *x) {
    mpz_t z, unit;
    ppf_exp_t e;
    uint64_t shift;
    int rc = -1;
    mpz_inits(z, unit, NULL);
    if (!ppf_regular_p(x) || ppf_get_exact_dyadic(z, &e, x) != 0)
        goto done;
    mpz_set_ui(unit, 1UL);
    if (e >= 0) {
        shift = (uint64_t)e;
        if (shift > (uint64_t)PPF_TRANS_MAX_STEPS ||
            shift > (uint64_t)ULONG_MAX)
            goto done;
        mpz_mul_2exp(z, z, (mp_bitcnt_t)shift);
        mpz_add_ui(numerator, z, 1UL);
        *exponent = 0;
    } else {
        shift = (uint64_t)(-(e + 1)) + 1u;
        if (shift > (uint64_t)PPF_TRANS_MAX_STEPS ||
            shift > (uint64_t)ULONG_MAX)
            goto done;
        mpz_mul_2exp(unit, unit, (mp_bitcnt_t)shift);
        mpz_add(numerator, unit, z);
        *exponent = e;
    }
    rc = 0;
done:
    mpz_clears(z, unit, NULL);
    return rc;
}

static int exact_log2_one_plus(ppf_t *rop, const ppf_t *x,
                               ppf_rnd_t rnd, ppf_context_t *ctx,
                               int *recognized) {
    mpz_t numerator, exponent_z;
    ppf_exp_t exponent;
    mp_bitcnt_t bit;
    int rc = PPF_TERNARY_ERROR;
    *recognized = 0;
    mpz_inits(numerator, exponent_z, NULL);
    if (exact_one_plus_dyadic(numerator, &exponent, x) != 0 ||
        mpz_sgn(numerator) <= 0 || mpz_popcount(numerator) != 1)
        goto done;
    bit = mpz_scan1(numerator, 0);
    mpz_set_si(exponent_z, (long)exponent);
    mpz_add_ui(exponent_z, exponent_z, bit);
    *recognized = 1;
    rc = ppf_set_z(rop, exponent_z, rnd, ctx);
done:
    mpz_clears(numerator, exponent_z, NULL);
    return rc;
}

static int exact_log10_one_plus(ppf_t *rop, const ppf_t *x,
                                ppf_rnd_t rnd, ppf_context_t *ctx,
                                int *recognized) {
    mpz_t numerator, expected, exponent_z;
    ppf_exp_t exponent;
    mp_bitcnt_t zeros;
    int rc = PPF_TERNARY_ERROR;
    *recognized = 0;
    mpz_inits(numerator, expected, exponent_z, NULL);
    if (exact_one_plus_dyadic(numerator, &exponent, x) != 0 ||
        mpz_sgn(numerator) <= 0)
        goto done;
    zeros = mpz_scan1(numerator, 0);
    if (zeros != 0) {
        mpz_fdiv_q_2exp(numerator, numerator, zeros);
        if ((uint64_t)zeros > (uint64_t)INT64_MAX ||
            exponent > INT64_MAX - (ppf_exp_t)zeros)
            goto done;
        exponent += (ppf_exp_t)zeros;
    }
    if (exponent < 0 || (uint64_t)exponent > (uint64_t)ULONG_MAX ||
        (uint64_t)exponent > (uint64_t)PPF_TRANS_MAX_STEPS)
        goto done;
    mpz_ui_pow_ui(expected, 5UL, (unsigned long)exponent);
    if (mpz_cmp(numerator, expected) != 0) goto done;
    mpz_set_si(exponent_z, (long)exponent);
    *recognized = 1;
    rc = ppf_set_z(rop, exponent_z, rnd, ctx);
done:
    mpz_clears(numerator, expected, exponent_z, NULL);
    return rc;
}

static int log1p_common(ppf_t *r, const ppf_t *x, int base,
                        ppf_rnd_t q, ppf_context_t *c) {
    log1p_arg_t arg;
    c = ppf_resolve_context(c);
    if (!r || !x || !c) return trans_error(r, c);
    if (ppf_nan_p(x)) return trans_nan(r, x->sign, c);
    if (ppf_zero_p(x)) { ppf_set_zero(r, x->sign); return 0; }
    if (ppf_inf_p(x)) {
        if (x->sign > 0) { ppf_set_inf(r, 1); return 0; }
        return trans_nan(r, 1, c);
    }
    if (ppf_cmp_si(x, -1L, c) == 0) return trans_divzero_inf(r, -1, c);
    if (ppf_cmp_si(x, -1L, c) < 0) return trans_nan(r, 1, c);
    if (base == 2) {
        int recognized = 0;
        int exact = exact_log2_one_plus(r, x, q, c, &recognized);
        if (recognized) return exact;
    } else if (base == 10) {
        int recognized = 0;
        int exact = exact_log10_one_plus(r, x, q, c, &recognized);
        if (recognized) return exact;
    }
    { int fast_ternary = 0;
      if (ppf_fast_log1p_internal(r, x, base, q, c,
                                  &fast_ternary))
          return fast_ternary; }
    arg.x = x;
    arg.base = base;
    return trans_evaluate(r, q, c, build_log1p_interval, &arg);
}

int ppf_log1p(ppf_t*r,const ppf_t*x,ppf_rnd_t q,ppf_context_t*c){return log1p_common(r,x,0,q,c);}
int ppf_log2p1(ppf_t*r,const ppf_t*x,ppf_rnd_t q,ppf_context_t*c){return log1p_common(r,x,2,q,c);}
int ppf_log10p1(ppf_t*r,const ppf_t*x,ppf_rnd_t q,ppf_context_t*c){return log1p_common(r,x,10,q,c);}

int ppf_exp(ppf_t *rop,const ppf_t *x,ppf_rnd_t rnd,ppf_context_t *ctx){
    unary_arg_t a={x};ctx=ppf_resolve_context(ctx);if(!rop||!x||!ctx)return trans_error(rop,ctx);
    if(ppf_nan_p(x))return trans_nan(rop,x->sign,ctx);
    if(ppf_inf_p(x)){if(x->sign>0)ppf_set_inf(rop,1);else ppf_set_zero(rop,1);return 0;}
    if(ppf_zero_p(x))return ppf_set_ui(rop,1UL,rnd,ctx);
    if(rnd==PPF_RND_DEFAULT)rnd=ctx->default_rounding;
    if(!ppf_rounding_valid(rnd))return trans_error(rop,ctx);
    /* For t >= 0, exp(t) >= 2^t; for t <= 0, exp(t) <= 2^t.
       These conservative comparisons prove range saturation without building
       an interval that overflows even the widened internal exponent range.
       emin-2 is the half-minimum-normal threshold, including nearest ties. */
    if(x->sign>0&&trans_cmp_exp(x,ctx->emax>0?ctx->emax:0,ctx)>=0)
        return trans_overflow_positive(rop,rnd,ctx);
    if(x->sign<0&&trans_cmp_exp(x,ctx->emin<2?ctx->emin-2:0,ctx)<=0)
        return trans_underflow_positive(rop,rnd,ctx);
    { int fast_ternary = 0;
      if (ppf_fast_exp_internal(rop, x, rnd, ctx, &fast_ternary))
          return fast_ternary; }
    return trans_evaluate(rop,rnd,ctx,build_exp,&a);
}

static int exp_scaled_constant(ppf_t *r, const ppf_t *x, int which,
                               int minus_one, ppf_rnd_t q,
                               ppf_context_t *c) {
    exp_scaled_arg_t arg;
    c = ppf_resolve_context(c);
    if (!r || !x || !c) return trans_error(r, c);
    if (ppf_nan_p(x)) return trans_nan(r, x->sign, c);
    if (ppf_inf_p(x)) {
        if (x->sign > 0) ppf_set_inf(r, 1);
        else if (minus_one) ppf_set_si(r, -1L, PPF_RNDN, c);
        else ppf_set_zero(r, 1);
        return 0;
    }
    if (ppf_zero_p(x)) {
        if (minus_one) { ppf_set_zero(r, x->sign); return 0; }
        return ppf_set_ui(r, 1UL, q, c);
    }
    if (q == PPF_RND_DEFAULT) q = c->default_rounding;
    if (!ppf_rounding_valid(q)) return trans_error(r, c);
    /* Both bases are at least two, so the same conservative range bounds
       apply as for exp. This does not apply after subtracting one. */
    if (!minus_one) {
        if (x->sign > 0 && trans_cmp_exp(x, c->emax > 0 ? c->emax : 0, c) >= 0)
            return trans_overflow_positive(r, q, c);
        if (x->sign < 0 && trans_cmp_exp(x, c->emin < 2 ? c->emin - 2 : 0, c) <= 0)
            return trans_underflow_positive(r, q, c);
    }
    if (ppf_regular_p(x) && ppf_integer_p(x) &&
        x->exponent <= (ppf_exp_t)(sizeof(long) * CHAR_BIT)) {
        mpz_t n;
        mpz_init(n);
        if (ppf_get_z(n, x, PPF_RNDZ, c) != PPF_TERNARY_ERROR &&
            mpz_fits_slong_p(n)) {
            long e = mpz_get_si(n);
            unsigned long u = e < 0
                ? (unsigned long)(-(e + 1L)) + 1UL
                : (unsigned long)e;
            if (which == 2 && !minus_one) {
                mpz_clear(n);
                return ppf_set_ui_2exp(r, 1UL, (ppf_exp_t)e, q, c);
            }
            /* Keep the exact-power shortcut proportional to the requested
               precision. Since log2(10) < 4, this budget also bounds decimal
               powers. Larger cases use the certified interval evaluator. */
            ppf_prec_t budget = r->precision > UINT64_MAX - 64u
                ? UINT64_MAX : r->precision + 64u;
            ppf_prec_t exponent_budget = which == 2 ? budget : budget / 4u;
            if ((uint64_t)u > exponent_budget) {
                mpz_clear(n);
                goto interval_path;
            }
            mpq_t exact;
            mpz_t power, num, den;
            int z;
            mpq_init(exact);
            mpz_inits(power, num, den, NULL);
            if (which == 2) {
                mpz_set_ui(power, 1UL);
                mpz_mul_2exp(power, power, (mp_bitcnt_t)u);
            } else {
                mpz_ui_pow_ui(power, 10UL, u);
            }
            if (e >= 0) { mpz_set(num, power); mpz_set_ui(den, 1UL); }
            else { mpz_set_ui(num, 1UL); mpz_set(den, power); }
            if (minus_one) mpz_sub(num, num, den);
            mpq_set_num(exact, num); mpq_set_den(exact, den);
            mpq_canonicalize(exact);
            z = ppf_set_q(r, exact, q, c);
            mpz_clears(power, num, den, NULL); mpq_clear(exact); mpz_clear(n);
            return z;
        }
        mpz_clear(n);
    }
interval_path:
    { int fast_ternary = 0;
      if (x->exponent <= (ppf_exp_t)(sizeof(long) * CHAR_BIT) &&
          ppf_fast_exp_scaled_internal(r, x, which, minus_one, q, c,
                                       &fast_ternary))
          return fast_ternary; }
    arg.x = x;
    arg.which = which;
    arg.minus_one = minus_one;
    return trans_evaluate(r, q, c, build_exp_scaled_interval, &arg);
}

int ppf_exp2(ppf_t*r,const ppf_t*x,ppf_rnd_t q,ppf_context_t*c){return exp_scaled_constant(r,x,2,0,q,c);}
int ppf_exp10(ppf_t*r,const ppf_t*x,ppf_rnd_t q,ppf_context_t*c){return exp_scaled_constant(r,x,10,0,q,c);}
int ppf_exp2m1(ppf_t*r,const ppf_t*x,ppf_rnd_t q,ppf_context_t*c){return exp_scaled_constant(r,x,2,1,q,c);}
int ppf_exp10m1(ppf_t*r,const ppf_t*x,ppf_rnd_t q,ppf_context_t*c){return exp_scaled_constant(r,x,10,1,q,c);}
typedef struct { const ppf_t *x; } expm1_arg_t;
static int build_expm1_interval(ppf_iv_t *out, ppf_prec_t work,
                                void *opaque, ppf_context_t *ctx) {
    expm1_arg_t *arg = (expm1_arg_t *)opaque;
    ppf_iv_t value, one;
    int rc = -1;
    memset(&value, 0, sizeof(value)); memset(&one, 0, sizeof(one));
    if (iv_init(&value, work, ctx) != 0 || iv_init(&one, work, ctx) != 0)
        goto done;
    if (exp_exact_interval(&value, arg->x, work, ctx) != 0 ||
        !iv_subtract_one_admitted(&value) ||
        iv_set_ui(&one, 1UL, ctx) != 0 || iv_sub(out, &value, &one, ctx) != 0)
        goto done;
    rc = 0;
done:
    iv_clear(&value); iv_clear(&one); return rc;
}

int ppf_expm1(ppf_t *r, const ppf_t *x, ppf_rnd_t q, ppf_context_t *c) {
    expm1_arg_t arg;
    c = ppf_resolve_context(c);
    if (!r || !x || !c) return trans_error(r, c);
    if (ppf_nan_p(x)) return trans_nan(r, x->sign, c);
    if (ppf_inf_p(x)) {
        if (x->sign > 0) ppf_set_inf(r, 1);
        else ppf_set_si(r, -1L, PPF_RNDN, c);
        return 0;
    }
    if (ppf_zero_p(x)) { ppf_set_zero(r, x->sign); return 0; }
    { int fast_ternary = 0;
      if (x->exponent <= (ppf_exp_t)(sizeof(long) * CHAR_BIT) &&
          ppf_fast_exp_scaled_internal(r, x, 0, 1, q, c,
                                       &fast_ternary))
          return fast_ternary; }
    arg.x = x;
    return trans_evaluate(r, q, c, build_expm1_interval, &arg);
}


static int exact_integer_power(ppf_t *rop, const ppf_t *base,
                               mpz_srcptr exponent, ppf_rnd_t rnd,
                               ppf_context_t *ctx) {
    mpz_t significand, magnitude, numerator, denominator;
    ppf_exp_t base_exp, product_exp;
    int negative_exponent, result_sign = 1;
    unsigned long power;
    int rc = PPF_TERNARY_ERROR;

    ctx = ppf_resolve_context(ctx);
    if (!rop || !base || !exponent || !ctx) return trans_error(rop, ctx);

    if (mpz_sgn(exponent) == 0)
        return ppf_set_ui(rop, 1UL, rnd, ctx);
    if (ppf_nan_p(base)) return trans_nan(rop, base->sign, ctx);

    negative_exponent = mpz_sgn(exponent) < 0;
    mpz_inits(significand, magnitude, numerator, denominator, NULL);
    mpz_abs(magnitude, exponent);
    result_sign = (base->sign < 0 && mpz_odd_p(magnitude)) ? -1 : 1;

    if (ppf_inf_p(base)) {
        if (negative_exponent) ppf_set_zero(rop, result_sign);
        else ppf_set_inf(rop, result_sign);
        rc = 0;
        goto done;
    }
    if (ppf_zero_p(base)) {
        if (negative_exponent)
            rc = trans_divzero_inf(rop, result_sign, ctx);
        else {
            ppf_set_zero(rop, result_sign);
            rc = 0;
        }
        goto done;
    }
    if (!ppf_regular_p(base)) goto done;
    if (ppf_cmpabs_ui(base, 1UL, ctx) == 0) {
        rc = ppf_set_si(rop, result_sign < 0 ? -1L : 1L, rnd, ctx);
        goto done;
    }
    if (!mpz_fits_ulong_p(magnitude)) goto done;
    power = mpz_get_ui(magnitude);
    if (ppf_get_exact_dyadic(significand, &base_exp, base) != 0) goto done;
    if (mpz_sgn(significand) < 0) mpz_neg(significand, significand);
    {
        mp_bitcnt_t zeros = mpz_scan1(significand, 0);
        if (zeros != 0) {
            if ((uint64_t)zeros > (uint64_t)INT64_MAX ||
                base_exp > INT64_MAX - (ppf_exp_t)zeros)
                goto done;
            mpz_fdiv_q_2exp(significand, significand, zeros);
            base_exp += (ppf_exp_t)zeros;
        }
    }
    if (mpz_cmp_ui(significand, 1UL) != 0) {
        uint64_t sig_bits = (uint64_t)mpz_sizeinbase(significand, 2);
        uint64_t limit = (uint64_t)rop->precision *
                         (uint64_t)PPF_TRANS_EXACT_POWER_FACTOR +
                         (uint64_t)PPF_TRANS_EXACT_POWER_SLACK;
        if (sig_bits > 1u && power > (limit - 1u) / (sig_bits - 1u))
            goto done;
    }
    mpz_pow_ui(significand, significand, power);
#if defined(__SIZEOF_INT128__)
    {
        ppf_trans_i128_t value =
            (ppf_trans_i128_t)base_exp * (ppf_trans_i128_t)power;
        if (value < (ppf_trans_i128_t)INT64_MIN ||
            value > (ppf_trans_i128_t)INT64_MAX)
            goto done;
        product_exp = (ppf_exp_t)value;
    }
#else
    if (power != 0 &&
        (base_exp > INT64_MAX / (ppf_exp_t)power ||
         base_exp < INT64_MIN / (ppf_exp_t)power))
        goto done;
    product_exp = base_exp * (ppf_exp_t)power;
#endif

    if (!negative_exponent) {
        if (result_sign < 0) mpz_neg(significand, significand);
        rc = ppf_set_z_2exp(rop, significand, product_exp, rnd, ctx);
    } else {
        mpq_t rational;
        mpq_init(rational);
        mpz_set_ui(numerator, 1UL);
        mpz_set(denominator, significand);
        if (product_exp >= 0) {
            if ((uint64_t)product_exp > (uint64_t)ULONG_MAX) {
                mpq_clear(rational);
                goto done;
            }
            mpz_mul_2exp(denominator, denominator,
                         (mp_bitcnt_t)product_exp);
        } else {
            uint64_t shift = (uint64_t)(-(product_exp + 1)) + 1u;
            if (shift > (uint64_t)ULONG_MAX) {
                mpq_clear(rational);
                goto done;
            }
            mpz_mul_2exp(numerator, numerator, (mp_bitcnt_t)shift);
        }
        if (result_sign < 0) mpz_neg(numerator, numerator);
        mpq_set_num(rational, numerator);
        mpq_set_den(rational, denominator);
        mpq_canonicalize(rational);
        rc = ppf_set_q(rop, rational, rnd, ctx);
        mpq_clear(rational);
    }

done:
    mpz_clears(significand, magnitude, numerator, denominator, NULL);
    return rc;
}

static void integer_properties(const ppf_t *value, ppf_context_t *ctx,
                               int *is_integer, int *is_odd) {
    mpz_t z;
    *is_integer = ppf_integer_p(value);
    *is_odd = 0;
    if (!*is_integer) return;
    mpz_init(z);
    if (ppf_get_z(z, value, PPF_RNDZ, ctx) != PPF_TERNARY_ERROR)
        *is_odd = mpz_odd_p(z);
    mpz_clear(z);
}


static int try_exact_power_of_two(ppf_t *rop, const ppf_t *base,
                                  const ppf_t *exponent, ppf_rnd_t rnd,
                                  ppf_context_t *ctx, int *recognized) {
    mpz_t ysig, product, one;
    ppf_exp_t ye, base_power;
    uint64_t shift;
    unsigned ones = 0;
    int rc = PPF_TERNARY_ERROR;
    *recognized = 0;
    mpz_inits(ysig, product, one, NULL);
    if (!ppf_regular_p(base) || base->sign < 0 ||
        !ppf_regular_p(exponent))
        goto done;
    for (size_t i = 0; i < base->used_limbs; ++i) {
        mp_limb_t v = base->limbs[i];
        while (v) {
            v &= v - 1u;
            if (++ones > 1u) goto done;
        }
    }
    if (ones != 1u || ppf_get_exact_dyadic(ysig, &ye, exponent) != 0)
        goto done;
    base_power = base->exponent - 1;
    mpz_mul_si(product, ysig, (long)base_power);
    if (ye >= 0) {
        shift = (uint64_t)ye;
        if (shift > (uint64_t)ULONG_MAX) goto done;
        mpz_mul_2exp(product, product, (mp_bitcnt_t)shift);
    } else {
        shift = (uint64_t)(-(ye + 1)) + 1u;
        if (shift > (uint64_t)ULONG_MAX || mpz_sgn(product) == 0 ||
            mpz_scan1(product, 0) < (mp_bitcnt_t)shift)
            goto done;
        mpz_fdiv_q_2exp(product, product, (mp_bitcnt_t)shift);
    }
    if (!mpz_fits_slong_p(product)) goto done;
    mpz_set_ui(one, 1UL);
    *recognized = 1;
    rc = ppf_set_z_2exp(rop, one, (ppf_exp_t)mpz_get_si(product), rnd, ctx);
done:
    mpz_clears(ysig, product, one, NULL);
    return rc;
}


/* Detect exact noninteger dyadic powers.  If y=c/2^m with c odd and
   positive, repeatedly take exact square roots of x=a*2^b and then raise
   the resulting dyadic value to c.  This is the exact-case counterpart to
   the interval exp(y*log(x)) path and is essential for a correct ternary
   result on cases such as 9^(1/2)=3. */
static int try_exact_dyadic_power(ppf_t *rop, const ppf_t *base,
                                  const ppf_t *exponent, ppf_rnd_t rnd,
                                  ppf_context_t *ctx, int *recognized) {
    mpz_t a, c;
    ppf_exp_t b, d, product_exp;
    mp_bitcnt_t zeros;
    unsigned long roots, power;
    int rc = PPF_TERNARY_ERROR;
    *recognized = 0;
    mpz_inits(a, c, NULL);
    if (!ppf_regular_p(base) || base->sign < 0 ||
        !ppf_regular_p(exponent) || exponent->sign < 0 ||
        ppf_integer_p(exponent) ||
        ppf_get_exact_dyadic(a, &b, base) != 0 ||
        ppf_get_exact_dyadic(c, &d, exponent) != 0)
        goto done;
    if (mpz_sgn(a) <= 0 || mpz_sgn(c) <= 0) goto done;
    zeros = mpz_scan1(a, 0);
    if (zeros != 0) {
        mpz_fdiv_q_2exp(a, a, zeros);
        if ((uint64_t)zeros > (uint64_t)INT64_MAX ||
            b > INT64_MAX - (ppf_exp_t)zeros)
            goto done;
        b += (ppf_exp_t)zeros;
    }
    zeros = mpz_scan1(c, 0);
    if (zeros != 0) {
        mpz_fdiv_q_2exp(c, c, zeros);
        if ((uint64_t)zeros > (uint64_t)INT64_MAX ||
            d > INT64_MAX - (ppf_exp_t)zeros)
            goto done;
        d += (ppf_exp_t)zeros;
    }
    if (d >= 0) goto done;
    if ((uint64_t)(-(d + 1)) + 1u > (uint64_t)ULONG_MAX) goto done;
    roots = (unsigned long)((uint64_t)(-(d + 1)) + 1u);
    if (roots > 4096UL) goto done;
    for (unsigned long i = 0; i < roots; ++i) {
        if ((b & 1) != 0) {
            mpz_mul_2exp(a, a, 1);
            --b;
        }
        if (!mpz_perfect_square_p(a)) goto done;
        mpz_sqrt(a, a);
        b /= 2;
    }
    if (!mpz_fits_ulong_p(c)) goto done;
    power = mpz_get_ui(c);
    if (mpz_cmp_ui(a, 1UL) != 0) {
        uint64_t sig_bits = (uint64_t)mpz_sizeinbase(a, 2);
        uint64_t limit = (uint64_t)rop->precision *
                         (uint64_t)PPF_TRANS_EXACT_POWER_FACTOR +
                         (uint64_t)PPF_TRANS_EXACT_POWER_SLACK;
        if (sig_bits > 1u && power > (limit - 1u) / (sig_bits - 1u))
            goto done;
    }
    mpz_pow_ui(a, a, power);
#if defined(__SIZEOF_INT128__)
    {
        ppf_trans_i128_t value =
            (ppf_trans_i128_t)b * (ppf_trans_i128_t)power;
        if (value < (ppf_trans_i128_t)INT64_MIN ||
            value > (ppf_trans_i128_t)INT64_MAX)
            goto done;
        product_exp = (ppf_exp_t)value;
    }
#else
    if (power != 0 &&
        (b > INT64_MAX / (ppf_exp_t)power ||
         b < INT64_MIN / (ppf_exp_t)power))
        goto done;
    product_exp = b * (ppf_exp_t)power;
#endif
    *recognized = 1;
    rc = ppf_set_z_2exp(rop, a, product_exp, rnd, ctx);
done:
    mpz_clears(a, c, NULL);
    return rc;
}

static int ordinary_power(ppf_t *rop, const ppf_t *base,
                          const ppf_t *exponent, ppf_rnd_t rnd,
                          ppf_context_t *ctx) {
    int exponent_integer = 0, exponent_odd = 0;
    int sign = 1;

    /* C/IEEE special cases, ordered so that NaN^0 and 1^NaN are exact 1. */
    if (ppf_zero_p(exponent)) return ppf_set_ui(rop, 1UL, rnd, ctx);
    if (ppf_nan_p(base)) return trans_nan(rop, base->sign, ctx);
    if (ppf_nan_p(exponent)) {
        if (ppf_regular_p(base) && base->sign > 0 &&
            ppf_cmp_ui(base, 1UL, ctx) == 0)
            return ppf_set_ui(rop, 1UL, rnd, ctx);
        return trans_nan(rop, exponent->sign, ctx);
    }

    integer_properties(exponent, ctx, &exponent_integer, &exponent_odd);

    if (ppf_inf_p(exponent)) {
        if (ppf_inf_p(base)) {
            if (exponent->sign > 0) ppf_set_inf(rop, 1);
            else ppf_set_zero(rop, 1);
            return 0;
        }
        {
            int cmp = ppf_cmpabs_ui(base, 1UL, ctx);
            int direction = cmp * exponent->sign;
            if (direction > 0) ppf_set_inf(rop, 1);
            else if (direction < 0) ppf_set_zero(rop, 1);
            else return ppf_set_ui(rop, 1UL, rnd, ctx);
            return 0;
        }
    }

    if (ppf_inf_p(base)) {
        sign = (base->sign < 0 && exponent_odd) ? -1 : 1;
        if (ppf_sgn(exponent, ctx) > 0) ppf_set_inf(rop, sign);
        else ppf_set_zero(rop, sign);
        return 0;
    }

    if (ppf_zero_p(base)) {
        sign = (base->sign < 0 && exponent_odd) ? -1 : 1;
        if (ppf_sgn(exponent, ctx) < 0)
            return trans_divzero_inf(rop, sign, ctx);
        ppf_set_zero(rop, sign);
        return 0;
    }

    if (base->sign < 0 && !exponent_integer)
        return trans_nan(rop, 1, ctx);

    if (ppf_cmpabs_ui(base, 1UL, ctx) == 0) {
        long value = (base->sign < 0 && exponent_odd) ? -1L : 1L;
        return ppf_set_si(rop, value, rnd, ctx);
    }

    sign = (base->sign < 0 && exponent_odd) ? -1 : 1;
    if (base->sign > 0) {
        int recognized = 0;
        int exact = try_exact_power_of_two(rop, base, exponent, rnd, ctx,
                                           &recognized);
        if (recognized) return exact;
        exact = try_exact_dyadic_power(rop, base, exponent, rnd, ctx,
                                       &recognized);
        if (recognized) return exact;
    }
    if (exponent_integer) {
        mpz_t n;
        int rc;
        mpz_init(n);
        if (ppf_get_z(n, exponent, PPF_RNDZ, ctx) != PPF_TERNARY_ERROR) {
            rc = exact_integer_power(rop, base, n, rnd, ctx);
            mpz_clear(n);
            if (rc != PPF_TERNARY_ERROR) return rc;
        } else {
            mpz_clear(n);
        }
    }

    {
        int fast_ternary = 0;
        if (ppf_fast_pow_internal(rop, base, exponent, sign, rnd, ctx,
                                  &fast_ternary))
            return fast_ternary;
    }
    {
        power_arg_t arg;
        arg.x = base;
        arg.y = exponent;
        arg.sign = sign;
        return trans_evaluate(rop, rnd, ctx,
                              build_general_power_interval, &arg);
    }
}

int ppf_pow(ppf_t *rop, const ppf_t *base, const ppf_t *exponent,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (!rop || !base || !exponent || !ctx) return trans_error(rop, ctx);
    return ordinary_power(rop, base, exponent, rnd, ctx);
}

int ppf_powr(ppf_t *rop, const ppf_t *base, const ppf_t *exponent,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (!rop || !base || !exponent || !ctx) return trans_error(rop, ctx);

    /* IEEE powr exclusions. Strict negative excludes negative zero. */
    if (ppf_nan_p(base) || ppf_sgn(base, ctx) < 0)
        return trans_nan(rop, 1, ctx);
    if ((ppf_zero_p(base) || ppf_inf_p(base)) && ppf_zero_p(exponent))
        return trans_nan(rop, 1, ctx);
    if (ppf_regular_p(base) && base->sign > 0 &&
        ppf_cmp_ui(base, 1UL, ctx) == 0 && ppf_inf_p(exponent))
        return trans_nan(rop, 1, ctx);

    if (ppf_zero_p(base) && base->sign < 0 &&
        ppf_regular_p(exponent)) {
        int sy = ppf_sgn(exponent, ctx);
        if (sy < 0) return trans_divzero_inf(rop, 1, ctx);
        if (sy > 0) {
            ppf_set_zero(rop, 1);
            return 0;
        }
    }
    return ordinary_power(rop, base, exponent, rnd, ctx);
}

int ppf_pow_ui(ppf_t *rop, const ppf_t *base, unsigned long exponent,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int rc;
    mpz_init_set_ui(z, exponent);
    rc = ppf_pow_z(rop, base, z, rnd, ctx);
    mpz_clear(z);
    return rc;
}

int ppf_pow_si(ppf_t *rop, const ppf_t *base, long exponent,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int rc;
    mpz_init_set_si(z, exponent);
    rc = ppf_pow_z(rop, base, z, rnd, ctx);
    mpz_clear(z);
    return rc;
}

int ppf_pow_uj(ppf_t *rop, const ppf_t *base, uintmax_t exponent,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int rc;
    mpz_init(z);
    mpz_import(z, 1, -1, sizeof(exponent), 0, 0, &exponent);
    rc = ppf_pow_z(rop, base, z, rnd, ctx);
    mpz_clear(z);
    return rc;
}

int ppf_pow_sj(ppf_t *rop, const ppf_t *base, intmax_t exponent,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    uintmax_t magnitude = exponent < 0
        ? (uintmax_t)(-(exponent + 1)) + 1u
        : (uintmax_t)exponent;
    int rc;
    mpz_init(z);
    mpz_import(z, 1, -1, sizeof(magnitude), 0, 0, &magnitude);
    if (exponent < 0) mpz_neg(z, z);
    rc = ppf_pow_z(rop, base, z, rnd, ctx);
    mpz_clear(z);
    return rc;
}

int ppf_pown(ppf_t *rop, const ppf_t *base, intmax_t exponent,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_pow_sj(rop, base, exponent, rnd, ctx);
}

typedef struct {
    const ppf_t *base;
    mpz_srcptr exponent;
    int sign;
} power_z_arg_t;

static int build_power_z_interval(ppf_iv_t *out, ppf_prec_t work,
                                  void *opaque, ppf_context_t *ctx) {
    power_z_arg_t *arg = (power_z_arg_t *)opaque;
    ppf_t magnitude;
    ppf_iv_t logarithm, exponent, product, value, negated;
    int rc = -1;
    memset(&magnitude, 0, sizeof(magnitude));
    memset(&logarithm, 0, sizeof(logarithm));
    memset(&exponent, 0, sizeof(exponent));
    memset(&product, 0, sizeof(product));
    memset(&value, 0, sizeof(value));
    memset(&negated, 0, sizeof(negated));
    if (ppf_init2_ctx(&magnitude, work, ctx) != 0 ||
        iv_init(&logarithm, work, ctx) != 0 ||
        iv_init(&exponent, work, ctx) != 0 ||
        iv_init(&product, work, ctx) != 0 ||
        iv_init(&value, work, ctx) != 0 ||
        iv_init(&negated, work, ctx) != 0)
        goto done;
    if (ppf_abs(&magnitude, arg->base, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        log_positive_interval(&logarithm, &magnitude, work, ctx) != 0 ||
        ppf_set_z(&exponent.lo, arg->exponent, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_z(&exponent.hi, arg->exponent, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
        iv_mul(&product, &logarithm, &exponent, ctx) != 0 ||
        exp_interval_input(&value, &product, work, ctx) != 0)
        goto done;
    if (arg->sign < 0) {
        if (iv_neg(&negated, &value, ctx) != 0 ||
            iv_copy(out, &negated, ctx) != 0)
            goto done;
    } else if (iv_copy(out, &value, ctx) != 0) {
        goto done;
    }
    rc = 0;
done:
    if (magnitude.limbs) ppf_clear(&magnitude);
    iv_clear(&logarithm); iv_clear(&exponent); iv_clear(&product);
    iv_clear(&value); iv_clear(&negated);
    return rc;
}

int ppf_pow_z(ppf_t *rop, const ppf_t *base, mpz_srcptr exponent,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    int rc, sign;
    power_z_arg_t arg;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !base || !exponent || !ctx) return trans_error(rop, ctx);
    rc = exact_integer_power(rop, base, exponent, rnd, ctx);
    if (rc != PPF_TERNARY_ERROR) return rc;
    if (!ppf_regular_p(base) || ppf_zero_p(base))
        return trans_error(rop, ctx);
    sign = (base->sign < 0 && mpz_odd_p(exponent)) ? -1 : 1;
    arg.base = base;
    arg.exponent = exponent;
    arg.sign = sign;
    return trans_evaluate(rop, rnd, ctx, build_power_z_interval, &arg);
}

int ppf_ui_pow_ui(ppf_t *rop, unsigned long base, unsigned long exponent,
                  ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t exact_base;
    ppf_prec_t precision = PPF_PREC_MIN;
    unsigned long value = base;
    int rc;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx) return trans_error(rop, ctx);
    while (value != 0) { ++precision; value >>= 1; }
    if (precision > PPF_PREC_MIN) --precision;
    memset(&exact_base, 0, sizeof(exact_base));
    if (ppf_init2_ctx(&exact_base, precision, ctx) != 0)
        return trans_error(rop, ctx);
    if (ppf_set_ui(&exact_base, base, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) {
        ppf_clear(&exact_base);
        return trans_error(rop, ctx);
    }
    rc = ppf_pow_ui(rop, &exact_base, exponent, rnd, ctx);
    ppf_clear(&exact_base);
    return rc;
}

int ppf_ui_pow(ppf_t *rop, unsigned long base, const ppf_t *exponent,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t exact_base;
    ppf_prec_t precision = 2;
    unsigned long v = base;
    int rc;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !exponent || !ctx) return trans_error(rop, ctx);
    while (v) { ++precision; v >>= 1; }
    if (precision > 2) --precision;
    memset(&exact_base, 0, sizeof(exact_base));
    if (ppf_init2_ctx(&exact_base, precision, ctx) != 0)
        return trans_error(rop, ctx);
    if (ppf_set_ui(&exact_base, base, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) {
        ppf_clear(&exact_base);
        return trans_error(rop, ctx);
    }
    rc = ordinary_power(rop, &exact_base, exponent, rnd, ctx);
    ppf_clear(&exact_base);
    return rc;
}

typedef struct {
    const ppf_t *x;
    long exponent;
} compound_arg_t;

static int build_compound_interval(ppf_iv_t *out, ppf_prec_t work,
                                   void *opaque, ppf_context_t *ctx) {
    compound_arg_t *arg = (compound_arg_t *)opaque;
    log1p_arg_t logarg;
    ppf_iv_t logarithm, ninterval, product, value;
    int rc = -1;
    memset(&logarithm, 0, sizeof(logarithm));
    memset(&ninterval, 0, sizeof(ninterval));
    memset(&product, 0, sizeof(product));
    memset(&value, 0, sizeof(value));
    if (iv_init(&logarithm, work, ctx) != 0 ||
        iv_init(&ninterval, work, ctx) != 0 ||
        iv_init(&product, work, ctx) != 0 ||
        iv_init(&value, work, ctx) != 0)
        goto done;
    logarg.x = arg->x;
    logarg.base = 0;
    if (build_log1p_interval(&logarithm, work, &logarg, ctx) != 0 ||
        iv_set_si(&ninterval, arg->exponent, ctx) != 0 ||
        iv_mul(&product, &logarithm, &ninterval, ctx) != 0 ||
        exp_interval_input(&value, &product, work, ctx) != 0 ||
        iv_copy(out, &value, ctx) != 0)
        goto done;
    rc = 0;
done:
    iv_clear(&logarithm); iv_clear(&ninterval);
    iv_clear(&product); iv_clear(&value);
    return rc;
}

int ppf_compound_si(ppf_t *rop, const ppf_t *x, long exponent,
                    ppf_rnd_t rnd, ppf_context_t *ctx) {
    int cmp;
    compound_arg_t arg;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !x || !ctx) return trans_error(rop, ctx);

    /* Follow the IEEE/MPFR special-case order. */
    if (ppf_inf_p(x) && x->sign < 0) return trans_nan(rop, 1, ctx);
    if (ppf_nan_p(x) || ppf_inf_p(x) || ppf_zero_p(x)) {
        if (exponent == 0 || ppf_zero_p(x))
            return ppf_set_ui(rop, 1UL, rnd, ctx);
        if (ppf_nan_p(x)) return trans_nan(rop, x->sign, ctx);
        /* The only remaining singular input is +Inf. */
        if (exponent < 0) ppf_set_zero(rop, 1);
        else ppf_set_inf(rop, 1);
        return 0;
    }

    cmp = ppf_cmp_si(x, -1L, ctx);
    if (cmp < 0) return trans_nan(rop, 1, ctx);
    if (exponent == 0) return ppf_set_ui(rop, 1UL, rnd, ctx);
    if (cmp == 0) {
        if (exponent < 0) return trans_divzero_inf(rop, 1, ctx);
        ppf_set_zero(rop, 1);
        return 0;
    }
    if (exponent == 1) {
        ppf_t one;
        int rc;
        memset(&one, 0, sizeof(one));
        if (ppf_init2_ctx(&one, rop->precision, ctx) != 0)
            return trans_error(rop, ctx);
        if (ppf_set_ui(&one, 1UL, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) {
            ppf_clear(&one);
            return trans_error(rop, ctx);
        }
        rc = ppf_add(rop, x, &one, rnd, ctx);
        ppf_clear(&one);
        return rc;
    }

    /* Exact dyadic 1+x values use the exact integer-power route.  This
       resolves exact results and their ternary value without losing the
       algebraic correlation in log/exp composition. */
    {
        mpz_t numerator, n;
        ppf_exp_t dyadic_exp;
        mp_bitcnt_t bits;
        ppf_t base;
        int rc;
        mpz_inits(numerator, n, NULL);
        memset(&base, 0, sizeof(base));
        if (exact_one_plus_dyadic(numerator, &dyadic_exp, x) == 0 &&
            mpz_sgn(numerator) > 0) {
            bits = mpz_sizeinbase(numerator, 2);
            if (bits < PPF_PREC_MIN) bits = PPF_PREC_MIN;
            if (ppf_init2_ctx(&base, (ppf_prec_t)bits, ctx) == 0 &&
                ppf_set_z_2exp(&base, numerator, dyadic_exp,
                               PPF_RNDN, ctx) != PPF_TERNARY_ERROR) {
                mpz_set_si(n, exponent);
                rc = exact_integer_power(rop, &base, n, rnd, ctx);
                ppf_clear(&base);
                mpz_clears(numerator, n, NULL);
                if (rc != PPF_TERNARY_ERROR) return rc;
            } else if (base.limbs) {
                ppf_clear(&base);
            }
        }
        mpz_clears(numerator, n, NULL);
    }

    arg.x = x;
    arg.exponent = exponent;
    return trans_evaluate(rop, rnd, ctx, build_compound_interval, &arg);
}

