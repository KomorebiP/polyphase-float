/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "special_internal.h"

#include "transactional_special_names.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int sb_nan(ppf_t*r,ppf_context_t*ctx){ppf_set_nan(r,1);ppf_raise(ctx,PPF_FLAG_NAN);return 0;}
static int sb_error(ppf_t*r,ppf_context_t*ctx){ppf_set_nan(r,1);ppf_raise(ctx,PPF_FLAG_ERANGE);return PPF_TERNARY_ERROR;}
static int iv_minmax(ppf_special_iv_t*out,const ppf_special_iv_t*a,const ppf_special_iv_t*b,ppf_context_t*ctx){const ppf_t*lo=ppf_cmp(&a->lo,&b->lo,ctx)<=0?&a->lo:&b->lo;const ppf_t*hi=ppf_cmp(&a->hi,&b->hi,ctx)>=0?&a->hi:&b->hi;return ppf_set(&out->lo,lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||ppf_set(&out->hi,hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR?-1:0;}
static int iv_recip(ppf_special_iv_t*out,const ppf_special_iv_t*x,ppf_context_t*ctx){ppf_special_iv_t one;int rc=-1;memset(&one,0,sizeof(one));if(ppf_special_iv_init(&one,out->lo.precision,ctx)||ppf_special_iv_set_ui(&one,1,ctx))goto done;rc=ppf_special_iv_div(out,&one,x,ctx);done:ppf_special_iv_clear(&one);return rc;}


/* Enlarge an enclosure by factor * |tail_term| on both sides. */
static int iv_widen_symmetric(
    ppf_special_iv_t *value,
    const ppf_special_iv_t *tail_term,
    unsigned long factor,
    ppf_context_t *ctx
) {
    ppf_special_iv_t magnitude;
    ppf_t radius, lower, upper, factor_value;
    int rc = -1;

    memset(&magnitude, 0, sizeof(magnitude));
    memset(&radius, 0, sizeof(radius));
    memset(&lower, 0, sizeof(lower));
    memset(&upper, 0, sizeof(upper));
    memset(&factor_value, 0, sizeof(factor_value));

    if (ppf_special_iv_init(&magnitude, value->lo.precision, ctx) ||
        ppf_init2_ctx(&radius, value->lo.precision, ctx) ||
        ppf_init2_ctx(&lower, value->lo.precision, ctx) ||
        ppf_init2_ctx(&upper, value->hi.precision, ctx) ||
        ppf_init2_ctx(&factor_value, value->hi.precision, ctx)) {
        goto done;
    }
    if (ppf_special_iv_abs(&magnitude, tail_term, ctx) ||
        ppf_set_ui(&factor_value, factor, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_mul(&radius, &magnitude.hi, &factor_value, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
        ppf_sub(&lower, &value->lo, &radius, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_add(&upper, &value->hi, &radius, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&value->lo, &lower, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&value->hi, &upper, PPF_RNDU, ctx) == PPF_TERNARY_ERROR) {
        goto done;
    }
    rc = 0;

done:
    ppf_special_iv_clear(&magnitude);
    if (radius.limbs) ppf_clear(&radius);
    if (lower.limbs) ppf_clear(&lower);
    if (upper.limbs) ppf_clear(&upper);
    if (factor_value.limbs) ppf_clear(&factor_value);
    return rc;
}

/* Exact sufficient test for 4*x <= numerator, with x >= 0. */
static int iv_four_upper_le_ui(
    const ppf_special_iv_t *x,
    unsigned long numerator,
    ppf_context_t *ctx
) {
    return ppf_cmp_ui_2exp(&x->hi, numerator, -2, ctx) <= 0;
}

/* Exact sufficient test for 2*|x| <= numerator. */
static int iv_twice_abs_upper_le_ui(
    const ppf_special_iv_t *x,
    unsigned long numerator,
    ppf_context_t *ctx
) {
    ppf_special_iv_t magnitude;
    int result = 0;
    memset(&magnitude, 0, sizeof(magnitude));
    if (ppf_special_iv_init(&magnitude, x->lo.precision, ctx) == 0 &&
        ppf_special_iv_abs(&magnitude, x, ctx) == 0) {
        result = ppf_cmp_ui_2exp(&magnitude.hi, numerator, -1, ctx) <= 0;
    }
    ppf_special_iv_clear(&magnitude);
    return result;
}

/* AGM. */
typedef struct{const ppf_t*a,*b;} binary_arg_t;
static int build_agm(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){
    binary_arg_t*x=opaque;ppf_special_iv_t a,b,an,gn,tmp,prod,diff;int rc=-1;
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));memset(&an,0,sizeof(an));memset(&gn,0,sizeof(gn));memset(&tmp,0,sizeof(tmp));memset(&prod,0,sizeof(prod));memset(&diff,0,sizeof(diff));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(a);INIT(b);INIT(an);INIT(gn);INIT(tmp);INIT(prod);INIT(diff);
#undef INIT
    if(ppf_special_iv_set_exact(&a,x->a,ctx)||ppf_special_iv_set_exact(&b,x->b,ctx))goto done;
    if(ppf_cmp(&a.lo,&b.lo,ctx)<0){
        ppf_swap(&a.lo,&b.lo);
        ppf_swap(&a.hi,&b.hi);
    }
    for(unsigned k=0;k<128;++k){
        if(ppf_special_iv_add(&an,&a,&b,ctx)||ppf_special_iv_scale2(&an,&an,-1,ctx)||ppf_special_iv_mul(&prod,&a,&b,ctx)||ppf_special_iv_sqrt(&gn,&prod,ctx)||ppf_special_iv_sub(&diff,&an,&gn,ctx))goto done;
        if(ppf_special_small(&diff.hi,work>32u?work-32u:work,0u)){if(ppf_set(&out->lo,&gn.lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||ppf_set(&out->hi,&an.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR)goto done;rc=0;goto done;}
        if(ppf_special_iv_copy(&a,&an,ctx)||ppf_special_iv_copy(&b,&gn,ctx))goto done;
    }
done:ppf_special_iv_clear(&a);ppf_special_iv_clear(&b);ppf_special_iv_clear(&an);ppf_special_iv_clear(&gn);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&prod);ppf_special_iv_clear(&diff);return rc;
}
int ppf_agm(ppf_t*rop,const ppf_t*a,const ppf_t*b,ppf_rnd_t rnd,ppf_context_t*ctx){binary_arg_t x={a,b};ctx=ppf_resolve_context(ctx);if(!rop||!a||!b||!ctx)return sb_error(rop,ctx);if(ppf_nan_p(a)||ppf_nan_p(b))return sb_nan(rop,ctx);if((a->sign<0&&!ppf_zero_p(a))||(b->sign<0&&!ppf_zero_p(b)))return sb_nan(rop,ctx);if(ppf_zero_p(a)||ppf_zero_p(b)){if(ppf_inf_p(a)||ppf_inf_p(b))return sb_nan(rop,ctx);ppf_set_zero(rop,1);return 0;}if(ppf_inf_p(a)||ppf_inf_p(b)){ppf_set_inf(rop,1);return 0;}return ppf_special_evaluate(rop,rnd,ctx,build_agm,&x);}

/* Bessel J_n power series. */
typedef struct{const ppf_t*x;long n;} order_arg_t;
static int bessel_j_interval(ppf_special_iv_t*out,const ppf_t*x,long order,ppf_prec_t work,ppf_context_t*ctx){
    unsigned long n=(unsigned long)(order<0?-(order+1)+1:order);int order_sign=(order<0&&(n&1u))?-1:1;
    ppf_special_iv_t xi,half,x2,term,sum,next,tmp,ratio,cand,faciv,powiv,neg;int rc=-1,settled=0;
    memset(&xi,0,sizeof(xi));memset(&half,0,sizeof(half));memset(&x2,0,sizeof(x2));memset(&term,0,sizeof(term));memset(&sum,0,sizeof(sum));memset(&next,0,sizeof(next));memset(&tmp,0,sizeof(tmp));memset(&ratio,0,sizeof(ratio));memset(&cand,0,sizeof(cand));memset(&faciv,0,sizeof(faciv));memset(&powiv,0,sizeof(powiv));memset(&neg,0,sizeof(neg));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(xi);INIT(half);INIT(x2);INIT(term);INIT(sum);INIT(next);INIT(tmp);INIT(ratio);INIT(cand);INIT(faciv);INIT(powiv);INIT(neg);
#undef INIT
    if(ppf_special_iv_set_exact(&xi,x,ctx)||ppf_special_iv_scale2(&half,&xi,-1,ctx)||ppf_special_iv_mul(&x2,&half,&half,ctx))goto done;
    mpz_t fac;mpz_init(fac);mpz_fac_ui(fac,n);if(ppf_pow_ui(&powiv.lo,&half.lo,n,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||ppf_pow_ui(&powiv.hi,&half.hi,n,PPF_RNDU,ctx)==PPF_TERNARY_ERROR||ppf_set_z(&faciv.lo,fac,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_set_z(&faciv.hi,fac,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_special_iv_div(&term,&powiv,&faciv,ctx)||ppf_special_iv_copy(&sum,&term,ctx)){mpz_clear(fac);goto done;}mpz_clear(fac);
    for(unsigned long k=0;k<1000000UL;++k){mpq_t q;mpq_init(q);mpq_set_si(q,-1,(long)((k+1UL)*(k+n+1UL)));if(ppf_special_iv_set_q(&ratio,q,ctx)||ppf_special_iv_mul(&tmp,&term,&x2,ctx)||ppf_special_iv_mul(&next,&tmp,&ratio,ctx)){mpq_clear(q);goto done;}mpq_clear(q);ppf_special_iv_t an;memset(&an,0,sizeof(an));if(ppf_special_iv_init(&an,work,ctx)||ppf_special_iv_abs(&an,&next,ctx)){ppf_special_iv_clear(&an);goto done;}if(ppf_special_small(&an.hi,work,28u)){unsigned long kp2=k+2UL;unsigned long np2=k+n+2UL;if(kp2<=ULONG_MAX/np2&&ppf_cmp_ui(&x2.hi,kp2*np2,ctx)<=0){if(ppf_special_iv_add(&cand,&sum,&next,ctx)||iv_minmax(&sum,&sum,&cand,ctx)){ppf_special_iv_clear(&an);goto done;}ppf_special_iv_clear(&an);settled=1;break;}}ppf_special_iv_clear(&an);if(ppf_special_iv_add(&tmp,&sum,&next,ctx)||ppf_special_iv_copy(&sum,&tmp,ctx)||ppf_special_iv_copy(&term,&next,ctx))goto done;}
    if (!settled) goto done;
    if (order_sign < 0) {
        if (ppf_special_iv_neg(&neg, &sum, ctx) ||
            ppf_special_iv_copy(out, &neg, ctx)) goto done;
    } else if (ppf_special_iv_copy(out, &sum, ctx)) {
        goto done;
    }
    rc = 0;
done:ppf_special_iv_clear(&xi);ppf_special_iv_clear(&half);ppf_special_iv_clear(&x2);ppf_special_iv_clear(&term);ppf_special_iv_clear(&sum);ppf_special_iv_clear(&next);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&ratio);ppf_special_iv_clear(&cand);ppf_special_iv_clear(&faciv);ppf_special_iv_clear(&powiv);ppf_special_iv_clear(&neg);return rc;
}
static int build_jn(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){order_arg_t*a=opaque;return bessel_j_interval(out,a->x,a->n,work,ctx);}
int ppf_jn(ppf_t*rop,long n,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){order_arg_t a={x,n};ctx=ppf_resolve_context(ctx);if(!rop||!x||!ctx)return sb_error(rop,ctx);if(ppf_nan_p(x))return sb_nan(rop,ctx);if(ppf_inf_p(x)){ppf_set_zero(rop,1);return 0;}if(ppf_zero_p(x)){int sign=1;if(n!=0&&x->sign<0&&((unsigned long)(n<0?-n:n)&1u))sign=-1;ppf_set_zero(rop,sign);return n==0?ppf_set_ui(rop,1,rnd,ctx):0;}return ppf_special_evaluate(rop,rnd,ctx,build_jn,&a);}
int ppf_j0(ppf_t*r,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){return ppf_jn(r,0,x,rnd,ctx);}int ppf_j1(ppf_t*r,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){return ppf_jn(r,1,x,rnd,ctx);}

static int harmonic_iv(ppf_special_iv_t*out,unsigned long n,ppf_context_t*ctx){mpq_t q;mpq_init(q);mpq_set_ui(q,0,1);for(unsigned long k=1;k<=n;++k){mpq_t t;mpq_init(t);mpq_set_ui(t,1,k);mpq_add(q,q,t);mpq_clear(t);}int rc=ppf_special_iv_set_q(out,q,ctx);mpq_clear(q);return rc;}

static int y0_interval(ppf_special_iv_t*out,const ppf_t*x,ppf_prec_t work,ppf_context_t*ctx){
    ppf_special_iv_t xi,half,x2,j,ln,gam,base,pi,twopi,fac,sum,term,next,tmp,h,cand;int rc=-1,settled=0;
    memset(&xi,0,sizeof(xi));memset(&half,0,sizeof(half));memset(&x2,0,sizeof(x2));memset(&j,0,sizeof(j));memset(&ln,0,sizeof(ln));memset(&gam,0,sizeof(gam));memset(&base,0,sizeof(base));memset(&pi,0,sizeof(pi));memset(&twopi,0,sizeof(twopi));memset(&fac,0,sizeof(fac));memset(&sum,0,sizeof(sum));memset(&term,0,sizeof(term));memset(&next,0,sizeof(next));memset(&tmp,0,sizeof(tmp));memset(&h,0,sizeof(h));memset(&cand,0,sizeof(cand));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(xi);INIT(half);INIT(x2);INIT(j);INIT(ln);INIT(gam);INIT(base);INIT(pi);INIT(twopi);INIT(fac);INIT(sum);INIT(term);INIT(next);INIT(tmp);INIT(h);INIT(cand);
#undef INIT
    if(ppf_special_iv_set_exact(&xi,x,ctx)||ppf_special_iv_scale2(&half,&xi,-1,ctx)||ppf_special_iv_mul(&x2,&half,&half,ctx)||bessel_j_interval(&j,x,0,work,ctx)||ppf_special_iv_log_positive(&ln,&half,ctx)||ppf_special_euler_interval(&gam,work,ctx)||ppf_special_iv_add(&base,&ln,&gam,ctx)||ppf_special_iv_mul(&base,&base,&j,ctx)||ppf_special_pi_interval(&pi,ctx)||iv_recip(&fac,&pi,ctx)||ppf_special_iv_scale2(&twopi,&fac,1,ctx)||ppf_special_iv_mul(&base,&base,&twopi,ctx)||ppf_special_iv_set_ui(&sum,0,ctx)||ppf_special_iv_set_ui(&term,1,ctx))goto done;
    for(unsigned long k=1;k<1000000UL;++k){mpq_t q;mpq_init(q);mpq_set_si(q,-1,(long)(k*k));if(ppf_special_iv_set_q(&h,q,ctx)||ppf_special_iv_mul(&tmp,&term,&x2,ctx)||ppf_special_iv_mul(&next,&tmp,&h,ctx)){mpq_clear(q);goto done;}mpq_clear(q);if(harmonic_iv(&h,k,ctx)||ppf_special_iv_mul(&tmp,&next,&h,ctx)||ppf_special_iv_add(&cand,&sum,&tmp,ctx)||ppf_special_iv_copy(&sum,&cand,ctx))goto done;ppf_special_iv_t an;memset(&an,0,sizeof(an));if(ppf_special_iv_init(&an,work,ctx)||ppf_special_iv_abs(&an,&tmp,ctx)){ppf_special_iv_clear(&an);goto done;}if(ppf_special_small(&an.hi,work,28u)){unsigned long kp1=k+1UL;if(kp1<=ULONG_MAX/kp1&&iv_four_upper_le_ui(&x2,kp1*kp1,ctx)){if(iv_widen_symmetric(&sum,&tmp,2UL,ctx)){ppf_special_iv_clear(&an);goto done;}ppf_special_iv_clear(&an);settled=1;break;}}ppf_special_iv_clear(&an);if(ppf_special_iv_copy(&term,&next,ctx))goto done;}
    if (!settled || ppf_special_iv_scale2(&sum, &sum, 1, ctx) ||
        ppf_special_iv_mul(&sum, &sum, &fac, ctx) ||
        ppf_special_iv_sub(out, &base, &sum, ctx)) goto done;
    rc = 0;
done:ppf_special_iv_clear(&xi);ppf_special_iv_clear(&half);ppf_special_iv_clear(&x2);ppf_special_iv_clear(&j);ppf_special_iv_clear(&ln);ppf_special_iv_clear(&gam);ppf_special_iv_clear(&base);ppf_special_iv_clear(&pi);ppf_special_iv_clear(&twopi);ppf_special_iv_clear(&fac);ppf_special_iv_clear(&sum);ppf_special_iv_clear(&term);ppf_special_iv_clear(&next);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&h);ppf_special_iv_clear(&cand);return rc;
}

static int y1_interval(ppf_special_iv_t*out,const ppf_t*x,ppf_prec_t work,ppf_context_t*ctx){
    ppf_special_iv_t xi,half,x2,j,ln,gam,base,pi,fac,sum,term,next,tmp,h,h2,cand,finite;int rc=-1,settled=0;
    memset(&xi,0,sizeof(xi));memset(&half,0,sizeof(half));memset(&x2,0,sizeof(x2));memset(&j,0,sizeof(j));memset(&ln,0,sizeof(ln));memset(&gam,0,sizeof(gam));memset(&base,0,sizeof(base));memset(&pi,0,sizeof(pi));memset(&fac,0,sizeof(fac));memset(&sum,0,sizeof(sum));memset(&term,0,sizeof(term));memset(&next,0,sizeof(next));memset(&tmp,0,sizeof(tmp));memset(&h,0,sizeof(h));memset(&h2,0,sizeof(h2));memset(&cand,0,sizeof(cand));memset(&finite,0,sizeof(finite));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(xi);INIT(half);INIT(x2);INIT(j);INIT(ln);INIT(gam);INIT(base);INIT(pi);INIT(fac);INIT(sum);INIT(term);INIT(next);INIT(tmp);INIT(h);INIT(h2);INIT(cand);INIT(finite);
#undef INIT
    if(ppf_special_iv_set_exact(&xi,x,ctx)||ppf_special_iv_scale2(&half,&xi,-1,ctx)||ppf_special_iv_mul(&x2,&half,&half,ctx)||bessel_j_interval(&j,x,1,work,ctx)||ppf_special_iv_log_positive(&ln,&half,ctx)||ppf_special_euler_interval(&gam,work,ctx)||ppf_special_iv_add(&base,&ln,&gam,ctx)||ppf_special_iv_mul(&base,&base,&j,ctx)||ppf_special_pi_interval(&pi,ctx)||iv_recip(&fac,&pi,ctx)||ppf_special_iv_scale2(&base,&base,1,ctx)||ppf_special_iv_mul(&base,&base,&fac,ctx)||ppf_special_iv_copy(&term,&half,ctx)||ppf_special_iv_set_ui(&sum,0,ctx))goto done;
    for(unsigned long k=0;k<1000000UL;++k){if(harmonic_iv(&h,k,ctx)||harmonic_iv(&h2,k+1,ctx)||ppf_special_iv_add(&h,&h,&h2,ctx)||ppf_special_iv_mul(&tmp,&term,&h,ctx)){goto done;}if((k&1u)==0){ppf_special_iv_t nt;memset(&nt,0,sizeof(nt));if(ppf_special_iv_init(&nt,work,ctx)||ppf_special_iv_neg(&nt,&tmp,ctx)||ppf_special_iv_add(&cand,&sum,&nt,ctx)||ppf_special_iv_copy(&sum,&cand,ctx)){ppf_special_iv_clear(&nt);goto done;}ppf_special_iv_clear(&nt);}else if(ppf_special_iv_add(&cand,&sum,&tmp,ctx)||ppf_special_iv_copy(&sum,&cand,ctx))goto done;ppf_special_iv_t an;memset(&an,0,sizeof(an));if(ppf_special_iv_init(&an,work,ctx)||ppf_special_iv_abs(&an,&tmp,ctx)){ppf_special_iv_clear(&an);goto done;}if(k>0&&ppf_special_small(&an.hi,work,28u)){unsigned long kp1=k+1UL;unsigned long kp2=k+2UL;if(kp1<=ULONG_MAX/kp2&&iv_four_upper_le_ui(&x2,kp1*kp2,ctx)){if(iv_widen_symmetric(&sum,&tmp,2UL,ctx)){ppf_special_iv_clear(&an);goto done;}ppf_special_iv_clear(&an);settled=1;break;}}ppf_special_iv_clear(&an);mpq_t q;mpq_init(q);mpq_set_ui(q,1,(k+1UL)*(k+2UL));if(ppf_special_iv_set_q(&h,q,ctx)||ppf_special_iv_mul(&next,&term,&x2,ctx)||ppf_special_iv_mul(&next,&next,&h,ctx)||ppf_special_iv_copy(&term,&next,ctx)){mpq_clear(q);goto done;}mpq_clear(q);}
    if(!settled||ppf_special_iv_mul(&sum,&sum,&fac,ctx))goto done;
    /* finite term -2/(pi*x) */
    if (ppf_special_iv_mul(&finite, &pi, &xi, ctx) ||
        iv_recip(&finite, &finite, ctx) ||
        ppf_special_iv_scale2(&finite, &finite, 1, ctx) ||
        ppf_special_iv_add(&tmp, &base, &sum, ctx) ||
        ppf_special_iv_sub(out, &tmp, &finite, ctx)) goto done;
    rc = 0;
done:ppf_special_iv_clear(&xi);ppf_special_iv_clear(&half);ppf_special_iv_clear(&x2);ppf_special_iv_clear(&j);ppf_special_iv_clear(&ln);ppf_special_iv_clear(&gam);ppf_special_iv_clear(&base);ppf_special_iv_clear(&pi);ppf_special_iv_clear(&fac);ppf_special_iv_clear(&sum);ppf_special_iv_clear(&term);ppf_special_iv_clear(&next);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&h);ppf_special_iv_clear(&h2);ppf_special_iv_clear(&cand);ppf_special_iv_clear(&finite);return rc;
}

static int yn_interval(ppf_special_iv_t*out,const ppf_t*x,long order,ppf_prec_t work,ppf_context_t*ctx){
    unsigned long n=(unsigned long)(order<0?-(order+1)+1:order);int sign=(order<0&&(n&1u))?-1:1;ppf_special_iv_t y0,y1,ym1,ycur,ynext,xi,coef,tmp,neg;int rc=-1;
    memset(&y0,0,sizeof(y0));memset(&y1,0,sizeof(y1));memset(&ym1,0,sizeof(ym1));memset(&ycur,0,sizeof(ycur));memset(&ynext,0,sizeof(ynext));memset(&xi,0,sizeof(xi));memset(&coef,0,sizeof(coef));memset(&tmp,0,sizeof(tmp));memset(&neg,0,sizeof(neg));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(y0);INIT(y1);INIT(ym1);INIT(ycur);INIT(ynext);INIT(xi);INIT(coef);INIT(tmp);INIT(neg);
#undef INIT
    if (y0_interval(&y0, x, work, ctx)) goto done;
    if (n == 0) {
        if (ppf_special_iv_copy(&ycur, &y0, ctx)) goto done;
        goto finish;
    }
    if (y1_interval(&y1, x, work, ctx)) goto done;
    if (n == 1) {
        if (ppf_special_iv_copy(&ycur, &y1, ctx)) goto done;
        goto finish;
    }
    if (ppf_special_iv_set_exact(&xi, x, ctx) ||
        ppf_special_iv_copy(&ym1, &y0, ctx) ||
        ppf_special_iv_copy(&ycur, &y1, ctx)) goto done;
    for(unsigned long k=1;k<n;++k){if(ppf_special_iv_set_ui(&coef,2*k,ctx)||ppf_special_iv_div(&coef,&coef,&xi,ctx)||ppf_special_iv_mul(&tmp,&coef,&ycur,ctx)||ppf_special_iv_sub(&ynext,&tmp,&ym1,ctx)||ppf_special_iv_copy(&ym1,&ycur,ctx)||ppf_special_iv_copy(&ycur,&ynext,ctx))goto done;}
finish:if(sign<0){if(ppf_special_iv_neg(&neg,&ycur,ctx)||ppf_special_iv_copy(out,&neg,ctx))goto done;}else if(ppf_special_iv_copy(out,&ycur,ctx))goto done;rc=0;
done:ppf_special_iv_clear(&y0);ppf_special_iv_clear(&y1);ppf_special_iv_clear(&ym1);ppf_special_iv_clear(&ycur);ppf_special_iv_clear(&ynext);ppf_special_iv_clear(&xi);ppf_special_iv_clear(&coef);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&neg);return rc;
}
static int build_yn(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){order_arg_t*a=opaque;return yn_interval(out,a->x,a->n,work,ctx);}
int ppf_yn(ppf_t*rop,long n,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){order_arg_t a={x,n};ctx=ppf_resolve_context(ctx);if(!rop||!x||!ctx)return sb_error(rop,ctx);if(ppf_nan_p(x)||(x->sign<0&&!ppf_zero_p(x)))return sb_nan(rop,ctx);if(ppf_inf_p(x)){ppf_set_zero(rop,1);return 0;}if(ppf_zero_p(x)){unsigned long an=(unsigned long)(n<0?-(n+1L)+1L:n);int sign=(n<0&&(an&1u))?1:-1;ppf_set_inf(rop,sign);ppf_raise(ctx,PPF_FLAG_DIVBY0);return 0;}return ppf_special_evaluate(rop,rnd,ctx,build_yn,&a);}int ppf_y0(ppf_t*r,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){return ppf_yn(r,0,x,rnd,ctx);}int ppf_y1(ppf_t*r,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){return ppf_yn(r,1,x,rnd,ctx);}

/* Airy Ai power series. */
static int build_ai(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){
    order_arg_t*a=opaque;ppf_special_iv_t xi,x3,three,third,two_third,g13,g23,p13,p23,a0,a1,s0,s1,t0,t1,n0,n1,tmp,ratio,tail0,tail1,one,den;
    int rc=-1,set0=0,set1=0;memset(&xi,0,sizeof(xi));memset(&x3,0,sizeof(x3));memset(&three,0,sizeof(three));memset(&third,0,sizeof(third));memset(&two_third,0,sizeof(two_third));memset(&g13,0,sizeof(g13));memset(&g23,0,sizeof(g23));memset(&p13,0,sizeof(p13));memset(&p23,0,sizeof(p23));memset(&a0,0,sizeof(a0));memset(&a1,0,sizeof(a1));memset(&s0,0,sizeof(s0));memset(&s1,0,sizeof(s1));memset(&t0,0,sizeof(t0));memset(&t1,0,sizeof(t1));memset(&n0,0,sizeof(n0));memset(&n1,0,sizeof(n1));memset(&tmp,0,sizeof(tmp));memset(&ratio,0,sizeof(ratio));memset(&tail0,0,sizeof(tail0));memset(&tail1,0,sizeof(tail1));memset(&one,0,sizeof(one));memset(&den,0,sizeof(den));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(xi);INIT(x3);INIT(three);INIT(third);INIT(two_third);INIT(g13);INIT(g23);INIT(p13);INIT(p23);INIT(a0);INIT(a1);INIT(s0);INIT(s1);INIT(t0);INIT(t1);INIT(n0);INIT(n1);INIT(tmp);INIT(ratio);INIT(tail0);INIT(tail1);INIT(one);INIT(den);
#undef INIT
    if(ppf_special_iv_set_exact(&xi,a->x,ctx)||ppf_special_iv_mul(&x3,&xi,&xi,ctx)||ppf_special_iv_mul(&x3,&x3,&xi,ctx)||ppf_special_iv_set_ui(&three,3,ctx))goto done;
    mpq_t q;mpq_init(q);mpq_set_ui(q,1,3);if(ppf_special_iv_set_q(&third,q,ctx)){mpq_clear(q);goto done;}mpq_set_ui(q,2,3);if(ppf_special_iv_set_q(&two_third,q,ctx)){mpq_clear(q);goto done;}mpq_clear(q);
    if(ppf_gamma(&g13.lo,&third.lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||ppf_gamma(&g13.hi,&third.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR||ppf_gamma(&g23.lo,&two_third.lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||ppf_gamma(&g23.hi,&two_third.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR||ppf_special_iv_pow_positive(&p13,&three,&third,ctx)||ppf_special_iv_pow_positive(&p23,&three,&two_third,ctx)||ppf_special_iv_mul(&a0,&p23,&g23,ctx)||iv_recip(&a0,&a0,ctx)||ppf_special_iv_mul(&a1,&p13,&g13,ctx)||iv_recip(&a1,&a1,ctx)||ppf_special_iv_neg(&a1,&a1,ctx)||ppf_special_iv_copy(&t0,&a0,ctx)||ppf_special_iv_mul(&t1,&a1,&xi,ctx)||ppf_special_iv_copy(&s0,&t0,ctx)||ppf_special_iv_copy(&s1,&t1,ctx)||ppf_special_iv_set_ui(&one,1,ctx))goto done;
    for(unsigned long k=0;k<1000000UL;++k){
        mpq_t q0,q1;mpq_inits(q0,q1,NULL);mpq_set_ui(q0,1,(3*k+3UL)*(3*k+2UL));mpq_set_ui(q1,1,(3*k+4UL)*(3*k+3UL));if(ppf_special_iv_set_q(&ratio,q0,ctx)||ppf_special_iv_mul(&n0,&t0,&x3,ctx)||ppf_special_iv_mul(&n0,&n0,&ratio,ctx)){mpq_clears(q0,q1,NULL);goto done;}if(ppf_special_iv_set_q(&ratio,q1,ctx)||ppf_special_iv_mul(&n1,&t1,&x3,ctx)||ppf_special_iv_mul(&n1,&n1,&ratio,ctx)){mpq_clears(q0,q1,NULL);goto done;}mpq_clears(q0,q1,NULL);
        ppf_special_iv_t an0,an1;memset(&an0,0,sizeof(an0));memset(&an1,0,sizeof(an1));if(ppf_special_iv_init(&an0,work,ctx)||ppf_special_iv_init(&an1,work,ctx)||ppf_special_iv_abs(&an0,&n0,ctx)||ppf_special_iv_abs(&an1,&n1,ctx)){ppf_special_iv_clear(&an0);ppf_special_iv_clear(&an1);goto done;}
        if(ppf_special_small(&an0.hi,work,28u)){unsigned long d0a=3UL*k+6UL;unsigned long d0b=3UL*k+5UL;if(d0a<=ULONG_MAX/d0b&&iv_twice_abs_upper_le_ui(&x3,d0a*d0b,ctx)){if(iv_widen_symmetric(&s0,&n0,2UL,ctx)){ppf_special_iv_clear(&an0);ppf_special_iv_clear(&an1);goto done;}set0=1;}}if(!set0&& (ppf_special_iv_add(&tmp,&s0,&n0,ctx)||ppf_special_iv_copy(&s0,&tmp,ctx)||ppf_special_iv_copy(&t0,&n0,ctx))){ppf_special_iv_clear(&an0);ppf_special_iv_clear(&an1);goto done;}
        if(ppf_special_small(&an1.hi,work,28u)){unsigned long d1a=3UL*k+7UL;unsigned long d1b=3UL*k+6UL;if(d1a<=ULONG_MAX/d1b&&iv_twice_abs_upper_le_ui(&x3,d1a*d1b,ctx)){if(iv_widen_symmetric(&s1,&n1,2UL,ctx)){ppf_special_iv_clear(&an0);ppf_special_iv_clear(&an1);goto done;}set1=1;}}if(!set1&& (ppf_special_iv_add(&tmp,&s1,&n1,ctx)||ppf_special_iv_copy(&s1,&tmp,ctx)||ppf_special_iv_copy(&t1,&n1,ctx))){ppf_special_iv_clear(&an0);ppf_special_iv_clear(&an1);goto done;}
        ppf_special_iv_clear(&an0);ppf_special_iv_clear(&an1);if(set0&&set1)break;
    }
    if (!(set0 && set1) || ppf_special_iv_add(out, &s0, &s1, ctx)) goto done;
    rc = 0;
done:ppf_special_iv_clear(&xi);ppf_special_iv_clear(&x3);ppf_special_iv_clear(&three);ppf_special_iv_clear(&third);ppf_special_iv_clear(&two_third);ppf_special_iv_clear(&g13);ppf_special_iv_clear(&g23);ppf_special_iv_clear(&p13);ppf_special_iv_clear(&p23);ppf_special_iv_clear(&a0);ppf_special_iv_clear(&a1);ppf_special_iv_clear(&s0);ppf_special_iv_clear(&s1);ppf_special_iv_clear(&t0);ppf_special_iv_clear(&t1);ppf_special_iv_clear(&n0);ppf_special_iv_clear(&n1);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&ratio);ppf_special_iv_clear(&tail0);ppf_special_iv_clear(&tail1);ppf_special_iv_clear(&one);ppf_special_iv_clear(&den);return rc;
}
int ppf_ai(ppf_t*rop,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){order_arg_t a={x,0};ctx=ppf_resolve_context(ctx);if(!rop||!x||!ctx)return sb_error(rop,ctx);if(ppf_nan_p(x))return sb_nan(rop,ctx);if(ppf_inf_p(x)){ppf_set_zero(rop,1);return 0;}long double xd=ppf_get_ld(x,PPF_RNDN,ctx);if(fabsl(xd)>500.0L)return sb_error(rop,ctx);return ppf_special_evaluate(rop,rnd,ctx,build_ai,&a);}
