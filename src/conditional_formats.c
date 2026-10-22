/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <float.h>
#include <math.h>
#include <string.h>

static ppf_rnd_t cf_rnd(ppf_rnd_t rnd, ppf_context_t *ctx) {
    return rnd == PPF_RND_DEFAULT ? ctx->default_rounding : rnd;
}

static int cf_increment(ppf_rnd_t rnd, int sign, mpz_srcptr q,
                        mpz_srcptr r, mpz_srcptr d) {
    mpz_t twice;
    int cmp;
    if (mpz_sgn(r) == 0) return 0;
    switch (rnd) {
        case PPF_RNDZ:
        case PPF_RNDF: return 0;
        case PPF_RNDA: return 1;
        case PPF_RNDU: return sign > 0;
        case PPF_RNDD: return sign < 0;
        case PPF_RNDN:
            mpz_init(twice); mpz_mul_2exp(twice, r, 1);
            cmp = mpz_cmp(twice, d); mpz_clear(twice);
            return cmp > 0 || (cmp == 0 && mpz_odd_p(q));
        default: return 0;
    }
}

static int round_binary_integer(mpz_ptr q, mpz_ptr rem,
                                mpz_srcptr magnitude, ppf_exp_t e2,
                                ppf_exp_t unit, int sign, ppf_rnd_t rnd) {
    uint64_t shift;
    mpz_t denominator;
    if (e2 >= unit) {
        shift = (uint64_t)(e2 - unit);
        if (shift > ULONG_MAX) return -1;
        mpz_mul_2exp(q, magnitude, (mp_bitcnt_t)shift);
        mpz_set_ui(rem, 0UL);
        return 0;
    }
    shift = (uint64_t)(unit - e2);
    if (shift > ULONG_MAX) {
        mpz_set_ui(q, 0UL); mpz_set(rem, magnitude);
        return cf_increment(rnd, sign, q, rem, magnitude);
    }
    mpz_fdiv_q_2exp(q, magnitude, (mp_bitcnt_t)shift);
    mpz_fdiv_r_2exp(rem, magnitude, (mp_bitcnt_t)shift);
    mpz_init_set_ui(denominator, 1UL);
    mpz_mul_2exp(denominator, denominator, (mp_bitcnt_t)shift);
    if (cf_increment(rnd, sign, q, rem, denominator)) mpz_add_ui(q, q, 1UL);
    mpz_clear(denominator);
    return 0;
}

#if PPF_HAVE_FLOAT128
__extension__ typedef unsigned __int128 ppf_u128_t;
typedef union { ppf_float128_t f; ppf_u128_t u; } ppf_float128_union_t;

int ppf_set_float128(ppf_t *rop, ppf_float128_t value,
                     ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_float128_union_t raw;
    ppf_u128_t frac, coeff, frac_mask;
    unsigned exponent, sign;
    ppf_exp_t e2;
    mpz_t z;
    int t;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !ctx || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    raw.f = value;
    sign = (unsigned)(raw.u >> 127);
    exponent = (unsigned)((raw.u >> 112) & 0x7fffu);
    frac_mask = (((ppf_u128_t)1 << 112) - 1);
    frac = raw.u & frac_mask;
    if (exponent == 0x7fffu) {
        if (frac == 0) { ppf_set_inf(rop, sign ? -1 : 1); return 0; }
        ppf_set_nan(rop, sign ? -1 : 1); ppf_raise(ctx, PPF_FLAG_NAN); return 0;
    }
    if (exponent == 0 && frac == 0) {
        ppf_set_zero(rop, sign ? -1 : 1); return 0;
    }
    if (exponent == 0) { coeff = frac; e2 = -16494; }
    else { coeff = ((ppf_u128_t)1 << 112) | frac;
           e2 = (ppf_exp_t)exponent - 16383 - 112; }
    mpz_init(z);
    mpz_import(z, 1, -1, sizeof(coeff), 0, 0, &coeff);
    if (sign) mpz_neg(z, z);
    t = ppf_set_z_2exp(rop, z, e2, rnd, ctx);
    mpz_clear(z);
    return t;
}

