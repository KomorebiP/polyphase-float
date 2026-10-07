/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include "transactional_trig_names.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef PPF_TRIG_INITIAL_GUARD
#define PPF_TRIG_INITIAL_GUARD 96u
#endif
#ifndef PPF_TRIG_MAX_ATTEMPTS
#define PPF_TRIG_MAX_ATTEMPTS 16u
#endif
#ifndef PPF_TRIG_MAX_REDUCTION_EXTRA
#define PPF_TRIG_MAX_REDUCTION_EXTRA 1000000u
#endif
#ifndef PPF_TRIG_MAX_STEPS
#define PPF_TRIG_MAX_STEPS 10000000u
#endif
#ifndef PPF_TRIG_FIXED_SCALED_MAX_PREC
#define PPF_TRIG_FIXED_SCALED_MAX_PREC 12000u
#endif

typedef struct {
    ppf_t lo;
    ppf_t hi;
    int initialized;
} th_iv_t;

typedef int (*th_builder_fn)(th_iv_t *, ppf_prec_t, void *, ppf_context_t *);
typedef int (*th_pair_builder_fn)(th_iv_t *, th_iv_t *, ppf_prec_t, void *, ppf_context_t *);

static int th_error(ppf_t *rop, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, 1);
    if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

static int th_nan(ppf_t *rop, int sign, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, sign);
    if (ctx) ppf_raise(ctx, PPF_FLAG_NAN);
    return 0;
}

static int th_divzero_inf(ppf_t *rop, int sign, ppf_context_t *ctx) {
    if (rop) ppf_set_inf(rop, sign);
    if (ctx) ppf_raise(ctx, PPF_FLAG_DIVBY0);
    return 0;
}

