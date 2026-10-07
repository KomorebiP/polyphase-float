/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <polyphase_float.h>

#include <string.h>

int main(void) {
    ppf_context_t context;
    ppf_t value;
    int rc = 1;

    if (strcmp(ppf_get_version(), PPF_VERSION_STRING) != 0) return 2;
    if (ppf_context_init(&context) != 0) return 3;
    if (ppf_init2_ctx(&value, 128, &context) != 0) {
        ppf_context_clear(&context);
        return 4;
    }
    if (ppf_const_pi(&value, PPF_RNDN, &context) == PPF_TERNARY_ERROR) goto done;
    if (!ppf_number_p(&value) || ppf_sgn(&value, &context) <= 0) goto done;
    rc = 0;

done:
    ppf_clear(&value);
    ppf_context_clear(&context);
    return rc;
}