ppf_float128_t ppf_get_float128(const ppf_t *op, ppf_rnd_t rnd,
                                ppf_context_t *ctx) {
    ppf_float128_union_t raw;
    ppf_u128_t bits = 0, frac = 0;
    mpz_t z, mag, q, rem, two112, maxq;
    ppf_exp_t e2, top, unit;
    int sign, inexact = 0, overflow = 0;
    unsigned field = 0;
    ctx = ppf_resolve_context(ctx);
    raw.u = 0;
    if (!op || !ctx) return raw.f;
    rnd = cf_rnd(rnd, ctx);
    sign = op->sign < 0;
    if (ppf_nan_p(op)) { raw.u = ((ppf_u128_t)sign << 127) | ((ppf_u128_t)0x7fff << 112) | ((ppf_u128_t)1 << 111); return raw.f; }
    if (ppf_inf_p(op)) { raw.u = ((ppf_u128_t)sign << 127) | ((ppf_u128_t)0x7fff << 112); return raw.f; }
    if (ppf_zero_p(op)) { raw.u = (ppf_u128_t)sign << 127; return raw.f; }
    mpz_inits(z, mag, q, rem, two112, maxq, NULL);
    e2 = ppf_get_z_2exp(z, op); mpz_abs(mag, z);
    top = e2 + (ppf_exp_t)mpz_sizeinbase(mag, 2);
    if (top > 16384) overflow = 1;
    if (!overflow) {
        unit = top >= -16381 ? top - 113 : -16494;
        if (round_binary_integer(q, rem, mag, e2, unit,
                                 sign ? -1 : 1, rnd) != 0) overflow = 1;
        inexact = mpz_sgn(rem) != 0;
        if (!overflow && mpz_sizeinbase(q, 2) > 113) {
            mpz_fdiv_q_2exp(q, q, 1); top++;
        }
        if (top > 16384) overflow = 1;
    }
    if (overflow) {
        /* The C floating conversion interfaces return an infinity when the
           exact value is outside the finite destination range. */
        bits = ((ppf_u128_t)0x7fff << 112);
        ppf_raise(ctx, PPF_FLAG_OVERFLOW | PPF_FLAG_INEXACT);
    } else {
        mpz_set_ui(two112, 1UL); mpz_mul_2exp(two112, two112, 112);
        if (mpz_sgn(q) == 0) {
            bits = 0;
            if (inexact) ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
        } else if (mpz_cmp(q, two112) < 0) {
            size_t count = 0; ppf_u128_t tmp = 0;
            mpz_export(&tmp, &count, -1, sizeof(tmp), 0, 0, q);
            bits = tmp;
            if (inexact) ppf_raise(ctx, PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT);
        } else {
            field = (unsigned)(top + 16382);
            mpz_sub(q, q, two112);
            { size_t count = 0; ppf_u128_t tmp = 0;
              mpz_export(&tmp, &count, -1, sizeof(tmp), 0, 0, q); frac = tmp; }
            bits = ((ppf_u128_t)field << 112) | frac;
            if (inexact) ppf_raise(ctx, PPF_FLAG_INEXACT);
        }
    }
    if (sign) bits |= (ppf_u128_t)1 << 127;
    raw.u = bits;
    mpz_clears(z, mag, q, rem, two112, maxq, NULL);
    return raw.f;
}
#endif

#if PPF_HAVE_DECIMAL_FLOAT
typedef union { ppf_decimal64_t d; uint64_t u; } ppf_d64_union_t;
typedef union {
    ppf_decimal128_t d;
    struct {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint64_t hi, lo;
#else
        uint64_t lo, hi;
#endif
    } u;
} ppf_d128_union_t;

static void cf_mpz_set_u64(mpz_ptr z, uint64_t value) {
    mpz_import(z, 1, 1, sizeof(value), 0, 0, &value);
}

static int set_decimal_components(ppf_t *rop, int sign, mpz_srcptr coeff,
                                  long q, ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpq_t value;
    mpz_t power;
    int t;
    if (mpz_sgn(coeff) == 0) { ppf_set_zero(rop, sign < 0 ? -1 : 1); return 0; }
    mpq_init(value); mpz_init(power);
    mpz_set(mpq_numref(value), coeff); mpz_set_ui(mpq_denref(value), 1UL);
    if (q >= 0) {
        mpz_ui_pow_ui(power, 10UL, (unsigned long)q);
        mpz_mul(mpq_numref(value), mpq_numref(value), power);
    } else {
        mpz_ui_pow_ui(power, 10UL, (unsigned long)(-q));
        mpz_set(mpq_denref(value), power);
    }
    if (sign < 0) mpz_neg(mpq_numref(value), mpq_numref(value));
    mpq_canonicalize(value);
    t = ppf_set_q(rop, value, rnd, ctx);
    mpq_clear(value); mpz_clear(power);
    return t;
}

