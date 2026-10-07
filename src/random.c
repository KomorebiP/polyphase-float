/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <stdlib.h>

#ifndef PPF_RANDOM_GUARD_BITS
#define PPF_RANDOM_GUARD_BITS 96u
#endif
#ifndef PPF_RANDOM_MAX_ATTEMPTS
#define PPF_RANDOM_MAX_ATTEMPTS 12u
#endif

typedef struct {
    mpz_t prefix;
    ppf_prec_t bits;
} ppf_random_prefix_t;

static ppf_rnd_t random_rnd(ppf_rnd_t rnd, ppf_context_t *ctx) {
    return rnd == PPF_RND_DEFAULT ? ctx->default_rounding : rnd;
}

static unsigned long draw_small_bits(gmp_randstate_t state, unsigned bits) {
    mpz_t z;
    unsigned long value;
    mpz_init(z);
    mpz_urandomb(z, state, bits);
    value = mpz_get_ui(z);
    mpz_clear(z);
    return value;
}

static int prefix_init(ppf_random_prefix_t *u, ppf_prec_t bits,
                       gmp_randstate_t state) {
    if (!u || bits == 0 || bits > ULONG_MAX) return -1;
    mpz_init(u->prefix);
    mpz_urandomb(u->prefix, state, (mp_bitcnt_t)bits);
    u->bits = bits;
    return 0;
}

static void prefix_clear(ppf_random_prefix_t *u) {
    if (!u) return;
    mpz_clear(u->prefix);
    u->bits = 0;
}

static int prefix_extend(ppf_random_prefix_t *u, ppf_prec_t more,
                         gmp_randstate_t state) {
    mpz_t tail;
    if (!u || more == 0 || more > ULONG_MAX ||
        u->bits > UINT64_MAX - more)
        return -1;
    mpz_init(tail);
    mpz_urandomb(tail, state, (mp_bitcnt_t)more);
    mpz_mul_2exp(u->prefix, u->prefix, (mp_bitcnt_t)more);
    mpz_add(u->prefix, u->prefix, tail);
    u->bits += more;
    mpz_clear(tail);
    return 0;
}

static int prefix_nonzero(ppf_random_prefix_t *u,
                          gmp_randstate_t state) {
    unsigned tries = 0;
    while (mpz_sgn(u->prefix) == 0 && tries++ < 64u) {
        if (prefix_extend(u, 64u, state) != 0) return -1;
    }
    return mpz_sgn(u->prefix) != 0 ? 0 : -1;
}

static int prefix_endpoints(ppf_t *lo, ppf_t *hi,
                            const ppf_random_prefix_t *u,
                            ppf_context_t *ctx) {
    mpz_t upper;
    int a, b;
    if (!lo || !hi || !u || u->bits > (ppf_prec_t)INT64_MAX)
        return -1;
    mpz_init_set(upper, u->prefix);
    mpz_add_ui(upper, upper, 1UL);
    a = ppf_set_z_2exp(lo, u->prefix, -(ppf_exp_t)u->bits,
                       PPF_RNDD, ctx);
    b = ppf_set_z_2exp(hi, upper, -(ppf_exp_t)u->bits,
                       PPF_RNDU, ctx);
    mpz_clear(upper);
    return a == PPF_TERNARY_ERROR || b == PPF_TERNARY_ERROR ? -1 : 0;
}

static int settle_object_interval(ppf_t *rop,
                                  const ppf_t *lo_obj,
                                  const ppf_t *hi_obj,
                                  ppf_rnd_t rnd, int *ternary,
                                  ppf_context_t *ctx) {
    mpz_t lo, hi;
    ppf_exp_t le, he, common;
    uint64_t shift;
    ppf_interval_status_t status;
    if (!rop || !lo_obj || !hi_obj || !ternary) return -1;
    mpz_inits(lo, hi, NULL);
    if (ppf_get_exact_dyadic(lo, &le, lo_obj) != 0 ||
        ppf_get_exact_dyadic(hi, &he, hi_obj) != 0) {
        mpz_clears(lo, hi, NULL);
        return -1;
    }
    common = le < he ? le : he;
    if (le > common) {
        shift = (uint64_t)(le - common);
        if (shift > ULONG_MAX) goto fail;
        mpz_mul_2exp(lo, lo, (mp_bitcnt_t)shift);
    }
    if (he > common) {
        shift = (uint64_t)(he - common);
        if (shift > ULONG_MAX) goto fail;
        mpz_mul_2exp(hi, hi, (mp_bitcnt_t)shift);
    }
    if (mpz_cmp(lo, hi) > 0) goto fail;
    status = ppf_set_dyadic_interval(rop, lo, hi, common, rnd,
                                     ternary, ctx);
    mpz_clears(lo, hi, NULL);
    return status == PPF_INTERVAL_DECIDED ? 1 :
           status == PPF_INTERVAL_UNDECIDED ? 0 : -1;
fail:
    mpz_clears(lo, hi, NULL);
    return -1;
}

