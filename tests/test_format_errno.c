/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _GNU_SOURCE
#include "polyphase_float.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*native_asprintf_fn)(char **, const char *, ...);

static void check_errno_format(const char *format, int error_number) {
    native_asprintf_fn reference = asprintf;
    char *actual = NULL;
    char *expected = NULL;
    int actual_length;
    int expected_length;

    errno = error_number;
    actual_length = ppf_asprintf(&actual, format);
    errno = error_number;
    expected_length = reference(&expected, format);

    assert(actual_length == expected_length);
    assert(actual_length >= 0);
    assert(actual != NULL);
    assert(expected != NULL);
    assert(memcmp(actual, expected, (size_t)actual_length + 1u) == 0);

    /* ppf_asprintf follows asprintf ownership and returns storage for free. */
    free(actual);
    free(expected);
}

int main(void) {
    static const char *formats[] = {
        "%m", "%30m", "%-30m", "%.5m", "%030m", "%+30m",
        "% 30m", "%'30m", "%#m", "%#20m", "%#-20.5m"
    };
    static const int errors[] = { 0, ENOENT, EINVAL, 123456 };

    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        for (size_t j = 0; j < sizeof(formats) / sizeof(formats[0]); ++j)
            check_errno_format(formats[j], errors[i]);
    }

    puts("native errno-format tests passed");
    return 0;
}
