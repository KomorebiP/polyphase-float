/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <string.h>

static _Thread_local ppf_context_t *tls_result_transaction_context;

ppf_context_t *ppf_result_transaction_context_internal(void) {
    return tls_result_transaction_context;
}

static int validate_same_precision(const ppf_t *destination,
                                   const ppf_t *staged) {
    size_t limbs;
    if (!destination || !staged || !destination->limbs || !staged->limbs ||
        destination->precision != staged->precision)
        return -1;
    limbs = ppf_limbs_for_precision(destination->precision);
    if (limbs == 0U || destination->allocated_limbs < limbs ||
        staged->allocated_limbs < limbs || staged->used_limbs > limbs)
        return -1;
    return 0;
}

static int commit_same_precision(ppf_t *destination, const ppf_t *staged) {
    size_t limbs;
    if (validate_same_precision(destination, staged) != 0) return -1;
    limbs = ppf_limbs_for_precision(destination->precision);

    memcpy(destination->limbs, staged->limbs, limbs * sizeof(mp_limb_t));
    if (destination->allocated_limbs > limbs) {
        memset(destination->limbs + limbs, 0,
               (destination->allocated_limbs - limbs) * sizeof(mp_limb_t));
    }
    destination->used_limbs = staged->used_limbs;
    destination->exponent = staged->exponent;
    destination->sign = staged->sign;
    destination->kind = staged->kind;
    ppf_zero_unused_bits(destination);
    return 0;
}

static int initialize_staged_result(ppf_result_transaction_t *tx,
                                    ppf_t *destination,
                                    ppf_context_t *ctx,
                                    int allow_inline) {
    size_t limbs = ppf_limbs_for_precision(destination->precision);
    if (limbs == 0U) return -1;

    if (allow_inline && limbs <= PPF_RESULT_TRANSACTION_INLINE_LIMBS) {
        memset(&tx->staged, 0, sizeof(tx->staged));
        memset(tx->inline_limbs, 0, sizeof(tx->inline_limbs));
        tx->staged.limbs = tx->inline_limbs;
        tx->staged.allocated_limbs = limbs;
        tx->staged.precision = destination->precision;
        tx->staged.allocator = ctx->allocator;
        tx->staged.storage_flags = PPF_STORAGE_EXTERNAL;
        tx->staged.sign = 1;
        tx->staged.kind = PPF_KIND_NAN;
        return 0;
    }
    return ppf_init2_ctx(&tx->staged, destination->precision, ctx);
}

static void clear_staged_result(ppf_result_transaction_t *tx) {
    if (!tx) return;
    ppf_clear(&tx->staged);
    memset(tx->inline_limbs, 0, sizeof(tx->inline_limbs));
}

static int begin_transaction(ppf_result_transaction_t *tx,
                             ppf_t *destination, ppf_context_t *ctx,
                             int allow_inline) {
    if (!tx) return -1;
    memset(tx, 0, sizeof(*tx));
    ctx = ppf_resolve_context(ctx);
    if (!destination || !ctx) return -1;

    tx->ctx = ctx;
    tx->saved_alloc_flag = ctx->flags & PPF_FLAG_ALLOC;
    ctx->flags &= ~PPF_FLAG_ALLOC;
    tx->failure_epoch = ppf_allocation_failure_epoch_internal();
    if (initialize_staged_result(tx, destination, ctx, allow_inline) != 0) {
        ctx->flags |= PPF_FLAG_ALLOC | tx->saved_alloc_flag;
        tx->allocation_failed = 1;
        return -1;
    }
    tx->previous_active_context = tls_result_transaction_context;
    tls_result_transaction_context = ctx;
    tx->active = 1;
    return 0;
}

int ppf_result_transaction_begin_internal(ppf_result_transaction_t *tx,
                                          ppf_t *destination,
                                          ppf_context_t *ctx) {
    return begin_transaction(tx, destination, ctx, 0);
}

int ppf_result_transaction_begin_inline_internal(
    ppf_result_transaction_t *tx, ppf_t *destination, ppf_context_t *ctx) {
    return begin_transaction(tx, destination, ctx, 1);
}

void ppf_result_transaction_abort_internal(ppf_result_transaction_t *tx) {
    if (!tx) return;
    if (tx->active) {
        clear_staged_result(tx);
        tls_result_transaction_context = tx->previous_active_context;
        tx->active = 0;
    }
    if (tx->ctx) tx->ctx->flags |= tx->saved_alloc_flag;
}

