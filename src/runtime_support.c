/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <stdlib.h>
#include <string.h>

static int runtime_make_context(ppf_context_t *dst, ppf_prec_t precision) {
    if (ppf_context_init(dst) != 0) return -1;
    dst->default_precision = precision;
    dst->emin = PPF_EMIN_MIN;
    dst->emax = PPF_EMAX_MAX;
    dst->default_rounding = PPF_RNDN;
    dst->flags = 0;
    return 0;
}


static int runtime_round_exact(ppf_t *out, mpz_srcptr z, ppf_exp_t e,
                               ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_flags_t saved;
    int t;
    saved = ctx->flags;
    ctx->flags = 0;
    t = ppf_set_z_2exp(out, z, e, rnd, ctx);
    ctx->flags = saved;
    return t;
}

int ppf_can_round(const ppf_t *b, ppf_exp_t err,
                  ppf_rnd_t rnd1, ppf_rnd_t rnd2,
                  ppf_prec_t precision) {
    ppf_context_t ctx;
    ppf_t lo_round = {0}, hi_round = {0}, d0 = {0}, d1 = {0};
    mpz_t center, lo, hi, error;
    ppf_exp_t ce, ee, common;
    int sign, decided = 0;

    if (!b || !ppf_regular_p(b) || !ppf_precision_valid(precision) ||
        !ppf_rounding_valid(rnd1) || !ppf_rounding_valid(rnd2))
        return 0;
    if (runtime_make_context(&ctx, precision) != 0) return 0;
    if (ppf_init2_ctx(&lo_round, precision, &ctx) != 0 ||
        ppf_init2_ctx(&hi_round, precision, &ctx) != 0 ||
        ppf_init2_ctx(&d0, precision, &ctx) != 0 ||
        ppf_init2_ctx(&d1, precision, &ctx) != 0) {
        ppf_clear(&lo_round); ppf_clear(&hi_round);
        ppf_clear(&d0); ppf_clear(&d1);
        ppf_context_clear(&ctx);
        return 0;
    }
    mpz_inits(center, lo, hi, error, NULL);
    ce = ppf_get_z_2exp(center, b);
    if (err == INT64_MIN || ppf_exp_add(b->exponent, -err, &ee) != 0)
        goto done;
    mpz_set(lo, center);
    mpz_set(hi, center);
    mpz_set_ui(error, 1UL);
    common = ce < ee ? ce : ee;
    if (ce > common) {
        uint64_t sh = (uint64_t)(ce - common);
        if (sh > ULONG_MAX) goto done;
        mpz_mul_2exp(lo, lo, (mp_bitcnt_t)sh);
        mpz_mul_2exp(hi, hi, (mp_bitcnt_t)sh);
    }
    if (ee > common) {
        uint64_t sh = (uint64_t)(ee - common);
        if (sh > ULONG_MAX) goto done;
        mpz_mul_2exp(error, error, (mp_bitcnt_t)sh);
    }
    sign = mpz_sgn(center) < 0 ? -1 : 1;
    switch (rnd1) {
        case PPF_RNDU:
            mpz_sub(lo, lo, error);
            break;
        case PPF_RNDD:
            mpz_add(hi, hi, error);
            break;
        case PPF_RNDZ:
            if (sign > 0) mpz_add(hi, hi, error);
            else mpz_sub(lo, lo, error);
            break;
        case PPF_RNDA:
            if (sign > 0) mpz_sub(lo, lo, error);
            else mpz_add(hi, hi, error);
            break;
        case PPF_RNDN:
        case PPF_RNDF:
            mpz_sub(lo, lo, error);
            mpz_add(hi, hi, error);
            break;
        default:
            goto done;
    }

    if (rnd2 != PPF_RNDF) {
        if (runtime_round_exact(&lo_round, lo, common, rnd2, &ctx) == PPF_TERNARY_ERROR ||
            runtime_round_exact(&hi_round, hi, common, rnd2, &ctx) == PPF_TERNARY_ERROR)
            goto done;
        decided = ppf_same_value(&lo_round, &hi_round);
    } else {
        ppf_rnd_t norm1, norm2, candidate_rnd;
        int t;
        int positive = b->sign > 0;

        if (rnd1 == PPF_RNDN || rnd1 == PPF_RNDF)
            norm1 = PPF_RNDN;
        else if (rnd1 == PPF_RNDZ ||
                 (positive && rnd1 == PPF_RNDD) ||
                 (!positive && rnd1 == PPF_RNDU))
            norm1 = PPF_RNDZ;
        else
            norm1 = PPF_RNDA;

        if (norm1 != PPF_RNDN) {
            /* This exact-target exception is part of the faithful contract. */
            t = ppf_set(&d0, b, PPF_RNDA, &ctx);
            if (t == PPF_TERNARY_ERROR) goto done;
            if (t == 0) { decided = 1; goto done; }
            norm2 = norm1 == PPF_RNDZ ? PPF_RNDA : PPF_RNDZ;
        } else {
            norm2 = PPF_RNDN;
        }

        /* Mirror the reference precision precondition before inspecting the
           exact significand pattern. */
        if (err < (ppf_exp_t)precision + (norm1 == PPF_RNDN) ||
            (err == (ppf_exp_t)precision + (norm1 == PPF_RNDN) &&
             (norm1 == PPF_RNDA || norm2 == PPF_RNDZ)))
            goto done;

        candidate_rnd = norm2;
        if (runtime_round_exact(&lo_round, lo, common, candidate_rnd, &ctx) == PPF_TERNARY_ERROR ||
            runtime_round_exact(&hi_round, hi, common, candidate_rnd, &ctx) == PPF_TERNARY_ERROR)
            goto done;
        decided = ppf_same_value(&lo_round, &hi_round);
    }

done:
    mpz_clears(center, lo, hi, error, NULL);
    ppf_clear(&lo_round); ppf_clear(&hi_round);
    ppf_clear(&d0); ppf_clear(&d1);
    ppf_context_clear(&ctx);
    return decided;
}

