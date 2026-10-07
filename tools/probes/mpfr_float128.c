/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#if !PPF_HAVE_FLOAT128
#error "compiler has no Polyphase binary128 type"
#endif

#define MPFR_USE_EXTENSION 1
#define MPFR_WANT_FLOAT128 1
#ifndef mpfr_float128
#define mpfr_float128 ppf_float128_t
#endif
#include "polyphase_float_mpfr.h"

int main(void) {
    mpfr_t value;
    ppf_float128_t input = (ppf_float128_t) 1;

    if (mpfr_buildopt_float128_p() == 0) return 1;
    mpfr_init2(value, 53);
    (void) mpfr_set_float128(value, input, MPFR_RNDN);
    input = (ppf_float128_t) mpfr_get_float128(value, MPFR_RNDN);
    mpfr_clear(value);
    return input == (ppf_float128_t) 1 ? 0 : 1;
}
