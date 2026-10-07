/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef PPF_PRODUCT_PROFILE_BUILD_H
#define PPF_PRODUCT_PROFILE_BUILD_H

#define PPF_PRODUCT_PROFILE_NAME "test-profile"
#define PPF_PRODUCT_PROFILE_SHA256 \
    "00c029cbde3fa57ca6cdfbae76f63a36cd67deb62cfdaa1eb8b7f22ff866d3f2"
#define PPF_PRODUCT_PROFILE_RULE_COUNT 2u

static const ppf_product_profile_rule_t
ppf_product_profile_rules[PPF_PRODUCT_PROFILE_RULE_COUNT] = {
    {
        PPF_PRODUCT_OP_MULTIPLY,
        100000u, 100000u,
        1563u, 1563u,
        5u, 10u,
        PPF_PRODUCT_TOOM3
    },
    {
        PPF_PRODUCT_OP_FMA,
        100000u, 100000u,
        1563u, 1563u,
        5u, 10u,
        PPF_PRODUCT_TOOM3
    }
};

#endif
