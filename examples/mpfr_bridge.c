/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t x;
    mpfr_t external;

    if (ppf_context_init(&ctx) != 0) return 1;
    if (ppf_init2_ctx(&x, 200, &ctx) != 0) return 1;
    mpfr_init2(external, 200);

    mpfr_const_pi(external, MPFR_RNDN);
    if (ppf_import_mpfr_exact(&x, external, &ctx) != 0) return 1;
    ppf_dump_file(stdout, &x);

    mpfr_clear(external);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    return 0;
}
