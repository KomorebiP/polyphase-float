/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ppf_context_t ctx;
    ppf_t value;
    char *text;
    char *end = NULL;
    ppf_prec_t precision;
    ppf_rnd_t rnd;
    int base;

    if (size > 4096u) return 0;
    text = (char *)malloc(size + 1u);
    if (!text) return 0;
    if (size) memcpy(text, data, size);
    text[size] = '\0';

    precision = 2u + (size ? (ppf_prec_t)(data[0] % 255u) : 0u);
    rnd = (ppf_rnd_t)(size > 1u ? data[1] % 6u : 0u);
    base = size > 2u && (data[2] & 1u)
        ? 2 + (int)(data[2] % 61u) : 0;

    if (ppf_context_init(&ctx) == 0 &&
        ppf_init2_ctx(&value, precision, &ctx) == 0) {
        (void)ppf_strtofr(&value, text, &end, base, rnd, &ctx);
        (void)ppf_set_str(&value, text, base, rnd, &ctx);
        if (!ppf_validate(&value)) abort();
        ppf_clear(&value);
        ppf_context_clear(&ctx);
    }
    free(text);
    return 0;
}
