/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#if !PPF_HAVE_DECIMAL_FLOAT
#error "compiler has no Polyphase decimal64/decimal128 types"
#endif

#define MPFR_USE_EXTENSION 1
#define MPFR_WANT_DECIMAL_FLOATS 1
#include "polyphase_float_mpfr.h"

int main(void) {
    mpfr_t value;
    ppf_decimal64_t input64 = (ppf_decimal64_t) 1;
    ppf_decimal128_t input128 = (ppf_decimal128_t) 1;

    if (mpfr_buildopt_decimal_p() == 0) return 1;
    mpfr_init2(value, 53);
    (void) mpfr_set_decimal64(value, input64, MPFR_RNDN);
    input64 = (ppf_decimal64_t) mpfr_get_decimal64(value, MPFR_RNDN);
    (void) mpfr_set_decimal128(value, input128, MPFR_RNDN);
    input128 = (ppf_decimal128_t) mpfr_get_decimal128(value, MPFR_RNDN);
    mpfr_clear(value);
    return input64 == (ppf_decimal64_t) 1 &&
           input128 == (ppf_decimal128_t) 1 ? 0 : 1;
}
