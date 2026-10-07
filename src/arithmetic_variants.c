/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef int (*ppf_binary_fn)(ppf_t *, const ppf_t *, const ppf_t *,
                             ppf_rnd_t, ppf_context_t *);

typedef enum {
    PPF_Q_ADD = 0,
    PPF_Q_SUB = 1,
    PPF_Q_MUL = 2,
    PPF_Q_DIV = 3
} ppf_q_operation_t;

static int variant_error(ppf_t *rop, ppf_context_t *ctx) {
    ctx = ppf_resolve_context(ctx);
    if (rop) ppf_set_nan(rop, 1);
    if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

static int wide_context_init(ppf_context_t *wide,
                             const ppf_context_t *base) {
    if (!wide || !base) return -1;
    if (ppf_context_init_custom(wide, &base->allocator) != 0) return -1;
    wide->default_precision = base->default_precision;
    wide->default_rounding = base->default_rounding;
    wide->emin = PPF_EMIN_MIN;
    wide->emax = PPF_EMAX_MAX;
    wide->flags = 0;
    return 0;
}

static ppf_prec_t precision_for_mpz(mpz_srcptr value) {
    mpz_t magnitude;
    size_t bits;
    if (!value || mpz_sgn(value) == 0) return PPF_PREC_MIN;
    mpz_init(magnitude);
    mpz_abs(magnitude, value);
    bits = mpz_sizeinbase(magnitude, 2);
    mpz_clear(magnitude);
    if (bits < PPF_PREC_MIN) bits = PPF_PREC_MIN;
    if ((uintmax_t)bits > UINT64_MAX) return 0;
    return (ppf_prec_t)bits;
}

static int temp_from_z(ppf_t *tmp, mpz_srcptr value,
                       ppf_context_t *base,
                       ppf_context_t *wide) {
    ppf_prec_t precision = precision_for_mpz(value);
    int t;
    if (!precision || wide_context_init(wide, base) != 0) return -1;
    if (ppf_init2_ctx(tmp, precision, base) != 0) {
        ppf_context_clear(wide);
        return -1;
    }
    t = ppf_set_z(tmp, value, PPF_RNDN, wide);
    if (t != 0) {
        ppf_clear(tmp);
        ppf_context_clear(wide);
        return -1;
    }
    return 0;
}

static int temp_from_d(ppf_t *tmp, double value,
                       ppf_context_t *base,
                       ppf_context_t *wide) {
    int t;
    if (wide_context_init(wide, base) != 0) return -1;
    if (ppf_init2_ctx(tmp, DBL_MANT_DIG, base) != 0) {
        ppf_context_clear(wide);
        return -1;
    }
    t = ppf_set_d(tmp, value, PPF_RNDN, wide);
    if (t == PPF_TERNARY_ERROR) {
        ppf_clear(tmp);
        ppf_context_clear(wide);
        return -1;
    }
    /* Every finite C double is exactly representable in DBL_MANT_DIG bits.
       NaN and infinity have no ternary relation but are valid temporaries. */
    if (isfinite(value) && t != 0) {
        ppf_clear(tmp);
        ppf_context_clear(wide);
        return -1;
    }
    return 0;
}

static int apply_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value,
                   int scalar_first, ppf_binary_fn function,
                   ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t temp;
    ppf_context_t wide;
    int t;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !value || !function || !ctx)
        return variant_error(rop, ctx);
    memset(&temp, 0, sizeof(temp));
    memset(&wide, 0, sizeof(wide));
    if (temp_from_z(&temp, value, ctx, &wide) != 0)
        return variant_error(rop, ctx);
    t = scalar_first ? function(rop, &temp, op, rnd, ctx)
                     : function(rop, op, &temp, rnd, ctx);
    ppf_clear(&temp);
    ppf_context_clear(&wide);
    return t;
}

static int apply_d(ppf_t *rop, const ppf_t *op, double value,
                   int scalar_first, ppf_binary_fn function,
                   ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t temp;
    ppf_context_t wide;
    int t;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !op || !function || !ctx)
        return variant_error(rop, ctx);
    memset(&temp, 0, sizeof(temp));
    memset(&wide, 0, sizeof(wide));
    if (temp_from_d(&temp, value, ctx, &wide) != 0)
        return variant_error(rop, ctx);
    t = scalar_first ? function(rop, &temp, op, rnd, ctx)
                     : function(rop, op, &temp, rnd, ctx);
    ppf_clear(&temp);
    ppf_context_clear(&wide);
    return t;
}

