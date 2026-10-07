/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include "polyphase_float.h"

int main(void) {
    ppf_context_t ctx;
    ppf_t x, y;
    gmp_randstate_t state;
    size_t bytes;
    void *storage;

    if (ppf_context_init(&ctx) != 0 ||
        ppf_init2_ctx(&x, 256, &ctx) != 0 ||
        ppf_init2_ctx(&y, 256, &ctx) != 0)
        return 1;

    gmp_randinit_mt(state);
    gmp_randseed_ui(state, 20260812UL);
    if (ppf_urandom(&x, state, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        return 1;
    ppf_printf("uniform=%+.40RNg\n", &x);

    bytes = ppf_custom_get_size(256);
    storage = aligned_alloc(_Alignof(mp_limb_t), bytes);
    if (!storage) return 1;
    ppf_custom_init(storage, 256);
    if (ppf_custom_init_set(&y, PPF_ZERO_KIND, 0, 256, storage) != 0)
        return 1;
    if (ppf_set(&y, &x, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        return 1;
    ppf_printf("external=%+.40RNg\n", &y);

    printf("version=%s float128=%d decimal=%d\n",
           ppf_get_version(), ppf_buildopt_float128_p(),
           ppf_buildopt_decimal_p());

    ppf_clear(&y); /* does not free application-owned storage */
    free(storage);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    gmp_randclear(state);
    ppf_mp_memory_cleanup();
    return 0;
}