int ppf_set_decimal64(ppf_t *rop, ppf_decimal64_t value,
                      ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_d64_union_t raw; uint64_t coeff; unsigned biased; long q; int sign;
    mpz_t z; int t;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !ctx || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    raw.d=value; sign=(raw.u>>63)?-1:1;
    if ((raw.u & UINT64_C(0x7c00000000000000)) == UINT64_C(0x7c00000000000000)) {
        ppf_set_nan(rop,sign);ppf_raise(ctx,PPF_FLAG_NAN);return 0;
    }
    if ((raw.u & UINT64_C(0x7800000000000000)) == UINT64_C(0x7800000000000000)) {
        ppf_set_inf(rop,sign);return 0;
    }
    if ((raw.u & UINT64_C(0x6000000000000000)) != UINT64_C(0x6000000000000000)) {
        biased=(unsigned)((raw.u>>53)&0x3ffu);
        coeff=raw.u & UINT64_C(0x001fffffffffffff);
    } else {
        biased=(unsigned)((raw.u>>51)&0x3ffu);
        coeff=(raw.u & UINT64_C(0x0007ffffffffffff)) | UINT64_C(0x0020000000000000);
        if (coeff > UINT64_C(9999999999999999)) coeff=0;
    }
    q=(long)biased-398L;mpz_init_set_ui(z,(unsigned long)coeff);
    t=set_decimal_components(rop,sign,z,q,rnd,ctx);mpz_clear(z);return t;
}

int ppf_set_decimal128(ppf_t *rop, ppf_decimal128_t value,
                       ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_d128_union_t raw; unsigned biased; long q; int sign;
    mpz_t z; uint64_t high;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !ctx || !ppf_rounding_valid(rnd)) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    raw.d=value; sign=(raw.u.hi>>63)?-1:1;
    if ((raw.u.hi & UINT64_C(0x7c00000000000000)) == UINT64_C(0x7c00000000000000)) {
        ppf_set_nan(rop,sign);ppf_raise(ctx,PPF_FLAG_NAN);return 0;
    }
    if ((raw.u.hi & UINT64_C(0x7800000000000000)) == UINT64_C(0x7800000000000000)) {
        ppf_set_inf(rop,sign);return 0;
    }
    biased=(unsigned)((raw.u.hi>>49)&0x3fffu);
    high=raw.u.hi & UINT64_C(0x0001ffffffffffff);
    q=(long)biased-6176L;
    mpz_init(z);
    {
        mpz_t low_part;
        mpz_init(low_part);
        cf_mpz_set_u64(z, high);
        mpz_mul_2exp(z, z, 64);
        cf_mpz_set_u64(low_part, raw.u.lo);
        mpz_add(z, z, low_part);
        mpz_clear(low_part);
    }
    {
        int t=set_decimal_components(rop,sign,z,q,rnd,ctx);
        mpz_clear(z);
        return t;
    }
}

static int compare_dyadic_pow10(mpz_srcptr mag, ppf_exp_t e2, long k) {
    mpz_t left,right,pow; int cmp;
    mpz_inits(left,right,pow,NULL);mpz_set(left,mag);mpz_set_ui(right,1UL);
    if(e2>=0)mpz_mul_2exp(left,left,(mp_bitcnt_t)e2);else mpz_mul_2exp(right,right,(mp_bitcnt_t)(-e2));
    mpz_ui_pow_ui(pow,10UL,(unsigned long)(k<0?-k:k));
    if(k>=0)mpz_mul(right,right,pow);else mpz_mul(left,left,pow);
    cmp=mpz_cmp(left,right);mpz_clears(left,right,pow,NULL);return (cmp>0)-(cmp<0);
}

static int decimal_coefficient(mpz_ptr coeff,long *qout,int *inexact,
                               const ppf_t *op,unsigned digits,long qmin,long qmax,
                               ppf_rnd_t rnd,ppf_context_t *ctx){
    mpz_t z,mag,num,den,pow,rem,maxc;ppf_exp_t e2;long E,q;long double appr;int sign,inc;
    mpz_inits(z,mag,num,den,pow,rem,maxc,NULL);e2=ppf_get_z_2exp(z,op);sign=mpz_sgn(z)<0?-1:1;mpz_abs(mag,z);
    appr=((long double)e2+(long double)mpz_sizeinbase(mag,2)-1.0L)*0.30102999566398119521L;
    E=(long)floorl(appr);if(E<qmin-2)E=qmin-2;if(E>qmax+(long)digits+2)E=qmax+(long)digits+2;
    while(compare_dyadic_pow10(mag,e2,E)<0) E--;
    while(compare_dyadic_pow10(mag,e2,E+1)>=0) E++;
    q=E-(long)digits+1;if(q<qmin)q=qmin;
    mpz_set(num,mag);mpz_set_ui(den,1UL);if(e2>=0)mpz_mul_2exp(num,num,(mp_bitcnt_t)e2);else mpz_mul_2exp(den,den,(mp_bitcnt_t)(-e2));
    mpz_ui_pow_ui(pow,10UL,(unsigned long)(q<0?-q:q));if(q<0)mpz_mul(num,num,pow);else mpz_mul(den,den,pow);
    mpz_fdiv_qr(coeff,rem,num,den);inc=cf_increment(rnd,sign,coeff,rem,den);if(inc)mpz_add_ui(coeff,coeff,1UL);*inexact=mpz_sgn(rem)!=0;
    mpz_ui_pow_ui(maxc,10UL,digits);if(mpz_cmp(coeff,maxc)>=0){mpz_fdiv_q_ui(coeff,coeff,10UL);q++;}
    *qout=q;mpz_clears(z,mag,num,den,pow,rem,maxc,NULL);(void)ctx;return sign;
}