const char *ppf_print_rnd_mode(ppf_rnd_t rnd) {
    switch (rnd) {
        case PPF_RNDN: return "PPF_RNDN";
        case PPF_RNDZ: return "PPF_RNDZ";
        case PPF_RNDU: return "PPF_RNDU";
        case PPF_RNDD: return "PPF_RNDD";
        case PPF_RNDA: return "PPF_RNDA";
        case PPF_RNDF: return "PPF_RNDF";
        default: return NULL;
    }
}

int ppf_round_nearest_away_finish(ppf_t *rop,
                                  const ppf_t *extended,
                                  int extended_ternary,
                                  ppf_context_t *ctx) {
    int lowbit, t;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !extended || !ctx || extended_ternary == PPF_TERNARY_ERROR ||
        extended->precision != rop->precision + 1u) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return PPF_TERNARY_ERROR;
    }
    if (!ppf_regular_p(extended))
        return ppf_set(rop, extended, PPF_RNDN, ctx);
    lowbit = (extended->limbs[0] & 1u) != 0;
    if (!lowbit) {
        t = ppf_set(rop, extended, PPF_RNDN, ctx);
        if (t == PPF_TERNARY_ERROR) return t;
        if (extended_ternary != 0) ppf_raise(ctx, PPF_FLAG_INEXACT);
        return extended_ternary;
    }
    if (extended_ternary == 0)
        return ppf_set(rop, extended, PPF_RNDA, ctx);
    return ppf_set(rop, extended,
                   extended_ternary > 0 ? PPF_RNDD : PPF_RNDU, ctx);
}

