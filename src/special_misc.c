/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "special_internal.h"

#include "transactional_special_names.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int sm_nan(ppf_t*r,ppf_context_t*ctx){ppf_set_nan(r,1);ppf_raise(ctx,PPF_FLAG_NAN);return 0;}
static int sm_error(ppf_t*r,ppf_context_t*ctx){ppf_set_nan(r,1);ppf_raise(ctx,PPF_FLAG_ERANGE);return PPF_TERNARY_ERROR;}
static int iv_minmax(ppf_special_iv_t*out,const ppf_special_iv_t*a,const ppf_special_iv_t*b,ppf_context_t*ctx){const ppf_t*lo=ppf_cmp(&a->lo,&b->lo,ctx)<=0?&a->lo:&b->lo;const ppf_t*hi=ppf_cmp(&a->hi,&b->hi,ctx)>=0?&a->hi:&b->hi;return ppf_set(&out->lo,lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||ppf_set(&out->hi,hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR?-1:0;}
static int iv_recip(ppf_special_iv_t*out,const ppf_special_iv_t*x,ppf_context_t*ctx){ppf_special_iv_t o;int rc=-1;memset(&o,0,sizeof(o));if(ppf_special_iv_init(&o,out->lo.precision,ctx)||ppf_special_iv_set_ui(&o,1,ctx))goto done;rc=ppf_special_iv_div(out,&o,x,ctx);done:ppf_special_iv_clear(&o);return rc;}
static int iv_add_ui(ppf_special_iv_t*out,const ppf_special_iv_t*x,unsigned long n,ppf_context_t*ctx){ppf_special_iv_t t;int rc=-1;memset(&t,0,sizeof(t));if(ppf_special_iv_init(&t,out->lo.precision,ctx)||ppf_special_iv_set_ui(&t,n,ctx))goto done;rc=ppf_special_iv_add(out,x,&t,ctx);done:ppf_special_iv_clear(&t);return rc;}
static int iv_sub_ui(ppf_special_iv_t*out,const ppf_special_iv_t*x,unsigned long n,ppf_context_t*ctx){ppf_special_iv_t t;int rc=-1;memset(&t,0,sizeof(t));if(ppf_special_iv_init(&t,out->lo.precision,ctx)||ppf_special_iv_set_ui(&t,n,ctx))goto done;rc=ppf_special_iv_sub(out,x,&t,ctx);done:ppf_special_iv_clear(&t);return rc;}

/* Ei(x) = gamma + log|x| + sum x^k/(k k!). */
typedef struct{const ppf_t*x;} unary_arg_t;
static int build_eint(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){
    unary_arg_t*a=opaque;ppf_special_iv_t xi,ax,gam,ln,sum,term,next,tmp,ratio,one,tail,cand,base;
    int rc=-1,negative=a->x->sign<0,settled=0;memset(&xi,0,sizeof(xi));memset(&ax,0,sizeof(ax));memset(&gam,0,sizeof(gam));memset(&ln,0,sizeof(ln));memset(&sum,0,sizeof(sum));memset(&term,0,sizeof(term));memset(&next,0,sizeof(next));memset(&tmp,0,sizeof(tmp));memset(&ratio,0,sizeof(ratio));memset(&one,0,sizeof(one));memset(&tail,0,sizeof(tail));memset(&cand,0,sizeof(cand));memset(&base,0,sizeof(base));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(xi);INIT(ax);INIT(gam);INIT(ln);INIT(sum);INIT(term);INIT(next);INIT(tmp);INIT(ratio);INIT(one);INIT(tail);INIT(cand);INIT(base);
#undef INIT
    if(ppf_special_iv_set_exact(&xi,a->x,ctx)||ppf_special_iv_abs(&ax,&xi,ctx)||ppf_special_iv_log_positive(&ln,&ax,ctx)||ppf_special_euler_interval(&gam,work,ctx)||ppf_special_iv_add(&base,&gam,&ln,ctx)||ppf_special_iv_copy(&term,&xi,ctx)||ppf_special_iv_copy(&sum,&xi,ctx)||ppf_special_iv_set_ui(&one,1,ctx))goto done;
    for(unsigned long k=1;k<1000000UL;++k){
        mpq_t q;mpq_init(q);mpq_set_ui(q,k,(k+1UL)*(k+1UL));if(ppf_special_iv_set_q(&ratio,q,ctx)||ppf_special_iv_mul(&tmp,&term,&xi,ctx)||ppf_special_iv_mul(&next,&tmp,&ratio,ctx)){mpq_clear(q);goto done;}mpq_clear(q);
        if(negative){
            ppf_special_iv_t an;memset(&an,0,sizeof(an));if(ppf_special_iv_init(&an,work,ctx)||ppf_special_iv_abs(&an,&next,ctx)){ppf_special_iv_clear(&an);goto done;}
            if(ppf_special_small(&an.hi,work,28u)){
                if(ppf_special_iv_add(&cand,&sum,&next,ctx)||iv_minmax(&tail,&sum,&cand,ctx)||ppf_special_iv_add(out,&base,&tail,ctx)){ppf_special_iv_clear(&an);goto done;}ppf_special_iv_clear(&an);settled=1;break;
            }ppf_special_iv_clear(&an);
            if(ppf_special_iv_add(&tmp,&sum,&next,ctx)||ppf_special_iv_copy(&sum,&tmp,ctx))goto done;
        }else{
            if(ppf_special_iv_add(&tmp,&sum,&next,ctx)||ppf_special_iv_copy(&sum,&tmp,ctx))goto done;
            /* q_next = x*(k+1)/(k+2)^2 */
            mpq_t q2;mpq_init(q2);mpq_set_ui(q2,k+1UL,(k+2UL)*(k+2UL));if(ppf_special_iv_set_q(&ratio,q2,ctx)||ppf_special_iv_mul(&ratio,&ratio,&ax,ctx)){mpq_clear(q2);goto done;}mpq_clear(q2);
            if(ppf_cmp_ui(&ratio.hi,1,ctx)<0&&ppf_special_small(&next.hi,work,28u)){
                /* Explicit denominator avoids a compound-literal interval. */
                ppf_special_iv_t den;memset(&den,0,sizeof(den));if(ppf_special_iv_init(&den,work,ctx)||ppf_special_iv_sub(&den,&one,&ratio,ctx)||ppf_special_iv_mul(&tail,&next,&ratio,ctx)||ppf_special_iv_div(&tail,&tail,&den,ctx)||ppf_special_iv_add(&tail,&sum,&tail,ctx)||ppf_special_iv_add(out,&base,&tail,ctx)){ppf_special_iv_clear(&den);goto done;}ppf_special_iv_clear(&den);settled=1;break;
            }
        }
        if(ppf_special_iv_copy(&term,&next,ctx))goto done;
    }
    if (!settled) goto done;
    rc = 0;
done:ppf_special_iv_clear(&xi);ppf_special_iv_clear(&ax);ppf_special_iv_clear(&gam);ppf_special_iv_clear(&ln);ppf_special_iv_clear(&sum);ppf_special_iv_clear(&term);ppf_special_iv_clear(&next);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&ratio);ppf_special_iv_clear(&one);ppf_special_iv_clear(&tail);ppf_special_iv_clear(&cand);ppf_special_iv_clear(&base);return rc;
}
int ppf_eint(ppf_t*rop,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){unary_arg_t a={x};ctx=ppf_resolve_context(ctx);if(!rop||!x||!ctx)return sm_error(rop,ctx);if(ppf_nan_p(x))return sm_nan(rop,ctx);if(ppf_zero_p(x)){ppf_set_inf(rop,-1);ppf_raise(ctx,PPF_FLAG_DIVBY0);return 0;}if(ppf_inf_p(x)){if(x->sign>0)ppf_set_inf(rop,1);else ppf_set_zero(rop,-1);return 0;}return ppf_special_evaluate(rop,rnd,ctx,build_eint,&a);}

static int li2_series(ppf_special_iv_t*out,const ppf_t*x,ppf_prec_t work,ppf_context_t*ctx){
    ppf_special_iv_t xi,ax,sum,term,next,tmp,ratio,tail,cand,one;int rc=-1,negative=x->sign<0,settled=0;
    memset(&xi,0,sizeof(xi));memset(&ax,0,sizeof(ax));memset(&sum,0,sizeof(sum));memset(&term,0,sizeof(term));memset(&next,0,sizeof(next));memset(&tmp,0,sizeof(tmp));memset(&ratio,0,sizeof(ratio));memset(&tail,0,sizeof(tail));memset(&cand,0,sizeof(cand));memset(&one,0,sizeof(one));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(xi);INIT(ax);INIT(sum);INIT(term);INIT(next);INIT(tmp);INIT(ratio);INIT(tail);INIT(cand);INIT(one);
#undef INIT
    if(ppf_special_iv_set_exact(&xi,x,ctx)||ppf_special_iv_abs(&ax,&xi,ctx)||ppf_special_iv_copy(&sum,&xi,ctx)||ppf_special_iv_copy(&term,&xi,ctx)||ppf_special_iv_set_ui(&one,1,ctx))goto done;
    for(unsigned long k=1;k<1000000UL;++k){mpq_t q;mpq_init(q);mpq_set_ui(q,k*k,(k+1UL)*(k+1UL));if(ppf_special_iv_set_q(&ratio,q,ctx)||ppf_special_iv_mul(&tmp,&term,&xi,ctx)||ppf_special_iv_mul(&next,&tmp,&ratio,ctx)){mpq_clear(q);goto done;}mpq_clear(q);
        if(negative&&ppf_special_small(&next.hi,work,28u)){if(ppf_special_iv_add(&cand,&sum,&next,ctx)||iv_minmax(out,&sum,&cand,ctx))goto done;settled=1;break;}
        if(ppf_special_iv_add(&tmp,&sum,&next,ctx)||ppf_special_iv_copy(&sum,&tmp,ctx))goto done;
        if(!negative&&ppf_special_small(&next.hi,work,28u)){
            if(ppf_special_iv_sub(&tail,&one,&ax,ctx)||ppf_special_iv_mul(&tail,&next,&ax,ctx))goto done;
            ppf_special_iv_t den;memset(&den,0,sizeof(den));if(ppf_special_iv_init(&den,work,ctx)||ppf_special_iv_sub(&den,&one,&ax,ctx)||ppf_special_iv_div(&tail,&tail,&den,ctx)||ppf_special_iv_add(out,&sum,&tail,ctx)){ppf_special_iv_clear(&den);goto done;}ppf_special_iv_clear(&den);settled=1;break;}
        if(ppf_special_iv_copy(&term,&next,ctx))goto done;
    }
    if (!settled) goto done;
    rc = 0;
done:ppf_special_iv_clear(&xi);ppf_special_iv_clear(&ax);ppf_special_iv_clear(&sum);ppf_special_iv_clear(&term);ppf_special_iv_clear(&next);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&ratio);ppf_special_iv_clear(&tail);ppf_special_iv_clear(&cand);ppf_special_iv_clear(&one);return rc;
}

