/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

int ppf_log(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_log_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_log_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_log_ui(ppf_t *rop, unsigned long op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_log_ui_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_log_ui_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_log2(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_log2_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_log2_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_log10(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_log10_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_log10_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_log1p(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_log1p_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_log1p_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_log2p1(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_log2p1_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_log2p1_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_log10p1(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_log10p1_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_log10p1_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_exp(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_exp_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_exp_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_exp2(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_exp2_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_exp2_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_exp10(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_exp10_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_exp10_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_expm1(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_expm1_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_expm1_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_exp2m1(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_exp2m1_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_exp2m1_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_exp10m1(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_exp10m1_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_exp10m1_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_pow(ppf_t *rop, const ppf_t *base, const ppf_t *exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_pow_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_pow_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_powr(ppf_t *rop, const ppf_t *base, const ppf_t *exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_powr_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_powr_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_pow_ui(ppf_t *rop, const ppf_t *base, unsigned long exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_pow_ui_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_pow_ui_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_pow_si(ppf_t *rop, const ppf_t *base, long exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_pow_si_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_pow_si_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_pow_uj(ppf_t *rop, const ppf_t *base, uintmax_t exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_pow_uj_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_pow_uj_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_pow_sj(ppf_t *rop, const ppf_t *base, intmax_t exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_pow_sj_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_pow_sj_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_pown(ppf_t *rop, const ppf_t *base, intmax_t exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_pown_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_pown_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_pow_z(ppf_t *rop, const ppf_t *base, mpz_srcptr exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_pow_z_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_pow_z_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_ui_pow_ui(ppf_t *rop, unsigned long base, unsigned long exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_ui_pow_ui_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_ui_pow_ui_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_ui_pow(ppf_t *rop, unsigned long base, const ppf_t *exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_ui_pow_uncommitted_internal(rop, base, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_ui_pow_uncommitted_internal(&tx.staged, base, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_compound_si(ppf_t *rop, const ppf_t *op, long exponent, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_compound_si_uncommitted_internal(rop, op, exponent, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_compound_si_uncommitted_internal(&tx.staged, op, exponent, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_cos(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_cos_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_cos_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sin(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sin_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sin_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_tan(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_tan_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_tan_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_cosu(ppf_t *rop, const ppf_t *op, unsigned long u, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_cosu_uncommitted_internal(rop, op, u, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_cosu_uncommitted_internal(&tx.staged, op, u, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sinu(ppf_t *rop, const ppf_t *op, unsigned long u, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sinu_uncommitted_internal(rop, op, u, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sinu_uncommitted_internal(&tx.staged, op, u, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_tanu(ppf_t *rop, const ppf_t *op, unsigned long u, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_tanu_uncommitted_internal(rop, op, u, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_tanu_uncommitted_internal(&tx.staged, op, u, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_cospi(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_cospi_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_cospi_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sinpi(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sinpi_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sinpi_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_tanpi(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_tanpi_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_tanpi_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sin_cos(ppf_t *sop, ppf_t *cop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t first, second;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!sop || !cop || !ctx || sop == cop)
        return ppf_sin_cos_uncommitted_internal(sop, cop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&first, sop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    if (ppf_result_transaction_begin_internal(&second, cop, ctx) != 0) {
        ppf_result_transaction_abort_internal(&first);
        return PPF_TERNARY_ERROR;
    }
    result = ppf_sin_cos_uncommitted_internal(&first.staged, &second.staged, op, rnd, ctx);
    return ppf_result_transaction_pair_finish_internal(
        &first, sop, &second, cop, result);
}

int ppf_sec(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sec_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sec_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_csc(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_csc_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_csc_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_cot(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_cot_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_cot_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_acos(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_acos_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_acos_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_asin(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_asin_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_asin_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_atan(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_atan_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_atan_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_acosu(ppf_t *rop, const ppf_t *op, unsigned long u, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_acosu_uncommitted_internal(rop, op, u, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_acosu_uncommitted_internal(&tx.staged, op, u, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_asinu(ppf_t *rop, const ppf_t *op, unsigned long u, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_asinu_uncommitted_internal(rop, op, u, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_asinu_uncommitted_internal(&tx.staged, op, u, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_atanu(ppf_t *rop, const ppf_t *op, unsigned long u, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_atanu_uncommitted_internal(rop, op, u, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_atanu_uncommitted_internal(&tx.staged, op, u, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_acospi(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_acospi_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_acospi_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_asinpi(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_asinpi_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_asinpi_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_atanpi(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_atanpi_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_atanpi_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_atan2(ppf_t *rop, const ppf_t *y, const ppf_t *x, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_atan2_uncommitted_internal(rop, y, x, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_atan2_uncommitted_internal(&tx.staged, y, x, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_atan2u(ppf_t *rop, const ppf_t *y, const ppf_t *x, unsigned long u, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_atan2u_uncommitted_internal(rop, y, x, u, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_atan2u_uncommitted_internal(&tx.staged, y, x, u, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_atan2pi(ppf_t *rop, const ppf_t *y, const ppf_t *x, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_atan2pi_uncommitted_internal(rop, y, x, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_atan2pi_uncommitted_internal(&tx.staged, y, x, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_cosh(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_cosh_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_cosh_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sinh(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sinh_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sinh_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_tanh(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_tanh_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_tanh_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sinh_cosh(ppf_t *sop, ppf_t *cop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t first, second;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!sop || !cop || !ctx || sop == cop)
        return ppf_sinh_cosh_uncommitted_internal(sop, cop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&first, sop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    if (ppf_result_transaction_begin_internal(&second, cop, ctx) != 0) {
        ppf_result_transaction_abort_internal(&first);
        return PPF_TERNARY_ERROR;
    }
    result = ppf_sinh_cosh_uncommitted_internal(&first.staged, &second.staged, op, rnd, ctx);
    return ppf_result_transaction_pair_finish_internal(
        &first, sop, &second, cop, result);
}

int ppf_sech(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sech_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sech_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_csch(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_csch_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_csch_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_coth(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_coth_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_coth_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_acosh(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_acosh_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_acosh_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_asinh(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_asinh_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_asinh_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_atanh(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_atanh_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_atanh_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_eint(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_eint_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_eint_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_li2(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_li2_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_li2_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_gamma(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_gamma_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_gamma_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_gamma_inc(ppf_t *rop, const ppf_t *op, const ppf_t *op2, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_gamma_inc_uncommitted_internal(rop, op, op2, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_gamma_inc_uncommitted_internal(&tx.staged, op, op2, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_lngamma(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_lngamma_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_lngamma_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_lgamma(ppf_t *rop, int *signp, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result, staged_sign;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !signp || !ctx)
        return ppf_lgamma_uncommitted_internal(rop, signp, op, rnd, ctx);
    staged_sign = *signp;
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_lgamma_uncommitted_internal(&tx.staged, &staged_sign, op, rnd, ctx);
    result = ppf_result_transaction_finish_internal(&tx, rop, result);
    if (!tx.allocation_failed) *signp = staged_sign;
    return result;
}

int ppf_digamma(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_digamma_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_digamma_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_beta(ppf_t *rop, const ppf_t *op1, const ppf_t *op2, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_beta_uncommitted_internal(rop, op1, op2, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_beta_uncommitted_internal(&tx.staged, op1, op2, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_zeta(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_zeta_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_zeta_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_zeta_ui(ppf_t *rop, unsigned long op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_zeta_ui_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_zeta_ui_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_erf(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_erf_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_erf_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_erfc(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_erfc_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_erfc_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_j0(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_j0_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_j0_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_j1(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_j1_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_j1_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_jn(ppf_t *rop, long n, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_jn_uncommitted_internal(rop, n, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_jn_uncommitted_internal(&tx.staged, n, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_y0(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_y0_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_y0_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_y1(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_y1_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_y1_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_yn(ppf_t *rop, long n, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_yn_uncommitted_internal(rop, n, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_yn_uncommitted_internal(&tx.staged, n, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_agm(ppf_t *rop, const ppf_t *op1, const ppf_t *op2, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_agm_uncommitted_internal(rop, op1, op2, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_agm_uncommitted_internal(&tx.staged, op1, op2, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_ai(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_ai_uncommitted_internal(rop, op, rnd, ctx);
    if (ppf_result_transaction_begin_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_ai_uncommitted_internal(&tx.staged, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