void ppf_set_prec_raw(ppf_t *x, ppf_prec_t precision) {
    mpz_t z;
    ppf_prec_t old;
    uint64_t delta;
    if (!x || !ppf_precision_valid(precision) ||
        ppf_limbs_for_precision(precision) > x->allocated_limbs)
        return;
    old = x->precision;
    if (old == precision) return;
    if (!ppf_regular_p(x)) {
        x->precision = precision;
        return;
    }
    mpz_init(z);
    (void)ppf_get_z_2exp(z, x);
    mpz_abs(z, z);
    if (precision > old) {
        delta = (uint64_t)(precision - old);
        if (delta <= ULONG_MAX) mpz_mul_2exp(z, z, (mp_bitcnt_t)delta);
        else { mpz_clear(z); return; }
    } else {
        delta = (uint64_t)(old - precision);
        if (delta <= ULONG_MAX) mpz_tdiv_q_2exp(z, z, (mp_bitcnt_t)delta);
        else { mpz_clear(z); return; }
    }
    x->precision = precision;
    (void)ppf_store_normalized_mpz(x, z, x->exponent, x->sign);
    mpz_clear(z);
}

int ppf_eq(const ppf_t *a, const ppf_t *b, unsigned long bits) {
    mpz_t za, zb;
    ppf_exp_t ea, eb;
    size_t ba, bb;
    if (!a || !b) return 0;
    if (ppf_zero_p(a) && ppf_zero_p(b)) return 1;
    if (ppf_inf_p(a) && ppf_inf_p(b)) return a->sign == b->sign;
    if (!ppf_regular_p(a) || !ppf_regular_p(b) ||
        a->sign != b->sign || a->exponent != b->exponent)
        return 0;
    if (bits == 0) return 1;
    mpz_inits(za, zb, NULL);
    ea = ppf_get_z_2exp(za, a);
    eb = ppf_get_z_2exp(zb, b);
    (void)ea; (void)eb;
    mpz_abs(za, za); mpz_abs(zb, zb);
    ba = mpz_sizeinbase(za, 2); bb = mpz_sizeinbase(zb, 2);
    if (bits < ba) mpz_fdiv_q_2exp(za, za, (mp_bitcnt_t)(ba - bits));
    else if (bits > ba) mpz_mul_2exp(za, za, (mp_bitcnt_t)(bits - ba));
    if (bits < bb) mpz_fdiv_q_2exp(zb, zb, (mp_bitcnt_t)(bb - bits));
    else if (bits > bb) mpz_mul_2exp(zb, zb, (mp_bitcnt_t)(bits - bb));
    {
        int same = mpz_cmp(za, zb) == 0;
        mpz_clears(za, zb, NULL);
        return same;
    }
}

static void reldiff_uncommitted(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                                ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t temp;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !a || !b || !ctx ||
        ppf_init2_ctx(&temp, rop->precision, ctx) != 0) {
        if (rop) ppf_set_nan(rop, 1);
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return;
    }
    if (ppf_sub(&temp, a, b, rnd, ctx) == PPF_TERNARY_ERROR ||
        ppf_abs(&temp, &temp, rnd, ctx) == PPF_TERNARY_ERROR ||
        ppf_div(rop, &temp, a, rnd, ctx) == PPF_TERNARY_ERROR)
        ppf_set_nan(rop, 1);
    ppf_clear(&temp);
}

void ppf_reldiff(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                 ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !a || !b || !ctx) {
        reldiff_uncommitted(rop, a, b, rnd, ctx);
        return;
    }
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return;
    reldiff_uncommitted(&tx.staged, a, b, rnd, ctx);
    (void)ppf_result_transaction_finish_internal(&tx, rop, 0);
}

int ppf_mul_2exp(ppf_t *rop, const ppf_t *op, unsigned long exponent,
                 ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_mul_2ui(rop, op, exponent, rnd, ctx);
}

int ppf_div_2exp(ppf_t *rop, const ppf_t *op, unsigned long exponent,
                 ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_div_2ui(rop, op, exponent, rnd, ctx);
}

const char *ppf_get_version(void) { return PPF_VERSION_STRING; }
const char *ppf_get_patches(void) { return ""; }
int ppf_buildopt_tls_p(void) { return 1; }
int ppf_buildopt_float128_p(void) { return PPF_HAVE_FLOAT128; }
int ppf_buildopt_decimal_p(void) { return PPF_HAVE_DECIMAL_FLOAT; }
int ppf_buildopt_gmpinternals_p(void) { return 0; }
int ppf_buildopt_sharedcache_p(void) { return 1; }
const char *ppf_buildopt_tune_case(void) {
    return "portable-gmp-mpn-runtime-complete";
}