static int apply_ui(ppf_t *rop, const ppf_t *op, unsigned long value,
                    int scalar_first, ppf_binary_fn function,
                    ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int t;
    mpz_init_set_ui(z, value);
    t = apply_z(rop, op, z, scalar_first, function, rnd, ctx);
    mpz_clear(z);
    return t;
}

static int apply_si(ppf_t *rop, const ppf_t *op, long value,
                    int scalar_first, ppf_binary_fn function,
                    ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    int t;
    mpz_init_set_si(z, value);
    t = apply_z(rop, op, z, scalar_first, function, rnd, ctx);
    mpz_clear(z);
    return t;
}

static int q_special_operation(ppf_t *rop, const ppf_t *op,
                               mpq_srcptr value,
                               ppf_q_operation_t operation,
                               ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t representative;
    int sign = mpq_sgn(value);
    int t;
    mpz_init_set_si(representative, sign < 0 ? -1 : sign > 0 ? 1 : 0);
    if (operation == PPF_Q_ADD)
        t = apply_z(rop, op, representative, 0, ppf_add, rnd, ctx);
    else if (operation == PPF_Q_SUB)
        t = apply_z(rop, op, representative, 0, ppf_sub, rnd, ctx);
    else if (operation == PPF_Q_MUL)
        t = apply_z(rop, op, representative, 0, ppf_mul, rnd, ctx);
    else
        t = apply_z(rop, op, representative, 0, ppf_div, rnd, ctx);
    mpz_clear(representative);
    return t;
}

static int apply_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value,
                   ppf_q_operation_t operation,
                   ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpq_t lhs, rhs, result;
    int result_sign = 1, t;
    int qsign;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !op || !value || !ctx || !ppf_rounding_valid(rnd))
        return variant_error(rop, ctx);
    if (mpz_sgn(mpq_denref(value)) == 0)
        return variant_error(rop, ctx);

    if (!ppf_regular_p(op) && !ppf_zero_p(op))
        return q_special_operation(rop, op, value, operation, rnd, ctx);

    qsign = mpq_sgn(value);
    if (operation == PPF_Q_DIV && qsign == 0) {
        if (ppf_zero_p(op)) {
            /* MPFR preserves the dividend sign in the NaN produced by
               signed-zero divided by an unsigned scalar zero. */
            ppf_set_nan(rop, op->sign);
            ppf_raise(ctx, PPF_FLAG_NAN);
            return 0;
        }
        ppf_set_inf(rop, op->sign);
        ppf_raise(ctx, PPF_FLAG_DIVBY0);
        return 0;
    }

    mpq_inits(lhs, rhs, result, NULL);
    ppf_get_q(lhs, op, ctx);
    mpq_set(rhs, value);
    mpq_canonicalize(rhs);
    if (operation == PPF_Q_ADD) mpq_add(result, lhs, rhs);
    else if (operation == PPF_Q_SUB) mpq_sub(result, lhs, rhs);
    else if (operation == PPF_Q_MUL) mpq_mul(result, lhs, rhs);
    else mpq_div(result, lhs, rhs);

    if (mpq_sgn(result) == 0) {
        if (operation == PPF_Q_ADD || operation == PPF_Q_SUB) {
            if (qsign == 0 && ppf_zero_p(op)) result_sign = op->sign;
            else result_sign = rnd == PPF_RNDD ? -1 : 1;
        } else {
            int scalar_sign = qsign < 0 ? -1 : 1;
            result_sign = op->sign * scalar_sign;
        }
    }
    t = ppf_set_q(rop, result, rnd, ctx);
    if (t != PPF_TERNARY_ERROR && mpq_sgn(result) == 0)
        ppf_set_zero(rop, result_sign);
    mpq_clears(lhs, rhs, result, NULL);
    return t;
}

int ppf_add_ui(ppf_t *rop, const ppf_t *op, unsigned long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_ui(rop, op, value, 0, ppf_add, rnd, ctx);
}

int ppf_add_si(ppf_t *rop, const ppf_t *op, long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_si(rop, op, value, 0, ppf_add, rnd, ctx);
}

int ppf_add_d(ppf_t *rop, const ppf_t *op, double value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_d(rop, op, value, 0, ppf_add, rnd, ctx);
}

int ppf_add_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_z(rop, op, value, 0, ppf_add, rnd, ctx);
}

int ppf_add_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_q(rop, op, value, PPF_Q_ADD, rnd, ctx);
}

int ppf_ui_sub(ppf_t *rop, unsigned long value, const ppf_t *op,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_ui(rop, op, value, 1, ppf_sub, rnd, ctx);
}

