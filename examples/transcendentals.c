/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>
#include <stdlib.h>

static void print_value(const char *label, const ppf_t *value,
                        ppf_context_t *ctx) {
    ppf_exp_t exponent = 0;
    char *digits = ppf_get_str(NULL, &exponent, 10, 50, value,
                               PPF_RNDN, ctx);
    if (!digits) {
        fprintf(stderr, "%s: conversion failed\n", label);
        return;
    }
    printf("%s = 0.%s e%lld\n", label, digits,
           (long long)exponent);
    ppf_free_str(digits);
}

int main(void) {
    ppf_context_t context;
    ppf_t x, y, z;

    if (ppf_context_init(&context) != 0 ||
        ppf_init2_ctx(&x, 256, &context) != 0 ||
        ppf_init2_ctx(&y, 256, &context) != 0 ||
        ppf_init2_ctx(&z, 256, &context) != 0) {
        fputs("initialization failed\n", stderr);
        return EXIT_FAILURE;
    }

    if (ppf_set_str(&x, "1.25", 10, PPF_RNDN, &context) != 0 ||
        ppf_log(&y, &x, PPF_RNDN, &context) == PPF_TERNARY_ERROR ||
        ppf_exp(&z, &y, PPF_RNDN, &context) == PPF_TERNARY_ERROR) {
        fputs("log/exp calculation failed\n", stderr);
        return EXIT_FAILURE;
    }

    print_value("log(1.25)", &y, &context);
    print_value("exp(log(1.25))", &z, &context);

    if (ppf_set_str(&y, "3.25", 10, PPF_RNDN, &context) != 0 ||
        ppf_pow(&z, &x, &y, PPF_RNDN, &context) == PPF_TERNARY_ERROR) {
        fputs("power calculation failed\n", stderr);
        return EXIT_FAILURE;
    }
    print_value("1.25^3.25", &z, &context);

    ppf_clear(&x);
    ppf_clear(&y);
    ppf_clear(&z);
    ppf_context_clear(&context);
    return EXIT_SUCCESS;
}
