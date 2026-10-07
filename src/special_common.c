/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "special_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef PPF_SPECIAL_INITIAL_GUARD
#define PPF_SPECIAL_INITIAL_GUARD 96u
#endif
#ifndef PPF_SPECIAL_MAX_ATTEMPTS
#define PPF_SPECIAL_MAX_ATTEMPTS 12u
#endif

static int special_error(ppf_t *rop, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, 1);
    if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

int ppf_special_iv_init(ppf_special_iv_t *x, ppf_prec_t precision,
                        ppf_context_t *ctx) {
    if (!x) return -1;
    memset(x, 0, sizeof(*x));
    if (ppf_init2_ctx(&x->lo, precision, ctx) != 0) return -1;
    if (ppf_init2_ctx(&x->hi, precision, ctx) != 0) {
        ppf_clear(&x->lo);
        return -1;
    }
    x->initialized = 1;
    return 0;
}

void ppf_special_iv_clear(ppf_special_iv_t *x) {
    if (!x || !x->initialized) return;
    ppf_clear(&x->lo);
    ppf_clear(&x->hi);
    memset(x, 0, sizeof(*x));
}

int ppf_special_iv_set_exact(ppf_special_iv_t *out, const ppf_t *x,
                             ppf_context_t *ctx) {
    return ppf_set(&out->lo, x, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
           ppf_set(&out->hi, x, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
}

int ppf_special_iv_set_ui(ppf_special_iv_t *out, unsigned long x,
                          ppf_context_t *ctx) {
    return ppf_set_ui(&out->lo, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
           ppf_set_ui(&out->hi, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
}

int ppf_special_iv_set_si(ppf_special_iv_t *out, long x,
                          ppf_context_t *ctx) {
    return ppf_set_si(&out->lo, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
           ppf_set_si(&out->hi, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
}

int ppf_special_iv_set_q(ppf_special_iv_t *out, mpq_srcptr x,
                         ppf_context_t *ctx) {
    return ppf_set_q(&out->lo, x, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
           ppf_set_q(&out->hi, x, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
}

int ppf_special_iv_copy(ppf_special_iv_t *out, const ppf_special_iv_t *x,
                        ppf_context_t *ctx) {
    return ppf_set(&out->lo, &x->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
           ppf_set(&out->hi, &x->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
}

static int iv_commit(ppf_special_iv_t *out, ppf_special_iv_t *tmp,
                     int status) {
    if (status == 0) {
        ppf_swap(&out->lo, &tmp->lo);
        ppf_swap(&out->hi, &tmp->hi);
    }
    ppf_special_iv_clear(tmp);
    return status;
}

int ppf_special_iv_add(ppf_special_iv_t *out,
                       const ppf_special_iv_t *a,
                       const ppf_special_iv_t *b,
                       ppf_context_t *ctx) {
    if (out == a || out == b) {
        ppf_special_iv_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        if (ppf_special_iv_init(&tmp, out->lo.precision, ctx) != 0) return -1;
        return iv_commit(out, &tmp,
            ppf_add(&tmp.lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_add(&tmp.hi, &a->hi, &b->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0);
    }
    return ppf_add(&out->lo, &a->lo, &b->lo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
           ppf_add(&out->hi, &a->hi, &b->hi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
}

int ppf_special_iv_sub(ppf_special_iv_t *out,
                       const ppf_special_iv_t *a,
                       const ppf_special_iv_t *b,
                       ppf_context_t *ctx) {
    if (out == a || out == b) {
        ppf_special_iv_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        if (ppf_special_iv_init(&tmp, out->lo.precision, ctx) != 0) return -1;
        return iv_commit(out, &tmp,
            ppf_sub(&tmp.lo, &a->lo, &b->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_sub(&tmp.hi, &a->hi, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0);
    }
    return ppf_sub(&out->lo, &a->lo, &b->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
           ppf_sub(&out->hi, &a->hi, &b->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
}

int ppf_special_iv_neg(ppf_special_iv_t *out,
                       const ppf_special_iv_t *x,
                       ppf_context_t *ctx) {
    if (out == x) {
        ppf_special_iv_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        if (ppf_special_iv_init(&tmp, out->lo.precision, ctx) != 0) return -1;
        return iv_commit(out, &tmp,
            ppf_neg(&tmp.lo, &x->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_neg(&tmp.hi, &x->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0);
    }
    return ppf_neg(&out->lo, &x->hi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
           ppf_neg(&out->hi, &x->lo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
}

static int set_min(ppf_t *dst, const ppf_t *x, ppf_context_t *ctx) {
    if (ppf_cmp(x, dst, ctx) < 0)
        return ppf_set(dst, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
    return 0;
}

static int set_max(ppf_t *dst, const ppf_t *x, ppf_context_t *ctx) {
    if (ppf_cmp(x, dst, ctx) > 0)
        return ppf_set(dst, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ? -1 : 0;
    return 0;
}

int ppf_special_iv_mul(ppf_special_iv_t *out,
                       const ppf_special_iv_t *a,
                       const ppf_special_iv_t *b,
                       ppf_context_t *ctx) {
    ppf_t lo, hi, t;
    int rc = -1;
    memset(&lo,0,sizeof(lo)); memset(&hi,0,sizeof(hi)); memset(&t,0,sizeof(t));
    if (ppf_init2_ctx(&lo,out->lo.precision,ctx) ||
        ppf_init2_ctx(&hi,out->hi.precision,ctx) ||
        ppf_init2_ctx(&t,out->lo.precision,ctx)) goto done;
    if (ppf_mul(&lo,&a->lo,&b->lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR ||
        ppf_mul(&hi,&a->lo,&b->lo,PPF_RNDU,ctx)==PPF_TERNARY_ERROR) goto done;
#define CAND(X,Y) do { \
    if(ppf_mul(&t,(X),(Y),PPF_RNDD,ctx)==PPF_TERNARY_ERROR||set_min(&lo,&t,ctx)) goto done; \
    if(ppf_mul(&t,(X),(Y),PPF_RNDU,ctx)==PPF_TERNARY_ERROR||set_max(&hi,&t,ctx)) goto done; \
} while(0)
    CAND(&a->lo,&b->hi); CAND(&a->hi,&b->lo); CAND(&a->hi,&b->hi);
#undef CAND
    ppf_swap(&out->lo,&lo); ppf_swap(&out->hi,&hi); rc=0;
done:
    if (lo.limbs) ppf_clear(&lo);
    if (hi.limbs) ppf_clear(&hi);
    if (t.limbs) ppf_clear(&t);
    return rc;
}

int ppf_special_iv_div(ppf_special_iv_t *out,
                       const ppf_special_iv_t *a,
                       const ppf_special_iv_t *b,
                       ppf_context_t *ctx) {
    ppf_t lo, hi, t;
    int rc=-1;
    if (ppf_sgn(&b->lo,ctx)<=0 && ppf_sgn(&b->hi,ctx)>=0) return -1;
    memset(&lo,0,sizeof(lo)); memset(&hi,0,sizeof(hi)); memset(&t,0,sizeof(t));
    if(ppf_init2_ctx(&lo,out->lo.precision,ctx)||ppf_init2_ctx(&hi,out->hi.precision,ctx)||
       ppf_init2_ctx(&t,out->lo.precision,ctx)) goto done;
    if(ppf_div(&lo,&a->lo,&b->lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
       ppf_div(&hi,&a->lo,&b->lo,PPF_RNDU,ctx)==PPF_TERNARY_ERROR) goto done;
#define DCAND(X,Y) do { \
    if(ppf_div(&t,(X),(Y),PPF_RNDD,ctx)==PPF_TERNARY_ERROR||set_min(&lo,&t,ctx)) goto done; \
    if(ppf_div(&t,(X),(Y),PPF_RNDU,ctx)==PPF_TERNARY_ERROR||set_max(&hi,&t,ctx)) goto done; \
} while(0)
    DCAND(&a->lo,&b->hi); DCAND(&a->hi,&b->lo); DCAND(&a->hi,&b->hi);
#undef DCAND
    ppf_swap(&out->lo,&lo); ppf_swap(&out->hi,&hi); rc=0;
done:
    if (lo.limbs) ppf_clear(&lo);
    if (hi.limbs) ppf_clear(&hi);
    if (t.limbs) ppf_clear(&t);
    return rc;
}

int ppf_special_iv_scale2(ppf_special_iv_t *out,
                          const ppf_special_iv_t *x,
                          long shift, ppf_context_t *ctx) {
    if (out == x) {
        ppf_special_iv_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        if (ppf_special_iv_init(&tmp, out->lo.precision, ctx) != 0) return -1;
        return iv_commit(out, &tmp,
            ppf_mul_2si(&tmp.lo,&x->lo,shift,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
            ppf_mul_2si(&tmp.hi,&x->hi,shift,PPF_RNDU,ctx)==PPF_TERNARY_ERROR ? -1:0);
    }
    return ppf_mul_2si(&out->lo,&x->lo,shift,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
           ppf_mul_2si(&out->hi,&x->hi,shift,PPF_RNDU,ctx)==PPF_TERNARY_ERROR ? -1:0;
}

int ppf_special_iv_sqrt(ppf_special_iv_t *out,
                        const ppf_special_iv_t *x,
                        ppf_context_t *ctx) {
    if (ppf_sgn(&x->lo,ctx)<0) return -1;
    if (out == x) {
        ppf_special_iv_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        if (ppf_special_iv_init(&tmp, out->lo.precision, ctx) != 0) return -1;
        return iv_commit(out, &tmp,
            ppf_sqrt(&tmp.lo,&x->lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
            ppf_sqrt(&tmp.hi,&x->hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR ? -1:0);
    }
    return ppf_sqrt(&out->lo,&x->lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
           ppf_sqrt(&out->hi,&x->hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR ? -1:0;
}

int ppf_special_iv_abs(ppf_special_iv_t *out,
                       const ppf_special_iv_t *x,
                       ppf_context_t *ctx) {
    if (out == x) {
        ppf_special_iv_t tmp;
        int rc;
        memset(&tmp, 0, sizeof(tmp));
        if (ppf_special_iv_init(&tmp, out->lo.precision, ctx) != 0) return -1;
        rc = ppf_special_iv_abs(&tmp, x, ctx);
        return iv_commit(out, &tmp, rc);
    }
    int sl=ppf_sgn(&x->lo,ctx), sh=ppf_sgn(&x->hi,ctx);
    if(sl>=0) return ppf_special_iv_copy(out,x,ctx);
    if(sh<=0) return ppf_special_iv_neg(out,x,ctx);
    ppf_set_zero(&out->lo,1);
    ppf_t a,b;
    int rc=-1;
    if(ppf_init2_ctx(&a,out->hi.precision,ctx)||ppf_init2_ctx(&b,out->hi.precision,ctx)) goto done;
    if(ppf_abs(&a,&x->lo,PPF_RNDU,ctx)==PPF_TERNARY_ERROR||
       ppf_abs(&b,&x->hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR) goto done;
    if(ppf_cmp(&a,&b,ctx)>=0) rc=ppf_set(&out->hi,&a,PPF_RNDU,ctx)==PPF_TERNARY_ERROR?-1:0;
    else rc=ppf_set(&out->hi,&b,PPF_RNDU,ctx)==PPF_TERNARY_ERROR?-1:0;
done:
    if (a.limbs) ppf_clear(&a);
    if (b.limbs) ppf_clear(&b);
    return rc;
}

int ppf_special_iv_exp(ppf_special_iv_t *out,
                       const ppf_special_iv_t *x,
                       ppf_context_t *ctx) {
    if (out == x) {
        ppf_special_iv_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        if (ppf_special_iv_init(&tmp, out->lo.precision, ctx) != 0) return -1;
        return iv_commit(out, &tmp,
            ppf_exp(&tmp.lo,&x->lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
            ppf_exp(&tmp.hi,&x->hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR ? -1:0);
    }
    return ppf_exp(&out->lo,&x->lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
           ppf_exp(&out->hi,&x->hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR ? -1:0;
}

int ppf_special_iv_log_positive(ppf_special_iv_t *out,
                                const ppf_special_iv_t *x,
                                ppf_context_t *ctx) {
    if(ppf_sgn(&x->lo,ctx)<=0) return -1;
    if (out == x) {
        ppf_special_iv_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        if (ppf_special_iv_init(&tmp, out->lo.precision, ctx) != 0) return -1;
        return iv_commit(out, &tmp,
            ppf_log(&tmp.lo,&x->lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
            ppf_log(&tmp.hi,&x->hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR ? -1:0);
    }
    return ppf_log(&out->lo,&x->lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
           ppf_log(&out->hi,&x->hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR ? -1:0;
}

int ppf_special_iv_pow_positive(ppf_special_iv_t *out,
                                const ppf_special_iv_t *base,
                                const ppf_special_iv_t *exponent,
                                ppf_context_t *ctx) {
    ppf_special_iv_t l,p;
    int rc=-1;
    memset(&l,0,sizeof(l)); memset(&p,0,sizeof(p));
    if(ppf_special_iv_init(&l,out->lo.precision,ctx)||
       ppf_special_iv_init(&p,out->lo.precision,ctx)) goto done;
    if(ppf_special_iv_log_positive(&l,base,ctx)||
       ppf_special_iv_mul(&p,&l,exponent,ctx)||
       ppf_special_iv_exp(out,&p,ctx)) goto done;
    rc=0;
done: ppf_special_iv_clear(&l); ppf_special_iv_clear(&p); return rc;
}

int ppf_special_iv_to_dyadic(mpz_ptr lower, mpz_ptr upper,
                             ppf_exp_t *exponent,
                             const ppf_special_iv_t *x) {
    mpz_t l,u; ppf_exp_t el,eu,e; uint64_t dl,du;
    if(!lower||!upper||!exponent) return -1;
    mpz_inits(l,u,NULL);
    el=ppf_get_z_2exp(l,&x->lo); eu=ppf_get_z_2exp(u,&x->hi); e=el<eu?el:eu;
    dl=(uint64_t)el-(uint64_t)e; du=(uint64_t)eu-(uint64_t)e;
    if(dl>ULONG_MAX||du>ULONG_MAX){mpz_clears(l,u,NULL);return -1;}
    if (dl) mpz_mul_2exp(l, l, (mp_bitcnt_t) dl);
    if (du) mpz_mul_2exp(u, u, (mp_bitcnt_t) du);
    mpz_set(lower,l);mpz_set(upper,u);*exponent=e;mpz_clears(l,u,NULL);return 0;
}

ppf_prec_t ppf_special_grow(ppf_prec_t p) {
    ppf_prec_t d=p/2u+64u; return p>UINT64_MAX-d?p:p+d;
}

int ppf_special_small(const ppf_t *x, ppf_prec_t work, unsigned guard) {
    if(ppf_zero_p(x)) return 1;
    if (!ppf_regular_p(x) ||
        work > (ppf_prec_t) INT64_MAX - (ppf_prec_t) guard) return 0;
    return x->exponent<=-(ppf_exp_t)(work+guard);
}

int ppf_special_evaluate(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx,
                         ppf_special_builder_fn builder, void *opaque) {
    ppf_context_t wide; ppf_special_iv_t iv; mpz_t l,u; ppf_exp_t e;
    ppf_prec_t work; int ternary=0,rc=PPF_TERNARY_ERROR;
    ctx=ppf_resolve_context(ctx);
    if(!rop||!ctx||!builder) return special_error(rop,ctx);
    if(rnd==PPF_RND_DEFAULT) rnd=ctx->default_rounding;
    if(!ppf_rounding_valid(rnd)||ppf_context_clone(&wide,ctx)) return special_error(rop,ctx);
    wide.emin=PPF_EMIN_MIN;wide.emax=PPF_EMAX_MAX;wide.flags=0;
    memset(&iv,0,sizeof(iv));mpz_inits(l,u,NULL);work=rop->precision+PPF_SPECIAL_INITIAL_GUARD;
    for(unsigned a=0;a<PPF_SPECIAL_MAX_ATTEMPTS;++a){
        if(ppf_special_iv_init(&iv,work,&wide)) break;
        if(builder(&iv,work,opaque,&wide)==0&&ppf_special_iv_to_dyadic(l,u,&e,&iv)==0){
            ppf_interval_status_t s=ppf_set_dyadic_interval(rop,l,u,e,rnd,&ternary,ctx);
            ppf_special_iv_clear(&iv);
            if(s==PPF_INTERVAL_DECIDED){rc=ternary;break;}
        } else ppf_special_iv_clear(&iv);
        ppf_prec_t next=ppf_special_grow(work);if(next<=work)break;work=next;
    }
    ppf_special_iv_clear(&iv);mpz_clears(l,u,NULL);ppf_context_clear(&wide);
    if(rc==PPF_TERNARY_ERROR) return special_error(rop,ctx);
    if (rc) ppf_raise(ctx, PPF_FLAG_INEXACT);
    return rc;
}

int ppf_special_is_integer(const ppf_t *x){return x&&ppf_number_p(x)&&ppf_integer_p(x);}

int ppf_special_get_long_if_integer(const ppf_t *x,long *out,ppf_context_t *ctx){
    mpz_t z;int ok=0;if(!x||!out||!ppf_special_is_integer(x))return 0;
    mpz_init(z);if(ppf_get_z(z,x,PPF_RNDZ,ctx)!=PPF_TERNARY_ERROR&&mpz_fits_slong_p(z)){*out=mpz_get_si(z);ok=1;}mpz_clear(z);return ok;
}

int ppf_special_bernoulli(mpq_t *out,unsigned n){
    if(!out)return -1;
    for(unsigned i=0;i<=n;++i)mpq_init(out[i]);
    mpq_set_ui(out[0],1,1);
    mpq_t sum,t;mpq_inits(sum,t,NULL);
    mpz_t c;mpz_init(c);
    for(unsigned m=1;m<=n;++m){
        mpq_set_ui(sum,0,1);mpz_set_ui(c,1);
        for(unsigned k=0;k<m;++k){
            /* c = binom(m+1,k) */
            if(k==0)mpz_set_ui(c,1);
            else {mpz_mul_ui(c,c,m+2-k);mpz_divexact_ui(c,c,k);}
            mpq_set_z(t,c);mpq_mul(t,t,out[k]);mpq_add(sum,sum,t);
        }
        mpq_neg(sum,sum);mpz_mul_ui(mpq_denref(sum),mpq_denref(sum),m+1);mpq_canonicalize(sum);mpq_set(out[m],sum);mpq_canonicalize(out[m]);
    }
    mpz_clear(c);mpq_clears(sum,t,NULL);return 0;
}

void ppf_special_bernoulli_clear(mpq_t *v,unsigned n){if(!v)return;for(unsigned i=0;i<=n;++i)mpq_clear(v[i]);}

int ppf_special_pi_interval(ppf_special_iv_t *out,ppf_context_t *ctx){
    return ppf_const_pi(&out->lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
           ppf_const_pi(&out->hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR?-1:0;
}

static int interval_minmax(ppf_special_iv_t *out,const ppf_special_iv_t *a,
                           const ppf_special_iv_t *b,ppf_context_t *ctx){
    const ppf_t *lo=ppf_cmp(&a->lo,&b->lo,ctx)<=0?&a->lo:&b->lo;
    const ppf_t *hi=ppf_cmp(&a->hi,&b->hi,ctx)>=0?&a->hi:&b->hi;
    return ppf_set(&out->lo,lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||
           ppf_set(&out->hi,hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR?-1:0;
}

int ppf_special_euler_interval(ppf_special_iv_t *out,ppf_prec_t work,
                               ppf_context_t *ctx){
    unsigned long n=(unsigned long)(work/2u+64u);if(n<64)n=64;if(n>20000)n=20000;
    unsigned maxm=(unsigned)(work/(2.0*log2((double)n))+12.0);if(maxm<8)maxm=8;if(maxm>192)maxm=192;
    mpq_t *B=(mpq_t *)ppf_owned_buffer_calloc_internal(
        ctx, (size_t)(2u*maxm+1u), sizeof(mpq_t));
    ppf_special_iv_t H,term,tmp,lnn,half,partial,next,candidate;
    int rc=-1; memset(&H,0,sizeof(H));memset(&term,0,sizeof(term));memset(&tmp,0,sizeof(tmp));
    memset(&lnn,0,sizeof(lnn));memset(&half,0,sizeof(half));memset(&partial,0,sizeof(partial));
    memset(&next,0,sizeof(next));memset(&candidate,0,sizeof(candidate));
    if(!B||ppf_special_bernoulli(B,2u*maxm))goto done;
    if(ppf_special_iv_init(&H,work,ctx)||ppf_special_iv_init(&term,work,ctx)||
       ppf_special_iv_init(&tmp,work,ctx)||ppf_special_iv_init(&lnn,work,ctx)||
       ppf_special_iv_init(&half,work,ctx)||ppf_special_iv_init(&partial,work,ctx)||
       ppf_special_iv_init(&next,work,ctx)||ppf_special_iv_init(&candidate,work,ctx))goto done;
    if(ppf_special_iv_set_ui(&H,0,ctx))goto done;
    mpq_t q;mpq_init(q);
    for(unsigned long j=1;j<=n;++j){mpq_set_ui(q,1,j);if(ppf_special_iv_set_q(&term,q,ctx)||ppf_special_iv_add(&tmp,&H,&term,ctx)||ppf_special_iv_copy(&H,&tmp,ctx)){mpq_clear(q);goto done;}}
    mpq_clear(q);
    if(ppf_set_ui(&lnn.lo,n,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_set_ui(&lnn.hi,n,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||
       ppf_log(&lnn.lo,&lnn.lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||ppf_log(&lnn.hi,&lnn.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR)goto done;
    mpq_t hq;mpq_init(hq);mpq_set_ui(hq,1,2*n);if(ppf_special_iv_set_q(&half,hq,ctx)){mpq_clear(hq);goto done;}mpq_clear(hq);
    if(ppf_special_iv_sub(&partial,&H,&lnn,ctx)||ppf_special_iv_sub(&tmp,&partial,&half,ctx)||ppf_special_iv_copy(&partial,&tmp,ctx))goto done;
    mpz_t npow;mpz_init_set_ui(npow,n);mpz_mul(npow,npow,npow);
    for(unsigned k=1;k<=maxm;++k){
        mpq_t tq;mpq_init(tq);mpq_set(tq,B[2u*k]);mpz_mul_ui(mpq_denref(tq),mpq_denref(tq),2u*k);mpq_canonicalize(tq);mpz_mul(mpq_denref(tq),mpq_denref(tq),npow);mpq_canonicalize(tq);
        if(ppf_special_iv_set_q(&next,tq,ctx)){mpq_clear(tq);mpz_clear(npow);goto done;}mpq_clear(tq);
        if(ppf_special_iv_add(&candidate,&partial,&next,ctx)) {mpz_clear(npow);goto done;}
        if(ppf_special_small(&next.hi,work,24u)||k==maxm){if(interval_minmax(out,&partial,&candidate,ctx)){mpz_clear(npow);goto done;}mpz_clear(npow);rc=0;goto done;}
        if(ppf_special_iv_copy(&partial,&candidate,ctx)){mpz_clear(npow);goto done;}
        mpz_mul_ui(npow,npow,n);mpz_mul_ui(npow,npow,n);
    }
done:
    if(B){ppf_special_bernoulli_clear(B,2u*maxm);ppf_owned_buffer_free_internal(B);}ppf_special_iv_clear(&H);ppf_special_iv_clear(&term);ppf_special_iv_clear(&tmp);
    ppf_special_iv_clear(&lnn);ppf_special_iv_clear(&half);ppf_special_iv_clear(&partial);ppf_special_iv_clear(&next);ppf_special_iv_clear(&candidate);return rc;
}

int ppf_special_catalan_interval(ppf_special_iv_t *out,ppf_prec_t work,
                                 ppf_context_t *ctx){
    ppf_special_iv_t pi,three,sqrt3,two,inside,ln,prod,sum,term,next,ratio,tmp,tail,factor;
    int rc=-1; memset(&pi,0,sizeof(pi));memset(&three,0,sizeof(three));memset(&sqrt3,0,sizeof(sqrt3));
    memset(&two,0,sizeof(two));memset(&inside,0,sizeof(inside));memset(&ln,0,sizeof(ln));memset(&prod,0,sizeof(prod));
    memset(&sum,0,sizeof(sum));memset(&term,0,sizeof(term));memset(&next,0,sizeof(next));memset(&ratio,0,sizeof(ratio));
    memset(&tmp,0,sizeof(tmp));memset(&tail,0,sizeof(tail));memset(&factor,0,sizeof(factor));
#define INIT(X) if(ppf_special_iv_init(&(X),work,ctx))goto done
    INIT(pi);INIT(three);INIT(sqrt3);INIT(two);INIT(inside);INIT(ln);INIT(prod);INIT(sum);INIT(term);INIT(next);INIT(ratio);INIT(tmp);INIT(tail);INIT(factor);
#undef INIT
    if(ppf_special_pi_interval(&pi,ctx)||ppf_special_iv_set_ui(&three,3,ctx)||ppf_special_iv_sqrt(&sqrt3,&three,ctx)||
       ppf_special_iv_set_ui(&two,2,ctx)||ppf_special_iv_add(&inside,&two,&sqrt3,ctx)||ppf_special_iv_log_positive(&ln,&inside,ctx)||
       ppf_special_iv_mul(&prod,&pi,&ln,ctx)||ppf_special_iv_scale2(&prod,&prod,-3,ctx)||
       ppf_special_iv_set_ui(&sum,1,ctx)||ppf_special_iv_set_ui(&term,1,ctx))goto done;
    for(unsigned long n=0;n<1000000UL;++n){
        /* ratio=(n+1)(2n+1)/(2(2n+3)^2) */
        mpq_t q;mpq_init(q);unsigned long long num=(unsigned long long)(n+1)*(2ULL*n+1ULL);unsigned long long den=2ULL*(2ULL*n+3ULL)*(2ULL*n+3ULL);
        mpq_set_ui(q,(unsigned long)num,(unsigned long)den);
        if(ppf_special_iv_set_q(&ratio,q,ctx)||ppf_special_iv_mul(&next,&term,&ratio,ctx)){mpq_clear(q);goto done;}mpq_clear(q);
        if(ppf_special_small(&next.hi,work,24u)){
            mpq_t fq;mpq_init(fq);mpq_set_ui(fq,4,3);if(ppf_special_iv_set_q(&factor,fq,ctx)||ppf_special_iv_mul(&tail,&next,&factor,ctx)){mpq_clear(fq);goto done;}mpq_clear(fq);
            if(ppf_special_iv_scale2(&tmp,&sum,-3,ctx))goto done; /* 1/8 sum */
            if(ppf_special_iv_mul(&tmp,&tmp,&three,ctx))goto done; /* 3/8 sum */
            if(ppf_special_iv_add(&out[0],&prod,&tmp,ctx))goto done;
            if(ppf_mul(&out->hi,&three.hi,&tail.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR||ppf_mul_2si(&out->hi,&out->hi,-3,PPF_RNDU,ctx)==PPF_TERNARY_ERROR||ppf_add(&out->hi,&out->hi,&tmp.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR||ppf_add(&out->hi,&out->hi,&prod.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR)goto done;
            rc=0;goto done;
        }
        if(ppf_special_iv_add(&tmp,&sum,&next,ctx)||ppf_special_iv_copy(&sum,&tmp,ctx)||ppf_special_iv_copy(&term,&next,ctx))goto done;
    }
done:
    ppf_special_iv_clear(&pi);ppf_special_iv_clear(&three);ppf_special_iv_clear(&sqrt3);ppf_special_iv_clear(&two);ppf_special_iv_clear(&inside);ppf_special_iv_clear(&ln);ppf_special_iv_clear(&prod);ppf_special_iv_clear(&sum);ppf_special_iv_clear(&term);ppf_special_iv_clear(&next);ppf_special_iv_clear(&ratio);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&tail);ppf_special_iv_clear(&factor);return rc;
}

/* Cached Euler and Catalan constants use the same generic constant engine.
   The descriptors are initialized exactly once and are immutable afterward;
   rewriting identical static fields on every accessor call is still a data
   race under the C memory model. */
static ppf_constant_cache_t euler_cache, catalan_cache;
static ppf_constant_spec_t euler_spec, catalan_spec;
static pthread_once_t special_once = PTHREAD_ONCE_INIT;
static int constant_from_builder(ppf_constant_interval_t *out,
                                 ppf_prec_t p,
                                 unsigned long workers,
                                 ppf_constant_stats_t *stats,
                                 ppf_context_t *ctx,
                                 int catalan) {
    ppf_context_t wide;
    ppf_special_iv_t iv;
    ppf_prec_t work;
    double started = ppf_constant_now();
    int rc = -1;

    (void)workers;
    if (!out || !ctx || p > UINT64_MAX - 96u) return -1;
    memset(&iv, 0, sizeof(iv));
    if (ppf_context_clone(&wide, ctx) != 0) return -1;
    wide.emin = PPF_EMIN_MIN;
    wide.emax = PPF_EMAX_MAX;
    wide.flags = 0;
    work = p + 96u;
    if (ppf_special_iv_init(&iv, work, &wide) != 0) goto done;
    if ((catalan
            ? ppf_special_catalan_interval(&iv, work, &wide)
            : ppf_special_euler_interval(&iv, work, &wide)) != 0 ||
        ppf_special_iv_to_dyadic(out->lower, out->upper,
                                 &out->exponent, &iv) != 0)
        goto done;
    if (stats) {
        stats->computed_precision = work;
        stats->interval_exponent = out->exponent;
        stats->compute_seconds = ppf_constant_now() - started;
    }
    rc = 0;

done:
    if ((wide.flags & PPF_FLAG_ALLOC) != 0) {
        ppf_raise(ctx, PPF_FLAG_ALLOC);
        rc = -1;
    }
    ppf_special_iv_clear(&iv);
    ppf_context_clear(&wide);
    return rc;
}

static int build_euler(ppf_constant_interval_t *out, ppf_prec_t p,
                       unsigned long workers, ppf_constant_stats_t *stats,
                       ppf_context_t *ctx) {
    return constant_from_builder(out, p, workers, stats, ctx, 0);
}

static int build_catalan(ppf_constant_interval_t *out, ppf_prec_t p,
                         unsigned long workers, ppf_constant_stats_t *stats,
                         ppf_context_t *ctx) {
    return constant_from_builder(out, p, workers, stats, ctx, 1);
}

static void special_cache_init_once(void) {
    ppf_constant_cache_init(&euler_cache);
    ppf_constant_cache_init(&catalan_cache);
    memset(&euler_spec, 0, sizeof(euler_spec));
    euler_spec.id = PPF_CONSTANT_EULER;
    euler_spec.name = "euler";
    euler_spec.normal_exponent = 0;
    euler_spec.required_emin = -2;
    euler_spec.required_emax = 2;
    euler_spec.build_interval = build_euler;
    euler_spec.cache = &euler_cache;
    memset(&catalan_spec, 0, sizeof(catalan_spec));
    catalan_spec.id = PPF_CONSTANT_CATALAN;
    catalan_spec.name = "catalan";
    catalan_spec.normal_exponent = 0;
    catalan_spec.required_emin = -2;
    catalan_spec.required_emax = 2;
    catalan_spec.build_interval = build_catalan;
    catalan_spec.cache = &catalan_cache;
}

const ppf_constant_spec_t *ppf_euler_constant_spec(void) {
    (void)pthread_once(&special_once, special_cache_init_once);
    return &euler_spec;
}

const ppf_constant_spec_t *ppf_catalan_constant_spec(void) {
    (void)pthread_once(&special_once, special_cache_init_once);
    return &catalan_spec;
}
