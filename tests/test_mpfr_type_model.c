/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <stdio.h>

_Static_assert((mpfr_prec_t)-1 < (mpfr_prec_t)0,
               "mpfr_prec_t must be a signed type");
_Static_assert((mpfr_exp_t)-1 < (mpfr_exp_t)0,
               "mpfr_exp_t must be a signed type");

int main(void)
{
    puts("MPFR PRECISION/EXPONENT TYPE MODEL PASSED");
    return 0;
}
