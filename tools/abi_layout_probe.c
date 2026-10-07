/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "polyphase_float.h"
#include <stddef.h>
#include <stdio.h>

#define U64(v) ((unsigned long long)(v))
#define FIELD(name, value, comma) \
    printf("\"%s\":%llu%s", name, U64(value), comma)

static unsigned endian_value(void)
{
    const uint32_t x = UINT32_C(0x01020304);
    const unsigned char *p = (const unsigned char *)&x;
    return p[0] == 4u ? 1u : (p[0] == 1u ? 2u : 0u);
}

int main(void)
{
    printf("{");
    printf("\"schema_version\":1,");
    printf("\"release_version\":\"%s\",", PPF_VERSION_STRING);
    printf("\"model\":{");
    FIELD("pointer_bits", sizeof(void *) * CHAR_BIT, ",");
    FIELD("size_t_bits", sizeof(size_t) * CHAR_BIT, ",");
    FIELD("limb_bits", sizeof(mp_limb_t) * CHAR_BIT, ",");
    FIELD("endian", endian_value(), "" );
    printf("},\"types\":{");
    FIELD("sizeof_ppf_prec_t", sizeof(ppf_prec_t), ",");
    FIELD("sizeof_ppf_exp_t", sizeof(ppf_exp_t), ",");
    FIELD("sizeof_ppf_flags_t", sizeof(ppf_flags_t), ",");
    FIELD("sizeof_ppf_allocator_t", sizeof(ppf_allocator_t), ",");
    FIELD("alignof_ppf_allocator_t", _Alignof(ppf_allocator_t), ",");
    FIELD("sizeof_ppf_arena_mark_t", sizeof(ppf_arena_mark_t), ",");
    FIELD("alignof_ppf_arena_mark_t", _Alignof(ppf_arena_mark_t), ",");
    FIELD("sizeof_ppf_context_t", sizeof(ppf_context_t), ",");
    FIELD("alignof_ppf_context_t", _Alignof(ppf_context_t), ",");
    FIELD("sizeof_ppf_t", sizeof(ppf_t), ",");
    FIELD("alignof_ppf_t", _Alignof(ppf_t), ",");
    FIELD("sizeof_ppf_product_plan_t", sizeof(ppf_product_plan_t), ",");
    FIELD("alignof_ppf_product_plan_t", _Alignof(ppf_product_plan_t), ",");
    FIELD("sizeof_ppf_constant_stats_t", sizeof(ppf_constant_stats_t), ",");
    FIELD("alignof_ppf_constant_stats_t", _Alignof(ppf_constant_stats_t), "");
    printf("},\"offsets\":{");
    FIELD("context.default_precision", offsetof(ppf_context_t, default_precision), ",");
    FIELD("context.emin", offsetof(ppf_context_t, emin), ",");
    FIELD("context.emax", offsetof(ppf_context_t, emax), ",");
    FIELD("context.default_rounding", offsetof(ppf_context_t, default_rounding), ",");
    FIELD("context.flags", offsetof(ppf_context_t, flags), ",");
    FIELD("context.allocator", offsetof(ppf_context_t, allocator), ",");
    FIELD("context.arena_head", offsetof(ppf_context_t, arena_head), ",");
    FIELD("context.arena_current", offsetof(ppf_context_t, arena_current), ",");
    FIELD("context.arena_block_bytes", offsetof(ppf_context_t, arena_block_bytes), ",");
    FIELD("number.limbs", offsetof(ppf_t, limbs), ",");
    FIELD("number.allocated_limbs", offsetof(ppf_t, allocated_limbs), ",");
    FIELD("number.used_limbs", offsetof(ppf_t, used_limbs), ",");
    FIELD("number.precision", offsetof(ppf_t, precision), ",");
    FIELD("number.exponent", offsetof(ppf_t, exponent), ",");
    FIELD("number.sign", offsetof(ppf_t, sign), ",");
    FIELD("number.kind", offsetof(ppf_t, kind), ",");
    FIELD("number.allocator", offsetof(ppf_t, allocator), ",");
    FIELD("number.storage_flags", offsetof(ppf_t, storage_flags), "");
    printf("},\"values\":{");
    FIELD("PPF_RND_DEFAULT", PPF_RND_DEFAULT, ",");
    FIELD("PPF_RNDN", PPF_RNDN, ",");
    FIELD("PPF_RNDZ", PPF_RNDZ, ",");
    FIELD("PPF_RNDU", PPF_RNDU, ",");
    FIELD("PPF_RNDD", PPF_RNDD, ",");
    FIELD("PPF_RNDA", PPF_RNDA, ",");
    FIELD("PPF_RNDF", PPF_RNDF, ",");
    FIELD("PPF_KIND_REGULAR", PPF_KIND_REGULAR, ",");
    FIELD("PPF_KIND_ZERO", PPF_KIND_ZERO, ",");
    FIELD("PPF_KIND_INF", PPF_KIND_INF, ",");
    FIELD("PPF_KIND_NAN", PPF_KIND_NAN, ",");
    FIELD("PPF_FLAG_UNDERFLOW", PPF_FLAG_UNDERFLOW, ",");
    FIELD("PPF_FLAG_OVERFLOW", PPF_FLAG_OVERFLOW, ",");
    FIELD("PPF_FLAG_DIVBY0", PPF_FLAG_DIVBY0, ",");
    FIELD("PPF_FLAG_NAN", PPF_FLAG_NAN, ",");
    FIELD("PPF_FLAG_INEXACT", PPF_FLAG_INEXACT, ",");
    FIELD("PPF_FLAG_ERANGE", PPF_FLAG_ERANGE, ",");
    FIELD("PPF_FLAG_ALLOC", PPF_FLAG_ALLOC, ",");
    FIELD("PPF_PRODUCT_AUTO", PPF_PRODUCT_AUTO, ",");
    FIELD("PPF_PRODUCT_FULL", PPF_PRODUCT_FULL, ",");
    FIELD("PPF_PRODUCT_KARATSUBA3", PPF_PRODUCT_KARATSUBA3, ",");
    FIELD("PPF_PRODUCT_TOOM3", PPF_PRODUCT_TOOM3, ",");
    FIELD("PPF_PRODUCT_WINDOW", PPF_PRODUCT_WINDOW, ",");
    FIELD("PPF_CONSTANT_PI", PPF_CONSTANT_PI, ",");
    FIELD("PPF_CONSTANT_LOG2", PPF_CONSTANT_LOG2, ",");
    FIELD("PPF_CONSTANT_EULER", PPF_CONSTANT_EULER, ",");
    FIELD("PPF_CONSTANT_CATALAN", PPF_CONSTANT_CATALAN, "");
    printf("}}\n");
    return 0;
}
