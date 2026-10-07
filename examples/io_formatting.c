/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    ppf_t x, y;
    FILE *file;

    if (ppf_init2(&x, 256) != 0 || ppf_init2(&y, 32) != 0) return 1;
    if (ppf_const_pi(&x, PPF_RNDN, NULL) == PPF_TERNARY_ERROR) return 1;

    ppf_printf("pi = %.60RNf\n", &x);
    ppf_printf("hex = %.20RNa\n", &x);

    file = tmpfile();
    if (!file) return 1;
    if (ppf_fpif_export(file, &x) != 0) return 1;
    rewind(file);
    if (ppf_fpif_import(&y, file, NULL) != 0) return 1;
    fclose(file);

    if (!ppf_equal_p(&x, &y)) return 1;
    ppf_printf("interchange precision = %Pu bits\n", ppf_get_prec(&y));

    ppf_clear(&x);
    ppf_clear(&y);
    return 0;
}