static int li2_interval(ppf_special_iv_t*out,const ppf_t*x,ppf_prec_t work,ppf_context_t*ctx,unsigned depth){
    if(depth>12)return -1;
    if (ppf_zero_p(x)) return ppf_special_iv_set_ui(out,0,ctx);
    if (ppf_cmp_ui(x,1,ctx)==0) {
        ppf_special_iv_t pi,pi2,six;
        int trc=-1;
        memset(&pi,0,sizeof(pi));memset(&pi2,0,sizeof(pi2));memset(&six,0,sizeof(six));
        if(ppf_special_iv_init(&pi,work,ctx)||ppf_special_iv_init(&pi2,work,ctx)||
           ppf_special_iv_init(&six,work,ctx))goto exact_done;
        if(ppf_special_pi_interval(&pi,ctx)||ppf_special_iv_mul(&pi2,&pi,&pi,ctx)||
           ppf_special_iv_set_ui(&six,6,ctx)||ppf_special_iv_div(out,&pi2,&six,ctx))goto exact_done;
        trc=0;
exact_done:
        ppf_special_iv_clear(&pi);ppf_special_iv_clear(&pi2);ppf_special_iv_clear(&six);
        return trc;
    }
    ppf_t y;ppf_special_iv_t li,one,xi,tmp,ln1,ln2,prod,pi,pi2,c,l2,neg;
    int rc=-1;memset(&y,0,sizeof(y));memset(&li,0,sizeof(li));memset(&one,0,sizeof(one));memset(&xi,0,sizeof(xi));memset(&tmp,0,sizeof(tmp));memset(&ln1,0,sizeof(ln1));memset(&ln2,0,sizeof(ln2));memset(&prod,0,sizeof(prod));memset(&pi,0,sizeof(pi));memset(&pi2,0,sizeof(pi2));memset(&c,0,sizeof(c));memset(&l2,0,sizeof(l2));memset(&neg,0,sizeof(neg));
    ppf_t half,nhalf,onev;memset(&half,0,sizeof(half));memset(&nhalf,0,sizeof(nhalf));memset(&onev,0,sizeof(onev));
    if(ppf_init2_ctx(&half,work,ctx)||ppf_init2_ctx(&nhalf,work,ctx)||ppf_init2_ctx(&onev,work,ctx))goto done;
    mpq_t hq;mpq_init(hq);mpq_set_ui(hq,1,2);if(ppf_set_q(&half,hq,PPF_RNDN,ctx)==PPF_TERNARY_ERROR){mpq_clear(hq);goto done;}mpq_clear(hq);if(ppf_neg(&nhalf,&half,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_set_ui(&onev,1,PPF_RNDN,ctx)==PPF_TERNARY_ERROR)goto done;
    if(ppf_cmp(x,&half,ctx)<=0&&ppf_cmp(x,&nhalf,ctx)>=0){rc=li2_series(out,x,work,ctx);goto done;}
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(li);INIT(one);INIT(xi);INIT(tmp);INIT(ln1);INIT(ln2);INIT(prod);INIT(pi);INIT(pi2);INIT(c);INIT(l2);INIT(neg);
#undef INIT
    if(ppf_special_iv_set_ui(&one,1,ctx)||ppf_special_iv_set_exact(&xi,x,ctx)||ppf_special_pi_interval(&pi,ctx)||ppf_special_iv_mul(&pi2,&pi,&pi,ctx))goto done;
    mpq_t sixth;mpq_init(sixth);mpq_set_ui(sixth,1,6);if(ppf_special_iv_set_q(&c,sixth,ctx)||ppf_special_iv_mul(&c,&c,&pi2,ctx)){mpq_clear(sixth);goto done;}mpq_clear(sixth);
    if(ppf_cmp(x,&half,ctx)>0){
        if(ppf_init2_ctx(&y,work,ctx)||ppf_sub(&y,&onev,x,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||li2_interval(&li,&y,work,ctx,depth+1))goto done;
        if(ppf_special_iv_log_positive(&ln1,&xi,ctx))goto done;
        if(ppf_special_iv_set_exact(&tmp,&y,ctx)||ppf_special_iv_abs(&tmp,&tmp,ctx)||ppf_special_iv_log_positive(&ln2,&tmp,ctx)||ppf_special_iv_mul(&prod,&ln1,&ln2,ctx)||ppf_special_iv_sub(&tmp,&c,&prod,ctx)||ppf_special_iv_sub(out,&tmp,&li,ctx))goto done;
    }else{
        if(ppf_init2_ctx(&y,work,ctx)||ppf_sub(&y,x,&onev,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_div(&y,x,&y,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||li2_interval(&li,&y,work,ctx,depth+1))goto done;
        if(ppf_special_iv_sub(&tmp,&one,&xi,ctx)||ppf_special_iv_log_positive(&ln1,&tmp,ctx)||ppf_special_iv_mul(&l2,&ln1,&ln1,ctx)||ppf_special_iv_scale2(&l2,&l2,-1,ctx)||ppf_special_iv_neg(&neg,&li,ctx)||ppf_special_iv_sub(out,&neg,&l2,ctx))goto done;
    }
    rc=0;
done:if(y.limbs)ppf_clear(&y);if(half.limbs)ppf_clear(&half);if(nhalf.limbs)ppf_clear(&nhalf);if(onev.limbs)ppf_clear(&onev);ppf_special_iv_clear(&li);ppf_special_iv_clear(&one);ppf_special_iv_clear(&xi);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&ln1);ppf_special_iv_clear(&ln2);ppf_special_iv_clear(&prod);ppf_special_iv_clear(&pi);ppf_special_iv_clear(&pi2);ppf_special_iv_clear(&c);ppf_special_iv_clear(&l2);ppf_special_iv_clear(&neg);return rc;
}
static int build_li2(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){unary_arg_t*a=opaque;return li2_interval(out,a->x,work,ctx,0);}
int ppf_li2(ppf_t*rop,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){unary_arg_t a={x};ctx=ppf_resolve_context(ctx);if(!rop||!x||!ctx)return sm_error(rop,ctx);if(ppf_nan_p(x))return sm_nan(rop,ctx);if(ppf_zero_p(x)){ppf_set_zero(rop,x->sign);return 0;}if(ppf_inf_p(x)){ppf_set_inf(rop,-1);return 0;}return ppf_special_evaluate(rop,rnd,ctx,build_li2,&a);}

/* Euler-Maclaurin for s>1. */
static int zeta_em(ppf_special_iv_t*out,const ppf_t*s,ppf_prec_t work,ppf_context_t*ctx){
    unsigned long N=(unsigned long)(work/4u+16u);if(N<16)N=16;if(N>100000)N=100000;
    unsigned maxm=(unsigned)(work/(2.0*log2((double)N))+12.0);if(maxm<8)maxm=8;if(maxm>128)maxm=128;
    mpq_t *B=(mpq_t*)ppf_owned_buffer_calloc_internal(ctx,2u*maxm+1u,sizeof(mpq_t));ppf_special_iv_t sum,term,tmp,base,niv,siv,negexp,one,sm1,pow,tail,partial,cand,rising,coeff;
    int rc=-1;memset(&sum,0,sizeof(sum));memset(&term,0,sizeof(term));memset(&tmp,0,sizeof(tmp));memset(&base,0,sizeof(base));memset(&niv,0,sizeof(niv));memset(&siv,0,sizeof(siv));memset(&negexp,0,sizeof(negexp));memset(&one,0,sizeof(one));memset(&sm1,0,sizeof(sm1));memset(&pow,0,sizeof(pow));memset(&tail,0,sizeof(tail));memset(&partial,0,sizeof(partial));memset(&cand,0,sizeof(cand));memset(&rising,0,sizeof(rising));memset(&coeff,0,sizeof(coeff));
    if(!B||ppf_special_bernoulli(B,2u*maxm))goto done;
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(sum);INIT(term);INIT(tmp);INIT(base);INIT(niv);INIT(siv);INIT(negexp);INIT(one);INIT(sm1);INIT(pow);INIT(tail);INIT(partial);INIT(cand);INIT(rising);INIT(coeff);
#undef INIT
    if(ppf_special_iv_set_ui(&sum,0,ctx)||ppf_special_iv_set_exact(&siv,s,ctx)||ppf_special_iv_set_ui(&one,1,ctx)||ppf_special_iv_sub(&sm1,&siv,&one,ctx))goto done;
    for(unsigned long n=1;n<N;++n){if(ppf_special_iv_set_ui(&base,n,ctx)||ppf_special_iv_neg(&negexp,&siv,ctx)||ppf_special_iv_pow_positive(&term,&base,&negexp,ctx)||ppf_special_iv_add(&tmp,&sum,&term,ctx)||ppf_special_iv_copy(&sum,&tmp,ctx))goto done;}
    if(ppf_special_iv_set_ui(&niv,N,ctx)||ppf_special_iv_neg(&negexp,&sm1,ctx)||ppf_special_iv_pow_positive(&term,&niv,&negexp,ctx)||ppf_special_iv_div(&term,&term,&sm1,ctx)||ppf_special_iv_add(&partial,&sum,&term,ctx)||ppf_special_iv_neg(&negexp,&siv,ctx)||ppf_special_iv_pow_positive(&term,&niv,&negexp,ctx)||ppf_special_iv_scale2(&term,&term,-1,ctx)||ppf_special_iv_add(&tmp,&partial,&term,ctx)||ppf_special_iv_copy(&partial,&tmp,ctx)||ppf_special_iv_set_ui(&rising,1,ctx))goto done;
    for(unsigned k=1;k<=maxm;++k){
        for(unsigned j=(k==1?0:2u*k-3u);j<=2u*k-2u;++j){ppf_special_iv_t sj;memset(&sj,0,sizeof(sj));if(ppf_special_iv_init(&sj,work,ctx)||iv_add_ui(&sj,&siv,j,ctx)||ppf_special_iv_mul(&tmp,&rising,&sj,ctx)||ppf_special_iv_copy(&rising,&tmp,ctx)){ppf_special_iv_clear(&sj);goto done;}ppf_special_iv_clear(&sj);}
        mpq_t q; mpq_init(q); mpq_set(q, B[2u*k]);
        mpz_t fact; mpz_init(fact); mpz_fac_ui(fact, 2u*k);
        mpz_mul(mpq_denref(q), mpq_denref(q), fact);
        mpz_clear(fact); mpq_canonicalize(q);
        if(ppf_special_iv_set_q(&coeff,q,ctx)){mpq_clear(q);goto done;}mpq_clear(q);
        ppf_special_iv_t exponent;memset(&exponent,0,sizeof(exponent));if(ppf_special_iv_init(&exponent,work,ctx)||ppf_special_iv_neg(&exponent,&siv,ctx)||iv_sub_ui(&exponent,&exponent,2u*k-1u,ctx)||ppf_special_iv_pow_positive(&pow,&niv,&exponent,ctx)||ppf_special_iv_mul(&term,&coeff,&rising,ctx)||ppf_special_iv_mul(&term,&term,&pow,ctx)||ppf_special_iv_add(&cand,&partial,&term,ctx)){ppf_special_iv_clear(&exponent);goto done;}ppf_special_iv_clear(&exponent);
        if(ppf_special_small(&term.hi,work,28u)||k==maxm){if(iv_minmax(out,&partial,&cand,ctx))goto done;rc=0;goto done;}if(ppf_special_iv_copy(&partial,&cand,ctx))goto done;
    }
done:if(B){ppf_special_bernoulli_clear(B,2u*maxm);ppf_owned_buffer_free_internal(B);}ppf_special_iv_clear(&sum);ppf_special_iv_clear(&term);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&base);ppf_special_iv_clear(&niv);ppf_special_iv_clear(&siv);ppf_special_iv_clear(&negexp);ppf_special_iv_clear(&one);ppf_special_iv_clear(&sm1);ppf_special_iv_clear(&pow);ppf_special_iv_clear(&tail);ppf_special_iv_clear(&partial);ppf_special_iv_clear(&cand);ppf_special_iv_clear(&rising);ppf_special_iv_clear(&coeff);return rc;
}

static int zeta_hasse(ppf_special_iv_t*out,const ppf_t*s,ppf_prec_t work,ppf_context_t*ctx){
    unsigned long N=(unsigned long)(work+40u);if(N>1024)N=1024;
    ppf_special_iv_t sum,an,term,tmp,base,expn,coef,weight,one,two,den,pow2,tail;
    int rc=-1;memset(&sum,0,sizeof(sum));memset(&an,0,sizeof(an));memset(&term,0,sizeof(term));memset(&tmp,0,sizeof(tmp));memset(&base,0,sizeof(base));memset(&expn,0,sizeof(expn));memset(&coef,0,sizeof(coef));memset(&weight,0,sizeof(weight));memset(&one,0,sizeof(one));memset(&two,0,sizeof(two));memset(&den,0,sizeof(den));memset(&pow2,0,sizeof(pow2));memset(&tail,0,sizeof(tail));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(sum);INIT(an);INIT(term);INIT(tmp);INIT(base);INIT(expn);INIT(coef);INIT(weight);INIT(one);INIT(two);INIT(den);INIT(pow2);INIT(tail);
#undef INIT
    if(ppf_special_iv_set_ui(&sum,0,ctx)||ppf_special_iv_set_ui(&one,1,ctx)||ppf_special_iv_set_ui(&two,2,ctx))goto done;
    ppf_special_iv_t siv;memset(&siv,0,sizeof(siv));if(ppf_special_iv_init(&siv,work,ctx)||ppf_special_iv_set_exact(&siv,s,ctx)||ppf_special_iv_neg(&expn,&siv,ctx)){ppf_special_iv_clear(&siv);goto done;}
    for(unsigned long n=0;n<N;++n){if(ppf_special_iv_set_ui(&an,0,ctx))goto done;mpz_t c;mpz_init_set_ui(c,1);
        for(unsigned long k=0;k<=n;++k){if(ppf_special_iv_set_ui(&base,k+1UL,ctx)||ppf_special_iv_pow_positive(&term,&base,&expn,ctx)) {mpz_clear(c);goto done;}mpq_t q;mpq_init(q);mpq_set_z(q,c);if((k&1u))mpq_neg(q,q);if(ppf_special_iv_set_q(&coef,q,ctx)||ppf_special_iv_mul(&term,&term,&coef,ctx)||ppf_special_iv_add(&tmp,&an,&term,ctx)||ppf_special_iv_copy(&an,&tmp,ctx)){mpq_clear(q);mpz_clear(c);goto done;}mpq_clear(q);if(k<n){mpz_mul_ui(c,c,n-k);mpz_divexact_ui(c,c,k+1UL);}}
        mpz_clear(c);if(ppf_special_iv_scale2(&weight,&an,-(long)(n+1UL),ctx)||ppf_special_iv_add(&tmp,&sum,&weight,ctx)||ppf_special_iv_copy(&sum,&tmp,ctx))goto done;
    }
    if(ppf_special_iv_set_ui(&tail,1,ctx)||ppf_special_iv_scale2(&tail,&tail,-(long)N,ctx))goto done;
    if(ppf_special_iv_set_ui(&pow2,2,ctx)){ppf_special_iv_clear(&siv);goto done;}ppf_special_iv_t one_minus_s;memset(&one_minus_s,0,sizeof(one_minus_s));if(ppf_special_iv_init(&one_minus_s,work,ctx)||ppf_special_iv_sub(&one_minus_s,&one,&siv,ctx)||ppf_special_iv_pow_positive(&pow2,&pow2,&one_minus_s,ctx)||ppf_special_iv_sub(&den,&one,&pow2,ctx)){ppf_special_iv_clear(&one_minus_s);ppf_special_iv_clear(&siv);goto done;}ppf_special_iv_clear(&one_minus_s);
    ppf_special_iv_t num;memset(&num,0,sizeof(num));if(ppf_special_iv_init(&num,work,ctx)||ppf_special_iv_add(&num,&sum,&tail,ctx)){ppf_special_iv_clear(&num);ppf_special_iv_clear(&siv);goto done;}
    /* numerator is positive; denominator negative on 0<s<1. */
    if(ppf_special_iv_div(out,&num,&den,ctx)){ppf_special_iv_clear(&num);ppf_special_iv_clear(&siv);goto done;}ppf_special_iv_clear(&num);ppf_special_iv_clear(&siv);rc=0;
done:ppf_special_iv_clear(&sum);ppf_special_iv_clear(&an);ppf_special_iv_clear(&term);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&base);ppf_special_iv_clear(&expn);ppf_special_iv_clear(&coef);ppf_special_iv_clear(&weight);ppf_special_iv_clear(&one);ppf_special_iv_clear(&two);ppf_special_iv_clear(&den);ppf_special_iv_clear(&pow2);ppf_special_iv_clear(&tail);return rc;
}

static int zeta_interval(ppf_special_iv_t*out,const ppf_t*s,ppf_prec_t work,ppf_context_t*ctx){
    if(ppf_cmp_ui(s,1,ctx)>0)return zeta_em(out,s,work,ctx);
    if(ppf_cmp_ui(s,0,ctx)>0)return zeta_hasse(out,s,work,ctx);
    if(ppf_cmp_ui(s,0,ctx)==0){mpq_t q;mpq_init(q);mpq_set_si(q,-1,2);int rc=ppf_special_iv_set_q(out,q,ctx);mpq_clear(q);return rc;}
    if(ppf_integer_p(s)){
        mpz_t z; mpz_init(z); ppf_get_z(z,s,PPF_RNDZ,ctx);
        if(mpz_sgn(z)<=0){
            if(mpz_even_p(z)){mpz_clear(z);return ppf_special_iv_set_ui(out,0,ctx);}
            if(mpz_fits_slong_p(z)){
                long sv=mpz_get_si(z);
                unsigned long n=(unsigned long)(-(sv+1L))+1UL;
                if(n<=4095UL){
                    mpq_t *B=(mpq_t*)ppf_owned_buffer_calloc_internal(ctx,n+2UL,sizeof(mpq_t));
                    if(B&&ppf_special_bernoulli(B,(unsigned)(n+1UL))==0){
                        mpq_t q; mpq_init(q); mpq_set(q,B[n+1UL]);
                        mpz_mul_ui(mpq_denref(q),mpq_denref(q),n+1UL);
                        if(n&1UL)mpq_neg(q,q);
                        mpq_canonicalize(q);
                        int rr=ppf_special_iv_set_q(out,q,ctx);
                        mpq_clear(q);ppf_special_bernoulli_clear(B,(unsigned)(n+1UL));ppf_owned_buffer_free_internal(B);mpz_clear(z);return rr;
                    }
                    if(B)ppf_owned_buffer_free_internal(B);
                }
            }
        }
        mpz_clear(z);
    }
    ppf_t one_minus;ppf_special_iv_t siv,oms,zp,pi,pim1,pow2,sinarg,sinval,gam,prod,tmp,one;
    int rc=-1;memset(&one_minus,0,sizeof(one_minus));memset(&siv,0,sizeof(siv));memset(&oms,0,sizeof(oms));memset(&zp,0,sizeof(zp));memset(&pi,0,sizeof(pi));memset(&pim1,0,sizeof(pim1));memset(&pow2,0,sizeof(pow2));memset(&sinarg,0,sizeof(sinarg));memset(&sinval,0,sizeof(sinval));memset(&gam,0,sizeof(gam));memset(&prod,0,sizeof(prod));memset(&tmp,0,sizeof(tmp));memset(&one,0,sizeof(one));if(ppf_init2_ctx(&one_minus,work,ctx))goto done;
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(siv);INIT(oms);INIT(zp);INIT(pi);INIT(pim1);INIT(pow2);INIT(sinarg);INIT(sinval);INIT(gam);INIT(prod);INIT(tmp);INIT(one);
#undef INIT
    if(ppf_set_ui(&one_minus,1,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_sub(&one_minus,&one_minus,s,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||zeta_em(&zp,&one_minus,work,ctx)||ppf_special_iv_set_exact(&siv,s,ctx)||ppf_special_iv_set_exact(&oms,&one_minus,ctx)||ppf_special_iv_set_ui(&one,1,ctx)||ppf_special_pi_interval(&pi,ctx)||ppf_special_iv_sub(&pim1,&siv,&one,ctx)||ppf_special_iv_pow_positive(&pi,&pi,&pim1,ctx)||ppf_special_iv_set_ui(&pow2,2,ctx)||ppf_special_iv_pow_positive(&pow2,&pow2,&siv,ctx)||ppf_special_iv_scale2(&sinarg,&siv,-1,ctx)||ppf_sinpi(&sinval.lo,&sinarg.lo,PPF_RNDD,ctx)==PPF_TERNARY_ERROR||ppf_sinpi(&sinval.hi,&sinarg.hi,PPF_RNDU,ctx)==PPF_TERNARY_ERROR)goto done;
    /* gamma(1-s) via public gamma endpoints. */
    if (ppf_gamma(&gam.lo, &one_minus, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_gamma(&gam.hi, &one_minus, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
        ppf_special_iv_mul(&prod, &pow2, &pi, ctx) ||
        ppf_special_iv_mul(&prod, &prod, &sinval, ctx) ||
        ppf_special_iv_mul(&prod, &prod, &gam, ctx) ||
        ppf_special_iv_mul(out, &prod, &zp, ctx)) goto done;
    rc = 0;
done:if(one_minus.limbs)ppf_clear(&one_minus);ppf_special_iv_clear(&siv);ppf_special_iv_clear(&oms);ppf_special_iv_clear(&zp);ppf_special_iv_clear(&pi);ppf_special_iv_clear(&pim1);ppf_special_iv_clear(&pow2);ppf_special_iv_clear(&sinarg);ppf_special_iv_clear(&sinval);ppf_special_iv_clear(&gam);ppf_special_iv_clear(&prod);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&one);return rc;
}
static int build_zeta(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){unary_arg_t*a=opaque;return zeta_interval(out,a->x,work,ctx);}
int ppf_zeta(ppf_t*rop,const ppf_t*s,ppf_rnd_t rnd,ppf_context_t*ctx){unary_arg_t a={s};ctx=ppf_resolve_context(ctx);if(!rop||!s||!ctx)return sm_error(rop,ctx);if(ppf_nan_p(s))return sm_nan(rop,ctx);if(ppf_inf_p(s)){if(s->sign>0){ppf_set_ui(rop,1,rnd,ctx);return 0;}return sm_nan(rop,ctx);}if(ppf_cmp_ui(s,1,ctx)==0){ppf_set_inf(rop,1);ppf_raise(ctx,PPF_FLAG_DIVBY0);return 0;}return ppf_special_evaluate(rop,rnd,ctx,build_zeta,&a);}
int ppf_zeta_ui(ppf_t*rop,unsigned long s,ppf_rnd_t rnd,ppf_context_t*ctx){ppf_t x;int rc;ctx=ppf_resolve_context(ctx);if(!rop||!ctx)return PPF_TERNARY_ERROR;if(ppf_init2_ctx(&x,64,ctx))return PPF_TERNARY_ERROR;if(ppf_set_ui(&x,s,PPF_RNDN,ctx)==PPF_TERNARY_ERROR){ppf_clear(&x);return PPF_TERNARY_ERROR;}rc=ppf_zeta(rop,&x,rnd,ctx);ppf_clear(&x);return rc;}

static int erf_series_interval(ppf_special_iv_t*out,const ppf_t*x,ppf_prec_t work,ppf_context_t*ctx){
    ppf_special_iv_t xi,x2,sum,term,next,tmp,ratio,cand,pi,sqrtpi,factor;int rc=-1,settled=0;
    memset(&xi,0,sizeof(xi));memset(&x2,0,sizeof(x2));memset(&sum,0,sizeof(sum));memset(&term,0,sizeof(term));memset(&next,0,sizeof(next));memset(&tmp,0,sizeof(tmp));memset(&ratio,0,sizeof(ratio));memset(&cand,0,sizeof(cand));memset(&pi,0,sizeof(pi));memset(&sqrtpi,0,sizeof(sqrtpi));memset(&factor,0,sizeof(factor));
#define INIT(V) if(ppf_special_iv_init(&(V),work,ctx))goto done
    INIT(xi);INIT(x2);INIT(sum);INIT(term);INIT(next);INIT(tmp);INIT(ratio);INIT(cand);INIT(pi);INIT(sqrtpi);INIT(factor);
#undef INIT
    if(ppf_special_iv_set_exact(&xi,x,ctx)||ppf_special_iv_mul(&x2,&xi,&xi,ctx)||ppf_special_iv_copy(&sum,&xi,ctx)||ppf_special_iv_copy(&term,&xi,ctx))goto done;
    for(unsigned long n=0;n<1000000UL;++n){mpq_t q;mpq_init(q);mpq_set_si(q,-(long)(2*n+1),(long)((n+1)*(2*n+3)));if(ppf_special_iv_set_q(&ratio,q,ctx)||ppf_special_iv_mul(&tmp,&term,&x2,ctx)||ppf_special_iv_mul(&next,&tmp,&ratio,ctx)){mpq_clear(q);goto done;}mpq_clear(q);ppf_special_iv_t an;memset(&an,0,sizeof(an));if(ppf_special_iv_init(&an,work,ctx)||ppf_special_iv_abs(&an,&next,ctx)){ppf_special_iv_clear(&an);goto done;}if(ppf_special_small(&an.hi,work,28u)){if(ppf_special_iv_add(&cand,&sum,&next,ctx)||iv_minmax(&sum,&sum,&cand,ctx)){ppf_special_iv_clear(&an);goto done;}ppf_special_iv_clear(&an);settled=1;break;}ppf_special_iv_clear(&an);if(ppf_special_iv_add(&tmp,&sum,&next,ctx)||ppf_special_iv_copy(&sum,&tmp,ctx)||ppf_special_iv_copy(&term,&next,ctx))goto done;}
    if (!settled || ppf_special_pi_interval(&pi, ctx) ||
        ppf_special_iv_sqrt(&sqrtpi, &pi, ctx) ||
        iv_recip(&factor, &sqrtpi, ctx) ||
        ppf_special_iv_scale2(&factor, &factor, 1, ctx) ||
        ppf_special_iv_mul(out, &sum, &factor, ctx)) goto done;
    rc = 0;
done:ppf_special_iv_clear(&xi);ppf_special_iv_clear(&x2);ppf_special_iv_clear(&sum);ppf_special_iv_clear(&term);ppf_special_iv_clear(&next);ppf_special_iv_clear(&tmp);ppf_special_iv_clear(&ratio);ppf_special_iv_clear(&cand);ppf_special_iv_clear(&pi);ppf_special_iv_clear(&sqrtpi);ppf_special_iv_clear(&factor);return rc;
}
static int build_erf(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){unary_arg_t*a=opaque;return erf_series_interval(out,a->x,work,ctx);}
int ppf_erf(ppf_t*rop,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){unary_arg_t a={x};ctx=ppf_resolve_context(ctx);if(!rop||!x||!ctx)return sm_error(rop,ctx);if(ppf_nan_p(x))return sm_nan(rop,ctx);if(ppf_inf_p(x)){return ppf_set_si(rop,x->sign>0?1:-1,rnd,ctx);}if(ppf_zero_p(x)){ppf_set_zero(rop,x->sign);return 0;}return ppf_special_evaluate(rop,rnd,ctx,build_erf,&a);}
static int build_erfc(ppf_special_iv_t*out,ppf_prec_t work,void*opaque,ppf_context_t*ctx){unary_arg_t*a=opaque;ppf_special_iv_t e,one;int rc=-1;memset(&e,0,sizeof(e));memset(&one,0,sizeof(one));if(ppf_special_iv_init(&e,work,ctx)||ppf_special_iv_init(&one,work,ctx)||erf_series_interval(&e,a->x,work,ctx)||ppf_special_iv_set_ui(&one,1,ctx)||ppf_special_iv_sub(out,&one,&e,ctx))goto done;rc=0;done:ppf_special_iv_clear(&e);ppf_special_iv_clear(&one);return rc;}
int ppf_erfc(ppf_t*rop,const ppf_t*x,ppf_rnd_t rnd,ppf_context_t*ctx){unary_arg_t a={x};ctx=ppf_resolve_context(ctx);if(!rop||!x||!ctx)return sm_error(rop,ctx);if(ppf_nan_p(x))return sm_nan(rop,ctx);if(ppf_inf_p(x)){if(x->sign>0){ppf_set_zero(rop,1);return 0;}return ppf_set_ui(rop,2,rnd,ctx);}return ppf_special_evaluate(rop,rnd,ctx,build_erfc,&a);}