void ppf_free_pool(void) {
    ppf_context_release_pool_internal(NULL);
}

void ppf_free_cache2(ppf_free_cache_t way) {
    if (((unsigned)way & PPF_FREE_LOCAL_CACHE) != 0u)
        ppf_free_pool();
    if (((unsigned)way & PPF_FREE_GLOBAL_CACHE) != 0u) {
        ppf_free_constant_caches();
        ppf_transcendental_cache_purge();
    }
}

void ppf_free_cache(void) {
    ppf_free_cache2((ppf_free_cache_t)(PPF_FREE_LOCAL_CACHE |
                                      PPF_FREE_GLOBAL_CACHE));
}

int ppf_mp_memory_cleanup(void) {
    ppf_free_cache();
    return 0;
}

size_t ppf_custom_get_size(ppf_prec_t precision) {
    size_t n = ppf_limbs_for_precision(precision);
    return n ? n * sizeof(mp_limb_t) : 0;
}

void ppf_custom_init(void *significand, ppf_prec_t precision) {
    /* The application owns and initializes this suitably aligned storage. */
    (void)significand;
    (void)precision;
}

int ppf_custom_init_set(ppf_t *x, int kind, ppf_exp_t exponent,
                        ppf_prec_t precision, void *significand) {
    int magnitude;
    size_t limbs;
    /* Validate the signed code before taking its magnitude.  In particular,
       negating INT_MIN would be undefined even for an invalid request. */
    if (kind < -PPF_REGULAR_KIND || kind > PPF_REGULAR_KIND) return -1;
    magnitude = kind < 0 ? -kind : kind;
    if (!x || !significand || !ppf_precision_valid(precision) ||
        magnitude < PPF_NAN_KIND || magnitude > PPF_REGULAR_KIND)
        return -1;
    limbs = ppf_limbs_for_precision(precision);
    memset(x, 0, sizeof(*x));
    x->limbs = (mp_limb_t *)significand;
    x->allocated_limbs = limbs;
    x->precision = precision;
    x->allocator = ppf_default_allocator();
    x->storage_flags = PPF_STORAGE_EXTERNAL;
    x->sign = kind < 0 ? -1 : 1;
    switch (magnitude) {
        case PPF_NAN_KIND: ppf_set_nan(x, x->sign); break;
        case PPF_INF_KIND: ppf_set_inf(x, x->sign); break;
        case PPF_ZERO_KIND: ppf_set_zero(x, x->sign); break;
        case PPF_REGULAR_KIND:
            x->kind = PPF_KIND_REGULAR;
            x->used_limbs = limbs;
            x->exponent = exponent;
            if (!ppf_validate(x)) return -1;
            break;
        default: return -1;
    }
    return 0;
}

int ppf_custom_get_kind(const ppf_t *x) {
    int k;
    if (!x) return PPF_NAN_KIND;
    switch (x->kind) {
        case PPF_KIND_REGULAR: k = PPF_REGULAR_KIND; break;
        case PPF_KIND_ZERO: k = PPF_ZERO_KIND; break;
        case PPF_KIND_INF: k = PPF_INF_KIND; break;
        case PPF_KIND_NAN: return PPF_NAN_KIND;
        default: return PPF_NAN_KIND;
    }
    return x->sign < 0 ? -k : k;
}

void *ppf_custom_get_significand(const ppf_t *x) {
    return x ? (void *)x->limbs : NULL;
}

ppf_exp_t ppf_custom_get_exp(const ppf_t *x) {
    return x ? x->exponent : 0;
}

void ppf_custom_move(ppf_t *x, void *new_position) {
    if (!x || !(x->storage_flags & PPF_STORAGE_EXTERNAL) || !new_position)
        return;
    x->limbs = (mp_limb_t *)new_position;
}