static int make_wide_context(ppf_context_t *wide,
                             ppf_context_t *source,
                             ppf_prec_t precision) {
    if (ppf_context_clone(wide, source) != 0) return -1;
    wide->default_precision = precision;
    wide->emin = PPF_EMIN_MIN;
    wide->emax = PPF_EMAX_MAX;
    wide->flags = 0;
    return 0;
}

int ppf_urandomb(ppf_t *rop, gmp_randstate_t state) {
    ppf_context_t wide;
    ppf_context_t *ctx = ppf_default_context();
    ppf_t temp;
    mpz_t z;
    ppf_exp_t exponent;
    int rc = 1;
    if (!rop || !ctx || rop->precision > ULONG_MAX ||
        make_wide_context(&wide, ctx, rop->precision) != 0 ||
        ppf_init2_ctx(&temp, rop->precision, &wide) != 0)
        return 1;
    mpz_init(z);
    mpz_urandomb(z, state, (mp_bitcnt_t)rop->precision);
    if (mpz_sgn(z) == 0) {
        ppf_set_zero(&temp, 1);
        rc = 0;
    } else {
        if (ppf_set_z_2exp(&temp, z, -(ppf_exp_t)rop->precision,
                           PPF_RNDN, &wide) == PPF_TERNARY_ERROR)
            goto done;
        exponent = temp.exponent;
        if (exponent < ctx->emin || exponent > ctx->emax) {
            ppf_set_nan(&temp, 1);
            ppf_raise(ctx, PPF_FLAG_NAN);
            rc = 1;
        } else rc = 0;
    }
    ppf_swap(rop, &temp);
done:
    mpz_clear(z);
    ppf_clear(&temp);
    ppf_context_clear(&wide);
    return rc;
}

int ppf_urandom(ppf_t *rop, gmp_randstate_t state,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_context_t wide;
    ppf_t temp;
    mpz_t mantissa;
    ppf_exp_t exp_offset = 0;
    unsigned long byte;
    unsigned cnt;
    int rbit, ternary;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx || rop->precision > ULONG_MAX) return PPF_TERNARY_ERROR;
    rnd = random_rnd(rnd, ctx);
    if (!ppf_rounding_valid(rnd) ||
        make_wide_context(&wide, ctx, rop->precision) != 0 ||
        ppf_init2_ctx(&temp, rop->precision, &wide) != 0)
        return PPF_TERNARY_ERROR;

    do {
        byte = draw_small_bits(state, 8);
        if (byte == 0) cnt = 8;
        else {
            cnt = 0;
            while (((byte >> (7u - cnt)) & 1UL) == 0UL) cnt++;
        }
        if (exp_offset >= PPF_EMIN_MIN - 1)
            exp_offset -= (ppf_exp_t)cnt;
    } while (cnt == 8);

    mpz_init(mantissa);
    mpz_set_ui(mantissa, 1UL);
    if (rop->precision > 1u) {
        mpz_t low;
        mpz_init(low);
        mpz_urandomb(low, state, (mp_bitcnt_t)(rop->precision - 1u));
        mpz_mul_2exp(mantissa, mantissa,
                     (mp_bitcnt_t)(rop->precision - 1u));
        mpz_add(mantissa, mantissa, low);
        mpz_clear(low);
    }
    rbit = (int)draw_small_bits(state, 1);
    if (ppf_set_z_2exp(&temp, mantissa,
                       exp_offset - (ppf_exp_t)rop->precision,
                       PPF_RNDN, &wide) == PPF_TERNARY_ERROR) {
        mpz_clear(mantissa); ppf_clear(&temp); ppf_context_clear(&wide);
        return PPF_TERNARY_ERROR;
    }
    if (rnd == PPF_RNDU || rnd == PPF_RNDA ||
        (rnd == PPF_RNDN && rbit != 0)) {
        ppf_nextabove(&temp, &wide);
        ternary = 1;
    } else ternary = -1;
    ternary = ppf_check_range(&temp, ternary, rnd, ctx);
    ppf_swap(rop, &temp);
    mpz_clear(mantissa);
    ppf_clear(&temp);
    ppf_context_clear(&wide);
    return ternary;
}

