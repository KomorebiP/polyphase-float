/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t x, y;

    if (ppf_context_init(&ctx) != 0) return 1;
    if (ppf_context_set_default_precision(&ctx, 256) != 0) return 1;
    if (ppf_init(&x) != 0 || ppf_init2_ctx(&y, 80, &ctx) != 0) return 1;

    (void)ppf_set_ui(&x, 31, PPF_RNDN, &ctx);
    (void)ppf_set(&y, &x, PPF_RNDN, &ctx);

    ppf_dump_file(stdout, &x);
    ppf_dump_file(stdout, &y);

    ppf_clear(&x);
    ppf_clear(&y);
    ppf_context_clear(&ctx);
    return 0;
}