int ppf_sub_ui(ppf_t *rop, const ppf_t *op, unsigned long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_ui(rop, op, value, 0, ppf_sub, rnd, ctx);
}

int ppf_si_sub(ppf_t *rop, long value, const ppf_t *op,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_si(rop, op, value, 1, ppf_sub, rnd, ctx);
}

int ppf_sub_si(ppf_t *rop, const ppf_t *op, long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_si(rop, op, value, 0, ppf_sub, rnd, ctx);
}

int ppf_d_sub(ppf_t *rop, double value, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_d(rop, op, value, 1, ppf_sub, rnd, ctx);
}

int ppf_sub_d(ppf_t *rop, const ppf_t *op, double value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_d(rop, op, value, 0, ppf_sub, rnd, ctx);
}

int ppf_z_sub(ppf_t *rop, mpz_srcptr value, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_z(rop, op, value, 1, ppf_sub, rnd, ctx);
}

int ppf_sub_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_z(rop, op, value, 0, ppf_sub, rnd, ctx);
}

int ppf_sub_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_q(rop, op, value, PPF_Q_SUB, rnd, ctx);
}

int ppf_mul_ui(ppf_t *rop, const ppf_t *op, unsigned long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_ui(rop, op, value, 0, ppf_mul, rnd, ctx);
}

int ppf_mul_si(ppf_t *rop, const ppf_t *op, long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_si(rop, op, value, 0, ppf_mul, rnd, ctx);
}

int ppf_mul_d(ppf_t *rop, const ppf_t *op, double value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_d(rop, op, value, 0, ppf_mul, rnd, ctx);
}

int ppf_mul_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_z(rop, op, value, 0, ppf_mul, rnd, ctx);
}

int ppf_mul_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_q(rop, op, value, PPF_Q_MUL, rnd, ctx);
}

int ppf_ui_div(ppf_t *rop, unsigned long value, const ppf_t *op,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_ui(rop, op, value, 1, ppf_div, rnd, ctx);
}

int ppf_div_ui(ppf_t *rop, const ppf_t *op, unsigned long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_ui(rop, op, value, 0, ppf_div, rnd, ctx);
}

int ppf_si_div(ppf_t *rop, long value, const ppf_t *op,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_si(rop, op, value, 1, ppf_div, rnd, ctx);
}

int ppf_div_si(ppf_t *rop, const ppf_t *op, long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_si(rop, op, value, 0, ppf_div, rnd, ctx);
}

int ppf_d_div(ppf_t *rop, double value, const ppf_t *op,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_d(rop, op, value, 1, ppf_div, rnd, ctx);
}

int ppf_div_d(ppf_t *rop, const ppf_t *op, double value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_d(rop, op, value, 0, ppf_div, rnd, ctx);
}

int ppf_div_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_z(rop, op, value, 0, ppf_div, rnd, ctx);
}

int ppf_div_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    return apply_q(rop, op, value, PPF_Q_DIV, rnd, ctx);
}

static int huge_scale_result(ppf_t *rop, const ppf_t *op, int multiply,
                             ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t z;
    ppf_exp_t exponent;
    int t;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !op || !ctx || !ppf_rounding_valid(rnd))
        return variant_error(rop, ctx);
    if (!ppf_regular_p(op))
        return multiply ? ppf_mul_2si(rop, op, 0, rnd, ctx)
                        : ppf_div_2si(rop, op, 0, rnd, ctx);
    mpz_init_set_si(z, op->sign < 0 ? -1 : 1);
    exponent = multiply ? ctx->emax + 1 : ctx->emin - 4;
    t = ppf_set_z_2exp(rop, z, exponent, rnd, ctx);
    mpz_clear(z);
    return t;
}

int ppf_mul_2ui(ppf_t *rop, const ppf_t *op, unsigned long shift,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    if (shift <= (unsigned long)LONG_MAX)
        return ppf_mul_2si(rop, op, (long)shift, rnd, ctx);
    return huge_scale_result(rop, op, 1, rnd, ctx);
}

int ppf_div_2ui(ppf_t *rop, const ppf_t *op, unsigned long shift,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    if (shift <= (unsigned long)LONG_MAX)
        return ppf_div_2si(rop, op, (long)shift, rnd, ctx);
    return huge_scale_result(rop, op, 0, rnd, ctx);
}

int ppf_fac_ui(ppf_t *rop, unsigned long value,
               ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t factorial;
    int t;
    if (!rop) return variant_error(rop, ctx);
    mpz_init(factorial);
    mpz_fac_ui(factorial, value);
    t = ppf_set_z(rop, factorial, rnd, ctx);
    mpz_clear(factorial);
    return t;
}