static int exponential_from_prefix(ppf_t *rop,
                                   ppf_random_prefix_t *u,
                                   ppf_rnd_t rnd,
                                   gmp_randstate_t state,
                                   ppf_context_t *ctx) {
    unsigned attempt;
    for (attempt = 0; attempt < PPF_RANDOM_MAX_ATTEMPTS; ++attempt) {
        ppf_prec_t work = rop->precision + PPF_RANDOM_GUARD_BITS +
                          (ppf_prec_t)attempt * 64u;
        ppf_context_t wide;
        ppf_t ul, uh, ll, lu, lo, hi;
        int t, status;
        if (work < rop->precision || make_wide_context(&wide, ctx, work) != 0)
            return PPF_TERNARY_ERROR;
        if (ppf_init2_ctx(&ul, work, &wide) != 0 ||
            ppf_init2_ctx(&uh, work, &wide) != 0 ||
            ppf_init2_ctx(&ll, work, &wide) != 0 ||
            ppf_init2_ctx(&lu, work, &wide) != 0 ||
            ppf_init2_ctx(&lo, work, &wide) != 0 ||
            ppf_init2_ctx(&hi, work, &wide) != 0) {
            ppf_context_clear(&wide); return PPF_TERNARY_ERROR;
        }
        status = -1;
        if (prefix_endpoints(&ul, &uh, u, &wide) != 0) goto cleanup;
        if (ppf_log(&lu, &uh, PPF_RNDU, &wide) == PPF_TERNARY_ERROR ||
            ppf_log(&ll, &ul, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
            ppf_neg(&lo, &lu, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
            ppf_neg(&hi, &ll, PPF_RNDU, &wide) == PPF_TERNARY_ERROR)
            goto cleanup;
        status = settle_object_interval(rop, &lo, &hi, rnd, &t, ctx);
        if (status == 1) {
            ppf_clear(&ul); ppf_clear(&uh); ppf_clear(&ll); ppf_clear(&lu);
            ppf_clear(&lo); ppf_clear(&hi); ppf_context_clear(&wide);
            return t;
        }
cleanup:
        ppf_clear(&ul); ppf_clear(&uh); ppf_clear(&ll); ppf_clear(&lu);
        ppf_clear(&lo); ppf_clear(&hi); ppf_context_clear(&wide);
        if (status < 0 || prefix_extend(u, 64u, state) != 0) break;
    }
    ppf_set_nan(rop, 1); ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

int ppf_erandom(ppf_t *rop, gmp_randstate_t state,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_random_prefix_t u;
    int t;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx) return PPF_TERNARY_ERROR;
    rnd = random_rnd(rnd, ctx);
    if (!ppf_rounding_valid(rnd) ||
        prefix_init(&u, rop->precision + PPF_RANDOM_GUARD_BITS,
                    state) != 0)
        return PPF_TERNARY_ERROR;
    if (prefix_nonzero(&u, state) != 0) {
        prefix_clear(&u); return PPF_TERNARY_ERROR;
    }
    t = exponential_from_prefix(rop, &u, rnd, state, ctx);
    prefix_clear(&u);
    return t;
}

static int product_interval(ppf_t *lo, ppf_t *hi,
                            const ppf_t *a0, const ppf_t *a1,
                            const ppf_t *b0, const ppf_t *b1,
                            ppf_context_t *ctx) {
    ppf_t low[4], high[4];
    const ppf_t *aa[2] = {a0, a1};
    const ppf_t *bb[2] = {b0, b1};
    int rc = -1;
    memset(low, 0, sizeof(low));
    memset(high, 0, sizeof(high));
    for (unsigned i = 0; i < 4; ++i)
        if (ppf_init2_ctx(&low[i], lo->precision, ctx) != 0 ||
            ppf_init2_ctx(&high[i], hi->precision, ctx) != 0)
            goto done;
    for (unsigned i = 0, k = 0; i < 2; ++i)
        for (unsigned j = 0; j < 2; ++j, ++k) {
            if (ppf_mul(&low[k], aa[i], bb[j], PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
                ppf_mul(&high[k], aa[i], bb[j], PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
                goto done;
        }
    (void)ppf_set(lo, &low[0], PPF_RNDN, ctx);
    (void)ppf_set(hi, &high[0], PPF_RNDN, ctx);
    for (unsigned k = 1; k < 4; ++k) {
        if (ppf_cmp(&low[k], lo, NULL) < 0)
            (void)ppf_set(lo, &low[k], PPF_RNDN, ctx);
        if (ppf_cmp(&high[k], hi, NULL) > 0)
            (void)ppf_set(hi, &high[k], PPF_RNDN, ctx);
    }
    rc = 0;
done:
    for (unsigned i = 0; i < 4; ++i) {
        if (low[i].limbs) ppf_clear(&low[i]);
        if (high[i].limbs) ppf_clear(&high[i]);
    }
    return rc;
}

static int normal_pair(ppf_t *rop1, ppf_t *rop2,
                       gmp_randstate_t state, ppf_rnd_t rnd,
                       ppf_context_t *ctx, int *t1, int *t2) {
    ppf_random_prefix_t u1, u2;
    ppf_prec_t initial = (rop1->precision > (rop2 ? rop2->precision : 0)
                          ? rop1->precision : (rop2 ? rop2->precision : 0)) +
                         PPF_RANDOM_GUARD_BITS;
    int result = PPF_TERNARY_ERROR;
    if (prefix_init(&u1, initial, state) != 0) return result;
    if (prefix_init(&u2, initial, state) != 0) {
        prefix_clear(&u1);
        return result;
    }
    if (prefix_nonzero(&u1, state) != 0) goto done;
    for (unsigned attempt = 0; attempt < PPF_RANDOM_MAX_ATTEMPTS; ++attempt) {
        ppf_prec_t work = initial + (ppf_prec_t)attempt * 64u;
        ppf_context_t wide;
        ppf_t u1l,u1h,u2l,u2h,logl,logh,al,ah,rl,rh;
        ppf_t pi,mid,theta_l,theta_h,width,slo,shi,clo,chi,tmp;
        ppf_t z1l,z1h,z2l,z2h;
        int s1, s2 = 1, astatus;
#define INITV(v) do { if (ppf_init2_ctx(&(v),work,&wide)!=0) goto init_fail; } while(0)
        if (make_wide_context(&wide, ctx, work) != 0) break;
        memset(&u1l,0,sizeof(u1l)); memset(&u1h,0,sizeof(u1h));
        memset(&u2l,0,sizeof(u2l)); memset(&u2h,0,sizeof(u2h));
        memset(&logl,0,sizeof(logl)); memset(&logh,0,sizeof(logh));
        memset(&al,0,sizeof(al)); memset(&ah,0,sizeof(ah));
        memset(&rl,0,sizeof(rl)); memset(&rh,0,sizeof(rh));
        memset(&pi,0,sizeof(pi)); memset(&mid,0,sizeof(mid));
        memset(&theta_l,0,sizeof(theta_l)); memset(&theta_h,0,sizeof(theta_h));
        memset(&width,0,sizeof(width)); memset(&slo,0,sizeof(slo));
        memset(&shi,0,sizeof(shi)); memset(&clo,0,sizeof(clo));
        memset(&chi,0,sizeof(chi)); memset(&tmp,0,sizeof(tmp));
        memset(&z1l,0,sizeof(z1l)); memset(&z1h,0,sizeof(z1h));
        memset(&z2l,0,sizeof(z2l)); memset(&z2h,0,sizeof(z2h));
        INITV(u1l);INITV(u1h);INITV(u2l);INITV(u2h);INITV(logl);INITV(logh);
        INITV(al);INITV(ah);INITV(rl);INITV(rh);INITV(pi);INITV(mid);
        INITV(theta_l);INITV(theta_h);INITV(width);INITV(slo);INITV(shi);
        INITV(clo);INITV(chi);INITV(tmp);INITV(z1l);INITV(z1h);
        INITV(z2l);INITV(z2h);
        if (prefix_endpoints(&u1l,&u1h,&u1,&wide)!=0 ||
            prefix_endpoints(&u2l,&u2h,&u2,&wide)!=0) goto iter_cleanup;
        /* r=sqrt(-2 log U1), decreasing in U1. */
        if (ppf_log(&logh,&u1h,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            ppf_log(&logl,&u1l,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_neg(&al,&logh,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_neg(&ah,&logl,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            ppf_mul_2ui(&al,&al,1,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_mul_2ui(&ah,&ah,1,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            ppf_sqrt(&rl,&al,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_sqrt(&rh,&ah,PPF_RNDU,&wide)==PPF_TERNARY_ERROR)
            goto iter_cleanup;
        /* theta interval = 2*pi*U2. */
        if (ppf_const_pi(&pi,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_mul(&theta_l,&pi,&u2l,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_mul_2ui(&theta_l,&theta_l,1,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_const_pi(&pi,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            ppf_mul(&theta_h,&pi,&u2h,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            ppf_mul_2ui(&theta_h,&theta_h,1,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            ppf_sub(&width,&theta_h,&theta_l,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            ppf_sin(&slo,&theta_l,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_sin(&shi,&theta_l,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            ppf_cos(&clo,&theta_l,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_cos(&chi,&theta_l,PPF_RNDU,&wide)==PPF_TERNARY_ERROR)
            goto iter_cleanup;
        if (ppf_sub(&slo,&slo,&width,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_add(&shi,&shi,&width,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            ppf_sub(&clo,&clo,&width,PPF_RNDD,&wide)==PPF_TERNARY_ERROR ||
            ppf_add(&chi,&chi,&width,PPF_RNDU,&wide)==PPF_TERNARY_ERROR ||
            product_interval(&z1l,&z1h,&rl,&rh,&clo,&chi,&wide)!=0 ||
            product_interval(&z2l,&z2h,&rl,&rh,&slo,&shi,&wide)!=0)
            goto iter_cleanup;
        astatus=settle_object_interval(rop1,&z1l,&z1h,rnd,&s1,ctx);
        if (rop2) s2=settle_object_interval(rop2,&z2l,&z2h,rnd,t2,ctx);
        if (astatus==1 && (!rop2 || s2==1)) {
            *t1=s1; result=0; goto iter_cleanup_done;
        }
iter_cleanup:
        astatus=0;
iter_cleanup_done:
#define CLEARV(v) do { if ((v).limbs) ppf_clear(&(v)); } while(0)
        CLEARV(u1l);CLEARV(u1h);CLEARV(u2l);CLEARV(u2h);CLEARV(logl);CLEARV(logh);
        CLEARV(al);CLEARV(ah);CLEARV(rl);CLEARV(rh);CLEARV(pi);CLEARV(mid);
        CLEARV(theta_l);CLEARV(theta_h);CLEARV(width);CLEARV(slo);CLEARV(shi);
        CLEARV(clo);CLEARV(chi);CLEARV(tmp);CLEARV(z1l);CLEARV(z1h);
        CLEARV(z2l);CLEARV(z2h);ppf_context_clear(&wide);
        if (result==0) break;
        if (prefix_extend(&u1,64u,state)!=0 || prefix_extend(&u2,64u,state)!=0)
            break;
        continue;
init_fail:
        ppf_context_clear(&wide);
        break;
#undef INITV
#undef CLEARV
    }
done:
    prefix_clear(&u1); prefix_clear(&u2);
    return result;
}

int ppf_nrandom(ppf_t *rop, gmp_randstate_t state,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    int t1=0,t2=0;
    ctx=ppf_resolve_context(ctx);
    if(!rop||!ctx)return PPF_TERNARY_ERROR;
    rnd=random_rnd(rnd,ctx);
    if(!ppf_rounding_valid(rnd))return PPF_TERNARY_ERROR;
    if(normal_pair(rop,NULL,state,rnd,ctx,&t1,&t2)!=0){
        ppf_set_nan(rop,1);ppf_raise(ctx,PPF_FLAG_ERANGE);return PPF_TERNARY_ERROR;
    }
    return t1;
}

static int encode_pair_ternary(int t1,int t2){
    int s=t1>0?1:t1<0?2:0;
    int c=t2>0?1:t2<0?2:0;
    return s+4*c;
}

int ppf_grandom(ppf_t *rop1, ppf_t *rop2,
                gmp_randstate_t state, ppf_rnd_t rnd,
                ppf_context_t *ctx) {
    int t1=0,t2=0;
    ctx=ppf_resolve_context(ctx);
    if(!rop1||!ctx)return PPF_TERNARY_ERROR;
    if (rop2 && rop1 == rop2) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    rnd=random_rnd(rnd,ctx);
    if(!ppf_rounding_valid(rnd))return PPF_TERNARY_ERROR;
    if(normal_pair(rop1,rop2,state,rnd,ctx,&t1,&t2)!=0){
        ppf_set_nan(rop1,1);if(rop2)ppf_set_nan(rop2,1);
        ppf_raise(ctx,PPF_FLAG_ERANGE);return PPF_TERNARY_ERROR;
    }
    return encode_pair_ternary(t1,rop2?t2:0);
}
