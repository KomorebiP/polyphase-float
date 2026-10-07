/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef PPF_FAST_ATAN_HALLEY_MIN_PREC
#define PPF_FAST_ATAN_HALLEY_MIN_PREC 12000u
#endif
#ifndef PPF_FAST_ATAN_HALLEY_GUARD
#define PPF_FAST_ATAN_HALLEY_GUARD 192u
#endif
#ifndef PPF_FAST_ATAN_HALLEY_ATTEMPTS
#define PPF_FAST_ATAN_HALLEY_ATTEMPTS 4u
#endif

static int ia_init(ppf_t *x, ppf_prec_t precision,
                   ppf_context_t *ctx) {
    memset(x, 0, sizeof(*x));
    return ppf_init2_ctx(x, precision, ctx);
}

static void ia_clear(ppf_t *x) {
    if (x && x->limbs) ppf_clear(x);
}

static int ia_tan_approx(ppf_t *out, const ppf_t *angle,
                         ppf_prec_t work, ppf_context_t *ctx) {
    ppf_t slo, shi, clo, chi;
    int rc = -1;
    memset(&slo, 0, sizeof(slo)); memset(&shi, 0, sizeof(shi));
    memset(&clo, 0, sizeof(clo)); memset(&chi, 0, sizeof(chi));
    if (ia_init(&slo, work, ctx) != 0 || ia_init(&shi, work, ctx) != 0 ||
        ia_init(&clo, work, ctx) != 0 || ia_init(&chi, work, ctx) != 0)
        goto done;
    if (!ppf_fast_sincos_reduced_internal(&slo, &shi, &clo, &chi,
                                           angle, angle, work, ctx))
        goto done;
    /* The Halley prepass only needs a work-precision approximation.  One
       directed pair already differs from tan(angle) by O(ulp(work)). */
    if (ppf_div(out, &slo, &clo, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
        goto done;
    rc = 0;
done:
    ia_clear(&slo); ia_clear(&shi); ia_clear(&clo); ia_clear(&chi);
    return rc;
}

static int ia_tan_interval(ppf_t *lo, ppf_t *hi, const ppf_t *angle,
                           ppf_prec_t work, ppf_context_t *ctx) {
    ppf_t slo, shi, clo, chi;
    int rc = -1;
    memset(&slo, 0, sizeof(slo)); memset(&shi, 0, sizeof(shi));
    memset(&clo, 0, sizeof(clo)); memset(&chi, 0, sizeof(chi));
    if (ia_init(&slo, work, ctx) != 0 || ia_init(&shi, work, ctx) != 0 ||
        ia_init(&clo, work, ctx) != 0 || ia_init(&chi, work, ctx) != 0)
        goto done;
    if (!ppf_fast_sincos_reduced_internal(&slo, &shi, &clo, &chi,
                                           angle, angle, work, ctx) ||
        ppf_sgn(&clo, ctx) <= 0)
        goto done;
    if (ppf_div(lo, &slo, &chi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_div(hi, &shi, &clo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        goto done;
    rc = 0;
done:
    ia_clear(&slo); ia_clear(&shi); ia_clear(&clo); ia_clear(&chi);
    return rc;
}

static int ia_common_interval(mpz_ptr lower, mpz_ptr upper,
                              ppf_exp_t *exponent,
                              const ppf_t *lo, const ppf_t *hi) {
    mpz_t zl, zu;
    ppf_exp_t el, eu, common;
    uint64_t sl, su;
    int rc = -1;
    mpz_inits(zl, zu, NULL);
    el = ppf_get_z_2exp(zl, lo);
    eu = ppf_get_z_2exp(zu, hi);
    if (el == INT64_MAX || eu == INT64_MAX) goto done;
    common = el < eu ? el : eu;
    sl = (uint64_t)el - (uint64_t)common;
    su = (uint64_t)eu - (uint64_t)common;
    if (sl > (uint64_t)ULONG_MAX || su > (uint64_t)ULONG_MAX) goto done;
    if (sl != 0u) mpz_mul_2exp(zl, zl, (mp_bitcnt_t)sl);
    if (su != 0u) mpz_mul_2exp(zu, zu, (mp_bitcnt_t)su);
    if (mpz_cmp(zl, zu) > 0) goto done;
    mpz_set(lower, zl);
    mpz_set(upper, zu);
    *exponent = common;
    rc = 0;
done:
    mpz_clears(zl, zu, NULL);
    return rc;
}

/* Compute a stable hardware seed for atan(A/B), where 0<A<=B. */
static long double ia_seed(const ppf_t *a, const ppf_t *b,
                           ppf_context_t *ctx) {
    long ea = 0L, eb = 0L;
    long double ma = ppf_get_ld_2exp(&ea, a, PPF_RNDN, ctx);
    long double mb = ppf_get_ld_2exp(&eb, b, PPF_RNDN, ctx);
    long diff;
    if (!(ma > 0.0L) || !(mb > 0.0L)) return 0.0L;
    if (ea <= eb) {
        if (eb - ea > (long)(LDBL_MAX_EXP + 8)) return 0.0L;
        diff = ea - eb;
        return atan2l(ldexpl(ma, (int)diff), mb);
    }
    if (ea - eb > (long)(LDBL_MAX_EXP + 8)) return atanl(1.0L);
    diff = eb - ea;
    return atan2l(ma, ldexpl(mb, (int)diff));
}

/* Build a cubic-Halley approximation y to atan(A/B).  The exact correction

       atan(A/B)-y = atan((A-B*tan(y))/(B+A*tan(y)))

   makes the update y <- y+u cubically convergent while avoiding a general
   inverse-angle series at every precision level. */
static int ia_build_halley(ppf_t *y, const ppf_t *a, const ppf_t *b,
                           ppf_prec_t qmax, ppf_context_t *ctx) {
    ppf_prec_t levels[40];
    unsigned count = 0u;
    ppf_prec_t q = qmax;
    long double seed;
    if (qmax < 96u) qmax = 96u;
    levels[count++] = qmax;
    q = qmax;
    while (q > 96u && count < 40u) {
        q = (q + 62u) / 3u + 16u;
        levels[count++] = q;
    }
    seed = ia_seed(a, b, ctx);
    if (ia_init(y, levels[count - 1u] + 40u, ctx) != 0) return -1;
    if (ppf_set_ld(y, seed, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) {
        ia_clear(y);
        return -1;
    }

    for (int level = (int)count - 2; level >= 0; --level) {
        ppf_prec_t work = levels[level] + 48u;
        ppf_t aw, bw, yw, t, bt, at, num, den, u, next;
        int ok = 0;
        memset(&aw, 0, sizeof(aw)); memset(&bw, 0, sizeof(bw));
        memset(&yw, 0, sizeof(yw)); memset(&t, 0, sizeof(t));
        memset(&bt, 0, sizeof(bt)); memset(&at, 0, sizeof(at));
        memset(&num, 0, sizeof(num)); memset(&den, 0, sizeof(den));
        memset(&u, 0, sizeof(u)); memset(&next, 0, sizeof(next));
        if (ia_init(&aw, work, ctx) != 0 || ia_init(&bw, work, ctx) != 0 ||
            ia_init(&yw, work, ctx) != 0 || ia_init(&t, work, ctx) != 0 ||
            ia_init(&bt, work, ctx) != 0 || ia_init(&at, work, ctx) != 0 ||
            ia_init(&num, work, ctx) != 0 || ia_init(&den, work, ctx) != 0 ||
            ia_init(&u, work, ctx) != 0 || ia_init(&next, work, ctx) != 0)
            goto stage_done;
        if (ppf_set(&aw, a, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_set(&bw, b, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_set(&yw, y, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ia_tan_approx(&t, &yw, work, ctx) != 0 ||
            ppf_mul(&bt, &bw, &t, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_mul(&at, &aw, &t, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_sub(&num, &aw, &bt, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_add(&den, &bw, &at, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(&u, &num, &den, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_add(&next, &yw, &u, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
            goto stage_done;
        ppf_swap(y, &next);
        ok = 1;
stage_done:
        ia_clear(&aw); ia_clear(&bw); ia_clear(&yw); ia_clear(&t);
        ia_clear(&bt); ia_clear(&at); ia_clear(&num); ia_clear(&den);
        ia_clear(&u); ia_clear(&next);
        if (!ok) {
            ia_clear(y);
            return -1;
        }
    }
    return 0;
}

static int ia_div_interval(ppf_t *ulo, ppf_t *uhi,
                           const ppf_t *nlo, const ppf_t *nhi,
                           const ppf_t *dlo, const ppf_t *dhi,
                           ppf_context_t *ctx) {
    int sl = ppf_sgn(nlo, ctx);
    int sh = ppf_sgn(nhi, ctx);
    if (ppf_sgn(dlo, ctx) <= 0) return -1;
    if (sl >= 0) {
        if (ppf_div(ulo, nlo, dhi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(uhi, nhi, dlo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
    } else if (sh <= 0) {
        if (ppf_div(ulo, nlo, dlo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(uhi, nhi, dhi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
    } else {
        if (ppf_div(ulo, nlo, dlo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(uhi, nhi, dlo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            return -1;
    }
    return 0;
}

/* Bound atan(u) from a narrow interval for u.  The alternating expansion

       u-u^3/3 <= atan(u) <= u       (u>=0)
       u <= atan(u) <= u-u^3/3       (u<=0)

   is enough because the Halley prepass leaves |u| about 2^(-p/3), making
   the cubic remainder about 2^-p. */
static int ia_correction_interval(ppf_t *clo, ppf_t *chi,
                                  const ppf_t *ulo, const ppf_t *uhi,
                                  ppf_context_t *ctx) {
    ppf_t cube, three;
    int sl = ppf_sgn(ulo, ctx);
    int sh = ppf_sgn(uhi, ctx);
    int rc = -1;
    memset(&cube, 0, sizeof(cube)); memset(&three, 0, sizeof(three));
    if (ppf_cmpabs_ui(ulo, 1UL, ctx) > 0 ||
        ppf_cmpabs_ui(uhi, 1UL, ctx) > 0)
        return -1;
    if (ppf_set(clo, ulo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_set(chi, uhi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    if (sl >= 0) {
        if (ia_init(&cube, clo->precision, ctx) != 0 ||
            ia_init(&three, clo->precision, ctx) != 0)
            goto done;
        if (ppf_sqr(&cube, ulo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
            ppf_mul(&cube, &cube, ulo, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
            ppf_set_ui(&three, 3UL, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(&cube, &cube, &three, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
            ppf_sub(clo, ulo, &cube, PPF_RNDD, ctx) == PPF_TERNARY_ERROR)
            goto done;
    } else if (sh <= 0) {
        if (ia_init(&cube, clo->precision, ctx) != 0 ||
            ia_init(&three, clo->precision, ctx) != 0)
            goto done;
        if (ppf_sqr(&cube, uhi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
            ppf_mul(&cube, &cube, uhi, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_set_ui(&three, 3UL, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
            ppf_div(&cube, &cube, &three, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
            ppf_sub(chi, uhi, &cube, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
            goto done;
    }
    rc = 0;
done:
    ia_clear(&cube); ia_clear(&three);
    return rc;
}

static int ia_get_pi(ppf_t *plo, ppf_t *phi, ppf_t *hlo, ppf_t *hhi,
                     ppf_prec_t precision, ppf_context_t *ctx) {
    if (ia_init(plo, precision, ctx) != 0 || ia_init(phi, precision, ctx) != 0 ||
        ia_init(hlo, precision, ctx) != 0 || ia_init(hhi, precision, ctx) != 0)
        return -1;
    if (ppf_const_pi(plo, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_const_pi(phi, PPF_RNDU, ctx) == PPF_TERNARY_ERROR ||
        ppf_mul_2si(hlo, plo, -1L, PPF_RNDD, ctx) == PPF_TERNARY_ERROR ||
        ppf_mul_2si(hhi, phi, -1L, PPF_RNDU, ctx) == PPF_TERNARY_ERROR)
        return -1;
    return 0;
}

/* Main exact-ratio inverse-angle path.  numerator and denominator must be
   positive regular numbers.  quadrant selects the final atan2 quadrant. */
int ppf_fast_atan_ratio_internal(ppf_t *rop,
                                 const ppf_t *numerator,
                                 const ppf_t *denominator,
                                 int quadrant,
                                 ppf_rnd_t rnd,
                                 ppf_context_t *ctx,
                                 int *ternary_out) {
    ppf_context_t wide, trial;
    ppf_t a, b, y, temp_result;
    int swapped = 0;
    int decided = 0;
    int ternary = 0;
    if (!rop || !numerator || !denominator || !ctx ||
        !ppf_regular_p(numerator) || !ppf_regular_p(denominator) ||
        ppf_sgn(numerator, ctx) <= 0 || ppf_sgn(denominator, ctx) <= 0 ||
        rop->precision < PPF_FAST_ATAN_HALLEY_MIN_PREC ||
        quadrant < PPF_ATAN_Q_POS || quadrant > PPF_ATAN_Q_NEG_PI_PLUS)
        return 0;
    if (rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!ppf_rounding_valid(rnd)) return 0;
    memset(&wide, 0, sizeof(wide)); memset(&trial, 0, sizeof(trial));
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    memset(&y, 0, sizeof(y)); memset(&temp_result, 0, sizeof(temp_result));
    if (ppf_context_clone(&wide, ctx) != 0 ||
        ppf_context_clone(&trial, ctx) != 0)
        goto done;
    wide.emin = PPF_EMIN_MIN; wide.emax = PPF_EMAX_MAX; wide.flags = 0;
    trial.flags = 0;
    if (ia_init(&a, numerator->precision, &wide) != 0 ||
        ia_init(&b, denominator->precision, &wide) != 0 ||
        ia_init(&temp_result, rop->precision, &trial) != 0)
        goto done;
    if (ppf_set(&a, numerator, PPF_RNDN, &wide) == PPF_TERNARY_ERROR ||
        ppf_set(&b, denominator, PPF_RNDN, &wide) == PPF_TERNARY_ERROR)
        goto done;
    if (ppf_cmp(&a, &b, &wide) > 0) {
        ppf_swap(&a, &b);
        swapped = 1;
    }
    if (ppf_cmp(&a, &b, &wide) == 0) {
        /* alpha=pi/4.  Skip the iteration and let the common composition
           below handle all quadrants and rounding modes. */
    } else {
        ppf_prec_t qmax = (rop->precision + 384u) / 3u + 96u;
        if (qmax < rop->precision / 3u) goto done;
        if (ia_build_halley(&y, &a, &b, qmax, &wide) != 0) goto done;
    }

    for (unsigned attempt = 0; attempt < PPF_FAST_ATAN_HALLEY_ATTEMPTS; ++attempt) {
        ppf_prec_t work = rop->precision + PPF_FAST_ATAN_HALLEY_GUARD +
                          (ppf_prec_t)attempt * 96u;
        ppf_t aw, bw, yw, t, tlo, thi, btlo, bthi, atlo, athi;
        ppf_t nlo, nhi, dlo, dhi, ulo, uhi, clo, chi, beta_lo, beta_hi;
        ppf_t alpha_lo, alpha_hi, pi_lo, pi_hi, half_lo, half_hi;
        ppf_t final_lo, final_hi;
        mpz_t zl, zh;
        ppf_exp_t exponent;
        ppf_interval_status_t status;
        int attempt_ok = 0;
#define IA_ZERO(X) memset(&(X), 0, sizeof(X))
        IA_ZERO(aw); IA_ZERO(bw); IA_ZERO(yw); IA_ZERO(t); IA_ZERO(tlo); IA_ZERO(thi);
        IA_ZERO(btlo); IA_ZERO(bthi); IA_ZERO(atlo); IA_ZERO(athi);
        IA_ZERO(nlo); IA_ZERO(nhi); IA_ZERO(dlo); IA_ZERO(dhi);
        IA_ZERO(ulo); IA_ZERO(uhi); IA_ZERO(clo); IA_ZERO(chi);
        IA_ZERO(beta_lo); IA_ZERO(beta_hi); IA_ZERO(alpha_lo); IA_ZERO(alpha_hi);
        IA_ZERO(pi_lo); IA_ZERO(pi_hi); IA_ZERO(half_lo); IA_ZERO(half_hi);
        IA_ZERO(final_lo); IA_ZERO(final_hi);
#undef IA_ZERO
        mpz_inits(zl, zh, NULL);
#define IA_INIT(X) do { if (ia_init(&(X), work, &wide) != 0) goto attempt_done; } while (0)
        IA_INIT(aw); IA_INIT(bw); IA_INIT(yw); IA_INIT(t); IA_INIT(tlo); IA_INIT(thi);
        IA_INIT(btlo); IA_INIT(bthi); IA_INIT(atlo); IA_INIT(athi);
        IA_INIT(nlo); IA_INIT(nhi); IA_INIT(dlo); IA_INIT(dhi);
        IA_INIT(ulo); IA_INIT(uhi); IA_INIT(clo); IA_INIT(chi);
        IA_INIT(beta_lo); IA_INIT(beta_hi); IA_INIT(alpha_lo); IA_INIT(alpha_hi);
        IA_INIT(final_lo); IA_INIT(final_hi);
#undef IA_INIT
        if (ppf_set(&aw, &a, PPF_RNDN, &wide) == PPF_TERNARY_ERROR ||
            ppf_set(&bw, &b, PPF_RNDN, &wide) == PPF_TERNARY_ERROR)
            goto attempt_done;
        if (ppf_cmp(&a, &b, &wide) == 0) {
            if (ia_get_pi(&pi_lo, &pi_hi, &half_lo, &half_hi, work, &wide) != 0)
                goto attempt_done;
            if (ppf_mul_2si(&beta_lo, &half_lo, -1L, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                ppf_mul_2si(&beta_hi, &half_hi, -1L, PPF_RNDU, &wide) == PPF_TERNARY_ERROR)
                goto attempt_done;
        } else {
            if (ppf_set(&yw, &y, PPF_RNDN, &wide) == PPF_TERNARY_ERROR ||
                ia_tan_interval(&tlo, &thi, &yw, work, &wide) != 0)
                goto attempt_done;

            /* u=(A-B*t)/(B+A*t), with directed endpoint arithmetic. */
            if (ppf_mul(&btlo, &bw, &tlo, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                ppf_mul(&bthi, &bw, &thi, PPF_RNDU, &wide) == PPF_TERNARY_ERROR ||
                ppf_mul(&atlo, &aw, &tlo, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                ppf_mul(&athi, &aw, &thi, PPF_RNDU, &wide) == PPF_TERNARY_ERROR ||
                ppf_sub(&nlo, &aw, &bthi, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                ppf_sub(&nhi, &aw, &btlo, PPF_RNDU, &wide) == PPF_TERNARY_ERROR ||
                ppf_add(&dlo, &bw, &atlo, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                ppf_add(&dhi, &bw, &athi, PPF_RNDU, &wide) == PPF_TERNARY_ERROR ||
                ia_div_interval(&ulo, &uhi, &nlo, &nhi, &dlo, &dhi, &wide) != 0 ||
                ia_correction_interval(&clo, &chi, &ulo, &uhi, &wide) != 0 ||
                ppf_add(&beta_lo, &yw, &clo, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                ppf_add(&beta_hi, &yw, &chi, PPF_RNDU, &wide) == PPF_TERNARY_ERROR)
                goto attempt_done;
        }

        if (swapped) {
            if (!pi_lo.limbs &&
                ia_get_pi(&pi_lo, &pi_hi, &half_lo, &half_hi, work, &wide) != 0)
                goto attempt_done;
            if (ppf_sub(&alpha_lo, &half_lo, &beta_hi, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                ppf_sub(&alpha_hi, &half_hi, &beta_lo, PPF_RNDU, &wide) == PPF_TERNARY_ERROR)
                goto attempt_done;
        } else {
            if (ppf_set(&alpha_lo, &beta_lo, PPF_RNDN, &wide) == PPF_TERNARY_ERROR ||
                ppf_set(&alpha_hi, &beta_hi, PPF_RNDN, &wide) == PPF_TERNARY_ERROR)
                goto attempt_done;
        }

        switch (quadrant) {
            case PPF_ATAN_Q_POS:
                if (ppf_set(&final_lo, &alpha_lo, PPF_RNDN, &wide) == PPF_TERNARY_ERROR ||
                    ppf_set(&final_hi, &alpha_hi, PPF_RNDN, &wide) == PPF_TERNARY_ERROR)
                    goto attempt_done;
                break;
            case PPF_ATAN_Q_NEG:
                if (ppf_neg(&final_lo, &alpha_hi, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                    ppf_neg(&final_hi, &alpha_lo, PPF_RNDU, &wide) == PPF_TERNARY_ERROR)
                    goto attempt_done;
                break;
            case PPF_ATAN_Q_PI_MINUS:
                if (!pi_lo.limbs &&
                    ia_get_pi(&pi_lo, &pi_hi, &half_lo, &half_hi, work, &wide) != 0)
                    goto attempt_done;
                if (ppf_sub(&final_lo, &pi_lo, &alpha_hi, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                    ppf_sub(&final_hi, &pi_hi, &alpha_lo, PPF_RNDU, &wide) == PPF_TERNARY_ERROR)
                    goto attempt_done;
                break;
            case PPF_ATAN_Q_NEG_PI_PLUS:
                if (!pi_lo.limbs &&
                    ia_get_pi(&pi_lo, &pi_hi, &half_lo, &half_hi, work, &wide) != 0)
                    goto attempt_done;
                if (ppf_sub(&final_lo, &alpha_lo, &pi_hi, PPF_RNDD, &wide) == PPF_TERNARY_ERROR ||
                    ppf_sub(&final_hi, &alpha_hi, &pi_lo, PPF_RNDU, &wide) == PPF_TERNARY_ERROR)
                    goto attempt_done;
                break;
        }

        if (ia_common_interval(zl, zh, &exponent, &final_lo, &final_hi) != 0)
            goto attempt_done;
        trial.flags = 0;
        status = ppf_set_dyadic_interval(&temp_result, zl, zh, exponent,
                                         rnd, &ternary, &trial);
        if (status == PPF_INTERVAL_DECIDED) {
            ppf_swap(rop, &temp_result);
            ctx->flags |= trial.flags;
            if (ternary_out) *ternary_out = ternary;
            decided = 1;
            attempt_ok = 1;
        }
attempt_done:
        ia_clear(&aw); ia_clear(&bw); ia_clear(&yw); ia_clear(&t);
        ia_clear(&tlo); ia_clear(&thi); ia_clear(&btlo); ia_clear(&bthi);
        ia_clear(&atlo); ia_clear(&athi); ia_clear(&nlo); ia_clear(&nhi);
        ia_clear(&dlo); ia_clear(&dhi); ia_clear(&ulo); ia_clear(&uhi);
        ia_clear(&clo); ia_clear(&chi); ia_clear(&beta_lo); ia_clear(&beta_hi);
        ia_clear(&alpha_lo); ia_clear(&alpha_hi); ia_clear(&pi_lo); ia_clear(&pi_hi);
        ia_clear(&half_lo); ia_clear(&half_hi); ia_clear(&final_lo); ia_clear(&final_hi);
        mpz_clears(zl, zh, NULL);
        if (attempt_ok) break;
    }
done:
    ia_clear(&a); ia_clear(&b); ia_clear(&y); ia_clear(&temp_result);
    if (wide.allocator.malloc_fn) ppf_context_clear(&wide);
    if (trial.allocator.malloc_fn) ppf_context_clear(&trial);
    return decided;
}

int ppf_fast_atan_halley_internal(ppf_t *rop, const ppf_t *x,
                                  ppf_rnd_t rnd, ppf_context_t *ctx,
                                  int *ternary_out) {
    ppf_t ax, one;
    int quadrant;
    int rc = 0;
    if (!rop || !x || !ctx || !ppf_regular_p(x) || ppf_zero_p(x) ||
        rop->precision < PPF_FAST_ATAN_HALLEY_MIN_PREC)
        return 0;
    memset(&ax, 0, sizeof(ax)); memset(&one, 0, sizeof(one));
    if (ia_init(&ax, x->precision, ctx) != 0 ||
        ia_init(&one, 2u, ctx) != 0)
        goto done;
    if (ppf_abs(&ax, x, PPF_RNDN, ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&one, 1UL, PPF_RNDN, ctx) == PPF_TERNARY_ERROR)
        goto done;
    quadrant = x->sign < 0 ? PPF_ATAN_Q_NEG : PPF_ATAN_Q_POS;
    rc = ppf_fast_atan_ratio_internal(rop, &ax, &one, quadrant,
                                      rnd, ctx, ternary_out);
done:
    ia_clear(&ax); ia_clear(&one);
    return rc;
}
