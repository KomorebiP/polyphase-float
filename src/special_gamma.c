/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "special_internal.h"

#include "transactional_special_names.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int sp_nan(ppf_t *r,ppf_context_t *ctx){ppf_set_nan(r,1);ppf_raise(ctx,PPF_FLAG_NAN);return 0;}
static int sp_error(ppf_t *r,ppf_context_t *ctx){ppf_set_nan(r,1);ppf_raise(ctx,PPF_FLAG_ERANGE);return PPF_TERNARY_ERROR;}

static int iv_minmax_local(ppf_special_iv_t *out,const ppf_special_iv_t *a,
                           const ppf_special_iv_t *b,ppf_context_t *ctx){
    const ppf_t *lo=ppf_cmp(&a->lo,&b->lo,ctx)<=0?&a->lo:&b->lo;
    const ppf_t *hi=ppf_cmp(&a->hi,&b->hi,ctx)>=0?&a->hi:&b->hi;
    return ppf_set(&out->lo,lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
           ppf_set(&out->hi,hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR?-1:0;
}

static int iv_add_ui_local(ppf_special_iv_t *out,const ppf_special_iv_t *a,
                           unsigned long n,ppf_context_t *ctx){
    ppf_special_iv_t b;int rc=-1;memset(&b,0,sizeof(b));
    if(ppf_special_iv_init(&b,out->lo.precision,ctx)||ppf_special_iv_set_ui(&b,n,ctx))goto done;
    rc=ppf_special_iv_add(out,a,&b,ctx);
done:ppf_special_iv_clear(&b);return rc;
}

static int iv_recip_local(ppf_special_iv_t *out,const ppf_special_iv_t *a,
                          ppf_context_t *ctx){
    ppf_special_iv_t one;int rc=-1;memset(&one,0,sizeof(one));
    if(ppf_special_iv_init(&one,out->lo.precision,ctx)||ppf_special_iv_set_ui(&one,1,ctx))goto done;
    rc=ppf_special_iv_div(out,&one,a,ctx);
done:ppf_special_iv_clear(&one);return rc;
}

static int iv_sinpi_exact(ppf_special_iv_t *out,const ppf_t *x,ppf_context_t *ctx){
    return ppf_sinpi(&out->lo,x,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
           ppf_sinpi(&out->hi,x,PPF_RNDU,ctx)==PPF_TERNARY_ERROR?-1:0;
}
static int iv_cospi_exact(ppf_special_iv_t *out,const ppf_t *x,ppf_context_t *ctx){
    return ppf_cospi(&out->lo,x,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
           ppf_cospi(&out->hi,x,PPF_RNDU,ctx)==PPF_TERNARY_ERROR?-1:0;
}

static int gamma_sign_exact(const ppf_t *x,int *sign,ppf_context_t *ctx){
    if(!x||!sign||!ppf_regular_p(x))return 0;
    if(x->sign>0){*sign=1;return 1;}
    if(ppf_integer_p(x))return 0;
    ppf_context_t wide;ppf_t s;int ok=0;
    memset(&s,0,sizeof(s));if(ppf_context_clone(&wide,ctx))return 0;
    wide.emin=PPF_EMIN_MIN;wide.emax=PPF_EMAX_MAX;wide.flags=0;
    if(ppf_init2_ctx(&s,128,&wide)==0&&ppf_sinpi(&s,x,PPF_RNDN,&wide)!=PPF_TERNARY_ERROR&&!ppf_zero_p(&s)){
        *sign=s.sign<0?-1:1;ok=1;
    }
    if (s.limbs) ppf_clear(&s);
    ppf_context_clear(&wide);
    return ok;
}

static int lgamma_positive_input_interval(ppf_special_iv_t *out,const ppf_special_iv_t *x,
                                          ppf_prec_t work,ppf_context_t *ctx){
    ppf_special_iv_t z,lnz,half,zmhalf,base,term,partial,candidate,next,z2,power,
                     coeff,shift_sum,tmp,twopi,pi,one;
    mpq_t *B=NULL;unsigned maxm=0;int rc=-1;unsigned long shift=0;
    memset(&z,0,sizeof(z));memset(&lnz,0,sizeof(lnz));memset(&half,0,sizeof(half));
    memset(&zmhalf,0,sizeof(zmhalf));memset(&base,0,sizeof(base));memset(&term,0,sizeof(term));
    memset(&partial,0,sizeof(partial));memset(&candidate,0,sizeof(candidate));memset(&next,0,sizeof(next));
    memset(&z2,0,sizeof(z2));memset(&power,0,sizeof(power));memset(&coeff,0,sizeof(coeff));
    memset(&shift_sum,0,sizeof(shift_sum));memset(&tmp,0,sizeof(tmp));memset(&twopi,0,sizeof(twopi));
    memset(&pi,0,sizeof(pi));memset(&one,0,sizeof(one));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(z);INIT(lnz);INIT(half);INIT(zmhalf);INIT(base);INIT(term);INIT(partial);INIT(candidate);INIT(next);INIT(z2);INIT(power);INIT(coeff);INIT(shift_sum);INIT(tmp);INIT(twopi);INIT(pi);INIT(one);
#undef INIT
    if(ppf_special_iv_copy(&z,x,ctx)||ppf_special_iv_set_ui(&shift_sum,0,ctx))goto done;
    {
        unsigned long threshold=(unsigned long)(work/8u+12u);if(threshold<16)threshold=16;if(threshold>100000)threshold=100000;
        while(ppf_cmp_ui(&z.lo,threshold,ctx)<0){
            if(ppf_special_iv_log_positive(&lnz,&z,ctx)||ppf_special_iv_add(&tmp,&shift_sum,&lnz,ctx)||ppf_special_iv_copy(&shift_sum,&tmp,ctx)||iv_add_ui_local(&tmp,&z,1,ctx)||ppf_special_iv_copy(&z,&tmp,ctx))goto done;
            if(++shift>100000UL)goto done;
        }
    }
    if(ppf_special_iv_log_positive(&lnz,&z,ctx))goto done;
    mpq_t hq;mpq_init(hq);mpq_set_ui(hq,1,2);if(ppf_special_iv_set_q(&half,hq,ctx)){mpq_clear(hq);goto done;}mpq_clear(hq);
    if(ppf_special_iv_sub(&zmhalf,&z,&half,ctx)||ppf_special_iv_mul(&base,&zmhalf,&lnz,ctx)||ppf_special_iv_sub(&tmp,&base,&z,ctx)||ppf_special_iv_copy(&base,&tmp,ctx))goto done;
    if(ppf_special_pi_interval(&pi,ctx)||ppf_special_iv_scale2(&twopi,&pi,1,ctx)||ppf_special_iv_log_positive(&twopi,&twopi,ctx)||ppf_special_iv_scale2(&twopi,&twopi,-1,ctx)||ppf_special_iv_add(&tmp,&base,&twopi,ctx)||ppf_special_iv_copy(&partial,&tmp,ctx))goto done;
    if(ppf_special_iv_mul(&z2,&z,&z,ctx)||ppf_special_iv_copy(&power,&z,ctx))goto done;
    maxm=(unsigned)(work/6u+16u);if(maxm<12)maxm=12;if(maxm>256)maxm=256;
    B=(mpq_t*)ppf_owned_buffer_calloc_internal(ctx,2u*maxm+1u,sizeof(mpq_t));if(!B||ppf_special_bernoulli(B,2u*maxm))goto done;
    for(unsigned k=1;k<=maxm;++k){
        mpq_t q;mpq_init(q);mpq_set(q,B[2u*k]);mpz_mul_ui(mpq_denref(q),mpq_denref(q),(unsigned long)(2u*k));mpz_mul_ui(mpq_denref(q),mpq_denref(q),(unsigned long)(2u*k-1u));mpq_canonicalize(q);
        if(ppf_special_iv_set_q(&coeff,q,ctx)||ppf_special_iv_div(&next,&coeff,&power,ctx)){mpq_clear(q);goto done;}mpq_clear(q);
        if(ppf_special_iv_add(&candidate,&partial,&next,ctx))goto done;
        if(ppf_special_small(&next.hi,work,28u)||k==maxm){if(iv_minmax_local(&base,&partial,&candidate,ctx)||ppf_special_iv_sub(out,&base,&shift_sum,ctx))goto done;rc=0;goto done;}
        if(ppf_special_iv_copy(&partial,&candidate,ctx)||ppf_special_iv_mul(&tmp,&power,&z2,ctx)||ppf_special_iv_copy(&power,&tmp,ctx))goto done;
    }
done:
    if(B){ppf_special_bernoulli_clear(B,2u*maxm);ppf_owned_buffer_free_internal(B);}ppf_special_iv_clear(&z);ppf_special_iv_clear(&lnz);ppf_special_iv_clear(&half);ppf_special_iv_clear(&zmhalf);ppf_special_iv_clear(&base);ppf_special_iv_clear(&term);ppf_special_iv_clear(&partial);ppf_special_iv_clear(&candidate);ppf_special_iv_clear(&next);ppf_special_iv_clear(&z2);ppf_special_iv_clear(&power);ppf_special_iv_clear(&coeff);ppf_special_iv_clear(&shift_sum);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&twopi);ppf_special_iv_clear(&pi);ppf_special_iv_clear(&one);return rc;
}

static int lgamma_positive_interval(ppf_special_iv_t *out, const ppf_t *x,
                                    ppf_prec_t work, ppf_context_t *ctx) {
    ppf_special_iv_t input;
    int rc = -1;
    memset(&input, 0, sizeof(input));
    if (ppf_special_iv_init(&input, work, ctx) ||
        ppf_special_iv_set_exact(&input, x, ctx)) goto done;
    rc = lgamma_positive_input_interval(out, &input, work, ctx);
done:
    ppf_special_iv_clear(&input);
    return rc;
}

static int lgamma_abs_interval(ppf_special_iv_t *out,int *sign,const ppf_t *x,
                              ppf_prec_t work,ppf_context_t *ctx){
    if(x->sign>0){*sign=1;return lgamma_positive_interval(out,x,work,ctx);}
    if(ppf_integer_p(x))return -1;
    ppf_special_iv_t y,lg,sp,asp,lsp,pi,lpi,tmp;
    int rc=-1,s=0;memset(&y,0,sizeof(y));memset(&lg,0,sizeof(lg));memset(&sp,0,sizeof(sp));memset(&asp,0,sizeof(asp));memset(&lsp,0,sizeof(lsp));memset(&pi,0,sizeof(pi));memset(&lpi,0,sizeof(lpi));memset(&tmp,0,sizeof(tmp));
    if(!gamma_sign_exact(x,&s,ctx))goto done;
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(y);INIT(lg);INIT(sp);INIT(asp);INIT(lsp);INIT(pi);INIT(lpi);INIT(tmp);
#undef INIT
    if(ppf_special_iv_set_ui(&y,1,ctx)||ppf_special_iv_set_exact(&tmp,x,ctx)||ppf_special_iv_sub(&y,&y,&tmp,ctx)||
       lgamma_positive_input_interval(&lg,&y,work,ctx)||iv_sinpi_exact(&sp,x,ctx)||ppf_special_iv_abs(&asp,&sp,ctx)||ppf_special_iv_log_positive(&lsp,&asp,ctx)||
       ppf_special_pi_interval(&pi,ctx)||ppf_special_iv_log_positive(&lpi,&pi,ctx)||ppf_special_iv_sub(&tmp,&lpi,&lsp,ctx)||ppf_special_iv_sub(out,&tmp,&lg,ctx))goto done;
    *sign=s;rc=0;
done:ppf_special_iv_clear(&y);ppf_special_iv_clear(&lg);ppf_special_iv_clear(&sp);ppf_special_iv_clear(&asp);ppf_special_iv_clear(&lsp);ppf_special_iv_clear(&pi);ppf_special_iv_clear(&lpi);ppf_special_iv_clear(&tmp);return rc;
}

typedef struct{const ppf_t*x;int require_positive_gamma;int *signp;} lg_arg_t;
static int build_lgamma(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){lg_arg_t*a=opaque;int s=0;if(lgamma_abs_interval(out,&s,a->x,work,ctx))return -1;if(a->signp)*a->signp=s;if(a->require_positive_gamma&&s<0)return -1;return 0;}

typedef struct{const ppf_t*x;} unary_arg_t;
static int build_gamma(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){
    unary_arg_t*a=opaque;ppf_special_iv_t lg,val,neg;int s=0,rc=-1;memset(&lg,0,sizeof(lg));memset(&val,0,sizeof(val));memset(&neg,0,sizeof(neg));
    if(ppf_special_iv_init(&lg,work,ctx)||ppf_special_iv_init(&val,work,ctx)||ppf_special_iv_init(&neg,work,ctx))goto done;
    if(lgamma_abs_interval(&lg,&s,a->x,work,ctx)||ppf_special_iv_exp(&val,&lg,ctx))goto done;
    if(s<0){if(ppf_special_iv_neg(&neg,&val,ctx)||ppf_special_iv_copy(out,&neg,ctx))goto done;}else if(ppf_special_iv_copy(out,&val,ctx))goto done;rc=0;
done:ppf_special_iv_clear(&lg);ppf_special_iv_clear(&val);ppf_special_iv_clear(&neg);return rc;
}

static int gamma_cmp_exp(const ppf_t *x, ppf_exp_t bound, ppf_context_t *ctx) {
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

/* Try one bounded log-|Gamma| enclosure before exponentiating a very large
   argument.  The exponent gate selects an optimization, not a range proof:
   only the directed enclosure below may establish saturation. */
static int gamma_range_preflight(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                                  ppf_context_t *ctx, int *ternary) {
    ppf_context_t wide;
    ppf_special_iv_t logarithm;
    int sign = 0, classification = 0, allocation_failed;
    if (x->exponent < 40) return 0;
    memset(&logarithm, 0, sizeof(logarithm));
    if (ppf_context_clone(&wide, ctx)) return -1;
    wide.emin = PPF_EMIN_MIN;
    wide.emax = PPF_EMAX_MAX;
    wide.flags = 0;
    if (ppf_special_iv_init(&logarithm, 128, &wide) == 0 &&
        lgamma_abs_interval(&logarithm, &sign, x, 128, &wide) == 0 &&
        ppf_number_p(&logarithm.lo) && ppf_number_p(&logarithm.hi) &&
        ppf_cmp(&logarithm.lo, &logarithm.hi, &wide) <= 0) {
        /* exp(t) >= 2^t for t >= 0, and <= 2^t for t <= 0.
           The underflow bound is at most half the minimum normal value. */
        if (gamma_cmp_exp(&logarithm.lo, ctx->emax > 0 ? ctx->emax : 0,
                          &wide) >= 0)
            classification = 1;
        else if (gamma_cmp_exp(&logarithm.hi, ctx->emin < 2 ? ctx->emin - 2 : 0,
                               &wide) <= 0)
            classification = -1;
    }
    allocation_failed = (wide.flags & PPF_FLAG_ALLOC) != 0;
    ppf_special_iv_clear(&logarithm);
    ppf_context_clear(&wide);
    if (allocation_failed) { ppf_raise(ctx, PPF_FLAG_ALLOC); return -1; }
    if (!classification) return 0;
    if (classification > 0) {
        int toward_finite = rnd == PPF_RNDZ ||
            (sign > 0 ? rnd == PPF_RNDD : rnd == PPF_RNDU);
        if (toward_finite) {
            if (ppf_set_max_finite(rop, sign, ctx) != 0) return -1;
            *ternary = -sign;
        } else { ppf_set_inf(rop, sign); *ternary = sign; }
        ppf_raise(ctx, PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT);
    } else {
        int away_from_zero = rnd == PPF_RNDA ||
            (sign > 0 ? rnd == PPF_RNDU : rnd == PPF_RNDD);
        if (away_from_zero) {
            if (ppf_set_min_normal(rop, sign, ctx) != 0) return -1;
            *ternary = sign;
        } else { ppf_set_zero(rop, sign); *ternary = -sign; }
        ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
    }
    return 1;
}

int ppf_lgamma(ppf_t *rop, int *signp, const ppf_t *x, ppf_rnd_t rnd,
               ppf_context_t *ctx) {
    lg_arg_t a = {x, 0, signp};
    ctx = ppf_resolve_context(ctx);
    if (!rop || !x || !signp || !ctx) return sp_error(rop, ctx);
    if (ppf_nan_p(x)) {
        *signp = 1;
        return sp_nan(rop, ctx);
    }
    if (ppf_inf_p(x)) {
        *signp = x->sign;
        ppf_set_inf(rop, 1);
        return 0;
    }
    if (ppf_zero_p(x)) {
        *signp = x->sign;
        ppf_set_inf(rop, 1);
        ppf_raise(ctx, PPF_FLAG_DIVBY0);
        return 0;
    }
    if (x->sign < 0 && ppf_integer_p(x)) {
        *signp = 1;
        ppf_set_inf(rop, 1);
        ppf_raise(ctx, PPF_FLAG_DIVBY0);
        return 0;
    }
    if (ppf_cmp_ui(x, 1, ctx) == 0 || ppf_cmp_ui(x, 2, ctx) == 0) {
        *signp = 1;
        ppf_set_zero(rop, 1);
        return 0;
    }
    return ppf_special_evaluate(rop, rnd, ctx, build_lgamma, &a);
}

int ppf_lngamma(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                ppf_context_t *ctx) {
    int s = 0;
    lg_arg_t a = {x, 1, &s};
    ctx = ppf_resolve_context(ctx);
    if (!rop || !x || !ctx) return sp_error(rop, ctx);
    if (ppf_nan_p(x)) return sp_nan(rop, ctx);
    if (ppf_inf_p(x)) {
        ppf_set_inf(rop, 1);
        return 0;
    }
    if (ppf_zero_p(x) || (x->sign < 0 && ppf_integer_p(x))) {
        ppf_set_inf(rop, 1);
        ppf_raise(ctx, PPF_FLAG_DIVBY0);
        return 0;
    }
    if (ppf_cmp_ui(x, 1, ctx) == 0 || ppf_cmp_ui(x, 2, ctx) == 0) {
        ppf_set_zero(rop, 1);
        return 0;
    }
    if (x->sign < 0 && !gamma_sign_exact(x, &s, ctx)) return sp_nan(rop, ctx);
    if (s < 0) return sp_nan(rop, ctx);
    return ppf_special_evaluate(rop, rnd, ctx, build_lgamma, &a);
}

int ppf_gamma(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
              ppf_context_t *ctx) {
    unary_arg_t a = {x};
    ctx = ppf_resolve_context(ctx);
    if (!rop || !x || !ctx) return sp_error(rop, ctx);
    if (ppf_nan_p(x) || (ppf_inf_p(x) && x->sign < 0)) return sp_nan(rop, ctx);
    if (ppf_inf_p(x)) {
        ppf_set_inf(rop, 1);
        return 0;
    }
    if (ppf_zero_p(x)) {
        ppf_set_inf(rop, x->sign);
        ppf_raise(ctx, PPF_FLAG_DIVBY0);
        return 0;
    }
    if (x->sign < 0 && ppf_integer_p(x)) return sp_nan(rop, ctx);
    /* A finite-width enclosure cannot certify equality with an exact factorial.
       Use the exact integer route for the bounded, practical factorial domain. */
    if (x->sign > 0 && ppf_integer_p(x) && ppf_cmp_ui(x, 100000, ctx) <= 0) {
        long integer;
        if (!ppf_special_get_long_if_integer(x, &integer, ctx)) return sp_error(rop, ctx);
        return ppf_fac_ui(rop, (unsigned long)(integer - 1), rnd, ctx);
    }
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!ppf_rounding_valid(rnd)) return sp_error(rop, ctx);
    {
        int ternary = 0;
        int classified = gamma_range_preflight(rop, x, rnd, ctx, &ternary);
        if (classified > 0) return ternary;
        if (classified < 0) return sp_error(rop, ctx);
    }
    return ppf_special_evaluate(rop, rnd, ctx, build_gamma, &a);
}

static int digamma_positive_input_interval(ppf_special_iv_t*out,const ppf_special_iv_t*x,ppf_prec_t work,ppf_context_t*ctx){
    ppf_special_iv_t z,shift,lnz,half,recip,base,z2,power,coeff,next,cand,partial,tmp;
    mpq_t *B=NULL;unsigned maxm=0;int rc=-1;unsigned long count=0;
    memset(&z,0,sizeof(z));memset(&shift,0,sizeof(shift));memset(&lnz,0,sizeof(lnz));memset(&half,0,sizeof(half));memset(&recip,0,sizeof(recip));memset(&base,0,sizeof(base));memset(&z2,0,sizeof(z2));memset(&power,0,sizeof(power));memset(&coeff,0,sizeof(coeff));memset(&next,0,sizeof(next));memset(&cand,0,sizeof(cand));memset(&partial,0,sizeof(partial));memset(&tmp,0,sizeof(tmp));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(z);INIT(shift);INIT(lnz);INIT(half);INIT(recip);INIT(base);INIT(z2);INIT(power);INIT(coeff);INIT(next);INIT(cand);INIT(partial);INIT(tmp);
#undef INIT
    if(ppf_special_iv_copy(&z,x,ctx)||ppf_special_iv_set_ui(&shift,0,ctx))goto done;
    unsigned long threshold=(unsigned long)(work/8u+12u);if(threshold<16)threshold=16;
    while(ppf_cmp_ui(&z.lo,threshold,ctx)<0){if(iv_recip_local(&recip,&z,ctx)||ppf_special_iv_add(&tmp,&shift,&recip,ctx)||ppf_special_iv_copy(&shift,&tmp,ctx)||iv_add_ui_local(&tmp,&z,1,ctx)||ppf_special_iv_copy(&z,&tmp,ctx))goto done;if(++count>100000)goto done;}
    if(ppf_special_iv_log_positive(&lnz,&z,ctx)||iv_recip_local(&recip,&z,ctx)||ppf_special_iv_scale2(&half,&recip,-1,ctx)||ppf_special_iv_sub(&partial,&lnz,&half,ctx)||ppf_special_iv_mul(&z2,&z,&z,ctx)||ppf_special_iv_copy(&power,&z2,ctx))goto done;
    maxm=(unsigned)(work/6u+16u);if(maxm<12)maxm=12;if(maxm>256)maxm=256;B=(mpq_t*)ppf_owned_buffer_calloc_internal(ctx,2u*maxm+1u,sizeof(mpq_t));if(!B||ppf_special_bernoulli(B,2u*maxm))goto done;
    for(unsigned k=1;k<=maxm;++k){mpq_t q;mpq_init(q);mpq_set(q,B[2u*k]);mpz_mul_ui(mpq_denref(q),mpq_denref(q),2u*k);mpq_neg(q,q);mpq_canonicalize(q);if(ppf_special_iv_set_q(&coeff,q,ctx)||ppf_special_iv_div(&next,&coeff,&power,ctx)){mpq_clear(q);goto done;}mpq_clear(q);if(ppf_special_iv_add(&cand,&partial,&next,ctx))goto done;if(ppf_special_small(&next.hi,work,28u)||k==maxm){if(iv_minmax_local(&base,&partial,&cand,ctx)||ppf_special_iv_sub(out,&base,&shift,ctx))goto done;rc=0;goto done;}if(ppf_special_iv_copy(&partial,&cand,ctx)||ppf_special_iv_mul(&tmp,&power,&z2,ctx)||ppf_special_iv_copy(&power,&tmp,ctx))goto done;}
done:if(B){ppf_special_bernoulli_clear(B,2u*maxm);ppf_owned_buffer_free_internal(B);}ppf_special_iv_clear(&z);ppf_special_iv_clear(&shift);ppf_special_iv_clear(&lnz);ppf_special_iv_clear(&half);ppf_special_iv_clear(&recip);ppf_special_iv_clear(&base);ppf_special_iv_clear(&z2);ppf_special_iv_clear(&power);ppf_special_iv_clear(&coeff);ppf_special_iv_clear(&next);ppf_special_iv_clear(&cand);ppf_special_iv_clear(&partial);ppf_special_iv_clear(&tmp);return rc;
}

static int digamma_positive_interval(ppf_special_iv_t *out, const ppf_t *x,
                                     ppf_prec_t work, ppf_context_t *ctx) {
    ppf_special_iv_t input;
    int rc = -1;
    memset(&input, 0, sizeof(input));
    if (ppf_special_iv_init(&input, work, ctx) ||
        ppf_special_iv_set_exact(&input, x, ctx)) goto done;
    rc = digamma_positive_input_interval(out, &input, work, ctx);
done:
    ppf_special_iv_clear(&input);
    return rc;
}

static int build_digamma(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){
    unary_arg_t*a=opaque;if(a->x->sign>0)return digamma_positive_interval(out,a->x,work,ctx);
    if(ppf_integer_p(a->x))return -1;
    ppf_special_iv_t y,d,sp,cp,cot,pi,prod,tmp;int rc=-1;
    memset(&y,0,sizeof(y));memset(&d,0,sizeof(d));memset(&sp,0,sizeof(sp));memset(&cp,0,sizeof(cp));memset(&cot,0,sizeof(cot));memset(&pi,0,sizeof(pi));memset(&prod,0,sizeof(prod));memset(&tmp,0,sizeof(tmp));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(y);INIT(d);INIT(sp);INIT(cp);INIT(cot);INIT(pi);INIT(prod);INIT(tmp);
#undef INIT
    if (ppf_special_iv_set_ui(&y, 1, ctx) ||
        ppf_special_iv_set_exact(&tmp, a->x, ctx) ||
        ppf_special_iv_sub(&y, &y, &tmp, ctx) ||
        digamma_positive_input_interval(&d, &y, work, ctx) ||
        iv_sinpi_exact(&sp, a->x, ctx) || iv_cospi_exact(&cp, a->x, ctx) ||
        ppf_special_iv_div(&cot, &cp, &sp, ctx) ||
        ppf_special_pi_interval(&pi, ctx) ||
        ppf_special_iv_mul(&prod, &pi, &cot, ctx) ||
        ppf_special_iv_sub(out, &d, &prod, ctx)) goto done;
    rc = 0;
done:ppf_special_iv_clear(&y);ppf_special_iv_clear(&d);ppf_special_iv_clear(&sp);ppf_special_iv_clear(&cp);ppf_special_iv_clear(&cot);ppf_special_iv_clear(&pi);ppf_special_iv_clear(&prod);ppf_special_iv_clear(&tmp);return rc;
}
int ppf_digamma(ppf_t*rop,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){unary_arg_t a={x};ctx=ppf_resolve_context(ctx);if(!rop||!x||!ctx)return sp_error(rop,ctx);if(ppf_nan_p(x)||(ppf_inf_p(x)&&x->sign<0))return sp_nan(rop,ctx);if(ppf_inf_p(x)){ppf_set_inf(rop,1);return 0;}if(ppf_zero_p(x)){ppf_set_inf(rop,-x->sign);ppf_raise(ctx,PPF_FLAG_DIVBY0);return 0;}if(x->sign<0&&ppf_integer_p(x))return sp_nan(rop,ctx);return ppf_special_evaluate(rop,rnd,ctx,build_digamma,&a);}

typedef struct{const ppf_t*a,*b;} binary_arg_t;
static int build_beta(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){binary_arg_t*x=opaque;ppf_t sum;ppf_special_iv_t la,lb,ls,t,val,neg;int sa=0,sb=0,ss=0,sign,rc=-1;memset(&sum,0,sizeof(sum));memset(&la,0,sizeof(la));memset(&lb,0,sizeof(lb));memset(&ls,0,sizeof(ls));memset(&t,0,sizeof(t));memset(&val,0,sizeof(val));memset(&neg,0,sizeof(neg));if(ppf_init2_ctx(&sum,work,ctx))goto done;
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(la);INIT(lb);INIT(ls);INIT(t);INIT(val);INIT(neg);
#undef INIT
    /* A rounded point sum would lose the argument certificate. Retry until exact. */
    if (ppf_add(&sum, x->a, x->b, PPF_RNDN, ctx) != 0 ||
        lgamma_abs_interval(&la, &sa, x->a, work, ctx) ||
        lgamma_abs_interval(&lb, &sb, x->b, work, ctx) ||
        lgamma_abs_interval(&ls, &ss, &sum, work, ctx) ||
        ppf_special_iv_add(&t, &la, &lb, ctx) ||
        ppf_special_iv_sub(&t, &t, &ls, ctx) ||
        ppf_special_iv_exp(&val, &t, ctx)) goto done;
    sign = sa * sb * ss;
    if (sign < 0) {
        if (ppf_special_iv_neg(&neg, &val, ctx) ||
            ppf_special_iv_copy(out, &neg, ctx)) goto done;
    } else if (ppf_special_iv_copy(out, &val, ctx)) {
        goto done;
    }
    rc = 0;
done:if(sum.limbs)ppf_clear(&sum);ppf_special_iv_clear(&la);ppf_special_iv_clear(&lb);ppf_special_iv_clear(&ls);ppf_special_iv_clear(&t);ppf_special_iv_clear(&val);ppf_special_iv_clear(&neg);return rc;}
int ppf_beta(ppf_t *rop, const ppf_t *a, const ppf_t *b,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    binary_arg_t x = {a, b};
    ctx = ppf_resolve_context(ctx);
    if (!rop || !a || !b || !ctx) return sp_error(rop, ctx);
    if (ppf_nan_p(a) || ppf_nan_p(b)) return sp_nan(rop, ctx);
    /* Ambiguous finite pole combinations retain the documented error policy. */
    if ((a->sign < 0 && ppf_integer_p(a) && !ppf_zero_p(a)) ||
        (b->sign < 0 && ppf_integer_p(b) && !ppf_zero_p(b))) return sp_error(rop, ctx);
    if (ppf_cmp_ui(a, 1, ctx) == 0) return ppf_ui_div(rop, 1, b, rnd, ctx);
    if (ppf_cmp_ui(b, 1, ctx) == 0) return ppf_ui_div(rop, 1, a, rnd, ctx);
    if (ppf_inf_p(a) || ppf_inf_p(b)) {
        const ppf_t *infinite = ppf_inf_p(a) ? a : b;
        const ppf_t *other = ppf_inf_p(a) ? b : a;
        if (ppf_zero_p(other) || (ppf_inf_p(other) &&
            (infinite->sign < 0 || other->sign < 0))) return sp_nan(rop, ctx);
        if (infinite->sign > 0) {
            if (other->sign > 0) ppf_set_zero(rop, 1);
            else ppf_set_inf(rop, 1);
            return 0;
        }
        if (other->sign > 0 && ppf_integer_p(other)) {
            mpz_t integer;
            mpz_init(integer);
            /* Read parity from the stored dyadic significand, without expanding
               a potentially enormous integer solely to decide its sign. */
            ppf_exp_t exponent = ppf_get_z_2exp(integer, other);
            int odd = exponent > 0 ? 0 :
                      (int)mpz_tstbit(integer, (mp_bitcnt_t)(-exponent));
            int sign = odd ? -1 : 1;
            mpz_clear(integer); ppf_set_zero(rop, sign); return 0;
        }
        return sp_nan(rop, ctx);
    }
    if (ppf_zero_p(a) || ppf_zero_p(b)) {
        const ppf_t *zero = ppf_zero_p(a) ? a : b;
        if (ppf_zero_p(a) && ppf_zero_p(b) && a->sign != b->sign) return sp_nan(rop, ctx);
        ppf_set_inf(rop, zero->sign); ppf_raise(ctx, PPF_FLAG_DIVBY0); return 0;
    }
    if (a->sign > 0 && b->sign > 0 && ppf_integer_p(a) && ppf_integer_p(b) &&
        ppf_cmp_ui(a, 100000, ctx) <= 0 && ppf_cmp_ui(b, 100000, ctx) <= 0) {
        long ai, bi;
        if (!ppf_special_get_long_if_integer(a, &ai, ctx) ||
            !ppf_special_get_long_if_integer(b, &bi, ctx)) return sp_error(rop, ctx);
        if (ai + bi <= 100000L) {
            mpq_t exact;
            mpz_t factorial;
            mpq_init(exact); mpz_init(factorial);
            mpz_fac_ui(mpq_numref(exact), (unsigned long)(ai - 1));
            mpz_fac_ui(factorial, (unsigned long)(bi - 1));
            mpz_mul(mpq_numref(exact), mpq_numref(exact), factorial);
            mpz_fac_ui(mpq_denref(exact), (unsigned long)(ai + bi - 1));
            mpq_canonicalize(exact);
            int result = ppf_set_q(rop, exact, rnd, ctx);
            mpq_clear(exact); mpz_clear(factorial); return result;
        }
    }
    return ppf_special_evaluate(rop, rnd, ctx, build_beta, &x);
}

static int build_gamma_inc_positive(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){
    binary_arg_t*x=opaque;
    ppf_special_iv_t lg,gamma,lower,powx,expneg,sum,term,next,den,tmp,tail,one,ratio,aiv,xiv,negx;
    int rc=-1,sg=0,settled=0;
    memset(&lg,0,sizeof(lg));memset(&gamma,0,sizeof(gamma));memset(&lower,0,sizeof(lower));
    memset(&powx,0,sizeof(powx));memset(&expneg,0,sizeof(expneg));memset(&sum,0,sizeof(sum));
    memset(&term,0,sizeof(term));memset(&next,0,sizeof(next));memset(&den,0,sizeof(den));
    memset(&tmp,0,sizeof(tmp));memset(&tail,0,sizeof(tail));memset(&one,0,sizeof(one));
    memset(&ratio,0,sizeof(ratio));memset(&aiv,0,sizeof(aiv));memset(&xiv,0,sizeof(xiv));memset(&negx,0,sizeof(negx));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(lg);INIT(gamma);INIT(lower);INIT(powx);INIT(expneg);INIT(sum);INIT(term);INIT(next);
    INIT(den);INIT(tmp);INIT(tail);INIT(one);INIT(ratio);INIT(aiv);INIT(xiv);INIT(negx);
#undef INIT
    if(ppf_special_iv_set_exact(&aiv,x->a,ctx)||ppf_special_iv_set_exact(&xiv,x->b,ctx)||
       lgamma_abs_interval(&lg,&sg,x->a,work,ctx)||sg<0||ppf_special_iv_exp(&gamma,&lg,ctx))goto done;
    if(ppf_zero_p(x->b)){rc=ppf_special_iv_copy(out,&gamma,ctx);goto done;}
    if(iv_recip_local(&term,&aiv,ctx)||ppf_special_iv_copy(&sum,&term,ctx)||ppf_special_iv_set_ui(&one,1,ctx))goto done;
    for(unsigned long n=0;n<1000000UL;++n){
        if(iv_add_ui_local(&den,&aiv,n+1UL,ctx)||ppf_special_iv_mul(&tmp,&term,&xiv,ctx)||
           ppf_special_iv_div(&next,&tmp,&den,ctx)||ppf_special_iv_add(&tmp,&sum,&next,ctx)||
           ppf_special_iv_copy(&sum,&tmp,ctx))goto done;
        if(iv_add_ui_local(&den,&aiv,n+2UL,ctx)||ppf_special_iv_div(&ratio,&xiv,&den,ctx))goto done;
        if(ppf_cmp_ui(&ratio.hi,1UL,ctx)<0&&ppf_special_small(&next.hi,work,28u)){
            if(ppf_special_iv_sub(&den,&one,&ratio,ctx)||ppf_special_iv_mul(&tail,&next,&ratio,ctx)||
               ppf_special_iv_div(&tail,&tail,&den,ctx))goto done;
            if(ppf_add(&sum.hi,&sum.hi,&tail.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR)goto done;
            settled=1;break;
        }
        if(ppf_special_iv_copy(&term,&next,ctx))goto done;
    }
    if(!settled)goto done;
    if(ppf_special_iv_pow_positive(&powx,&xiv,&aiv,ctx)||ppf_special_iv_neg(&negx,&xiv,ctx)||
       ppf_special_iv_exp(&expneg,&negx,ctx)||ppf_special_iv_mul(&lower,&powx,&expneg,ctx)||
       ppf_special_iv_mul(&lower,&lower,&sum,ctx)||ppf_special_iv_sub(out,&gamma,&lower,ctx))goto done;
    if(ppf_sgn(&out->lo,ctx)<0)ppf_set_zero(&out->lo,1);
    rc=0;
done:
    ppf_special_iv_clear(&lg);ppf_special_iv_clear(&gamma);ppf_special_iv_clear(&lower);
    ppf_special_iv_clear(&powx);ppf_special_iv_clear(&expneg);ppf_special_iv_clear(&sum);
    ppf_special_iv_clear(&term);ppf_special_iv_clear(&next);ppf_special_iv_clear(&den);
    ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&tail);ppf_special_iv_clear(&one);
    ppf_special_iv_clear(&ratio);ppf_special_iv_clear(&aiv);ppf_special_iv_clear(&xiv);
    ppf_special_iv_clear(&negx);return rc;
}


static int gamma_inc_zero_interval(ppf_special_iv_t *out, const ppf_t *x,
                                   ppf_context_t *ctx) {
    ppf_t negx;
    ppf_special_iv_t ei;
    int rc = -1;
    memset(&negx, 0, sizeof(negx));
    memset(&ei, 0, sizeof(ei));
    if (ppf_init2_ctx(&negx, out->lo.precision, ctx) ||
        ppf_special_iv_init(&ei, out->lo.precision, ctx)) goto done;
    if (ppf_neg(&negx, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_eint(&ei.lo, &negx, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_eint(&ei.hi, &negx, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
        ppf_special_iv_neg(out, &ei, ctx)) goto done;
    rc = 0;
done:
    if (negx.limbs) ppf_clear(&negx);
    ppf_special_iv_clear(&ei);
    return rc;
}

static int build_gamma_inc(ppf_special_iv_t *out, ppf_prec_t work,
                           void *opaque, ppf_context_t *ctx) {
    binary_arg_t *arg = (binary_arg_t *)opaque;
    if (ppf_zero_p(arg->a)) return gamma_inc_zero_interval(out, arg->b, ctx);
    if (arg->a->sign > 0) return build_gamma_inc_positive(out, work, opaque, ctx);

    ppf_t neg_a, shift_integer, shifted_a, target, one;
    ppf_special_iv_t current, target_iv, x_iv, exp_neg_x, pow_x, term, tmp, neg_x;
    mpz_t steps_z;
    unsigned long steps = 0;
    int rc = -1;
    memset(&neg_a, 0, sizeof(neg_a)); memset(&shift_integer, 0, sizeof(shift_integer));
    memset(&shifted_a, 0, sizeof(shifted_a)); memset(&target, 0, sizeof(target));
    memset(&one, 0, sizeof(one)); memset(&current, 0, sizeof(current));
    memset(&target_iv, 0, sizeof(target_iv)); memset(&x_iv, 0, sizeof(x_iv));
    memset(&exp_neg_x, 0, sizeof(exp_neg_x)); memset(&pow_x, 0, sizeof(pow_x));
    memset(&term, 0, sizeof(term)); memset(&tmp, 0, sizeof(tmp));
    memset(&neg_x, 0, sizeof(neg_x)); mpz_init(steps_z);

    if (ppf_init2_ctx(&neg_a, work, ctx) || ppf_init2_ctx(&shift_integer, work, ctx) ||
        ppf_init2_ctx(&shifted_a, work, ctx) || ppf_init2_ctx(&target, work, ctx) ||
        ppf_init2_ctx(&one, work, ctx) ||
        ppf_special_iv_init(&current, work, ctx) ||
        ppf_special_iv_init(&target_iv, work, ctx) ||
        ppf_special_iv_init(&x_iv, work, ctx) ||
        ppf_special_iv_init(&exp_neg_x, work, ctx) ||
        ppf_special_iv_init(&pow_x, work, ctx) ||
        ppf_special_iv_init(&term, work, ctx) ||
        ppf_special_iv_init(&tmp, work, ctx) ||
        ppf_special_iv_init(&neg_x, work, ctx)) goto done;

    /* All scalar recurrence arguments must remain exact at this work precision. */
    if (ppf_neg(&neg_a, arg->a, PPF_RNDN, ctx) != 0 ||
        ppf_get_z(steps_z, &neg_a, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
        !mpz_fits_ulong_p(steps_z)) goto done;
    steps = mpz_get_ui(steps_z);
    if (steps > 100000UL) goto done;

    if (ppf_set_z(&shift_integer, steps_z, PPF_RNDN, ctx) != 0 ||
        ppf_add(&shifted_a, arg->a, &shift_integer, PPF_RNDN, ctx) != 0 ||
        ppf_set_ui(&one, 1, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_special_iv_set_exact(&x_iv, arg->b, ctx) ||
        ppf_special_iv_neg(&neg_x, &x_iv, ctx) ||
        ppf_special_iv_exp(&exp_neg_x, &neg_x, ctx)) goto done;

    if (ppf_zero_p(&shifted_a)) {
        if (gamma_inc_zero_interval(&current, arg->b, ctx)) goto done;
    } else {
        binary_arg_t positive = {&shifted_a, arg->b};
        if (build_gamma_inc_positive(&current, work, &positive, ctx)) goto done;
    }

    if (steps == 0) {
        rc = ppf_special_iv_copy(out, &current, ctx);
        goto done;
    }
    if (ppf_sub(&target, &shifted_a, &one, PPF_RNDN, ctx) != 0)
        goto done;

    for (unsigned long i = 0; i < steps; ++i) {
        if (ppf_zero_p(&target)) {
            if (gamma_inc_zero_interval(&current, arg->b, ctx)) goto done;
        } else {
            if (ppf_special_iv_set_exact(&target_iv, &target, ctx) ||
                ppf_special_iv_pow_positive(&pow_x, &x_iv, &target_iv, ctx) ||
                ppf_special_iv_mul(&term, &pow_x, &exp_neg_x, ctx) ||
                ppf_special_iv_sub(&tmp, &current, &term, ctx) ||
                ppf_special_iv_div(&current, &tmp, &target_iv, ctx)) goto done;
        }
        if (i + 1UL < steps &&
            ppf_sub(&target, &target, &one, PPF_RNDN, ctx) != 0)
            goto done;
    }
    rc = ppf_special_iv_copy(out, &current, ctx);

done:
    if (neg_a.limbs) ppf_clear(&neg_a);
    if (shift_integer.limbs) ppf_clear(&shift_integer);
    if (shifted_a.limbs) ppf_clear(&shifted_a);
    if (target.limbs) ppf_clear(&target);
    if (one.limbs) ppf_clear(&one);
    ppf_special_iv_clear(&current);
    ppf_special_iv_clear(&target_iv);
    ppf_special_iv_clear(&x_iv);
    ppf_special_iv_clear(&exp_neg_x);
    ppf_special_iv_clear(&pow_x);
    ppf_special_iv_clear(&term);
    ppf_special_iv_clear(&tmp);
    ppf_special_iv_clear(&neg_x);
    mpz_clear(steps_z);
    return rc;
}

int ppf_gamma_inc(ppf_t *rop, const ppf_t *a, const ppf_t *x,
                  ppf_rnd_t rnd, ppf_context_t *ctx) {
    binary_arg_t arg = {a, x};
    ctx = ppf_resolve_context(ctx);
    if (!rop || !a || !x || !ctx) return sp_error(rop, ctx);
    if (ppf_nan_p(a) || ppf_nan_p(x) || (x->sign < 0 && !ppf_zero_p(x)))
        return sp_nan(rop, ctx);
    if (ppf_inf_p(a)) {
        if (a->sign > 0) {
            if (ppf_inf_p(x)) return sp_nan(rop, ctx);
            ppf_set_inf(rop, 1); return 0;
        }
        if (ppf_zero_p(x)) return sp_nan(rop, ctx);
        ppf_set_zero(rop, 1); return 0;
    }
    if (ppf_zero_p(x)) return ppf_gamma(rop, a, rnd, ctx);
    if (ppf_inf_p(x)) { ppf_set_zero(rop, 1); return 0; }
    return ppf_special_evaluate(rop, rnd, ctx, build_gamma_inc, &arg);
}

