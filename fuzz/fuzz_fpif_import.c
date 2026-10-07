/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _GNU_SOURCE
#include "polyphase_float.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ppf_context_t ctx;
    ppf_t value;
    FILE *stream;
    unsigned char empty = 0;
    void *buffer;

    if (size > 8192u) return 0;
    buffer = size ? (void *)(uintptr_t)data : (void *)&empty;
    stream = fmemopen(buffer, size, "rb");
    if (!stream) return 0;
    if (ppf_context_init(&ctx) == 0 &&
        ppf_init2_ctx(&value, 128u, &ctx) == 0) {
        ppf_set_zero(&value, 1);
        (void)ppf_fpif_import(&value, stream, &ctx);
        if (!ppf_validate(&value)) abort();
        ppf_clear(&value);
        ppf_context_clear(&ctx);
    }
    fclose(stream);
    return 0;
}