ppf_decimal64_t ppf_get_decimal64(const ppf_t *op, ppf_rnd_t rnd,
                                  ppf_context_t *ctx){
    ppf_d64_union_t out;mpz_t c;long q;int sign,inexact;uint64_t coeff,bits;unsigned biased;
    out.u=0;ctx=ppf_resolve_context(ctx);rnd=cf_rnd(rnd,ctx);if(!op||!ctx)return out.d;sign=op->sign<0?-1:1;
    if(ppf_nan_p(op)){out.u=(sign<0?UINT64_C(1)<<63:0)|UINT64_C(0x7c00000000000000);return out.d;}
    if(ppf_inf_p(op)){out.u=(sign<0?UINT64_C(1)<<63:0)|UINT64_C(0x7800000000000000);return out.d;}
    if(ppf_zero_p(op)){out.u=sign<0?UINT64_C(1)<<63:0;return out.d;}
    mpz_init(c);sign=decimal_coefficient(c,&q,&inexact,op,16,-398,369,rnd,ctx);
    if(q>369){int inf=rnd==PPF_RNDN||rnd==PPF_RNDA||(rnd==PPF_RNDU&&sign>0)||(rnd==PPF_RNDD&&sign<0);bits=inf?UINT64_C(0x7800000000000000):UINT64_C(0x77fb86f26fc0ffff);ppf_raise(ctx,PPF_FLAG_OVERFLOW|PPF_FLAG_INEXACT);}
    else{coeff=mpz_get_ui(c);biased=(unsigned)(q+398);if(coeff<UINT64_C(0x0020000000000000))bits=((uint64_t)biased<<53)|coeff;else bits=UINT64_C(0x6000000000000000)|((uint64_t)biased<<51)|(coeff&UINT64_C(0x0007ffffffffffff));if(inexact)ppf_raise(ctx,PPF_FLAG_INEXACT);}
    if(sign<0) bits|=UINT64_C(1)<<63;
    out.u=bits;
    mpz_clear(c);
    return out.d;
}

ppf_decimal128_t ppf_get_decimal128(const ppf_t *op, ppf_rnd_t rnd,
                                    ppf_context_t *ctx){
    ppf_d128_union_t out;mpz_t c,tmp;long q;int sign,inexact;unsigned biased;uint64_t hi,lo;out.u.lo=out.u.hi=0;
    ctx=ppf_resolve_context(ctx);rnd=cf_rnd(rnd,ctx);if(!op||!ctx)return out.d;sign=op->sign<0?-1:1;
    if(ppf_nan_p(op)){out.u.hi=(sign<0?UINT64_C(1)<<63:0)|UINT64_C(0x7c00000000000000);return out.d;}
    if(ppf_inf_p(op)){out.u.hi=(sign<0?UINT64_C(1)<<63:0)|UINT64_C(0x7800000000000000);return out.d;}
    if(ppf_zero_p(op)){out.u.hi=sign<0?UINT64_C(1)<<63:0;return out.d;}
    mpz_inits(c,tmp,NULL);sign=decimal_coefficient(c,&q,&inexact,op,34,-6176,6111,rnd,ctx);
    if(q>6111){int inf=rnd==PPF_RNDN||rnd==PPF_RNDA||(rnd==PPF_RNDU&&sign>0)||(rnd==PPF_RNDD&&sign<0);if(inf){out.u.hi=UINT64_C(0x7800000000000000);out.u.lo=0;}else{mpz_ui_pow_ui(c,10UL,34);mpz_sub_ui(c,c,1);q=6111;goto finite;}ppf_raise(ctx,PPF_FLAG_OVERFLOW|PPF_FLAG_INEXACT);}
    else{finite:lo=mpz_get_ui(c);mpz_fdiv_q_2exp(tmp,c,64);hi=mpz_get_ui(tmp);biased=(unsigned)(q+6176);out.u.lo=lo;out.u.hi=((uint64_t)biased<<49)|(hi&UINT64_C(0x0001ffffffffffff));if(inexact)ppf_raise(ctx,PPF_FLAG_INEXACT);}
    if(sign<0) out.u.hi|=UINT64_C(1)<<63;
    mpz_clears(c,tmp,NULL);
    return out.d;
}
#endif