static int transaction_has_allocation_failure(
    const ppf_result_transaction_t *tx) {
    if (!tx || !tx->ctx) return 0;
    return ppf_allocation_failure_epoch_internal() != tx->failure_epoch ||
           (tx->ctx->flags & PPF_FLAG_ALLOC) != 0;
}

int ppf_result_transaction_finish_internal(ppf_result_transaction_t *tx,
                                           ppf_t *destination,
                                           int result) {
    int allocation_failed;
    if (!tx || !tx->active || !tx->ctx || !destination)
        return PPF_TERNARY_ERROR;

    allocation_failed = transaction_has_allocation_failure(tx);
    tx->allocation_failed = allocation_failed;
    if (!allocation_failed &&
        commit_same_precision(destination, &tx->staged) != 0) {
        ppf_raise(tx->ctx, PPF_FLAG_ERANGE);
        result = PPF_TERNARY_ERROR;
    } else if (allocation_failed) {
        tx->ctx->flags |= PPF_FLAG_ALLOC;
        result = PPF_TERNARY_ERROR;
    }
    clear_staged_result(tx);
    tls_result_transaction_context = tx->previous_active_context;
    tx->active = 0;
    tx->ctx->flags |= tx->saved_alloc_flag;
    return result;
}

int ppf_result_transaction_pair_finish_internal(
    ppf_result_transaction_t *first, ppf_t *first_destination,
    ppf_result_transaction_t *second, ppf_t *second_destination,
    int result) {
    int allocation_failed;
    if (!first || !second || !first->active || !second->active ||
        !first->ctx || first->ctx != second->ctx || !first_destination ||
        !second_destination)
        return PPF_TERNARY_ERROR;

    allocation_failed = transaction_has_allocation_failure(first) ||
                        transaction_has_allocation_failure(second);
    first->allocation_failed = allocation_failed;
    second->allocation_failed = allocation_failed;
    if (!allocation_failed) {
        /* Validate both destinations before either fixed-size commit so the
           paired public API remains atomic even under invalid-object misuse. */
        if (validate_same_precision(first_destination, &first->staged) != 0 ||
            validate_same_precision(second_destination, &second->staged) != 0) {
            ppf_raise(first->ctx, PPF_FLAG_ERANGE);
            result = PPF_TERNARY_ERROR;
        } else {
            (void)commit_same_precision(first_destination, &first->staged);
            (void)commit_same_precision(second_destination, &second->staged);
        }
    } else {
        first->ctx->flags |= PPF_FLAG_ALLOC;
        result = PPF_TERNARY_ERROR;
    }
    clear_staged_result(second);
    clear_staged_result(first);
    tls_result_transaction_context = first->previous_active_context;
    first->active = 0;
    second->active = 0;
    first->ctx->flags |= first->saved_alloc_flag | second->saved_alloc_flag;
    return result;
}

int ppf_result_transaction_many_finish_internal(
    ppf_result_transaction_t transactions[], ppf_t *destinations[],
    size_t count, int result) {
    ppf_context_t *ctx;
    ppf_context_t *previous;
    ppf_flags_t saved_alloc_flag = 0;
    int allocation_failed = 0;
    int valid = 1;
    size_t i;

    if (!transactions || !destinations || count == 0U ||
        !transactions[0].active || !transactions[0].ctx)
        return -1;
    ctx = transactions[0].ctx;
    previous = transactions[0].previous_active_context;

    for (i = 0; i < count; ++i) {
        if (!transactions[i].active || transactions[i].ctx != ctx ||
            !destinations[i]) {
            valid = 0;
            break;
        }
        saved_alloc_flag |= transactions[i].saved_alloc_flag;
        if (transaction_has_allocation_failure(&transactions[i]))
            allocation_failed = 1;
    }

    /* The batch status reports whether every element completed.  No output is
       committed unless all elements completed and every destination validates. */
    if (valid && !allocation_failed && result == 0) {
        for (i = 0; i < count; ++i) {
            if (validate_same_precision(destinations[i],
                                        &transactions[i].staged) != 0) {
                valid = 0;
                break;
            }
        }
        if (valid) {
            for (i = 0; i < count; ++i) {
                (void)commit_same_precision(destinations[i],
                                            &transactions[i].staged);
            }
        }
    }

    if (!valid) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        result = -1;
    }
    if (allocation_failed) {
        ctx->flags |= PPF_FLAG_ALLOC;
        result = -1;
    }

    for (i = count; i != 0U; --i) {
        clear_staged_result(&transactions[i - 1U]);
        transactions[i - 1U].active = 0;
        transactions[i - 1U].allocation_failed = allocation_failed;
    }
    tls_result_transaction_context = previous;
    ctx->flags |= saved_alloc_flag;
    return result;
}
