/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>
#include <string.h>

static void print_decimal(const char *label, const ppf_t *value, ppf_context_t *ctx) {
    ppf_exp_t exponent = 0;
    char *digits = ppf_get_str(NULL, &exponent, 10, 50, value, PPF_RNDN, ctx);
    if (digits == NULL) {
        printf("%s: conversion failed\n", label);
        return;
    }
    const char *body = digits;
    if (*body == '-') {
        putchar('-');
        ++body;
    }
    if (strchr(body, '@') != NULL || strcmp(body, "0") == 0) {
        printf("%s: %s\n", label, digits);
    } else {
        printf("%s: %c.%s e%lld\n", label, body[0], body + 1,
               (long long)(exponent - 1));
    }
    ppf_free_str(digits);
}

int main(void) {
    ppf_context_t ctx;
    ppf_t x, s, c, a, h;

    if (ppf_context_init(&ctx) != 0 ||
        ppf_init2_ctx(&x, 256, &ctx) != 0 ||
        ppf_init2_ctx(&s, 256, &ctx) != 0 ||
        ppf_init2_ctx(&c, 256, &ctx) != 0 ||
        ppf_init2_ctx(&a, 256, &ctx) != 0 ||
        ppf_init2_ctx(&h, 256, &ctx) != 0) {
        return 1;
    }

    if (ppf_set_str(&x, "0.3", 10, PPF_RNDN, &ctx) != 0 ||
        ppf_sin_cos(&s, &c, &x, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_atan(&a, &x, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_sinh(&h, &x, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR) {
        return 2;
    }

    print_decimal("sin(0.3)", &s, &ctx);
    print_decimal("cos(0.3)", &c, &ctx);
    print_decimal("atan(0.3)", &a, &ctx);
    print_decimal("sinh(0.3)", &h, &ctx);

    ppf_clear(&x);
    ppf_clear(&s);
    ppf_clear(&c);
    ppf_clear(&a);
    ppf_clear(&h);
    ppf_context_clear(&ctx);
    return 0;
}
