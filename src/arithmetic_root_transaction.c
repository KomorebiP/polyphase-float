/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define PPF_BUILDING_ARITHMETIC_ROOT_TRANSACTION_WRAPPERS 1
#include "ppf_internal.h"

#include <stddef.h>
#include <string.h>

int ppf_add(ppf_t *rop, const ppf_t *a, const ppf_t *b, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_add_uncommitted_internal(rop, a, b, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_add_uncommitted_internal(&tx.staged, a, b, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_add_ui(ppf_t *rop, const ppf_t *op, unsigned long value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_add_ui_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_add_ui_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_add_si(ppf_t *rop, const ppf_t *op, long value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_add_si_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_add_si_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_add_d(ppf_t *rop, const ppf_t *op, double value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_add_d_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_add_d_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_add_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_add_z_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_add_z_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_add_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_add_q_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_add_q_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sub(ppf_t *rop, const ppf_t *a, const ppf_t *b, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sub_uncommitted_internal(rop, a, b, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sub_uncommitted_internal(&tx.staged, a, b, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_ui_sub(ppf_t *rop, unsigned long value, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_ui_sub_uncommitted_internal(rop, value, op, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_ui_sub_uncommitted_internal(&tx.staged, value, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sub_ui(ppf_t *rop, const ppf_t *op, unsigned long value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sub_ui_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sub_ui_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_si_sub(ppf_t *rop, long value, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_si_sub_uncommitted_internal(rop, value, op, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_si_sub_uncommitted_internal(&tx.staged, value, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sub_si(ppf_t *rop, const ppf_t *op, long value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sub_si_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sub_si_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_d_sub(ppf_t *rop, double value, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_d_sub_uncommitted_internal(rop, value, op, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_d_sub_uncommitted_internal(&tx.staged, value, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sub_d(ppf_t *rop, const ppf_t *op, double value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sub_d_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sub_d_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_z_sub(ppf_t *rop, mpz_srcptr value, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_z_sub_uncommitted_internal(rop, value, op, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_z_sub_uncommitted_internal(&tx.staged, value, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sub_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sub_z_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sub_z_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sub_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sub_q_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sub_q_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_mul(ppf_t *rop, const ppf_t *a, const ppf_t *b, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_mul_uncommitted_internal(rop, a, b, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_mul_uncommitted_internal(&tx.staged, a, b, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_mul_ui(ppf_t *rop, const ppf_t *op, unsigned long value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_mul_ui_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_mul_ui_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_mul_si(ppf_t *rop, const ppf_t *op, long value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_mul_si_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_mul_si_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_mul_d(ppf_t *rop, const ppf_t *op, double value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_mul_d_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_mul_d_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_mul_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_mul_z_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_mul_z_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_mul_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_mul_q_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_mul_q_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_mul_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b, ppf_rnd_t rnd, unsigned long workers, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_mul_threads_uncommitted_internal(rop, a, b, rnd, workers, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_mul_threads_uncommitted_internal(&tx.staged, a, b, rnd, workers, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sqr_threads(ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd, unsigned long workers, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sqr_threads_uncommitted_internal(rop, a, rnd, workers, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sqr_threads_uncommitted_internal(&tx.staged, a, rnd, workers, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sqr_algorithm_threads(ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd, unsigned long workers, ppf_product_algorithm_t algorithm, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sqr_algorithm_threads_uncommitted_internal(rop, a, rnd, workers, algorithm, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sqr_algorithm_threads_uncommitted_internal(&tx.staged, a, rnd, workers, algorithm, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_ui_div(ppf_t *rop, unsigned long value, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_ui_div_uncommitted_internal(rop, value, op, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_ui_div_uncommitted_internal(&tx.staged, value, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_div_ui(ppf_t *rop, const ppf_t *op, unsigned long value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_div_ui_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_div_ui_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_si_div(ppf_t *rop, long value, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_si_div_uncommitted_internal(rop, value, op, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_si_div_uncommitted_internal(&tx.staged, value, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_div_si(ppf_t *rop, const ppf_t *op, long value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_div_si_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_div_si_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_d_div(ppf_t *rop, double value, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_d_div_uncommitted_internal(rop, value, op, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_d_div_uncommitted_internal(&tx.staged, value, op, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_div_d(ppf_t *rop, const ppf_t *op, double value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_div_d_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_div_d_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_div_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_div_z_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_div_z_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_div_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_div_q_uncommitted_internal(rop, op, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_div_q_uncommitted_internal(&tx.staged, op, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_fma(ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_fma_uncommitted_internal(rop, a, b, c, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_fma_uncommitted_internal(&tx.staged, a, b, c, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_fma_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c, ppf_rnd_t rnd, unsigned long workers, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_fma_threads_uncommitted_internal(rop, a, b, c, rnd, workers, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_fma_threads_uncommitted_internal(&tx.staged, a, b, c, rnd, workers, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_fma_algorithm_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c, ppf_rnd_t rnd, unsigned long workers, ppf_product_algorithm_t algorithm, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_fma_algorithm_threads_uncommitted_internal(rop, a, b, c, rnd, workers, algorithm, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_fma_algorithm_threads_uncommitted_internal(&tx.staged, a, b, c, rnd, workers, algorithm, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_fms(ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_fms_uncommitted_internal(rop, a, b, c, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_fms_uncommitted_internal(&tx.staged, a, b, c, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_fmma(ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c, const ppf_t *d, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_fmma_uncommitted_internal(rop, a, b, c, d, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_fmma_uncommitted_internal(&tx.staged, a, b, c, d, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_fmms(ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c, const ppf_t *d, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_fmms_uncommitted_internal(rop, a, b, c, d, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_fmms_uncommitted_internal(&tx.staged, a, b, c, d, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_fms_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c, ppf_rnd_t rnd, unsigned long workers, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_fms_threads_uncommitted_internal(rop, a, b, c, rnd, workers, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_fms_threads_uncommitted_internal(&tx.staged, a, b, c, rnd, workers, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_fms_algorithm_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c, ppf_rnd_t rnd, unsigned long workers, ppf_product_algorithm_t algorithm, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_fms_algorithm_threads_uncommitted_internal(rop, a, b, c, rnd, workers, algorithm, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_fms_algorithm_threads_uncommitted_internal(&tx.staged, a, b, c, rnd, workers, algorithm, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_mul_2ui(ppf_t *rop, const ppf_t *a, unsigned long shift, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_mul_2ui_uncommitted_internal(rop, a, shift, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_mul_2ui_uncommitted_internal(&tx.staged, a, shift, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_mul_2si(ppf_t *rop, const ppf_t *a, long shift, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_mul_2si_uncommitted_internal(rop, a, shift, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_mul_2si_uncommitted_internal(&tx.staged, a, shift, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_div_2ui(ppf_t *rop, const ppf_t *a, unsigned long shift, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_div_2ui_uncommitted_internal(rop, a, shift, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_div_2ui_uncommitted_internal(&tx.staged, a, shift, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_div_2si(ppf_t *rop, const ppf_t *a, long shift, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_div_2si_uncommitted_internal(rop, a, shift, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_div_2si_uncommitted_internal(&tx.staged, a, shift, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_fac_ui(ppf_t *rop, unsigned long value, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_fac_ui_uncommitted_internal(rop, value, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_fac_ui_uncommitted_internal(&tx.staged, value, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_dot_threads(ppf_t *rop, const ppf_t *const a[], const ppf_t *const b[], size_t length, ppf_rnd_t rnd, unsigned long workers, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_dot_threads_uncommitted_internal(rop, a, b, length, rnd, workers, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_dot_threads_uncommitted_internal(&tx.staged, a, b, length, rnd, workers, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_sqrt(ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_sqrt_uncommitted_internal(rop, a, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_sqrt_uncommitted_internal(&tx.staged, a, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_rec_sqrt_threads(ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd, unsigned long workers, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_rec_sqrt_threads_uncommitted_internal(rop, a, rnd, workers, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_rec_sqrt_threads_uncommitted_internal(&tx.staged, a, rnd, workers, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_rec_sqrt_algorithm_threads(ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd, unsigned long workers, ppf_product_algorithm_t algorithm, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_rec_sqrt_algorithm_threads_uncommitted_internal(rop, a, rnd, workers, algorithm, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_rec_sqrt_algorithm_threads_uncommitted_internal(&tx.staged, a, rnd, workers, algorithm, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_cbrt(ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_cbrt_uncommitted_internal(rop, a, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_cbrt_uncommitted_internal(&tx.staged, a, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_rootn_ui(ppf_t *rop, const ppf_t *a, unsigned long degree, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_rootn_ui_uncommitted_internal(rop, a, degree, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_rootn_ui_uncommitted_internal(&tx.staged, a, degree, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_rootn_si(ppf_t *rop, const ppf_t *a, long degree, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_rootn_si_uncommitted_internal(rop, a, degree, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_rootn_si_uncommitted_internal(&tx.staged, a, degree, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_root(ppf_t *rop, const ppf_t *a, unsigned long degree, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_root_uncommitted_internal(rop, a, degree, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_root_uncommitted_internal(&tx.staged, a, degree, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

int ppf_hypot(ppf_t *rop, const ppf_t *a, const ppf_t *b, ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_result_transaction_t tx;
    int result;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx)
        return ppf_hypot_uncommitted_internal(rop, a, b, rnd, ctx);
    if (ppf_result_transaction_begin_inline_internal(&tx, rop, ctx) != 0)
        return PPF_TERNARY_ERROR;
    result = ppf_hypot_uncommitted_internal(&tx.staged, a, b, rnd, ctx);
    return ppf_result_transaction_finish_internal(&tx, rop, result);
}

static int transactional_batch_alias_valid(
    ppf_t *outputs[], const ppf_t *const numerators[], size_t length) {
    size_t i;
    size_t j;
    for (i = 0; i < length; ++i) {
        if (!outputs[i] || !numerators[i]) return 0;
        for (j = 0; j < length; ++j) {
            if (i != j && outputs[i] == numerators[j]) return 0;
            if (i != j && outputs[i] == outputs[j]) return 0;
        }
    }
    return 1;
}

int ppf_div_with_plan_many(
    ppf_t *outputs[], const ppf_t *const numerators[], size_t length,
    const ppf_divisor_plan_t *plan, ppf_rnd_t rnd, unsigned long workers,
    int ternaries[], ppf_context_t *ctx) {
    ppf_result_transaction_t *transactions = NULL;
    ppf_t **staged_outputs = NULL;
    int *staged_ternaries = NULL;
    ppf_flags_t saved_alloc_flag;
    uint64_t failure_epoch;
    size_t begun = 0;
    int result;
    int finish_result;

    ctx = ppf_resolve_context(ctx);
    if (!ctx || !plan || (!outputs && length != 0U) ||
        (!numerators && length != 0U) ||
        !transactional_batch_alias_valid(outputs, numerators, length)) {
        return ppf_div_with_plan_many_uncommitted_internal(
            outputs, numerators, length, plan, rnd, workers, ternaries, ctx);
    }
    if (length == 0U) {
        return ppf_div_with_plan_many_uncommitted_internal(
            outputs, numerators, length, plan, rnd, workers, ternaries, ctx);
    }

    saved_alloc_flag = ctx->flags & PPF_FLAG_ALLOC;
    ctx->flags &= ~PPF_FLAG_ALLOC;
    failure_epoch = ppf_allocation_failure_epoch_internal();

    transactions = (ppf_result_transaction_t *)
        ppf_owned_buffer_calloc_internal(ctx, length, sizeof(*transactions));
    staged_outputs = (ppf_t **)ppf_owned_buffer_calloc_internal(
        ctx, length, sizeof(*staged_outputs));
    if (ternaries) {
        staged_ternaries = (int *)ppf_owned_buffer_calloc_internal(
            ctx, length, sizeof(*staged_ternaries));
    }
    if (!transactions || !staged_outputs || (ternaries && !staged_ternaries)) {
        ppf_owned_buffer_free_internal(staged_ternaries);
        ppf_owned_buffer_free_internal(staged_outputs);
        ppf_owned_buffer_free_internal(transactions);
        ctx->flags |= saved_alloc_flag;
        if (ppf_allocation_failure_epoch_internal() != failure_epoch ||
            (ctx->flags & PPF_FLAG_ALLOC) != 0) {
            ctx->flags |= PPF_FLAG_ALLOC;
        }
        return -1;
    }

    for (begun = 0; begun < length; ++begun) {
        if (ppf_result_transaction_begin_inline_internal(&transactions[begun],
                                                  outputs[begun], ctx) != 0)
            break;
        staged_outputs[begun] = &transactions[begun].staged;
    }
    if (begun != length) {
        while (begun != 0U) {
            --begun;
            ppf_result_transaction_abort_internal(&transactions[begun]);
        }
        ppf_owned_buffer_free_internal(staged_ternaries);
        ppf_owned_buffer_free_internal(staged_outputs);
        ppf_owned_buffer_free_internal(transactions);
        ctx->flags |= saved_alloc_flag | PPF_FLAG_ALLOC;
        return -1;
    }

    result = ppf_div_with_plan_many_uncommitted_internal(
        staged_outputs, numerators, length, plan, rnd, workers,
        staged_ternaries, ctx);
    finish_result = ppf_result_transaction_many_finish_internal(
        transactions, outputs, length, result);
    if (finish_result == 0 && ternaries) {
        memcpy(ternaries, staged_ternaries, length * sizeof(*ternaries));
    }

    ppf_owned_buffer_free_internal(staged_ternaries);
    ppf_owned_buffer_free_internal(staged_outputs);
    ppf_owned_buffer_free_internal(transactions);
    ctx->flags |= saved_alloc_flag;
    return finish_result;
}