static int th_iv_init(th_iv_t *x, ppf_prec_t precision,
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

static void th_iv_clear(th_iv_t *x) {
    if (!x || !x->initialized) return;
    ppf_clear(&x->lo);
    ppf_clear(&x->hi);
    memset(x, 0, sizeof(*x));
}

static int th_iv_set_exact(th_iv_t *out, const ppf_t *x,
                           ppf_context_t *ctx) {
    if (ppf_set(&out->lo, x, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&out->hi, x, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_set_ui(th_iv_t *out, unsigned long x,
                        ppf_context_t *ctx) {
    if (ppf_set_ui(&out->lo, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&out->hi, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_set_q(th_iv_t *out, mpq_srcptr x,
                       ppf_context_t *ctx) {
    if (ppf_set_q(&out->lo, x, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_q(&out->hi, x, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_copy(th_iv_t *out, const th_iv_t *x,
                      ppf_context_t *ctx) {
    if (ppf_set(&out->lo, &x->lo, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&out->hi, &x->hi, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_add(th_iv_t *out, const th_iv_t *a, const th_iv_t *b,
                     ppf_context_t *ctx) {
    if (ppf_add(&out->lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_add(&out->hi, &a->hi, &b->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_sub(th_iv_t *out, const th_iv_t *a, const th_iv_t *b,
                     ppf_context_t *ctx) {
    if (ppf_sub(&out->lo, &a->lo, &b->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_sub(&out->hi, &a->hi, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_neg(th_iv_t *out, const th_iv_t *x,
                     ppf_context_t *ctx) {
    if (ppf_neg(&out->lo, &x->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_neg(&out->hi, &x->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_min_assign(ppf_t *dst, const ppf_t *x,
                            ppf_context_t *ctx) {
    if (ppf_cmp(x, dst, ctx) < 0)
        return ppf_set(dst, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
    return 0;
}

static int th_iv_max_assign(ppf_t *dst, const ppf_t *x,
                            ppf_context_t *ctx) {
    if (ppf_cmp(x, dst, ctx) > 0)
        return ppf_set(dst, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
    return 0;
}

static int th_iv_mul(th_iv_t *out, const th_iv_t *a, const th_iv_t *b,
                     ppf_context_t *ctx) {
    int alo = ppf_sgn(&a->lo, ctx), ahi = ppf_sgn(&a->hi, ctx);
    int blo = ppf_sgn(&b->lo, ctx), bhi = ppf_sgn(&b->hi, ctx);
    /* Sign-stable intervals need only the two extremal products.  This is the
       common case in reduction, exp/log composition, and hyperbolic formulas. */
    if (alo >= 0 && blo >= 0) {
        if (ppf_mul(&out->lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_mul(&out->hi, &a->hi, &b->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
        return 0;
    }
    if (ahi <= 0 && bhi <= 0) {
        if (ppf_mul(&out->lo, &a->hi, &b->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_mul(&out->hi, &a->lo, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
        return 0;
    }
    if (alo >= 0 && bhi <= 0) {
        if (ppf_mul(&out->lo, &a->hi, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_mul(&out->hi, &a->lo, &b->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
        return 0;
    }
    if (ahi <= 0 && blo >= 0) {
        if (ppf_mul(&out->lo, &a->lo, &b->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_mul(&out->hi, &a->hi, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
        return 0;
    }
    {
        ppf_t lo, hi, t;
        int rc = -1;
        memset(&lo, 0, sizeof(lo)); memset(&hi, 0, sizeof(hi)); memset(&t, 0, sizeof(t));
        if (ppf_init2_ctx(&lo, out->lo.precision, ctx) != 0 ||
            ppf_init2_ctx(&hi, out->lo.precision, ctx) != 0 ||
            ppf_init2_ctx(&t, out->lo.precision, ctx) != 0)
            goto done;
        if (ppf_mul(&lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_mul(&hi, &a->lo, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            goto done;
#define TH_MUL_CAND(X,Y) do { \
        if (ppf_mul(&t, (X), (Y), PPF_RNDD, ctx) == PPF_TERNARY_ERROR) goto done; \
        if (th_iv_min_assign(&lo, &t, ctx) != 0) goto done; \
        if (ppf_mul(&t, (X), (Y), PPF_RNDU, ctx) == PPF_TERNARY_ERROR) goto done; \
        if (th_iv_max_assign(&hi, &t, ctx) != 0) goto done; \
    } while (0)
        TH_MUL_CAND(&a->lo, &b->hi);
        TH_MUL_CAND(&a->hi, &b->lo);
        TH_MUL_CAND(&a->hi, &b->hi);
#undef TH_MUL_CAND
        ppf_swap(&out->lo, &lo); ppf_swap(&out->hi, &hi); rc = 0;
done:
        if (lo.limbs) ppf_clear(&lo);
        if (hi.limbs) ppf_clear(&hi);
        if (t.limbs) ppf_clear(&t);
        return rc;
    }
}

static int th_iv_square(th_iv_t *out, const th_iv_t *x,
                        ppf_context_t *ctx) {
    int slo = ppf_sgn(&x->lo, ctx);
    int shi = ppf_sgn(&x->hi, ctx);
    ppf_t a, b;
    int rc = -1;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    if (ppf_init2_ctx(&a, out->lo.precision, ctx) != 0 ||
        ppf_init2_ctx(&b, out->lo.precision, ctx) != 0)
        goto done;
    if (slo >= 0) {
        if (ppf_sqr(&out->lo, &x->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_sqr(&out->hi, &x->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            goto done;
    } else if (shi <= 0) {
        if (ppf_sqr(&out->lo, &x->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_sqr(&out->hi, &x->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            goto done;
    } else {
        ppf_set_zero(&out->lo, 1);
        if (ppf_sqr(&a, &x->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
            ppf_sqr(&b, &x->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            goto done;
        if (ppf_cmp(&a, &b, ctx) >= 0) ppf_swap(&out->hi, &a);
        else ppf_swap(&out->hi, &b);
    }
    rc = 0;
done:
    if (a.limbs) ppf_clear(&a);
    if (b.limbs) ppf_clear(&b);
    return rc;
}

static int th_iv_scale2(th_iv_t *out, const th_iv_t *x, long shift,
                        ppf_context_t *ctx) {
    if (ppf_mul_2si(&out->lo, &x->lo, shift, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_mul_2si(&out->hi, &x->hi, shift, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_sqrt(th_iv_t *out, const th_iv_t *x,
                      ppf_context_t *ctx) {
    if (ppf_sgn(&x->lo, ctx) < 0) return -1;
    if (ppf_sqrt(&out->lo, &x->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_sqrt(&out->hi, &x->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_div_positive(th_iv_t *out, const th_iv_t *a,
                              const th_iv_t *b, ppf_context_t *ctx) {
    int alo = ppf_sgn(&a->lo, ctx);
    int ahi = ppf_sgn(&a->hi, ctx);
    if (ppf_sgn(&b->lo, ctx) <= 0) return -1;
    if (alo >= 0) {
        if (ppf_div(&out->lo, &a->lo, &b->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(&out->hi, &a->hi, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
    } else if (ahi <= 0) {
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

static int th_iv_div_nonzero(th_iv_t *out, const th_iv_t *a,
                             const th_iv_t *b, ppf_context_t *ctx) {
    int blo = ppf_sgn(&b->lo, ctx);
    int bhi = ppf_sgn(&b->hi, ctx);
    if (blo > 0) return th_iv_div_positive(out, a, b, ctx);
    if (bhi < 0) {
        th_iv_t nb, q;
        int rc = -1;
        memset(&nb, 0, sizeof(nb));
        memset(&q, 0, sizeof(q));
        if (th_iv_init(&nb, out->lo.precision, ctx) != 0 ||
            th_iv_init(&q, out->lo.precision, ctx) != 0)
            goto done;
        if (th_iv_neg(&nb, b, ctx) != 0 ||
            th_iv_div_positive(&q, a, &nb, ctx) != 0 ||
            th_iv_neg(out, &q, ctx) != 0)
            goto done;
        rc = 0;
done:
        th_iv_clear(&nb);
        th_iv_clear(&q);
        return rc;
    }
    return -1;
}

static int th_iv_set_pi(th_iv_t *out, ppf_context_t *ctx) {
    if (ppf_const_pi(&out->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_const_pi(&out->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_to_dyadic(mpz_ptr lower, mpz_ptr upper,
                           ppf_exp_t *exponent, const th_iv_t *x) {
    mpz_t zl, zu;
    ppf_exp_t el, eu, common;
    uint64_t dl, du;
    if (!lower || !upper || !exponent) return -1;
    mpz_inits(zl, zu, NULL);
    el = ppf_get_z_2exp(zl, &x->lo);
    eu = ppf_get_z_2exp(zu, &x->hi);
    common = el < eu ? el : eu;
    dl = (uint64_t)el - (uint64_t)common;
    du = (uint64_t)eu - (uint64_t)common;
    if (dl > (uint64_t)ULONG_MAX || du > (uint64_t)ULONG_MAX) {
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

static ppf_prec_t th_grow(ppf_prec_t p) {
    ppf_prec_t extra = p / 2u + 64u;
    if (p > UINT64_MAX - extra) return p;
    return p + extra;
}

static int th_evaluate_start(ppf_t *rop, ppf_rnd_t rnd,
                             ppf_context_t *ctx, th_builder_fn builder,
                             void *opaque, ppf_prec_t initial_work) {
    ppf_context_t wide;
    th_iv_t interval;
    mpz_t lower, upper;
    ppf_exp_t exponent;
    ppf_prec_t work;
    int ternary = 0, rc = PPF_TERNARY_ERROR;
    ppf_interval_status_t status;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx || !builder) return th_error(rop, ctx);
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!ppf_rounding_valid(rnd)) return th_error(rop, ctx);
    if (ppf_context_clone(&wide, ctx) != 0) return th_error(rop, ctx);
    wide.emin = PPF_EMIN_MIN;
    wide.emax = PPF_EMAX_MAX;
    wide.flags = 0;
    memset(&interval, 0, sizeof(interval));
    mpz_inits(lower, upper, NULL);
    work = initial_work;
    if (work < rop->precision + PPF_TRIG_INITIAL_GUARD)
        work = rop->precision + PPF_TRIG_INITIAL_GUARD;
    if (work < rop->precision) goto done;
    for (unsigned attempt = 0; attempt < PPF_TRIG_MAX_ATTEMPTS; ++attempt) {
        if (th_iv_init(&interval, work, &wide) != 0) goto done;
        if (builder(&interval, work, opaque, &wide) == 0 &&
            th_iv_to_dyadic(lower, upper, &exponent, &interval) == 0) {
            status = ppf_set_dyadic_interval(rop, lower, upper, exponent,
                                             rnd, &ternary, ctx);
            th_iv_clear(&interval);
            if (status == PPF_INTERVAL_DECIDED) {
                rc = ternary;
                goto done;
            }
        } else {
            th_iv_clear(&interval);
        }
        {
            ppf_prec_t next = th_grow(work);
            if (next <= work) break;
            work = next;
        }
    }
done:
    th_iv_clear(&interval);
    mpz_clears(lower, upper, NULL);
    ppf_context_clear(&wide);
    if (rc == PPF_TERNARY_ERROR) return th_error(rop, ctx);
    return rc;
}

static int th_evaluate(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx,
                       th_builder_fn builder, void *opaque) {
    return th_evaluate_start(rop, rnd, ctx, builder, opaque,
                             rop ? rop->precision + PPF_TRIG_INITIAL_GUARD : 0);
}

/* Evaluate two correlated functions from one certified construction.  The
   destinations are committed only after both enclosures settle, so aliasing
   and flags have the same transactional behavior as the single-result loop. */
static int th_evaluate_pair_start(ppf_t *aout, ppf_t *bout,
                                  ppf_rnd_t rnd, ppf_context_t *ctx,
                                  th_pair_builder_fn builder, void *opaque,
                                  ppf_prec_t initial_work,
                                  int *ta_out, int *tb_out) {
    ppf_context_t wide, trial;
    th_iv_t ai, bi;
    ppf_t atmp, btmp;
    mpz_t al, au, bl, bu;
    ppf_exp_t ae, be;
    ppf_prec_t work;
    int ta = 0, tb = 0, rc = PPF_TERNARY_ERROR;
    ppf_interval_status_t sa, sb;
    ctx = ppf_resolve_context(ctx);
    if (!aout || !bout || !ctx || !builder || aout == bout)
        return PPF_TERNARY_ERROR;
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!ppf_rounding_valid(rnd)) return PPF_TERNARY_ERROR;
    if (ppf_context_clone(&wide, ctx) != 0) return PPF_TERNARY_ERROR;
    if (ppf_context_clone(&trial, ctx) != 0) {
        ppf_context_clear(&wide);
        return PPF_TERNARY_ERROR;
    }
    wide.emin = PPF_EMIN_MIN;
    wide.emax = PPF_EMAX_MAX;
    wide.flags = 0;
    trial.flags = 0;
    memset(&ai, 0, sizeof(ai));
    memset(&bi, 0, sizeof(bi));
    memset(&atmp, 0, sizeof(atmp));
    memset(&btmp, 0, sizeof(btmp));
    mpz_inits(al, au, bl, bu, NULL);
    if (ppf_init2_ctx(&atmp, aout->precision, &trial) != 0 ||
        ppf_init2_ctx(&btmp, bout->precision, &trial) != 0)
        goto done;
    work = initial_work;
    {
        ppf_prec_t need = aout->precision > bout->precision
                            ? aout->precision : bout->precision;
        if (need > UINT64_MAX - PPF_TRIG_INITIAL_GUARD) goto done;
        need += PPF_TRIG_INITIAL_GUARD;
        if (work < need) work = need;
    }
    for (unsigned attempt = 0; attempt < PPF_TRIG_MAX_ATTEMPTS; ++attempt) {
        if (th_iv_init(&ai, work, &wide) != 0 ||
            th_iv_init(&bi, work, &wide) != 0)
            goto done;
        if (builder(&ai, &bi, work, opaque, &wide) == 0 &&
            th_iv_to_dyadic(al, au, &ae, &ai) == 0 &&
            th_iv_to_dyadic(bl, bu, &be, &bi) == 0) {
            trial.flags = 0;
            sa = ppf_set_dyadic_interval(&atmp, al, au, ae,
                                         rnd, &ta, &trial);
            sb = ppf_set_dyadic_interval(&btmp, bl, bu, be,
                                         rnd, &tb, &trial);
            th_iv_clear(&ai);
            th_iv_clear(&bi);
            if (sa == PPF_INTERVAL_DECIDED && sb == PPF_INTERVAL_DECIDED) {
                ppf_swap(aout, &atmp);
                ppf_swap(bout, &btmp);
                ctx->flags |= trial.flags;
                if (ta_out) *ta_out = ta;
                if (tb_out) *tb_out = tb;
                rc = 0;
                goto done;
            }
        } else {
            th_iv_clear(&ai);
            th_iv_clear(&bi);
        }
        {
            ppf_prec_t next = th_grow(work);
            if (next <= work) break;
            work = next;
        }
    }
done:
    th_iv_clear(&ai);
    th_iv_clear(&bi);
    if (atmp.limbs) ppf_clear(&atmp);
    if (btmp.limbs) ppf_clear(&btmp);
    mpz_clears(al, au, bl, bu, NULL);
    ppf_context_clear(&wide);
    ppf_context_clear(&trial);
    if (rc != 0) {
        ppf_set_nan(aout, 1);
        ppf_set_nan(bout, 1);
        ppf_raise(ctx, PPF_FLAG_ERANGE);
    }
    return rc;
}

static int th_term_small(const ppf_t *x, ppf_prec_t work, unsigned extra) {
    ppf_exp_t limit;
    if (ppf_zero_p(x)) return 1;
    if (!ppf_regular_p(x)) return 0;
    if (work > (ppf_prec_t)INT64_MAX - extra) return 0;
    limit = -(ppf_exp_t)(work + extra);
    return x->exponent <= limit;
}

static int th_iv_abs_upper(ppf_t *bound, const th_iv_t *x,
                           ppf_context_t *ctx) {
    ppf_t a, b;
    int rc = -1;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    if (ppf_init2_ctx(&a, bound->precision, ctx) != 0 ||
        ppf_init2_ctx(&b, bound->precision, ctx) != 0)
        goto done;
    if (ppf_abs(&a, &x->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
        ppf_abs(&b, &x->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        goto done;
    if (ppf_cmp(&a, &b, ctx) >= 0) ppf_swap(bound, &a);
    else ppf_swap(bound, &b);
    rc = 0;
done:
    if (a.limbs) ppf_clear(&a);
    if (b.limbs) ppf_clear(&b);
    return rc;
}

static int th_iv_add_symmetric_tail(th_iv_t *out, const th_iv_t *sum,
                                    const ppf_t *bound,
                                    ppf_context_t *ctx) {
    if (ppf_sub(&out->lo, &sum->lo, bound, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_add(&out->hi, &sum->hi, bound, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_sin_series(th_iv_t *out, const th_iv_t *r,
                         ppf_prec_t work, ppf_context_t *ctx) {
    th_iv_t r2, sum, term, next, tmp, denom;
    ppf_t bound;
    int rc = -1;
    memset(&r2, 0, sizeof(r2));
    memset(&sum, 0, sizeof(sum));
    memset(&term, 0, sizeof(term));
    memset(&next, 0, sizeof(next));
    memset(&tmp, 0, sizeof(tmp));
    memset(&denom, 0, sizeof(denom));
    memset(&bound, 0, sizeof(bound));
    if (th_iv_init(&r2, work, ctx) != 0 ||
        th_iv_init(&sum, work, ctx) != 0 ||
        th_iv_init(&term, work, ctx) != 0 ||
        th_iv_init(&next, work, ctx) != 0 ||
        th_iv_init(&tmp, work, ctx) != 0 ||
        th_iv_init(&denom, work, ctx) != 0 ||
        ppf_init2_ctx(&bound, work, ctx) != 0)
        goto done;
    if (th_iv_square(&r2, r, ctx) != 0 ||
        th_iv_copy(&sum, r, ctx) != 0 ||
        th_iv_copy(&term, r, ctx) != 0)
        goto done;
    for (unsigned long k = 0; k < PPF_TRIG_MAX_STEPS; ++k) {
        unsigned long a = 2UL * k + 2UL;
        unsigned long b = a + 1UL;
        unsigned long d;
        if (a > ULONG_MAX / b) goto done;
        d = a * b;
        if (th_iv_mul(&tmp, &term, &r2, ctx) != 0 ||
            th_iv_set_ui(&denom, d, ctx) != 0 ||
            th_iv_div_positive(&next, &tmp, &denom, ctx) != 0 ||
            th_iv_neg(&tmp, &next, ctx) != 0 ||
            th_iv_copy(&next, &tmp, ctx) != 0)
            goto done;
        if (th_iv_abs_upper(&bound, &next, ctx) != 0) goto done;
        if (th_term_small(&bound, work, 24u)) {
            if (ppf_mul_2si(&bound, &bound, 1L,
                            PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
                th_iv_add_symmetric_tail(out, &sum, &bound, ctx) != 0)
                goto done;
            rc = 0;
            goto done;
        }
        if (th_iv_add(&tmp, &sum, &next, ctx) != 0 ||
            th_iv_copy(&sum, &tmp, ctx) != 0 ||
            th_iv_copy(&term, &next, ctx) != 0)
            goto done;
    }
done:
    th_iv_clear(&r2);
    th_iv_clear(&sum);
    th_iv_clear(&term);
    th_iv_clear(&next);
    th_iv_clear(&tmp);
    th_iv_clear(&denom);
    if (bound.limbs) ppf_clear(&bound);
    return rc;
}

static int th_cos_series(th_iv_t *out, const th_iv_t *r,
                         ppf_prec_t work, ppf_context_t *ctx) {
    th_iv_t r2, sum, term, next, tmp, denom;
    ppf_t bound;
    int rc = -1;
    memset(&r2, 0, sizeof(r2));
    memset(&sum, 0, sizeof(sum));
    memset(&term, 0, sizeof(term));
    memset(&next, 0, sizeof(next));
    memset(&tmp, 0, sizeof(tmp));
    memset(&denom, 0, sizeof(denom));
    memset(&bound, 0, sizeof(bound));
    if (th_iv_init(&r2, work, ctx) != 0 ||
        th_iv_init(&sum, work, ctx) != 0 ||
        th_iv_init(&term, work, ctx) != 0 ||
        th_iv_init(&next, work, ctx) != 0 ||
        th_iv_init(&tmp, work, ctx) != 0 ||
        th_iv_init(&denom, work, ctx) != 0 ||
        ppf_init2_ctx(&bound, work, ctx) != 0)
        goto done;
    if (th_iv_square(&r2, r, ctx) != 0 ||
        th_iv_set_ui(&sum, 1UL, ctx) != 0 ||
        th_iv_set_ui(&term, 1UL, ctx) != 0)
        goto done;
    for (unsigned long k = 0; k < PPF_TRIG_MAX_STEPS; ++k) {
        unsigned long a = 2UL * k + 1UL;
        unsigned long b = a + 1UL;
        unsigned long d;
        if (a > ULONG_MAX / b) goto done;
        d = a * b;
        if (th_iv_mul(&tmp, &term, &r2, ctx) != 0 ||
            th_iv_set_ui(&denom, d, ctx) != 0 ||
            th_iv_div_positive(&next, &tmp, &denom, ctx) != 0 ||
            th_iv_neg(&tmp, &next, ctx) != 0 ||
            th_iv_copy(&next, &tmp, ctx) != 0)
            goto done;
        if (th_iv_abs_upper(&bound, &next, ctx) != 0) goto done;
        if (th_term_small(&bound, work, 24u)) {
            if (ppf_mul_2si(&bound, &bound, 1L,
                            PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
                th_iv_add_symmetric_tail(out, &sum, &bound, ctx) != 0)
                goto done;
            rc = 0;
            goto done;
        }
        if (th_iv_add(&tmp, &sum, &next, ctx) != 0 ||
            th_iv_copy(&sum, &tmp, ctx) != 0 ||
            th_iv_copy(&term, &next, ctx) != 0)
            goto done;
    }
done:
    th_iv_clear(&r2);
    th_iv_clear(&sum);
    th_iv_clear(&term);
    th_iv_clear(&next);
    th_iv_clear(&tmp);
    th_iv_clear(&denom);
    if (bound.limbs) ppf_clear(&bound);
    return rc;
}

static int th_sincos_reduced(th_iv_t *s, th_iv_t *c, const th_iv_t *r,
                             ppf_prec_t work, ppf_context_t *ctx) {
    if (ppf_fast_sincos_reduced_internal(&s->lo, &s->hi,
                                         &c->lo, &c->hi,
                                         &r->lo, &r->hi, work, ctx))
        return 0;
    if (th_sin_series(s, r, work, ctx) != 0) return -1;
    if (th_cos_series(c, r, work, ctx) != 0) return -1;
    return 0;
}

static int th_apply_quadrant(th_iv_t *sout, th_iv_t *cout,
                             const th_iv_t *s, const th_iv_t *c,
                             unsigned quadrant, ppf_context_t *ctx) {
    switch (quadrant & 3u) {
        case 0u:
            return th_iv_copy(sout, s, ctx) || th_iv_copy(cout, c, ctx) ? -1 : 0;
        case 1u:
            return th_iv_copy(sout, c, ctx) || th_iv_neg(cout, s, ctx) ? -1 : 0;
        case 2u:
            return th_iv_neg(sout, s, ctx) || th_iv_neg(cout, c, ctx) ? -1 : 0;
        default:
            return th_iv_neg(sout, c, ctx) || th_iv_copy(cout, s, ctx) ? -1 : 0;
    }
}

static int th_reduce_general(th_iv_t *r, unsigned *quadrant,
                             const ppf_t *x, ppf_prec_t work,
                             ppf_context_t *ctx) {
    th_iv_t xi, pi, halfpi, q, ni, prod;
    mpz_t nlo, nhi;
    int rc = -1;
    (void)work;
    memset(&xi, 0, sizeof(xi));
    memset(&pi, 0, sizeof(pi));
    memset(&halfpi, 0, sizeof(halfpi));
    memset(&q, 0, sizeof(q));
    memset(&ni, 0, sizeof(ni));
    memset(&prod, 0, sizeof(prod));
    mpz_inits(nlo, nhi, NULL);
    if (th_iv_init(&xi, r->lo.precision, ctx) != 0 ||
        th_iv_init(&pi, r->lo.precision, ctx) != 0 ||
        th_iv_init(&halfpi, r->lo.precision, ctx) != 0 ||
        th_iv_init(&q, r->lo.precision, ctx) != 0 ||
        th_iv_init(&ni, r->lo.precision, ctx) != 0 ||
        th_iv_init(&prod, r->lo.precision, ctx) != 0)
        goto done;
    if (th_iv_set_exact(&xi, x, ctx) != 0 ||
        th_iv_set_pi(&pi, ctx) != 0 ||
        th_iv_scale2(&halfpi, &pi, -1L, ctx) != 0 ||
        th_iv_div_positive(&q, &xi, &halfpi, ctx) != 0)
        goto done;
    if (ppf_get_z(nlo, &q.lo, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_get_z(nhi, &q.hi, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        mpz_cmp(nlo, nhi) != 0)
        goto done;
    if (ppf_set_z(&ni.lo, nlo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_z(&ni.hi, nlo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
        th_iv_mul(&prod, &ni, &halfpi, ctx) != 0 ||
        th_iv_sub(r, &xi, &prod, ctx) != 0)
        goto done;
    *quadrant = (unsigned)mpz_fdiv_ui(nlo, 4UL);
    rc = 0;
done:
    th_iv_clear(&xi);
    th_iv_clear(&pi);
    th_iv_clear(&halfpi);
    th_iv_clear(&q);
    th_iv_clear(&ni);
    th_iv_clear(&prod);
    mpz_clears(nlo, nhi, NULL);
    return rc;
}

static int th_round_mpq_nearest_even(mpz_ptr out, mpq_srcptr x) {
    mpz_t num, den, q, rem, twice;
    int sign, cmp;
    mpz_inits(num, den, q, rem, twice, NULL);
    sign = mpq_sgn(x) < 0 ? -1 : 1;
    mpz_abs(num, mpq_numref(x));
    mpz_set(den, mpq_denref(x));
    mpz_fdiv_qr(q, rem, num, den);
    mpz_mul_2exp(twice, rem, 1u);
    cmp = mpz_cmp(twice, den);
    if (cmp > 0 || (cmp == 0 && mpz_odd_p(q))) mpz_add_ui(q, q, 1UL);
    if (sign < 0) mpz_neg(q, q);
    mpz_set(out, q);
    mpz_clears(num, den, q, rem, twice, NULL);
    return 0;
}

/* Round the exact signed dyadic z*2^exponent to the nearest integer,
   with halfway cases to even, without constructing a rational object. */
static int th_round_dyadic_nearest_even(mpz_ptr out, mpz_srcptr z,
                                        ppf_exp_t exponent) {
    mpz_t a, q, rem;
    int sign;
    uint64_t shift;
    mpz_inits(a, q, rem, NULL);
    sign = mpz_sgn(z) < 0 ? -1 : 1;
    mpz_abs(a, z);
    if (exponent >= 0) {
        if ((uint64_t)exponent > (uint64_t)ULONG_MAX) goto fail;
        mpz_mul_2exp(q, a, (mp_bitcnt_t)(uint64_t)exponent);
    } else {
        shift = (uint64_t)(-(exponent + 1)) + 1u;
        if (shift > (uint64_t)ULONG_MAX) {
            mpz_set_ui(q, 0UL);
        } else {
            mpz_fdiv_q_2exp(q, a, (mp_bitcnt_t)shift);
            mpz_fdiv_r_2exp(rem, a, (mp_bitcnt_t)shift);
            if (shift != 0u && mpz_sgn(rem) != 0) {
                mp_bitcnt_t half = (mp_bitcnt_t)(shift - 1u);
                if (mpz_tstbit(rem, half)) {
                    mp_bitcnt_t low = mpz_scan1(rem, 0UL);
                    if (low < half || mpz_odd_p(q)) mpz_add_ui(q, q, 1UL);
                }
            }
        }
    }
    if (sign < 0) mpz_neg(q, q);
    mpz_set(out, q);
    mpz_clears(a, q, rem, NULL);
    return 0;
fail:
    mpz_clears(a, q, rem, NULL);
    return -1;
}

/* For pi-scaled functions, reduce x to x-n/2 directly in the exact dyadic
   representation.  This avoids multi-million-bit mpq canonicalization. */
static int th_scaled_delta_pi_dyadic(mpz_ptr delta_z,
                                     ppf_exp_t *delta_exp,
                                     mpz_ptr n, const ppf_t *x) {
    mpz_t z, a, b;
    ppf_exp_t e, common;
    uint64_t sx, sn;
    int rc = -1;
    mpz_inits(z, a, b, NULL);
    e = ppf_get_z_2exp(z, x);
    if (e == INT64_MAX) goto done;
    if (th_round_dyadic_nearest_even(n, z, e + 1) != 0) goto done;
    common = e < -1 ? e : -1;
    sx = (uint64_t)(e - common);
    sn = (uint64_t)(-1 - common);
    if (sx > (uint64_t)ULONG_MAX || sn > (uint64_t)ULONG_MAX) goto done;
    mpz_mul_2exp(a, z, (mp_bitcnt_t)sx);
    mpz_mul_2exp(b, n, (mp_bitcnt_t)sn);
    mpz_sub(delta_z, a, b);
    *delta_exp = common;
    rc = 0;
done:
    mpz_clears(z, a, b, NULL);
    return rc;
}

static int th_dyadic_equals_quarter(mpz_srcptr z, ppf_exp_t e, int sign) {
    mpz_t target;
    ppf_exp_t common = e < -2 ? e : -2;
    uint64_t sz = (uint64_t)(e - common);
    uint64_t st = (uint64_t)(-2 - common);
    int equal = 0;
    mpz_init(target);
    if (sz <= (uint64_t)ULONG_MAX && st <= (uint64_t)ULONG_MAX) {
        mpz_mul_2exp(target, z, (mp_bitcnt_t)sz);
        if (sign < 0) mpz_neg(target, target);
        {
            mpz_t one;
            mpz_init_set_ui(one, 1UL);
            mpz_mul_2exp(one, one, (mp_bitcnt_t)st);
            equal = mpz_cmp(target, one) == 0;
            mpz_clear(one);
        }
    }
    mpz_clear(target);
    return equal;
}

/* mode 0: angle = pi*x.  mode 1: angle = 2*pi*x/u. */
static int th_scaled_delta(mpq_ptr delta, mpz_ptr n, const ppf_t *x,
                           int mode, unsigned long u,
                           ppf_context_t *ctx) {
    mpq_t qx, qn, quotient;
    int rc = -1;
    if (mode == 1 && u == 0UL) return -1;
    mpq_inits(qx, qn, quotient, NULL);
    ppf_get_q(qx, x, ctx);
    if (mode == 0) {
        mpq_mul_2exp(quotient, qx, 1u); /* 2*x */
        th_round_mpq_nearest_even(n, quotient);
        mpq_set_z(qn, n);
        mpq_div_2exp(qn, qn, 1u);
        mpq_sub(delta, qx, qn);         /* x - n/2 */
    } else {
        mpq_mul_2exp(quotient, qx, 2u); /* 4*x/u */
        mpz_mul_ui(mpq_denref(quotient), mpq_denref(quotient), u);
        mpq_canonicalize(quotient);
        th_round_mpq_nearest_even(n, quotient);
        mpq_mul_2exp(delta, qx, 1u);    /* 2*x/u */
        mpz_mul_ui(mpq_denref(delta), mpq_denref(delta), u);
        mpq_canonicalize(delta);
        mpq_set_z(qn, n);
        mpq_div_2exp(qn, qn, 1u);
        mpq_sub(delta, delta, qn);
    }
    rc = 0;
    mpq_clears(qx, qn, quotient, NULL);
    return rc;
}


static int th_fixed_shift(ppf_exp_t exponent, ppf_prec_t scale,
                          int *negative, uint64_t *magnitude) {
#if defined(__SIZEOF_INT128__)
    __extension__ __int128 v = (__int128)exponent + (__int128)scale;
    if (v < 0) {
        __extension__ unsigned __int128 m = (unsigned __int128)(-v);
        if (m > UINT64_MAX) return -1;
        *negative = 1; *magnitude = (uint64_t)m;
    } else {
        __extension__ unsigned __int128 m = (unsigned __int128)v;
        if (m > UINT64_MAX) return -1;
        *negative = 0; *magnitude = (uint64_t)m;
    }
    return 0;
#else
    if (exponent >= 0) {
        uint64_t e = (uint64_t)exponent;
        if (e > UINT64_MAX - scale) return -1;
        *negative = 0; *magnitude = e + scale; return 0;
    }
    {
        uint64_t m = (uint64_t)(-(exponent + 1)) + 1u;
        if (m > scale) { *negative = 1; *magnitude = m - scale; }
        else { *negative = 0; *magnitude = scale - m; }
    }
    return 0;
#endif
}

static int th_fixed_dyadic(mpz_ptr out, mpz_srcptr value,
                           ppf_exp_t exponent, ppf_prec_t scale,
                           int upper) {
    int negative;
    uint64_t shift;
    if (th_fixed_shift(exponent, scale, &negative, &shift) != 0 ||
        shift > (uint64_t)ULONG_MAX) return -1;
    if (!negative) mpz_mul_2exp(out, value, (mp_bitcnt_t)shift);
    else if (upper) mpz_cdiv_q_2exp(out, value, (mp_bitcnt_t)shift);
    else mpz_fdiv_q_2exp(out, value, (mp_bitcnt_t)shift);
    return 0;
}

static int th_fixed_q(mpz_ptr lo, mpz_ptr hi, mpq_srcptr q,
                      ppf_prec_t scale, mpz_ptr scratch) {
    if (scale > (ppf_prec_t)ULONG_MAX) return -1;
    mpz_mul_2exp(scratch, mpq_numref(q), (mp_bitcnt_t)scale);
    mpz_fdiv_q(lo, scratch, mpq_denref(q));
    mpz_cdiv_q(hi, scratch, mpq_denref(q));
    return 0;
}

static int th_fixed_mul_interval(mpz_ptr lo, mpz_ptr hi,
                                 mpz_srcptr alo, mpz_srcptr ahi,
                                 mpz_srcptr blo, mpz_srcptr bhi,
                                 ppf_prec_t scale,
                                 mpz_ptr product, mpz_ptr qlo, mpz_ptr qhi) {
    /* The second interval is positive for the pi reduction.  Exploit the
       sign of the first interval so ordinary cases need two products rather
       than four. */
    if (mpz_sgn(alo) >= 0) {
        mpz_mul(product, alo, blo);
        mpz_fdiv_q_2exp(lo, product, (mp_bitcnt_t)scale);
        mpz_mul(product, ahi, bhi);
        mpz_cdiv_q_2exp(hi, product, (mp_bitcnt_t)scale);
        return 0;
    }
    if (mpz_sgn(ahi) <= 0) {
        mpz_mul(product, alo, bhi);
        mpz_fdiv_q_2exp(lo, product, (mp_bitcnt_t)scale);
        mpz_mul(product, ahi, blo);
        mpz_cdiv_q_2exp(hi, product, (mp_bitcnt_t)scale);
        return 0;
    }
    mpz_set_ui(lo, 0UL); mpz_set_ui(hi, 0UL);
    int first = 1;
    mpz_srcptr aa[2] = { alo, ahi };
    mpz_srcptr bb[2] = { blo, bhi };
    for (unsigned i = 0; i < 2; ++i) for (unsigned j = 0; j < 2; ++j) {
        mpz_mul(product, aa[i], bb[j]);
        mpz_fdiv_q_2exp(qlo, product, (mp_bitcnt_t)scale);
        mpz_cdiv_q_2exp(qhi, product, (mp_bitcnt_t)scale);
        if (first || mpz_cmp(qlo, lo) < 0) mpz_set(lo, qlo);
        if (first || mpz_cmp(qhi, hi) > 0) mpz_set(hi, qhi);
        first = 0;
    }
    return 0;
}

static int th_reduce_scaled_fixed(th_iv_t *r, unsigned *quadrant,
                            const ppf_t *x, int mode, unsigned long u,
                            ppf_context_t *ctx) {
    mpq_t delta;
    mpz_t n, delta_z, dlo, dhi, plo, phi, clo, chi, scratch, product, qlo, qhi;
    mpz_t pi_lo, pi_hi;
    ppf_exp_t pi_exp, delta_exp = 0, result_exp;
    ppf_prec_t scale;
    int rc = -1;
    mpq_init(delta);
    mpz_inits(n, delta_z, dlo, dhi, plo, phi, clo, chi, scratch, product,
              qlo, qhi, pi_lo, pi_hi, NULL);
    if (mode == 0) {
        if (th_scaled_delta_pi_dyadic(delta_z, &delta_exp, n, x) != 0)
            goto done;
    } else if (th_scaled_delta(delta, n, x, mode, u, ctx) != 0) {
        goto done;
    }
    if (r->lo.precision > UINT64_MAX - 128u) goto done;
    scale = r->lo.precision + 128u;
    if (scale > (ppf_prec_t)INT64_MAX || scale > (ppf_prec_t)ULONG_MAX)
        goto done;
    if ((mode == 0
           ? (th_fixed_dyadic(dlo, delta_z, delta_exp, scale, 0) != 0 ||
              th_fixed_dyadic(dhi, delta_z, delta_exp, scale, 1) != 0)
           : th_fixed_q(dlo, dhi, delta, scale, scratch) != 0) ||
        ppf_get_constant_interval(PPF_CONSTANT_PI, pi_lo, pi_hi, &pi_exp,
                                  scale + 32u, 1UL) != 0 ||
        th_fixed_dyadic(plo, pi_lo, pi_exp, scale, 0) != 0 ||
        th_fixed_dyadic(phi, pi_hi, pi_exp, scale, 1) != 0 ||
        th_fixed_mul_interval(clo, chi, dlo, dhi, plo, phi, scale,
                              product, qlo, qhi) != 0)
        goto done;
    result_exp = -(ppf_exp_t)scale;
    if (ppf_set_z_2exp(&r->lo, clo, result_exp, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_z_2exp(&r->hi, chi, result_exp, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        goto done;
    *quadrant = (unsigned)mpz_fdiv_ui(n, 4UL);
    rc = 0;
done:
    mpq_clear(delta);
    mpz_clears(n, delta_z, dlo, dhi, plo, phi, clo, chi, scratch, product,
               qlo, qhi, pi_lo, pi_hi, NULL);
    return rc;
}

static int th_reduce_scaled_reference(th_iv_t *r, unsigned *quadrant,
                                      const ppf_t *x, int mode,
                                      unsigned long u,
                                      ppf_context_t *ctx) {
    mpq_t delta;
    mpz_t n;
    th_iv_t d, pi;
    int rc = -1;
    mpq_init(delta);
    mpz_init(n);
    memset(&d, 0, sizeof(d));
    memset(&pi, 0, sizeof(pi));
    if (th_scaled_delta(delta, n, x, mode, u, ctx) != 0 ||
        th_iv_init(&d, r->lo.precision, ctx) != 0 ||
        th_iv_init(&pi, r->lo.precision, ctx) != 0 ||
        th_iv_set_q(&d, delta, ctx) != 0 ||
        th_iv_set_pi(&pi, ctx) != 0 ||
        th_iv_mul(r, &d, &pi, ctx) != 0)
        goto done;
    *quadrant = (unsigned)mpz_fdiv_ui(n, 4UL);
    rc = 0;
done:
    th_iv_clear(&d);
    th_iv_clear(&pi);
    mpq_clear(delta);
    mpz_clear(n);
    return rc;
}

static int th_reduce_scaled(th_iv_t *r, unsigned *quadrant,
                            const ppf_t *x, int mode, unsigned long u,
                            ppf_context_t *ctx) {
    /* The common-scale path wins while the certified pi interval and its
       product remain cache-sized.  Above the measured crossover the
       existing floating interval multiplication moves less memory. */
    if (mode == 0 || r->lo.precision <= PPF_TRIG_FIXED_SCALED_MAX_PREC) {
        int rc = th_reduce_scaled_fixed(r, quadrant, x, mode, u, ctx);
        if (rc == 0) return 0;
    }
    return th_reduce_scaled_reference(r, quadrant, x, mode, u, ctx);
}

typedef enum {
    TH_FN_SIN = 0,
    TH_FN_COS = 1,
    TH_FN_TAN = 2,
    TH_FN_SEC = 3,
    TH_FN_CSC = 4,
    TH_FN_COT = 5
} th_periodic_fn_t;

typedef struct {
    const ppf_t *x;
    th_periodic_fn_t fn;
    int scaled_mode; /* 0 ordinary, 1 pi-scaled, 2 unit-scaled */
    unsigned long u;
} th_periodic_arg_t;

static int th_periodic_builder(th_iv_t *out, ppf_prec_t work,
                               void *opaque, ppf_context_t *ctx) {
    th_periodic_arg_t *arg = (th_periodic_arg_t *)opaque;
    th_iv_t r, sr, cr, s, c, one;
    unsigned quadrant = 0;
    int rc = -1;
    memset(&r, 0, sizeof(r));
    memset(&sr, 0, sizeof(sr));
    memset(&cr, 0, sizeof(cr));
    memset(&s, 0, sizeof(s));
    memset(&c, 0, sizeof(c));
    memset(&one, 0, sizeof(one));
    if (th_iv_init(&r, work, ctx) != 0 ||
        th_iv_init(&sr, work, ctx) != 0 ||
        th_iv_init(&cr, work, ctx) != 0 ||
        th_iv_init(&s, work, ctx) != 0 ||
        th_iv_init(&c, work, ctx) != 0 ||
        th_iv_init(&one, work, ctx) != 0)
        goto done;
    if (arg->scaled_mode == 0) {
        /* A normalized value with exponent <= -1 satisfies |x| < 1/2,
           hence it is already inside the principal [-pi/4,pi/4] interval.
           Avoid constructing pi and dividing by pi/2 in this very common
           case; the fixed-point sine/cosine kernel can consume x directly. */
        if (arg->x->exponent <= -1) {
            if (th_iv_set_exact(&r, arg->x, ctx) != 0) goto done;
            quadrant = 0u;
        } else if (th_reduce_general(&r, &quadrant, arg->x, work, ctx) != 0) {
            goto done;
        }
    } else {
        int mode = arg->scaled_mode == 1 ? 0 : 1;
        if (th_reduce_scaled(&r, &quadrant, arg->x, mode, arg->u, ctx) != 0)
            goto done;
    }
    if (th_sincos_reduced(&sr, &cr, &r, work, ctx) != 0 ||
        th_apply_quadrant(&s, &c, &sr, &cr, quadrant, ctx) != 0)
        goto done;
    switch (arg->fn) {
        case TH_FN_SIN:
            rc = th_iv_copy(out, &s, ctx);
            break;
        case TH_FN_COS:
            rc = th_iv_copy(out, &c, ctx);
            break;
        case TH_FN_TAN:
            rc = th_iv_div_nonzero(out, &s, &c, ctx);
            break;
        case TH_FN_SEC:
            if (th_iv_set_ui(&one, 1UL, ctx) == 0)
                rc = th_iv_div_nonzero(out, &one, &c, ctx);
            break;
        case TH_FN_CSC:
            if (th_iv_set_ui(&one, 1UL, ctx) == 0)
                rc = th_iv_div_nonzero(out, &one, &s, ctx);
            break;
        case TH_FN_COT:
            rc = th_iv_div_nonzero(out, &c, &s, ctx);
            break;
    }
done:
    th_iv_clear(&r);
    th_iv_clear(&sr);
    th_iv_clear(&cr);
    th_iv_clear(&s);
    th_iv_clear(&c);
    th_iv_clear(&one);
    return rc;
}

typedef struct {
    const ppf_t *x;
} th_sincos_pair_arg_t;

static int th_sincos_pair_builder(th_iv_t *sout, th_iv_t *cout,
                                   ppf_prec_t work, void *opaque,
                                   ppf_context_t *ctx) {
    th_sincos_pair_arg_t *arg = (th_sincos_pair_arg_t *)opaque;
    th_iv_t r, sr, cr;
    unsigned quadrant = 0u;
    int rc = -1;
    memset(&r, 0, sizeof(r));
    memset(&sr, 0, sizeof(sr));
    memset(&cr, 0, sizeof(cr));
    if (th_iv_init(&r, work, ctx) != 0 ||
        th_iv_init(&sr, work, ctx) != 0 ||
        th_iv_init(&cr, work, ctx) != 0)
        goto done;
    if (arg->x->exponent <= -1) {
        if (th_iv_set_exact(&r, arg->x, ctx) != 0) goto done;
    } else if (th_reduce_general(&r, &quadrant, arg->x, work, ctx) != 0) {
        goto done;
    }
    if (th_sincos_reduced(&sr, &cr, &r, work, ctx) != 0 ||
        th_apply_quadrant(sout, cout, &sr, &cr, quadrant, ctx) != 0)
        goto done;
    rc = 0;
done:
    th_iv_clear(&r);
    th_iv_clear(&sr);
    th_iv_clear(&cr);
    return rc;
}

static int th_set_grid_value(ppf_t *rop, th_periodic_fn_t fn,
                             unsigned quadrant, int input_sign,
                             ppf_rnd_t rnd, ppf_context_t *ctx) {
    quadrant &= 3u;
    switch (fn) {
        case TH_FN_SIN:
            if (quadrant == 0u || quadrant == 2u) {
                ppf_set_zero(rop, input_sign);
                return 0;
            }
            return ppf_set_si(rop, quadrant == 1u ? 1L : -1L, rnd, ctx);
        case TH_FN_COS:
            if (quadrant == 1u || quadrant == 3u) {
                ppf_set_zero(rop, 1);
                return 0;
            }
            return ppf_set_si(rop, quadrant == 0u ? 1L : -1L, rnd, ctx);
        case TH_FN_TAN:
            if (quadrant == 1u || quadrant == 3u)
                return th_divzero_inf(rop, quadrant == 1u ? 1 : -1, ctx);
            ppf_set_zero(rop, quadrant == 0u ? input_sign : -input_sign);
            return 0;
        default:
            return th_error(rop, ctx);
    }
}

static int th_set_q_fraction(ppf_t *rop, long numerator,
                             unsigned long denominator,
                             ppf_rnd_t rnd, ppf_context_t *ctx);

/* Detect the rational values allowed by Niven's theorem on the
   exact unit-scaled grid.  For q=angle/pi=m/6, sin(q*pi) or cos(q*pi)
   can be 0, +/-1/2, or +/-1.  The irrational +/-sqrt(3)/2 cases are left
   to the certified interval evaluator. */
static int th_scaled_exact_sincos(ppf_t *rop, const ppf_t *x,
                                  th_periodic_fn_t fn, int scaled_mode,
                                  unsigned long u, ppf_rnd_t rnd,
                                  ppf_context_t *ctx, int *handled) {
    mpq_t q;
    unsigned long m;
    int value_num = 0, value_den = 1;
    *handled = 0;
    if (fn != TH_FN_SIN && fn != TH_FN_COS) return 0;
    if (scaled_mode == 0 || (scaled_mode == 2 && u == 0UL)) return 0;
    mpq_init(q);
    ppf_get_q(q, x, ctx);
    if (scaled_mode == 2) {
        mpz_mul_2exp(mpq_numref(q), mpq_numref(q), 1u);
        mpz_mul_ui(mpq_denref(q), mpq_denref(q), u);
        mpq_canonicalize(q);
    }
    mpz_mul_ui(mpq_numref(q), mpq_numref(q), 6UL);
    mpq_canonicalize(q);
    if (mpz_cmp_ui(mpq_denref(q), 1UL) != 0) {
        mpq_clear(q);
        return 0;
    }
    m = mpz_fdiv_ui(mpq_numref(q), 12UL);
    if (fn == TH_FN_SIN) {
        switch (m) {
            case 0u: case 6u:
                ppf_set_zero(rop, x->sign);
                *handled = 1;
                mpq_clear(q);
                return 0;
            case 1u: case 5u: value_num = 1; value_den = 2; break;
            case 3u: value_num = 1; break;
            case 7u: case 11u: value_num = -1; value_den = 2; break;
            case 9u: value_num = -1; break;
            default: mpq_clear(q); return 0;
        }
    } else {
        switch (m) {
            case 0u: value_num = 1; break;
            case 2u: case 10u: value_num = 1; value_den = 2; break;
            case 3u: case 9u:
                ppf_set_zero(rop, 1);
                *handled = 1;
                mpq_clear(q);
                return 0;
            case 4u: case 8u: value_num = -1; value_den = 2; break;
            case 6u: value_num = -1; break;
            default: mpq_clear(q); return 0;
        }
    }
    *handled = 1;
    mpq_clear(q);
    return th_set_q_fraction(rop, value_num, (unsigned long)value_den,
                             rnd, ctx);
}

static int th_periodic_dispatch(ppf_t *rop, const ppf_t *x,
                                th_periodic_fn_t fn, int scaled_mode,
                                unsigned long u, ppf_rnd_t rnd,
                                ppf_context_t *ctx) {
    th_periodic_arg_t arg;
    ppf_prec_t work;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !x || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(x)) return th_nan(rop, x->sign, ctx);
    if (ppf_inf_p(x)) return th_nan(rop, 1, ctx);
    if (scaled_mode == 2 && u == 0UL) return th_nan(rop, 1, ctx);
    if (ppf_zero_p(x)) {
        if (fn == TH_FN_COS || fn == TH_FN_SEC)
            return ppf_set_ui(rop, 1UL, rnd, ctx);
        if (fn == TH_FN_CSC || fn == TH_FN_COT)
            return th_divzero_inf(rop, x->sign, ctx);
        ppf_set_zero(rop, x->sign);
        return 0;
    }
    if (scaled_mode != 0 && ppf_min_prec(x) <= 256u &&
        (fn == TH_FN_SIN || fn == TH_FN_COS)) {
        int handled = 0;
        int erc = th_scaled_exact_sincos(rop, x, fn, scaled_mode, u,
                                         rnd, ctx, &handled);
        if (erc == PPF_TERNARY_ERROR) return erc;
        if (handled) return erc;
    }
    if (scaled_mode != 0 && fn <= TH_FN_TAN) {
        if (scaled_mode == 1) {
            mpz_t delta_z, n;
            ppf_exp_t delta_exp = 0;
            int exact_grid = 0, delta_sign = 0;
            mpz_inits(delta_z, n, NULL);
            if (th_scaled_delta_pi_dyadic(delta_z, &delta_exp, n, x) == 0) {
                exact_grid = mpz_sgn(delta_z) == 0;
                if (fn == TH_FN_TAN && !exact_grid) {
                    if (th_dyadic_equals_quarter(delta_z, delta_exp, 1))
                        delta_sign = 1;
                    else if (th_dyadic_equals_quarter(delta_z, delta_exp, -1))
                        delta_sign = -1;
                }
            }
            if (exact_grid) {
                unsigned quadrant = (unsigned)mpz_fdiv_ui(n, 4UL);
                int rc = th_set_grid_value(rop, fn, quadrant, x->sign,
                                           rnd, ctx);
                mpz_clears(delta_z, n, NULL);
                return rc;
            }
            if (delta_sign != 0) {
                unsigned quadrant = (unsigned)mpz_fdiv_ui(n, 4UL);
                int value = delta_sign * ((quadrant & 1u) ? -1 : 1);
                int rc = ppf_set_si(rop, value, rnd, ctx);
                mpz_clears(delta_z, n, NULL);
                return rc;
            }
            mpz_clears(delta_z, n, NULL);
        } else {
            mpq_t delta;
            mpz_t n;
            int exact_grid;
            mpq_init(delta);
            mpz_init(n);
            exact_grid = th_scaled_delta(delta, n, x, 1, u, ctx) == 0 &&
                         mpq_sgn(delta) == 0;
            if (exact_grid) {
                unsigned quadrant = (unsigned)mpz_fdiv_ui(n, 4UL);
                int rc = th_set_grid_value(rop, fn, quadrant, x->sign,
                                           rnd, ctx);
                mpq_clear(delta); mpz_clear(n); return rc;
            }
            if (fn == TH_FN_TAN) {
                mpq_t quarter, neg_quarter;
                int delta_sign = 0;
                mpq_inits(quarter, neg_quarter, NULL);
                mpq_set_ui(quarter, 1UL, 4UL);
                mpq_neg(neg_quarter, quarter);
                if (mpq_cmp(delta, quarter) == 0) delta_sign = 1;
                else if (mpq_cmp(delta, neg_quarter) == 0) delta_sign = -1;
                if (delta_sign != 0) {
                    unsigned quadrant = (unsigned)mpz_fdiv_ui(n, 4UL);
                    int value = delta_sign * ((quadrant & 1u) ? -1 : 1);
                    int rc = ppf_set_si(rop, value, rnd, ctx);
                    mpq_clears(quarter, neg_quarter, NULL);
                    mpq_clear(delta); mpz_clear(n); return rc;
                }
                mpq_clears(quarter, neg_quarter, NULL);
            }
            mpq_clear(delta); mpz_clear(n);
        }
    }
    arg.x = x;
    arg.fn = fn;
    arg.scaled_mode = scaled_mode;
    arg.u = u;
    work = rop->precision + PPF_TRIG_INITIAL_GUARD;
    if (scaled_mode == 0 && x->exponent > 0) {
        uint64_t extra = (uint64_t)x->exponent;
        if (extra > PPF_TRIG_MAX_REDUCTION_EXTRA)
            return th_error(rop, ctx);
        if (work > UINT64_MAX - extra - 32u)
            return th_error(rop, ctx);
        work += extra + 32u;
    }
    return th_evaluate_start(rop, rnd, ctx, th_periodic_builder, &arg, work);
}

int ppf_sin(ppf_t *rop, const ppf_t *op,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_SIN, 0, 0UL, rnd, ctx);
}

int ppf_cos(ppf_t *rop, const ppf_t *op,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_COS, 0, 0UL, rnd, ctx);
}

int ppf_tan(ppf_t *rop, const ppf_t *op,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_TAN, 0, 0UL, rnd, ctx);
}

int ppf_sec(ppf_t *rop, const ppf_t *op,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_SEC, 0, 0UL, rnd, ctx);
}

int ppf_csc(ppf_t *rop, const ppf_t *op,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_CSC, 0, 0UL, rnd, ctx);
}

int ppf_cot(ppf_t *rop, const ppf_t *op,
            ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_COT, 0, 0UL, rnd, ctx);
}

int ppf_sinpi(ppf_t *rop, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_SIN, 1, 0UL, rnd, ctx);
}

int ppf_cospi(ppf_t *rop, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_COS, 1, 0UL, rnd, ctx);
}

int ppf_tanpi(ppf_t *rop, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_TAN, 1, 0UL, rnd, ctx);
}

int ppf_sinu(ppf_t *rop, const ppf_t *op, unsigned long u,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_SIN, 2, u, rnd, ctx);
}

int ppf_cosu(ppf_t *rop, const ppf_t *op, unsigned long u,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_COS, 2, u, rnd, ctx);
}

int ppf_tanu(ppf_t *rop, const ppf_t *op, unsigned long u,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_periodic_dispatch(rop, op, TH_FN_TAN, 2, u, rnd, ctx);
}

static int th_pair_code(int s, int c) {
    int sb = s > 0 ? 1 : (s < 0 ? 2 : 0);
    int cb = c > 0 ? 1 : (c < 0 ? 2 : 0);
    return sb + 4 * cb;
}

int ppf_sin_cos(ppf_t *sop, ppf_t *cop, const ppf_t *op,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t input;
    th_sincos_pair_arg_t arg;
    ppf_prec_t work, need;
    int ts = 0, tc = 0;
    ctx = ppf_resolve_context(ctx);
    if (!sop || !cop || !op || !ctx || sop == cop)
        return PPF_TERNARY_ERROR;
    memset(&input, 0, sizeof(input));
    if (op == sop || op == cop) {
        if (ppf_init2_ctx(&input, op->precision, ctx) != 0 ||
            ppf_set(&input, op, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
            goto error;
        op = &input;
    }
    /* Singular and exact-zero cases are uncommon and already have exact,
       branch-specific semantics in the scalar dispatchers. */
    if (!ppf_regular_p(op) || ppf_zero_p(op)) {
        ppf_t s, c;
        memset(&s, 0, sizeof(s)); memset(&c, 0, sizeof(c));
        if (ppf_init2_ctx(&s, sop->precision, ctx) != 0 ||
            ppf_init2_ctx(&c, cop->precision, ctx) != 0) {
            if (s.limbs) ppf_clear(&s);
            if (c.limbs) ppf_clear(&c);
            goto error;
        }
        ts = ppf_sin(&s, op, rnd, ctx);
        tc = ppf_cos(&c, op, rnd, ctx);
        if (ts == PPF_TERNARY_ERROR || tc == PPF_TERNARY_ERROR) {
            ppf_clear(&s); ppf_clear(&c); goto error;
        }
        ppf_swap(sop, &s); ppf_swap(cop, &c);
        ppf_clear(&s); ppf_clear(&c);
        if (input.limbs) ppf_clear(&input);
        return th_pair_code(ts, tc);
    }
    need = sop->precision > cop->precision ? sop->precision : cop->precision;
    if (need > UINT64_MAX - PPF_TRIG_INITIAL_GUARD) goto error;
    work = need + PPF_TRIG_INITIAL_GUARD;
    if (op->exponent > 0) {
        uint64_t extra = (uint64_t)op->exponent;
        if (extra > PPF_TRIG_MAX_REDUCTION_EXTRA ||
            work > UINT64_MAX - extra - 32u)
            goto error;
        work += extra + 32u;
    }
    arg.x = op;
    if (th_evaluate_pair_start(sop, cop, rnd, ctx,
                               th_sincos_pair_builder, &arg, work,
                               &ts, &tc) != 0)
        goto error;
    if (input.limbs) ppf_clear(&input);
    return th_pair_code(ts, tc);
error:
    if (input.limbs) ppf_clear(&input);
    ppf_set_nan(sop, 1);
    ppf_set_nan(cop, 1);
    ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

static int th_atan_series(th_iv_t *out, const th_iv_t *t,
                          ppf_prec_t work, ppf_context_t *ctx) {
    if (ppf_fast_atan_reduced_internal(&out->lo, &out->hi,
                                       &t->lo, &t->hi, work, ctx))
        return 0;
    th_iv_t t2, sum, term, next, tmp, scalar, denom;
    ppf_t bound;
    int rc = -1;
    memset(&t2, 0, sizeof(t2));
    memset(&sum, 0, sizeof(sum));
    memset(&term, 0, sizeof(term));
    memset(&next, 0, sizeof(next));
    memset(&tmp, 0, sizeof(tmp));
    memset(&scalar, 0, sizeof(scalar));
    memset(&denom, 0, sizeof(denom));
    memset(&bound, 0, sizeof(bound));
    if (th_iv_init(&t2, work, ctx) != 0 ||
        th_iv_init(&sum, work, ctx) != 0 ||
        th_iv_init(&term, work, ctx) != 0 ||
        th_iv_init(&next, work, ctx) != 0 ||
        th_iv_init(&tmp, work, ctx) != 0 ||
        th_iv_init(&scalar, work, ctx) != 0 ||
        th_iv_init(&denom, work, ctx) != 0 ||
        ppf_init2_ctx(&bound, work, ctx) != 0)
        goto done;
    if (th_iv_square(&t2, t, ctx) != 0 ||
        th_iv_copy(&sum, t, ctx) != 0 ||
        th_iv_copy(&term, t, ctx) != 0)
        goto done;
    for (unsigned long k = 0; k < PPF_TRIG_MAX_STEPS; ++k) {
        unsigned long num = 2UL * k + 1UL;
        unsigned long den = num + 2UL;
        if (th_iv_mul(&tmp, &term, &t2, ctx) != 0 ||
            th_iv_set_ui(&scalar, num, ctx) != 0 ||
            th_iv_mul(&next, &tmp, &scalar, ctx) != 0 ||
            th_iv_set_ui(&denom, den, ctx) != 0 ||
            th_iv_div_positive(&tmp, &next, &denom, ctx) != 0 ||
            th_iv_neg(&next, &tmp, ctx) != 0)
            goto done;
        if (th_iv_abs_upper(&bound, &next, ctx) != 0) goto done;
        if (th_term_small(&bound, work, 24u)) {
            if (ppf_mul_2si(&bound, &bound, 1L,
                            PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
                th_iv_add_symmetric_tail(out, &sum, &bound, ctx) != 0)
                goto done;
            rc = 0;
            goto done;
        }
        if (th_iv_add(&tmp, &sum, &next, ctx) != 0 ||
            th_iv_copy(&sum, &tmp, ctx) != 0 ||
            th_iv_copy(&term, &next, ctx) != 0)
            goto done;
    }
done:
    th_iv_clear(&t2);
    th_iv_clear(&sum);
    th_iv_clear(&term);
    th_iv_clear(&next);
    th_iv_clear(&tmp);
    th_iv_clear(&scalar);
    th_iv_clear(&denom);
    if (bound.limbs) ppf_clear(&bound);
    return rc;
}

/* atan(x) for an interval known to be nonnegative. */
static int th_atan_positive_iv(th_iv_t *out, const th_iv_t *x,
                               ppf_prec_t work, ppf_context_t *ctx) {
    th_iv_t one, x2, root, denom, current, next, a, pi, halfpi, tmp;
    int use_reciprocal;
    unsigned reductions;
    int rc = -1;
    memset(&one, 0, sizeof(one));
    memset(&x2, 0, sizeof(x2));
    memset(&root, 0, sizeof(root));
    memset(&denom, 0, sizeof(denom));
    memset(&current, 0, sizeof(current));
    memset(&next, 0, sizeof(next));
    memset(&a, 0, sizeof(a));
    memset(&pi, 0, sizeof(pi));
    memset(&halfpi, 0, sizeof(halfpi));
    memset(&tmp, 0, sizeof(tmp));
    if (ppf_sgn(&x->lo, ctx) < 0) return -1;
    if (ppf_fast_atan_positive_internal(&out->lo, &out->hi,
                                        &x->lo, &x->hi, work, ctx))
        return 0;
    if (th_iv_init(&one, work, ctx) != 0 ||
        th_iv_init(&x2, work, ctx) != 0 ||
        th_iv_init(&root, work, ctx) != 0 ||
        th_iv_init(&denom, work, ctx) != 0 ||
        th_iv_init(&current, work, ctx) != 0 ||
        th_iv_init(&next, work, ctx) != 0 ||
        th_iv_init(&a, work, ctx) != 0 ||
        th_iv_init(&pi, work, ctx) != 0 ||
        th_iv_init(&halfpi, work, ctx) != 0 ||
        th_iv_init(&tmp, work, ctx) != 0)
        goto done;
    use_reciprocal = ppf_cmp_ui(&x->lo, 1UL, ctx) > 0;
    if (use_reciprocal) {
        if (th_iv_set_ui(&one, 1UL, ctx) != 0 ||
            th_iv_div_positive(&current, &one, x, ctx) != 0)
            goto done;
    } else if (th_iv_copy(&current, x, ctx) != 0) goto done;

    /* Each half-angle step roughly squares the convergence ratio of the
       alternating atan series.  A slowly growing count is faster than
       thousands of full-width interval recurrence steps. */
    reductions = 3u;
    {
        uint64_t v = work > 1u ? work : 1u;
        while (v > 1u) { v >>= 1; ++reductions; }
        reductions = reductions / 2u + 2u;
        if (reductions < 4u) reductions = 4u;
        if (reductions > 24u) reductions = 24u;
    }
    if (th_iv_set_ui(&one, 1UL, ctx) != 0) goto done;
    for (unsigned i = 0; i < reductions; ++i) {
        if (th_iv_square(&x2, &current, ctx) != 0 ||
            th_iv_add(&denom, &one, &x2, ctx) != 0 ||
            th_iv_sqrt(&root, &denom, ctx) != 0 ||
            th_iv_add(&denom, &one, &root, ctx) != 0 ||
            th_iv_div_positive(&next, &current, &denom, ctx) != 0)
            goto done;
        ppf_swap(&current.lo, &next.lo);
        ppf_swap(&current.hi, &next.hi);
    }
    if (th_atan_series(&a, &current, work, ctx) != 0 ||
        th_iv_scale2(&tmp, &a, (long)reductions, ctx) != 0)
        goto done;
    if (use_reciprocal) {
        if (th_iv_set_pi(&pi, ctx) != 0 ||
            th_iv_scale2(&halfpi, &pi, -1L, ctx) != 0 ||
            th_iv_sub(out, &halfpi, &tmp, ctx) != 0)
            goto done;
    } else if (th_iv_copy(out, &tmp, ctx) != 0) goto done;
    rc = 0;
done:
    th_iv_clear(&one);
    th_iv_clear(&x2);
    th_iv_clear(&root);
    th_iv_clear(&denom);
    th_iv_clear(&current);
    th_iv_clear(&next);
    th_iv_clear(&a);
    th_iv_clear(&pi);
    th_iv_clear(&halfpi);
    th_iv_clear(&tmp);
    return rc;
}

static int th_atan_iv(th_iv_t *out, const th_iv_t *x,
                      ppf_prec_t work, ppf_context_t *ctx) {
    int slo = ppf_sgn(&x->lo, ctx);
    int shi = ppf_sgn(&x->hi, ctx);
    if (slo >= 0) return th_atan_positive_iv(out, x, work, ctx);
    if (shi <= 0) {
        th_iv_t ax, a;
        int rc = -1;
        memset(&ax, 0, sizeof(ax));
        memset(&a, 0, sizeof(a));
        if (th_iv_init(&ax, work, ctx) != 0 ||
            th_iv_init(&a, work, ctx) != 0)
            goto done;
        if (th_iv_neg(&ax, x, ctx) != 0 ||
            th_atan_positive_iv(&a, &ax, work, ctx) != 0 ||
            th_iv_neg(out, &a, ctx) != 0)
            goto done;
        rc = 0;
done:
        th_iv_clear(&ax);
        th_iv_clear(&a);
        return rc;
    }
    /* atan is monotone; endpoint evaluation resolves a sign-crossing input. */
    {
        th_iv_t loarg, hiarg, lores, hires;
        int rc = -1;
        memset(&loarg, 0, sizeof(loarg));
        memset(&hiarg, 0, sizeof(hiarg));
        memset(&lores, 0, sizeof(lores));
        memset(&hires, 0, sizeof(hires));
        if (th_iv_init(&loarg, work, ctx) != 0 ||
            th_iv_init(&hiarg, work, ctx) != 0 ||
            th_iv_init(&lores, work, ctx) != 0 ||
            th_iv_init(&hires, work, ctx) != 0)
            goto cross_done;
        if (ppf_set(&loarg.lo, &x->lo, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_set(&loarg.hi, &x->lo, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_set(&hiarg.lo, &x->hi, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_set(&hiarg.hi, &x->hi, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            th_atan_iv(&lores, &loarg, work, ctx) != 0 ||
            th_atan_iv(&hires, &hiarg, work, ctx) != 0 ||
            ppf_set(&out->lo, &lores.lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_set(&out->hi, &hires.hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            goto cross_done;
        rc = 0;
cross_done:
        th_iv_clear(&loarg);
        th_iv_clear(&hiarg);
        th_iv_clear(&lores);
        th_iv_clear(&hires);
        return rc;
    }
}

static int th_asin_iv(th_iv_t *out, const th_iv_t *x,
                      ppf_prec_t work, ppf_context_t *ctx) {
    th_iv_t ax, x2, one, rad, root, ratio, angle;
    int neg = ppf_sgn(&x->hi, ctx) <= 0;
    int rc = -1;
    memset(&ax, 0, sizeof(ax));
    memset(&x2, 0, sizeof(x2));
    memset(&one, 0, sizeof(one));
    memset(&rad, 0, sizeof(rad));
    memset(&root, 0, sizeof(root));
    memset(&ratio, 0, sizeof(ratio));
    memset(&angle, 0, sizeof(angle));
    if (th_iv_init(&ax, work, ctx) != 0 ||
        th_iv_init(&x2, work, ctx) != 0 ||
        th_iv_init(&one, work, ctx) != 0 ||
        th_iv_init(&rad, work, ctx) != 0 ||
        th_iv_init(&root, work, ctx) != 0 ||
        th_iv_init(&ratio, work, ctx) != 0 ||
        th_iv_init(&angle, work, ctx) != 0)
        goto done;
    if (neg) {
        if (th_iv_neg(&ax, x, ctx) != 0) goto done;
    } else {
        if (th_iv_copy(&ax, x, ctx) != 0) goto done;
    }
    if (th_iv_square(&x2, &ax, ctx) != 0 ||
        th_iv_set_ui(&one, 1UL, ctx) != 0 ||
        th_iv_sub(&rad, &one, &x2, ctx) != 0 ||
        th_iv_sqrt(&root, &rad, ctx) != 0 ||
        th_iv_div_positive(&ratio, &ax, &root, ctx) != 0 ||
        th_atan_positive_iv(&angle, &ratio, work, ctx) != 0)
        goto done;
    if (neg) rc = th_iv_neg(out, &angle, ctx);
    else rc = th_iv_copy(out, &angle, ctx);
done:
    th_iv_clear(&ax);
    th_iv_clear(&x2);
    th_iv_clear(&one);
    th_iv_clear(&rad);
    th_iv_clear(&root);
    th_iv_clear(&ratio);
    th_iv_clear(&angle);
    return rc;
}

typedef enum { TH_INV_ASIN = 0, TH_INV_ACOS = 1, TH_INV_ATAN = 2 } th_inv_fn_t;
typedef struct { const ppf_t *x; th_inv_fn_t fn; } th_inv_arg_t;

static int th_inverse_builder(th_iv_t *out, ppf_prec_t work,
                              void *opaque, ppf_context_t *ctx) {
    th_inv_arg_t *arg = (th_inv_arg_t *)opaque;
    th_iv_t x, angle, pi, halfpi;
    int rc = -1;
    memset(&x, 0, sizeof(x));
    memset(&angle, 0, sizeof(angle));
    memset(&pi, 0, sizeof(pi));
    memset(&halfpi, 0, sizeof(halfpi));
    if (th_iv_init(&x, work, ctx) != 0 ||
        th_iv_init(&angle, work, ctx) != 0 ||
        th_iv_init(&pi, work, ctx) != 0 ||
        th_iv_init(&halfpi, work, ctx) != 0)
        goto done;
    if (th_iv_set_exact(&x, arg->x, ctx) != 0) goto done;
    if (arg->fn == TH_INV_ATAN) {
        rc = th_atan_iv(out, &x, work, ctx);
        goto done;
    }
    if (th_asin_iv(&angle, &x, work, ctx) != 0) goto done;
    if (arg->fn == TH_INV_ASIN) {
        rc = th_iv_copy(out, &angle, ctx);
    } else {
        if (th_iv_set_pi(&pi, ctx) != 0 ||
            th_iv_scale2(&halfpi, &pi, -1L, ctx) != 0 ||
            th_iv_sub(out, &halfpi, &angle, ctx) != 0)
            goto done;
        rc = 0;
    }
done:
    th_iv_clear(&x);
    th_iv_clear(&angle);
    th_iv_clear(&pi);
    th_iv_clear(&halfpi);
    return rc;
}

static int th_set_pi_fraction(ppf_t *rop, long numerator,
                              unsigned long denominator,
                              ppf_rnd_t rnd, ppf_context_t *ctx);

int ppf_atan(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    th_inv_arg_t arg = { op, TH_INV_ATAN };
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op)) return th_nan(rop, op->sign, ctx);
    if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
    if (ppf_inf_p(op)) return th_set_pi_fraction(rop, op->sign, 2UL, rnd, ctx);
    {
        int fast_ternary = 0;
        if (ppf_fast_atan_internal(rop, op, rnd, ctx, &fast_ternary))
            return fast_ternary;
    }
    return th_evaluate(rop, rnd, ctx, th_inverse_builder, &arg);
}

int ppf_asin(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    th_inv_arg_t arg = { op, TH_INV_ASIN };
    int cmp;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op) || ppf_inf_p(op)) return th_nan(rop, op->sign, ctx);
    if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
    cmp = ppf_cmpabs_ui(op, 1UL, ctx);
    if (cmp > 0) return th_nan(rop, 1, ctx);
    if (cmp == 0) return th_set_pi_fraction(rop, op->sign, 2UL, rnd, ctx);
    return th_evaluate(rop, rnd, ctx, th_inverse_builder, &arg);
}

int ppf_acos(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    th_inv_arg_t arg = { op, TH_INV_ACOS };
    int cmp;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op) || ppf_inf_p(op)) return th_nan(rop, op->sign, ctx);
    cmp = ppf_cmpabs_ui(op, 1UL, ctx);
    if (cmp > 0) return th_nan(rop, 1, ctx);
    if (ppf_cmp_ui(op, 1UL, ctx) == 0) { ppf_set_zero(rop, 1); return 0; }
    if (ppf_cmp_si(op, -1L, ctx) == 0)
        return ppf_const_pi(rop, rnd, ctx);
    if (ppf_zero_p(op)) return th_set_pi_fraction(rop, 1, 2UL, rnd, ctx);
    return th_evaluate(rop, rnd, ctx, th_inverse_builder, &arg);
}

/* ---------- Scaled inverse angles and atan2 ---------- */

typedef struct {
    long numerator;
    unsigned long denominator;
} th_pi_fraction_arg_t;

static int th_pi_fraction_builder(th_iv_t *out, ppf_prec_t work,
                                  void *opaque, ppf_context_t *ctx) {
    th_pi_fraction_arg_t *arg = (th_pi_fraction_arg_t *)opaque;
    th_iv_t pi, scale;
    mpq_t q;
    int rc = -1;
    (void)work;
    memset(&pi, 0, sizeof(pi));
    memset(&scale, 0, sizeof(scale));
    mpq_init(q);
    if (arg->denominator == 0UL ||
        th_iv_init(&pi, out->lo.precision, ctx) != 0 ||
        th_iv_init(&scale, out->lo.precision, ctx) != 0)
        goto done;
    mpq_set_si(q, arg->numerator, arg->denominator);
    mpq_canonicalize(q);
    if (th_iv_set_pi(&pi, ctx) != 0 ||
        th_iv_set_q(&scale, q, ctx) != 0 ||
        th_iv_mul(out, &pi, &scale, ctx) != 0)
        goto done;
    rc = 0;
done:
    th_iv_clear(&pi);
    th_iv_clear(&scale);
    mpq_clear(q);
    return rc;
}

static int th_set_pi_fraction(ppf_t *rop, long numerator,
                              unsigned long denominator,
                              ppf_rnd_t rnd, ppf_context_t *ctx) {
    th_pi_fraction_arg_t arg;
    if (denominator == 0UL) return th_error(rop, ctx);
    if (numerator == 0L) {
        ppf_set_zero(rop, numerator < 0 ? -1 : 1);
        return 0;
    }
    arg.numerator = numerator;
    arg.denominator = denominator;
    return th_evaluate(rop, rnd, ctx, th_pi_fraction_builder, &arg);
}

static int th_set_q_fraction(ppf_t *rop, long numerator,
                             unsigned long denominator,
                             ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpq_t q;
    int rc;
    if (!rop || !ctx || denominator == 0UL) return th_error(rop, ctx);
    mpq_init(q);
    mpq_set_si(q, numerator, denominator);
    mpq_canonicalize(q);
    rc = ppf_set_q(rop, q, rnd, ctx);
    mpq_clear(q);
    return rc;
}

static int th_set_signed_ui_fraction(ppf_t *rop, int sign,
                                     unsigned long numerator,
                                     unsigned long denominator,
                                     ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpq_t q;
    int rc;
    if (!rop || !ctx || denominator == 0UL) return th_error(rop, ctx);
    mpq_init(q);
    mpq_set_ui(q, numerator, denominator);
    if (sign < 0) mpz_neg(mpq_numref(q), mpq_numref(q));
    mpq_canonicalize(q);
    rc = ppf_set_q(rop, q, rnd, ctx);
    mpq_clear(q);
    return rc;
}

typedef enum {
    TH_SCALE_NONE = 0,
    TH_SCALE_PI = 1,
    TH_SCALE_UNIT = 2
} th_inverse_scale_t;

typedef struct {
    const ppf_t *x;
    th_inv_fn_t fn;
    th_inverse_scale_t scale;
    unsigned long u;
} th_scaled_inverse_arg_t;

static int th_scaled_inverse_builder(th_iv_t *out, ppf_prec_t work,
                                     void *opaque, ppf_context_t *ctx) {
    th_scaled_inverse_arg_t *arg = (th_scaled_inverse_arg_t *)opaque;
    th_inv_arg_t base_arg;
    th_iv_t angle, pi, scale, denom, tmp;
    mpq_t q;
    int rc = -1;
    memset(&angle, 0, sizeof(angle));
    memset(&pi, 0, sizeof(pi));
    memset(&scale, 0, sizeof(scale));
    memset(&denom, 0, sizeof(denom));
    memset(&tmp, 0, sizeof(tmp));
    mpq_init(q);
    if (th_iv_init(&angle, work, ctx) != 0 ||
        th_iv_init(&pi, work, ctx) != 0 ||
        th_iv_init(&scale, work, ctx) != 0 ||
        th_iv_init(&denom, work, ctx) != 0 ||
        th_iv_init(&tmp, work, ctx) != 0)
        goto done;
    base_arg.x = arg->x;
    base_arg.fn = arg->fn;
    if (th_inverse_builder(&angle, work, &base_arg, ctx) != 0)
        goto done;
    if (arg->scale == TH_SCALE_NONE) {
        rc = th_iv_copy(out, &angle, ctx);
        goto done;
    }
    if (th_iv_set_pi(&pi, ctx) != 0) goto done;
    if (arg->scale == TH_SCALE_PI) {
        rc = th_iv_div_positive(out, &angle, &pi, ctx);
        goto done;
    }
    if (arg->u == 0UL) {
        if (th_iv_set_ui(out, 0UL, ctx) != 0) goto done;
        /* The sign is restored by the exact special path in public calls. */
        rc = 0;
        goto done;
    }
    mpq_set_ui(q, arg->u, 2UL);
    mpq_canonicalize(q);
    if (th_iv_set_q(&scale, q, ctx) != 0 ||
        th_iv_mul(&tmp, &angle, &scale, ctx) != 0 ||
        th_iv_div_positive(out, &tmp, &pi, ctx) != 0)
        goto done;
    rc = 0;
done:
    th_iv_clear(&angle);
    th_iv_clear(&pi);
    th_iv_clear(&scale);
    th_iv_clear(&denom);
    th_iv_clear(&tmp);
    mpq_clear(q);
    return rc;
}

static int th_scaled_inverse_dispatch(ppf_t *rop, const ppf_t *op,
                                      th_inv_fn_t fn,
                                      th_inverse_scale_t scale,
                                      unsigned long u,
                                      ppf_rnd_t rnd, ppf_context_t *ctx) {
    th_scaled_inverse_arg_t arg;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (scale == TH_SCALE_UNIT && u == 0UL) {
        ppf_set_zero(rop, op->sign);
        return 0;
    }
    arg.x = op;
    arg.fn = fn;
    arg.scale = scale;
    arg.u = u;
    return th_evaluate(rop, rnd, ctx, th_scaled_inverse_builder, &arg);
}

int ppf_asinpi(ppf_t *rop, const ppf_t *op,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    int cmp;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op) || ppf_inf_p(op)) return th_nan(rop, op->sign, ctx);
    if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
    cmp = ppf_cmpabs_ui(op, 1UL, ctx);
    if (cmp > 0) return th_nan(rop, 1, ctx);
    if (cmp == 0) return th_set_q_fraction(rop, op->sign, 2UL, rnd, ctx);
    return th_scaled_inverse_dispatch(rop, op, TH_INV_ASIN,
                                      TH_SCALE_PI, 0UL, rnd, ctx);
}

int ppf_acospi(ppf_t *rop, const ppf_t *op,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    int cmp;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op) || ppf_inf_p(op)) return th_nan(rop, op->sign, ctx);
    cmp = ppf_cmpabs_ui(op, 1UL, ctx);
    if (cmp > 0) return th_nan(rop, 1, ctx);
    if (ppf_cmp_ui(op, 1UL, ctx) == 0) { ppf_set_zero(rop, 1); return 0; }
    if (ppf_cmp_si(op, -1L, ctx) == 0) return ppf_set_ui(rop, 1UL, rnd, ctx);
    if (ppf_zero_p(op)) return th_set_q_fraction(rop, 1L, 2UL, rnd, ctx);
    return th_scaled_inverse_dispatch(rop, op, TH_INV_ACOS,
                                      TH_SCALE_PI, 0UL, rnd, ctx);
}

int ppf_atanpi(ppf_t *rop, const ppf_t *op,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op)) return th_nan(rop, op->sign, ctx);
    if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
    if (ppf_inf_p(op)) return th_set_q_fraction(rop, op->sign, 2UL, rnd, ctx);
    if (ppf_cmpabs_ui(op, 1UL, ctx) == 0)
        return th_set_q_fraction(rop, op->sign, 4UL, rnd, ctx);
    return th_scaled_inverse_dispatch(rop, op, TH_INV_ATAN,
                                      TH_SCALE_PI, 0UL, rnd, ctx);
}

int ppf_asinu(ppf_t *rop, const ppf_t *op, unsigned long u,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    int cmp;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op) || ppf_inf_p(op)) return th_nan(rop, op->sign, ctx);
    if (u == 0UL) {
        ppf_set_zero(rop, ppf_zero_p(op) ? op->sign : 1);
        return 0;
    }
    if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
    cmp = ppf_cmpabs_ui(op, 1UL, ctx);
    if (cmp > 0) return th_nan(rop, 1, ctx);
    if (cmp == 0) return th_set_signed_ui_fraction(rop, op->sign, u, 4UL, rnd, ctx);
    return th_scaled_inverse_dispatch(rop, op, TH_INV_ASIN,
                                      TH_SCALE_UNIT, u, rnd, ctx);
}

int ppf_acosu(ppf_t *rop, const ppf_t *op, unsigned long u,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    int cmp;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op) || ppf_inf_p(op)) return th_nan(rop, op->sign, ctx);
    if (u == 0UL) { ppf_set_zero(rop, 1); return 0; }
    cmp = ppf_cmpabs_ui(op, 1UL, ctx);
    if (cmp > 0) return th_nan(rop, 1, ctx);
    if (ppf_cmp_ui(op, 1UL, ctx) == 0) { ppf_set_zero(rop, 1); return 0; }
    if (ppf_cmp_si(op, -1L, ctx) == 0) return th_set_signed_ui_fraction(rop, 1, u, 2UL, rnd, ctx);
    if (ppf_zero_p(op)) return th_set_signed_ui_fraction(rop, 1, u, 4UL, rnd, ctx);
    return th_scaled_inverse_dispatch(rop, op, TH_INV_ACOS,
                                      TH_SCALE_UNIT, u, rnd, ctx);
}

int ppf_atanu(ppf_t *rop, const ppf_t *op, unsigned long u,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op)) return th_nan(rop, op->sign, ctx);
    if (u == 0UL) { ppf_set_zero(rop, op->sign); return 0; }
    if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
    if (ppf_inf_p(op)) return th_set_signed_ui_fraction(rop, op->sign, u, 4UL, rnd, ctx);
    if (ppf_cmpabs_ui(op, 1UL, ctx) == 0)
        return th_set_signed_ui_fraction(rop, op->sign, u, 8UL, rnd, ctx);
    return th_scaled_inverse_dispatch(rop, op, TH_INV_ATAN,
                                      TH_SCALE_UNIT, u, rnd, ctx);
}

typedef struct {
    const ppf_t *y;
    const ppf_t *x;
    th_inverse_scale_t scale;
    unsigned long u;
} th_atan2_arg_t;

static int th_atan2_builder(th_iv_t *out, ppf_prec_t work,
                            void *opaque, ppf_context_t *ctx) {
    th_atan2_arg_t *arg = (th_atan2_arg_t *)opaque;
    th_iv_t y, x, ratio, angle, pi, scale, tmp;
    mpq_t q;
    int xsign, ysign, rc = -1;
    memset(&y, 0, sizeof(y));
    memset(&x, 0, sizeof(x));
    memset(&ratio, 0, sizeof(ratio));
    memset(&angle, 0, sizeof(angle));
    memset(&pi, 0, sizeof(pi));
    memset(&scale, 0, sizeof(scale));
    memset(&tmp, 0, sizeof(tmp));
    mpq_init(q);
    if (th_iv_init(&y, work, ctx) != 0 ||
        th_iv_init(&x, work, ctx) != 0 ||
        th_iv_init(&ratio, work, ctx) != 0 ||
        th_iv_init(&angle, work, ctx) != 0 ||
        th_iv_init(&pi, work, ctx) != 0 ||
        th_iv_init(&scale, work, ctx) != 0 ||
        th_iv_init(&tmp, work, ctx) != 0)
        goto done;
    if (th_iv_set_exact(&y, arg->y, ctx) != 0 ||
        th_iv_set_exact(&x, arg->x, ctx) != 0)
        goto done;
    xsign = ppf_sgn(arg->x, ctx);
    ysign = ppf_sgn(arg->y, ctx);
    if (xsign > 0) {
        if (th_iv_div_nonzero(&ratio, &y, &x, ctx) != 0 ||
            th_atan_iv(&angle, &ratio, work, ctx) != 0)
            goto done;
    } else {
        if (th_iv_div_nonzero(&ratio, &y, &x, ctx) != 0 ||
            th_atan_iv(&angle, &ratio, work, ctx) != 0 ||
            th_iv_set_pi(&pi, ctx) != 0)
            goto done;
        if (ysign > 0) {
            if (th_iv_add(&tmp, &angle, &pi, ctx) != 0) goto done;
        } else {
            if (th_iv_sub(&tmp, &angle, &pi, ctx) != 0) goto done;
        }
        if (th_iv_copy(&angle, &tmp, ctx) != 0) goto done;
    }
    if (arg->scale == TH_SCALE_NONE) {
        rc = th_iv_copy(out, &angle, ctx);
        goto done;
    }
    if (!pi.initialized || ppf_nan_p(&pi.lo)) {
        if (th_iv_set_pi(&pi, ctx) != 0) goto done;
    }
    if (arg->scale == TH_SCALE_PI) {
        rc = th_iv_div_positive(out, &angle, &pi, ctx);
        goto done;
    }
    if (arg->u == 0UL) {
        if (th_iv_set_ui(out, 0UL, ctx) != 0) goto done;
        rc = 0;
        goto done;
    }
    mpq_set_ui(q, arg->u, 2UL);
    mpq_canonicalize(q);
    if (th_iv_set_q(&scale, q, ctx) != 0 ||
        th_iv_mul(&tmp, &angle, &scale, ctx) != 0 ||
        th_iv_div_positive(out, &tmp, &pi, ctx) != 0)
        goto done;
    rc = 0;
done:
    th_iv_clear(&y);
    th_iv_clear(&x);
    th_iv_clear(&ratio);
    th_iv_clear(&angle);
    th_iv_clear(&pi);
    th_iv_clear(&scale);
    th_iv_clear(&tmp);
    mpq_clear(q);
    return rc;
}

/* angle_code is the exact angle divided by pi: numerator/denominator. */
static int th_atan2_exact_scaled(ppf_t *rop, long numerator,
                                 unsigned long denominator,
                                 th_inverse_scale_t scale, unsigned long u,
                                 ppf_rnd_t rnd, ppf_context_t *ctx,
                                 int zero_sign) {
    if (numerator == 0L) {
        ppf_set_zero(rop, zero_sign);
        return 0;
    }
    if (scale == TH_SCALE_NONE)
        return th_set_pi_fraction(rop, numerator, denominator, rnd, ctx);
    if (scale == TH_SCALE_PI)
        return th_set_q_fraction(rop, numerator, denominator, rnd, ctx);
    if (u == 0UL) {
        ppf_set_zero(rop, numerator < 0 ? -1 : 1);
        return 0;
    }
    {
        mpq_t q;
        int rc;
        mpq_init(q);
        mpq_set_si(q, numerator, denominator);
        mpq_mul_2exp(q, q, 0u);
        mpz_mul_ui(mpq_numref(q), mpq_numref(q), u);
        mpz_mul_ui(mpq_denref(q), mpq_denref(q), 2UL);
        mpq_canonicalize(q);
        rc = ppf_set_q(rop, q, rnd, ctx);
        mpq_clear(q);
        return rc;
    }
}

static int th_atan2_dispatch(ppf_t *rop, const ppf_t *y, const ppf_t *x,
                             th_inverse_scale_t scale, unsigned long u,
                             ppf_rnd_t rnd, ppf_context_t *ctx) {
    th_atan2_arg_t arg;
    int ys, xs;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !x || !y || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(x) || ppf_nan_p(y)) return th_nan(rop, 1, ctx);
    ys = y->sign < 0 ? -1 : 1;
    xs = x->sign < 0 ? -1 : 1;
    if (scale == TH_SCALE_UNIT && u == 0UL) {
        ppf_set_zero(rop, ys);
        return 0;
    }
    if (ppf_inf_p(y) && ppf_inf_p(x)) {
        return th_atan2_exact_scaled(rop,
                                     xs > 0 ? ys : 3L * ys,
                                     4UL, scale, u, rnd, ctx, ys);
    }
    if (ppf_inf_p(y))
        return th_atan2_exact_scaled(rop, ys, 2UL, scale, u, rnd, ctx, ys);
    if (ppf_inf_p(x)) {
        if (xs > 0)
            return th_atan2_exact_scaled(rop, 0L, 1UL, scale, u, rnd, ctx, ys);
        return th_atan2_exact_scaled(rop, ys, 1UL, scale, u, rnd, ctx, ys);
    }
    if (ppf_zero_p(y)) {
        if (xs > 0)
            return th_atan2_exact_scaled(rop, 0L, 1UL, scale, u, rnd, ctx, ys);
        return th_atan2_exact_scaled(rop, ys, 1UL, scale, u, rnd, ctx, ys);
    }
    if (ppf_zero_p(x))
        return th_atan2_exact_scaled(rop, ys, 2UL, scale, u, rnd, ctx, ys);
    if (scale != TH_SCALE_NONE && ppf_cmpabs(y, x, ctx) == 0) {
        long numerator = xs > 0 ? ys : 3L * ys;
        return th_atan2_exact_scaled(rop, numerator, 4UL,
                                     scale, u, rnd, ctx, ys);
    }
    if (scale == TH_SCALE_NONE && rop->precision >= 12000u) {
        ppf_t ay, ax;
        int quadrant, ternary = 0, fast = 0;
        memset(&ay, 0, sizeof(ay));
        memset(&ax, 0, sizeof(ax));
        if (ppf_init2_ctx(&ay, y->precision, ctx) == 0 &&
            ppf_init2_ctx(&ax, x->precision, ctx) == 0 &&
            ppf_abs(&ay, y, PPF_RNDN, ctx) != PPF_TERNARY_ERROR &&
            ppf_abs(&ax, x, PPF_RNDN, ctx) != PPF_TERNARY_ERROR) {
            if (xs > 0)
                quadrant = ys > 0 ? PPF_ATAN_Q_POS : PPF_ATAN_Q_NEG;
            else
                quadrant = ys > 0 ? PPF_ATAN_Q_PI_MINUS
                                  : PPF_ATAN_Q_NEG_PI_PLUS;
            fast = ppf_fast_atan_ratio_internal(rop, &ay, &ax, quadrant,
                                                rnd, ctx, &ternary);
        }
        if (ay.limbs) ppf_clear(&ay);
        if (ax.limbs) ppf_clear(&ax);
        if (fast) return ternary;
    }
    arg.y = y;
    arg.x = x;
    arg.scale = scale;
    arg.u = u;
    return th_evaluate(rop, rnd, ctx, th_atan2_builder, &arg);
}

int ppf_atan2(ppf_t *rop, const ppf_t *y, const ppf_t *x,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_atan2_dispatch(rop, y, x, TH_SCALE_NONE, 0UL, rnd, ctx);
}

int ppf_atan2pi(ppf_t *rop, const ppf_t *y, const ppf_t *x,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_atan2_dispatch(rop, y, x, TH_SCALE_PI, 0UL, rnd, ctx);
}

int ppf_atan2u(ppf_t *rop, const ppf_t *y, const ppf_t *x,
               unsigned long u, ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_atan2_dispatch(rop, y, x, TH_SCALE_UNIT, u, rnd, ctx);
}

/* ---------- Hyperbolic functions ---------- */

static int th_iv_exp(th_iv_t *out, const th_iv_t *x,
                     ppf_context_t *ctx) {
    if (x->lo.exponent <= 12 && ppf_same_value(&x->lo, &x->hi) &&
        ppf_fast_exp_interval_internal(&out->lo, &out->hi, &x->lo,
                                       out->lo.precision, ctx))
        return 0;
    if (ppf_exp(&out->lo, &x->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_exp(&out->hi, &x->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

static int th_iv_log(th_iv_t *out, const th_iv_t *x,
                     ppf_context_t *ctx) {
    if (ppf_sgn(&x->lo, ctx) <= 0) return -1;
    if (ppf_log(&out->lo, &x->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_log(&out->hi, &x->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

typedef enum {
    TH_HYP_SINH = 0,
    TH_HYP_COSH = 1,
    TH_HYP_TANH = 2,
    TH_HYP_SECH = 3,
    TH_HYP_CSCH = 4,
    TH_HYP_COTH = 5
} th_hyp_fn_t;

typedef struct {
    const ppf_t *x;
    th_hyp_fn_t fn;
} th_hyp_arg_t;

static int th_hyp_builder(th_iv_t *out, ppf_prec_t work,
                          void *opaque, ppf_context_t *ctx) {
    th_hyp_arg_t *arg = (th_hyp_arg_t *)opaque;
    th_iv_t x, ax, negax, u, u2, one, num, den, tmp, two;
    int neg, rc = -1;
    memset(&x, 0, sizeof(x));
    memset(&ax, 0, sizeof(ax));
    memset(&negax, 0, sizeof(negax));
    memset(&u, 0, sizeof(u));
    memset(&u2, 0, sizeof(u2));
    memset(&one, 0, sizeof(one));
    memset(&num, 0, sizeof(num));
    memset(&den, 0, sizeof(den));
    memset(&tmp, 0, sizeof(tmp));
    memset(&two, 0, sizeof(two));
    if (th_iv_init(&x, work, ctx) != 0 ||
        th_iv_init(&ax, work, ctx) != 0 ||
        th_iv_init(&negax, work, ctx) != 0 ||
        th_iv_init(&u, work, ctx) != 0 ||
        th_iv_init(&u2, work, ctx) != 0 ||
        th_iv_init(&one, work, ctx) != 0 ||
        th_iv_init(&num, work, ctx) != 0 ||
        th_iv_init(&den, work, ctx) != 0 ||
        th_iv_init(&tmp, work, ctx) != 0 ||
        th_iv_init(&two, work, ctx) != 0)
        goto done;
    if (th_iv_set_exact(&x, arg->x, ctx) != 0) goto done;
    neg = ppf_sgn(arg->x, ctx) < 0;
    if (neg) {
        if (th_iv_neg(&ax, &x, ctx) != 0) goto done;
    } else if (th_iv_copy(&ax, &x, ctx) != 0) goto done;
    /* Direct paired fixed-point sinh/cosh avoids two exp evaluations and
       shares every series and reconstruction product. */
    if (ax.hi.exponent <= 8 &&
        ppf_fast_sinhcosh_internal(&num.lo, &num.hi,
                                   &den.lo, &den.hi,
                                   &ax.lo, &ax.hi, work, ctx)) {
        switch (arg->fn) {
            case TH_HYP_SINH:
                if (neg) rc = th_iv_neg(out, &num, ctx);
                else rc = th_iv_copy(out, &num, ctx);
                goto done;
            case TH_HYP_COSH:
                rc = th_iv_copy(out, &den, ctx);
                goto done;
            case TH_HYP_TANH:
                if (th_iv_div_positive(&tmp, &num, &den, ctx) != 0) goto done;
                if (neg) rc = th_iv_neg(out, &tmp, ctx);
                else rc = th_iv_copy(out, &tmp, ctx);
                goto done;
            case TH_HYP_SECH:
                if (th_iv_set_ui(&one, 1UL, ctx) != 0 ||
                    th_iv_div_positive(out, &one, &den, ctx) != 0) goto done;
                rc = 0; goto done;
            case TH_HYP_CSCH:
                if (th_iv_set_ui(&one, 1UL, ctx) != 0 ||
                    th_iv_div_positive(&tmp, &one, &num, ctx) != 0) goto done;
                if (neg) rc = th_iv_neg(out, &tmp, ctx);
                else rc = th_iv_copy(out, &tmp, ctx);
                goto done;
            case TH_HYP_COTH:
                if (th_iv_div_positive(&tmp, &den, &num, ctx) != 0) goto done;
                if (neg) rc = th_iv_neg(out, &tmp, ctx);
                else rc = th_iv_copy(out, &tmp, ctx);
                goto done;
        }
    }
    if (th_iv_neg(&negax, &ax, ctx) != 0 ||
        th_iv_exp(&u, &negax, ctx) != 0)
        goto done;
    if (ppf_fast_hyperbolic_from_exp_internal(&out->lo, &out->hi,
                                              &u.lo, &u.hi,
                                              (int)arg->fn, neg,
                                              work, ctx)) {
        rc = 0;
        goto done;
    }
    if (th_iv_square(&u2, &u, ctx) != 0 ||
        th_iv_set_ui(&one, 1UL, ctx) != 0 ||
        th_iv_set_ui(&two, 2UL, ctx) != 0)
        goto done;
    switch (arg->fn) {
        case TH_HYP_SINH:
            /* sinh(a) = (1-u^2)/(2u), u=exp(-a). */
            if (th_iv_sub(&num, &one, &u2, ctx) != 0 ||
                th_iv_mul(&den, &two, &u, ctx) != 0 ||
                th_iv_div_positive(&tmp, &num, &den, ctx) != 0)
                goto done;
            if (neg) rc = th_iv_neg(out, &tmp, ctx);
            else rc = th_iv_copy(out, &tmp, ctx);
            break;
        case TH_HYP_COSH:
            if (th_iv_add(&num, &one, &u2, ctx) != 0 ||
                th_iv_mul(&den, &two, &u, ctx) != 0 ||
                th_iv_div_positive(out, &num, &den, ctx) != 0)
                goto done;
            rc = 0;
            break;
        case TH_HYP_TANH:
            if (th_iv_sub(&num, &one, &u2, ctx) != 0 ||
                th_iv_add(&den, &one, &u2, ctx) != 0 ||
                th_iv_div_positive(&tmp, &num, &den, ctx) != 0)
                goto done;
            if (neg) rc = th_iv_neg(out, &tmp, ctx);
            else rc = th_iv_copy(out, &tmp, ctx);
            break;
        case TH_HYP_SECH:
            if (th_iv_mul(&num, &two, &u, ctx) != 0 ||
                th_iv_add(&den, &one, &u2, ctx) != 0 ||
                th_iv_div_positive(out, &num, &den, ctx) != 0)
                goto done;
            rc = 0;
            break;
        case TH_HYP_CSCH:
            if (th_iv_mul(&num, &two, &u, ctx) != 0 ||
                th_iv_sub(&den, &one, &u2, ctx) != 0 ||
                th_iv_div_positive(&tmp, &num, &den, ctx) != 0)
                goto done;
            if (neg) rc = th_iv_neg(out, &tmp, ctx);
            else rc = th_iv_copy(out, &tmp, ctx);
            break;
        case TH_HYP_COTH:
            if (th_iv_add(&num, &one, &u2, ctx) != 0 ||
                th_iv_sub(&den, &one, &u2, ctx) != 0 ||
                th_iv_div_positive(&tmp, &num, &den, ctx) != 0)
                goto done;
            if (neg) rc = th_iv_neg(out, &tmp, ctx);
            else rc = th_iv_copy(out, &tmp, ctx);
            break;
        default:
            goto done;
    }
done:
    th_iv_clear(&x);
    th_iv_clear(&ax);
    th_iv_clear(&negax);
    th_iv_clear(&u);
    th_iv_clear(&u2);
    th_iv_clear(&one);
    th_iv_clear(&num);
    th_iv_clear(&den);
    th_iv_clear(&tmp);
    th_iv_clear(&two);
    return rc;
}

static int th_hyp_overflow_result(ppf_t *rop, int sign, ppf_rnd_t rnd,
                                  ppf_context_t *ctx) {
    int to_inf;
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (rnd == PPF_RNDN || rnd == PPF_RNDA) to_inf = 1;
    else if (rnd == PPF_RNDU) to_inf = sign > 0;
    else if (rnd == PPF_RNDD) to_inf = sign < 0;
    else to_inf = 0;
    if (to_inf) ppf_set_inf(rop, sign);
    else if (ppf_set_max_finite(rop, sign, ctx) != 0)
        return th_error(rop, ctx);
    ppf_raise(ctx, PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT);
    if (sign > 0) return to_inf ? +1 : -1;
    return to_inf ? -1 : +1;
}

static int th_hyp_underflow_tiny(ppf_t *rop, int sign, ppf_rnd_t rnd,
                                 ppf_context_t *ctx) {
    int to_min;
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (rnd == PPF_RNDA) to_min = 1;
    else if (rnd == PPF_RNDU) to_min = sign > 0;
    else if (rnd == PPF_RNDD) to_min = sign < 0;
    else to_min = 0; /* the bound is strictly below the nearest midpoint */
    if (to_min) {
        if (ppf_set_min_normal(rop, sign, ctx) != 0)
            return th_error(rop, ctx);
    } else {
        ppf_set_zero(rop, sign);
    }
    ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
    if (sign > 0) return to_min ? +1 : -1;
    return to_min ? -1 : +1;
}

static int th_hyp_extreme_range_result(ppf_t *rop, const ppf_t *op,
                                       th_hyp_fn_t fn, ppf_rnd_t rnd,
                                       ppf_context_t *ctx, int *handled) {
    uint64_t threshold;
    int sign;
    *handled = 0;
    if (!ppf_regular_p(op) || ppf_zero_p(op)) return 0;
    sign = op->sign < 0 ? -1 : 1;
#if defined(__SIZEOF_INT128__)
    if ((fn == TH_HYP_SINH || fn == TH_HYP_COSH) && ctx->emax >= 0) {
        __extension__ unsigned __int128 num =
            (unsigned __int128)(uint64_t)ctx->emax + 3u;
        num *= 5u;
        threshold = (uint64_t)((num + 6u) / 7u);
        if (threshold <= ULONG_MAX && ppf_cmpabs_ui(op, (unsigned long)threshold, ctx) > 0) {
            *handled = 1;
            return th_hyp_overflow_result(rop,
                    fn == TH_HYP_COSH ? 1 : sign, rnd, ctx);
        }
    }
    if ((fn == TH_HYP_SECH || fn == TH_HYP_CSCH) && ctx->emin < 0) {
        __extension__ unsigned __int128 num =
            (unsigned __int128)(uint64_t)(-(ctx->emin + 1)) + 6u;
        num *= 5u;
        threshold = (uint64_t)((num + 6u) / 7u);
        if (threshold <= ULONG_MAX && ppf_cmpabs_ui(op, (unsigned long)threshold, ctx) > 0) {
            *handled = 1;
            return th_hyp_underflow_tiny(rop,
                    fn == TH_HYP_SECH ? 1 : sign, rnd, ctx);
        }
    }
#else
    (void)threshold;
    (void)sign;
#endif
    return 0;
}

static int th_hyp_saturated_result(ppf_t *rop, const ppf_t *op,
                                   th_hyp_fn_t fn, ppf_rnd_t rnd,
                                   ppf_context_t *ctx, int *handled) {
    unsigned long threshold;
    int sign;
    *handled = 0;
    if ((fn != TH_HYP_TANH && fn != TH_HYP_COTH) ||
        !ppf_regular_p(op) || ppf_zero_p(op) || ctx->emax < 1)
        return 0;
    if (rop->precision > (ppf_prec_t)(ULONG_MAX - 8UL)) return 0;
    threshold = (unsigned long)((rop->precision + 7u) / 2u + 2u);
    if (ppf_cmpabs_ui(op, threshold, ctx) <= 0) return 0;
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    sign = op->sign < 0 ? -1 : 1;
    if (ppf_set_si(rop, sign, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
        return -1;
    if (fn == TH_HYP_TANH) {
        if (sign > 0) {
            if (rnd == PPF_RNDZ || rnd == PPF_RNDD) {
                ppf_nextbelow(rop, ctx);
                ppf_raise(ctx, PPF_FLAG_INEXACT);
                *handled = 1;
                return -1;
            }
            ppf_raise(ctx, PPF_FLAG_INEXACT);
            *handled = 1;
            return +1;
        }
        if (rnd == PPF_RNDZ || rnd == PPF_RNDU) {
            ppf_nextabove(rop, ctx);
            ppf_raise(ctx, PPF_FLAG_INEXACT);
            *handled = 1;
            return +1;
        }
        ppf_raise(ctx, PPF_FLAG_INEXACT);
        *handled = 1;
        return -1;
    }
    /* coth(a) lies just outside +/-1. */
    if (sign > 0) {
        if (rnd == PPF_RNDU || rnd == PPF_RNDA) {
            ppf_nextabove(rop, ctx);
            ppf_raise(ctx, PPF_FLAG_INEXACT);
            *handled = 1;
            return +1;
        }
        ppf_raise(ctx, PPF_FLAG_INEXACT);
        *handled = 1;
        return -1;
    }
    if (rnd == PPF_RNDD || rnd == PPF_RNDA) {
        ppf_nextbelow(rop, ctx);
        ppf_raise(ctx, PPF_FLAG_INEXACT);
        *handled = 1;
        return -1;
    }
    ppf_raise(ctx, PPF_FLAG_INEXACT);
    *handled = 1;
    return +1;
}

static int th_hyp_dispatch(ppf_t *rop, const ppf_t *op, th_hyp_fn_t fn,
                           ppf_rnd_t rnd, ppf_context_t *ctx) {
    th_hyp_arg_t arg;
    ppf_prec_t work;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op)) return th_nan(rop, op->sign, ctx);
    if (ppf_inf_p(op)) {
        switch (fn) {
            case TH_HYP_SINH: ppf_set_inf(rop, op->sign); return 0;
            case TH_HYP_COSH: ppf_set_inf(rop, 1); return 0;
            case TH_HYP_TANH: return ppf_set_si(rop, op->sign, rnd, ctx);
            case TH_HYP_SECH: ppf_set_zero(rop, 1); return 0;
            case TH_HYP_CSCH: ppf_set_zero(rop, op->sign); return 0;
            case TH_HYP_COTH: return ppf_set_si(rop, op->sign, rnd, ctx);
            default: return th_error(rop, ctx);
        }
    }
    if (ppf_zero_p(op)) {
        switch (fn) {
            case TH_HYP_SINH:
            case TH_HYP_TANH:
                ppf_set_zero(rop, op->sign); return 0;
            case TH_HYP_COSH:
            case TH_HYP_SECH:
                return ppf_set_ui(rop, 1UL, rnd, ctx);
            case TH_HYP_CSCH:
            case TH_HYP_COTH:
                return th_divzero_inf(rop, op->sign, ctx);
            default:
                return th_error(rop, ctx);
        }
    }
    {
        int handled = 0;
        int extreme = th_hyp_extreme_range_result(rop, op, fn, rnd, ctx, &handled);
        if (extreme == PPF_TERNARY_ERROR) return extreme;
        if (handled) return extreme;
    }
    {
        int handled = 0;
        int sat = th_hyp_saturated_result(rop, op, fn, rnd, ctx, &handled);
        if (sat == PPF_TERNARY_ERROR) return sat;
        if (handled) return sat;
    }
    arg.x = op;
    arg.fn = fn;
    work = rop->precision + PPF_TRIG_INITIAL_GUARD;
    if (op->exponent < 0) {
        uint64_t cancel = (uint64_t)(-(op->exponent + 1)) + 32u;
        if (cancel < PPF_TRIG_MAX_REDUCTION_EXTRA &&
            work <= UINT64_MAX - cancel)
            work += cancel;
    }
    return th_evaluate_start(rop, rnd, ctx, th_hyp_builder, &arg, work);
}

typedef struct {
    const ppf_t *x;
} th_sinhcosh_pair_arg_t;

static int th_sinhcosh_pair_builder(th_iv_t *sout, th_iv_t *cout,
                                     ppf_prec_t work, void *opaque,
                                     ppf_context_t *ctx) {
    th_sinhcosh_pair_arg_t *arg = (th_sinhcosh_pair_arg_t *)opaque;
    th_iv_t x, ax, s, c;
    int neg, rc = -1;
    memset(&x, 0, sizeof(x)); memset(&ax, 0, sizeof(ax));
    memset(&s, 0, sizeof(s)); memset(&c, 0, sizeof(c));
    if (th_iv_init(&x, work, ctx) != 0 ||
        th_iv_init(&ax, work, ctx) != 0 ||
        th_iv_init(&s, work, ctx) != 0 ||
        th_iv_init(&c, work, ctx) != 0)
        goto done;
    if (th_iv_set_exact(&x, arg->x, ctx) != 0) goto done;
    neg = ppf_sgn(arg->x, ctx) < 0;
    if (neg) {
        if (th_iv_neg(&ax, &x, ctx) != 0) goto done;
    } else if (th_iv_copy(&ax, &x, ctx) != 0) goto done;
    if (ax.hi.exponent <= 8 &&
        ppf_fast_sinhcosh_internal(&s.lo, &s.hi, &c.lo, &c.hi,
                                   &ax.lo, &ax.hi, work, ctx)) {
        if (neg) {
            if (th_iv_neg(sout, &s, ctx) != 0) goto done;
        } else if (th_iv_copy(sout, &s, ctx) != 0) goto done;
        if (th_iv_copy(cout, &c, ctx) != 0) goto done;
        rc = 0;
        goto done;
    }
    {
        th_hyp_arg_t sa = { arg->x, TH_HYP_SINH };
        th_hyp_arg_t ca = { arg->x, TH_HYP_COSH };
        if (th_hyp_builder(sout, work, &sa, ctx) != 0 ||
            th_hyp_builder(cout, work, &ca, ctx) != 0)
            goto done;
    }
    rc = 0;
done:
    th_iv_clear(&x); th_iv_clear(&ax); th_iv_clear(&s); th_iv_clear(&c);
    return rc;
}

int ppf_sinh(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_hyp_dispatch(rop, op, TH_HYP_SINH, rnd, ctx);
}

int ppf_cosh(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_hyp_dispatch(rop, op, TH_HYP_COSH, rnd, ctx);
}

int ppf_tanh(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_hyp_dispatch(rop, op, TH_HYP_TANH, rnd, ctx);
}

int ppf_sech(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_hyp_dispatch(rop, op, TH_HYP_SECH, rnd, ctx);
}

int ppf_csch(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_hyp_dispatch(rop, op, TH_HYP_CSCH, rnd, ctx);
}

int ppf_coth(ppf_t *rop, const ppf_t *op,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_hyp_dispatch(rop, op, TH_HYP_COTH, rnd, ctx);
}

int ppf_sinh_cosh(ppf_t *sop, ppf_t *cop, const ppf_t *op,
                  ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t input;
    th_sinhcosh_pair_arg_t arg;
    ppf_prec_t work, need;
    int ts = 0, tc = 0;
    ctx = ppf_resolve_context(ctx);
    if (!sop || !cop || !op || !ctx || sop == cop)
        return PPF_TERNARY_ERROR;
    memset(&input, 0, sizeof(input));
    if (op == sop || op == cop) {
        if (ppf_init2_ctx(&input, op->precision, ctx) != 0 ||
            ppf_set(&input, op, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
            goto error;
        op = &input;
    }
    /* Singular, zero, and extremely large cases already have exact scalar
       shortcuts.  Keep those branches while sharing all ordinary work. */
    if (!ppf_regular_p(op) || ppf_zero_p(op) || op->exponent > 12) {
        ppf_t s, c;
        memset(&s, 0, sizeof(s)); memset(&c, 0, sizeof(c));
        if (ppf_init2_ctx(&s, sop->precision, ctx) != 0 ||
            ppf_init2_ctx(&c, cop->precision, ctx) != 0) {
            if (s.limbs) ppf_clear(&s);
            if (c.limbs) ppf_clear(&c);
            goto error;
        }
        ts = ppf_sinh(&s, op, rnd, ctx);
        tc = ppf_cosh(&c, op, rnd, ctx);
        if (ts == PPF_TERNARY_ERROR || tc == PPF_TERNARY_ERROR) {
            ppf_clear(&s); ppf_clear(&c); goto error;
        }
        ppf_swap(sop, &s); ppf_swap(cop, &c);
        ppf_clear(&s); ppf_clear(&c);
        if (input.limbs) ppf_clear(&input);
        return th_pair_code(ts, tc);
    }
    need = sop->precision > cop->precision ? sop->precision : cop->precision;
    if (need > UINT64_MAX - PPF_TRIG_INITIAL_GUARD) goto error;
    work = need + PPF_TRIG_INITIAL_GUARD;
    if (op->exponent < 0) {
        uint64_t cancel = (uint64_t)(-(op->exponent + 1)) + 32u;
        if (cancel < PPF_TRIG_MAX_REDUCTION_EXTRA &&
            work <= UINT64_MAX - cancel)
            work += cancel;
    }
    arg.x = op;
    if (th_evaluate_pair_start(sop, cop, rnd, ctx,
                               th_sinhcosh_pair_builder, &arg, work,
                               &ts, &tc) != 0)
        goto error;
    if (input.limbs) ppf_clear(&input);
    return th_pair_code(ts, tc);
error:
    if (input.limbs) ppf_clear(&input);
    ppf_set_nan(sop, 1);
    ppf_set_nan(cop, 1);
    ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

typedef enum {
    TH_INVH_ASINH = 0,
    TH_INVH_ACOSH = 1,
    TH_INVH_ATANH = 2
} th_invh_fn_t;

typedef struct {
    const ppf_t *x;
    th_invh_fn_t fn;
} th_invh_arg_t;

static int th_invh_builder(th_iv_t *out, ppf_prec_t work,
                           void *opaque, ppf_context_t *ctx) {
    th_invh_arg_t *arg = (th_invh_arg_t *)opaque;
    th_iv_t x, ax, x2, one, rad, root, sum, num, den, ratio, logv, tmp;
    int neg, rc = -1;
    memset(&x, 0, sizeof(x));
    memset(&ax, 0, sizeof(ax));
    memset(&x2, 0, sizeof(x2));
    memset(&one, 0, sizeof(one));
    memset(&rad, 0, sizeof(rad));
    memset(&root, 0, sizeof(root));
    memset(&sum, 0, sizeof(sum));
    memset(&num, 0, sizeof(num));
    memset(&den, 0, sizeof(den));
    memset(&ratio, 0, sizeof(ratio));
    memset(&logv, 0, sizeof(logv));
    memset(&tmp, 0, sizeof(tmp));
    if (th_iv_init(&x, work, ctx) != 0 ||
        th_iv_init(&ax, work, ctx) != 0 ||
        th_iv_init(&x2, work, ctx) != 0 ||
        th_iv_init(&one, work, ctx) != 0 ||
        th_iv_init(&rad, work, ctx) != 0 ||
        th_iv_init(&root, work, ctx) != 0 ||
        th_iv_init(&sum, work, ctx) != 0 ||
        th_iv_init(&num, work, ctx) != 0 ||
        th_iv_init(&den, work, ctx) != 0 ||
        th_iv_init(&ratio, work, ctx) != 0 ||
        th_iv_init(&logv, work, ctx) != 0 ||
        th_iv_init(&tmp, work, ctx) != 0)
        goto done;
    if (th_iv_set_exact(&x, arg->x, ctx) != 0 ||
        th_iv_set_ui(&one, 1UL, ctx) != 0)
        goto done;
    neg = ppf_sgn(arg->x, ctx) < 0;
    if (arg->fn == TH_INVH_ASINH) {
        if (neg) {
            if (th_iv_neg(&ax, &x, ctx) != 0) goto done;
        } else if (th_iv_copy(&ax, &x, ctx) != 0) goto done;
        if (th_iv_square(&x2, &ax, ctx) != 0 ||
            th_iv_add(&rad, &x2, &one, ctx) != 0 ||
            th_iv_sqrt(&root, &rad, ctx) != 0 ||
            th_iv_add(&sum, &ax, &root, ctx) != 0 ||
            th_iv_log(&logv, &sum, ctx) != 0)
            goto done;
        if (neg) rc = th_iv_neg(out, &logv, ctx);
        else rc = th_iv_copy(out, &logv, ctx);
    } else if (arg->fn == TH_INVH_ACOSH) {
        if (th_iv_square(&x2, &x, ctx) != 0 ||
            th_iv_sub(&rad, &x2, &one, ctx) != 0 ||
            th_iv_sqrt(&root, &rad, ctx) != 0 ||
            th_iv_add(&sum, &x, &root, ctx) != 0 ||
            th_iv_log(out, &sum, ctx) != 0)
            goto done;
        rc = 0;
    } else {
        if (neg) {
            if (th_iv_neg(&ax, &x, ctx) != 0) goto done;
        } else if (th_iv_copy(&ax, &x, ctx) != 0) goto done;
        if (th_iv_add(&num, &one, &ax, ctx) != 0 ||
            th_iv_sub(&den, &one, &ax, ctx) != 0 ||
            th_iv_div_positive(&ratio, &num, &den, ctx) != 0 ||
            th_iv_log(&logv, &ratio, ctx) != 0 ||
            th_iv_scale2(&tmp, &logv, -1L, ctx) != 0)
            goto done;
        if (neg) rc = th_iv_neg(out, &tmp, ctx);
        else rc = th_iv_copy(out, &tmp, ctx);
    }
done:
    th_iv_clear(&x);
    th_iv_clear(&ax);
    th_iv_clear(&x2);
    th_iv_clear(&one);
    th_iv_clear(&rad);
    th_iv_clear(&root);
    th_iv_clear(&sum);
    th_iv_clear(&num);
    th_iv_clear(&den);
    th_iv_clear(&ratio);
    th_iv_clear(&logv);
    th_iv_clear(&tmp);
    return rc;
}

static int th_invh_dispatch(ppf_t *rop, const ppf_t *op, th_invh_fn_t fn,
                            ppf_rnd_t rnd, ppf_context_t *ctx) {
    th_invh_arg_t arg;
    ppf_prec_t work;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !ctx) return th_error(rop, ctx);
    if (ppf_nan_p(op)) return th_nan(rop, op->sign, ctx);
    if (fn == TH_INVH_ASINH) {
        if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
        if (ppf_inf_p(op)) { ppf_set_inf(rop, op->sign); return 0; }
    } else if (fn == TH_INVH_ACOSH) {
        if (ppf_inf_p(op)) {
            if (op->sign < 0) return th_nan(rop, 1, ctx);
            ppf_set_inf(rop, 1); return 0;
        }
        if (ppf_cmp_ui(op, 1UL, ctx) < 0) return th_nan(rop, 1, ctx);
        if (ppf_cmp_ui(op, 1UL, ctx) == 0) { ppf_set_zero(rop, 1); return 0; }
    } else {
        int cmp;
        if (ppf_inf_p(op)) return th_nan(rop, 1, ctx);
        if (ppf_zero_p(op)) { ppf_set_zero(rop, op->sign); return 0; }
        cmp = ppf_cmpabs_ui(op, 1UL, ctx);
        if (cmp > 0) return th_nan(rop, 1, ctx);
        if (cmp == 0) return th_divzero_inf(rop, op->sign, ctx);
    }
    arg.x = op;
    arg.fn = fn;
    work = rop->precision + PPF_TRIG_INITIAL_GUARD;
    if (op->exponent < 0) {
        uint64_t cancel = (uint64_t)(-(op->exponent + 1)) + 32u;
        if (cancel < PPF_TRIG_MAX_REDUCTION_EXTRA &&
            work <= UINT64_MAX - cancel)
            work += cancel;
    }
    return th_evaluate_start(rop, rnd, ctx, th_invh_builder, &arg, work);
}

int ppf_asinh(ppf_t *rop, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_invh_dispatch(rop, op, TH_INVH_ASINH, rnd, ctx);
}

int ppf_acosh(ppf_t *rop, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_invh_dispatch(rop, op, TH_INVH_ACOSH, rnd, ctx);
}

int ppf_atanh(ppf_t *rop, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return th_invh_dispatch(rop, op, TH_INVH_ATANH, rnd, ctx);
}
