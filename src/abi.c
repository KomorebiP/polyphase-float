/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stddef.h>
#include <string.h>

static uint32_t ppf_abi_endian_value(void)
{
    const uint32_t value = UINT32_C(0x01020304);
    const unsigned char *bytes = (const unsigned char *)&value;

    if (bytes[0] == 0x04u)
        return PPF_ABI_ENDIAN_LITTLE;
    if (bytes[0] == 0x01u)
        return PPF_ABI_ENDIAN_BIG;
    return PPF_ABI_ENDIAN_UNKNOWN;
}

uint32_t ppf_abi_epoch(void)
{
    return (uint32_t)PPF_ABI_EPOCH;
}

uint32_t ppf_abi_revision(void)
{
    return (uint32_t)PPF_ABI_REVISION;
}

const char *ppf_abi_tag(void)
{
    return PPF_ABI_TAG;
}

const char *ppf_abi_contract_sha256(void)
{
    return PPF_ABI_CONTRACT_SHA256;
}

size_t ppf_abi_info_size(void)
{
    return sizeof(ppf_abi_info_t);
}

int ppf_get_abi_info(ppf_abi_info_t *info, size_t info_size)
{
    ppf_abi_info_t current;
    size_t copy_size;

    if (info == NULL || info_size < 4u * sizeof(uint32_t))
        return -1;

    memset(&current, 0, sizeof(current));
    current.struct_size = (uint32_t)sizeof(current);
    current.info_version = (uint32_t)PPF_ABI_INFO_VERSION;
    current.abi_epoch = (uint32_t)PPF_ABI_EPOCH;
    current.abi_revision = (uint32_t)PPF_ABI_REVISION;
    current.endian = ppf_abi_endian_value();
    current.pointer_bits = (uint32_t)(sizeof(void *) * CHAR_BIT);
    current.size_t_bits = (uint32_t)(sizeof(size_t) * CHAR_BIT);
    current.limb_bits = (uint32_t)(sizeof(mp_limb_t) * CHAR_BIT);
#if PPF_HAVE_FLOAT128
    current.feature_flags |= PPF_ABI_FEATURE_FLOAT128;
#endif
#if PPF_HAVE_DECIMAL_FLOAT
    current.feature_flags |= PPF_ABI_FEATURE_DECIMAL_FLOAT;
#endif
    if (ppf_buildopt_tls_p())
        current.feature_flags |= PPF_ABI_FEATURE_TLS;
    if (ppf_buildopt_thread_cleanup_p())
        current.feature_flags |= PPF_ABI_FEATURE_THREAD_EXIT_CLEANUP;

#define PPF_ABI_SIZE_ALIGN(type, size_field, align_field) \
    do { \
        current.size_field = (uint64_t)sizeof(type); \
        current.align_field = (uint64_t)_Alignof(type); \
    } while (0)

    PPF_ABI_SIZE_ALIGN(ppf_allocator_t,
                       sizeof_ppf_allocator_t,
                       alignof_ppf_allocator_t);
    PPF_ABI_SIZE_ALIGN(ppf_arena_mark_t,
                       sizeof_ppf_arena_mark_t,
                       alignof_ppf_arena_mark_t);
    PPF_ABI_SIZE_ALIGN(ppf_context_t,
                       sizeof_ppf_context_t,
                       alignof_ppf_context_t);
    PPF_ABI_SIZE_ALIGN(ppf_t, sizeof_ppf_t, alignof_ppf_t);
    PPF_ABI_SIZE_ALIGN(ppf_product_plan_t,
                       sizeof_ppf_product_plan_t,
                       alignof_ppf_product_plan_t);
    PPF_ABI_SIZE_ALIGN(ppf_constant_stats_t,
                       sizeof_ppf_constant_stats_t,
                       alignof_ppf_constant_stats_t);
    PPF_ABI_SIZE_ALIGN(ppf_abi_info_t,
                       sizeof_ppf_abi_info_t,
                       alignof_ppf_abi_info_t);

#undef PPF_ABI_SIZE_ALIGN

#define PPF_ABI_OFFSET(type, member, field) \
    current.field = (uint64_t)offsetof(type, member)

    PPF_ABI_OFFSET(ppf_context_t, default_precision,
                   offsetof_context_default_precision);
    PPF_ABI_OFFSET(ppf_context_t, emin, offsetof_context_emin);
    PPF_ABI_OFFSET(ppf_context_t, emax, offsetof_context_emax);
    PPF_ABI_OFFSET(ppf_context_t, default_rounding,
                   offsetof_context_default_rounding);
    PPF_ABI_OFFSET(ppf_context_t, flags, offsetof_context_flags);
    PPF_ABI_OFFSET(ppf_context_t, allocator, offsetof_context_allocator);
    PPF_ABI_OFFSET(ppf_context_t, arena_head, offsetof_context_arena_head);
    PPF_ABI_OFFSET(ppf_context_t, arena_current,
                   offsetof_context_arena_current);
    PPF_ABI_OFFSET(ppf_context_t, arena_block_bytes,
                   offsetof_context_arena_block_bytes);

    PPF_ABI_OFFSET(ppf_t, limbs, offsetof_number_limbs);
    PPF_ABI_OFFSET(ppf_t, allocated_limbs,
                   offsetof_number_allocated_limbs);
    PPF_ABI_OFFSET(ppf_t, used_limbs, offsetof_number_used_limbs);
    PPF_ABI_OFFSET(ppf_t, precision, offsetof_number_precision);
    PPF_ABI_OFFSET(ppf_t, exponent, offsetof_number_exponent);
    PPF_ABI_OFFSET(ppf_t, sign, offsetof_number_sign);
    PPF_ABI_OFFSET(ppf_t, kind, offsetof_number_kind);
    PPF_ABI_OFFSET(ppf_t, allocator, offsetof_number_allocator);
    PPF_ABI_OFFSET(ppf_t, storage_flags, offsetof_number_storage_flags);

#undef PPF_ABI_OFFSET

    copy_size = info_size < sizeof(current) ? info_size : sizeof(current);
    memcpy(info, &current, copy_size);
    return info_size < sizeof(current) ? 1 : 0;
}
