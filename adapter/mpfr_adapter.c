/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

ppf_rnd_t ppf_rounding_from_mpfr(mpfr_rnd_t rnd) {
    switch (rnd) {
        case MPFR_RNDN: return PPF_RNDN;
        case MPFR_RNDZ: return PPF_RNDZ;
        case MPFR_RNDU: return PPF_RNDU;
        case MPFR_RNDD: return PPF_RNDD;
        case MPFR_RNDA: return PPF_RNDA;
        case MPFR_RNDF: return PPF_RNDF;
        default: return PPF_RNDN;
    }
}

mpfr_rnd_t ppf_rounding_to_mpfr(ppf_rnd_t rnd) {
    switch (rnd) {
        case PPF_RNDN: return MPFR_RNDN;
        case PPF_RNDZ: return MPFR_RNDZ;
        case PPF_RNDU: return MPFR_RNDU;
        case PPF_RNDD: return MPFR_RNDD;
        case PPF_RNDA: return MPFR_RNDA;
        case PPF_RNDF: return MPFR_RNDF;
        default: return MPFR_RNDN;
    }
}

static int import_special(ppf_t *rop, mpfr_srcptr op) {
    if (mpfr_nan_p(op)) {
        ppf_set_nan(rop, mpfr_signbit(op) ? -1 : 1);
        return 1;
    }
    if (mpfr_inf_p(op)) {
        ppf_set_inf(rop, mpfr_signbit(op) ? -1 : 1);
        return 1;
    }
    if (mpfr_zero_p(op)) {
        ppf_set_zero(rop, mpfr_signbit(op) ? -1 : 1);
        return 1;
    }
    return 0;
}

int ppf_import_mpfr_exact(ppf_t *rop, mpfr_srcptr op,
                          ppf_context_t *ctx) {
    mpz_t z;
    mpfr_exp_t e;
    ppf_context_t wide;
    ppf_prec_t precision;
    int t;
    if (!rop || !op) return -1;
    ctx = ctx ? ctx : ppf_default_context();
    if (!ctx) return -1;
    precision = (ppf_prec_t)mpfr_get_prec(op);
    if (ppf_set_prec(rop, precision) != 0) return -1;
    if (import_special(rop, op)) return 0;
    if (ppf_context_clone(&wide, ctx) != 0) return -1;
    wide.emin = PPF_EMIN_MIN;
    wide.emax = PPF_EMAX_MAX;
    wide.flags = 0;
    mpz_init(z);
    e = mpfr_get_z_2exp(z, op);
    t = ppf_set_z_2exp(rop, z, (ppf_exp_t)e, PPF_RNDN, &wide);
    mpz_clear(z);
    ppf_context_clear(&wide);
    return t == 0 ? 0 : -1;
}

int ppf_set_mpfr(ppf_t *rop, mpfr_srcptr op,
                 ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    mpfr_exp_t e;
    int t;
    if (!rop || !op) return 0;
    if (mpfr_nan_p(op)) {
        ppf_set_nan(rop, mpfr_signbit(op) ? -1 : 1);
        ppf_context_flags_set(ctx, PPF_FLAG_NAN);
        return 0;
    }
    if (mpfr_inf_p(op)) {
        ppf_set_inf(rop, mpfr_signbit(op) ? -1 : 1);
        return 0;
    }
    if (mpfr_zero_p(op)) {
        ppf_set_zero(rop, mpfr_signbit(op) ? -1 : 1);
        return 0;
    }
    mpz_init(z);
    e = mpfr_get_z_2exp(z, op);
    t = ppf_set_z_2exp(rop, z, (ppf_exp_t)e, rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_get_mpfr(mpfr_ptr rop, const ppf_t *op, mpfr_rnd_t rnd) {
    mpz_t z;
    ppf_exp_t e;
    int t;
    if (!rop || !op) return 0;
    if (ppf_nan_p(op)) {
        mpfr_set_nan(rop);
        (void)mpfr_setsign(rop, rop, op->sign < 0, MPFR_RNDN);
        return 0;
    }
    if (ppf_inf_p(op)) {
        mpfr_set_inf(rop, op->sign);
        return 0;
    }
    if (ppf_zero_p(op)) {
        mpfr_set_zero(rop, op->sign);
        return 0;
    }
    mpz_init(z);
    e = ppf_get_z_2exp(z, op);
    t = mpfr_set_z_2exp(rop, z, (mpfr_exp_t)e, rnd);
    mpz_clear(z);
    return t;
}
