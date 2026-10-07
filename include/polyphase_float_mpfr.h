/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef POLYPHASE_FLOAT_MPFR_H
#define POLYPHASE_FLOAT_MPFR_H

#include "polyphase_float.h"

#ifdef PPF_USE_MINIMAL_MPFR_ABI
#include "mpfr_minimal.h"
#else
#include <mpfr.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Exact import adopts the source precision and does not narrow the value. */
PPF_API int ppf_import_mpfr_exact(ppf_t *rop, mpfr_srcptr op,
                                  ppf_context_t *ctx);

/* Rounded import keeps the existing precision of rop and applies the supplied
   Polyphase rounding mode and context range. */
PPF_API int ppf_set_mpfr(ppf_t *rop, mpfr_srcptr op,
                         ppf_rnd_t rnd, ppf_context_t *ctx);

/* Export to an initialized MPFR destination using its own precision and the
   requested MPFR rounding mode. */
PPF_API int ppf_get_mpfr(mpfr_ptr rop, const ppf_t *op, mpfr_rnd_t rnd);

PPF_API ppf_rnd_t ppf_rounding_from_mpfr(mpfr_rnd_t rnd);
PPF_API mpfr_rnd_t ppf_rounding_to_mpfr(ppf_rnd_t rnd);

#ifdef __cplusplus
}
#endif

#endif
