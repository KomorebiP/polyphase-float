/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t pi, log2_value;
    if (ppf_context_init(&ctx) != 0) return 1;
    if (ppf_init2_ctx(&pi, 256, &ctx) != 0 ||
        ppf_init2_ctx(&log2_value, 256, &ctx) != 0) return 1;
    ppf_set_constant_threads(4UL);
    if (ppf_const_pi(&pi, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_const_log2(&log2_value, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        return 1;
    fputs("pi = ", stdout); ppf_dump_file(stdout, &pi);
    fputs("log(2) = ", stdout); ppf_dump_file(stdout, &log2_value);
    ppf_clear(&pi);
    ppf_clear(&log2_value);
    ppf_context_clear(&ctx);
    ppf_free_constant_caches();
    return 0;
}
