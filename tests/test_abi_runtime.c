/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "polyphase_float.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK_SIZE_ALIGN(info, type, size_field, align_field) do { \
    assert((info).size_field == (uint64_t)sizeof(type)); \
    assert((info).align_field == (uint64_t)_Alignof(type)); \
} while (0)

int main(void)
{
    ppf_abi_info_t info;
    uint32_t prefix[4] = {0, 0, 0, 0};

    memset(&info, 0xa5, sizeof(info));
    assert(ppf_abi_epoch() == PPF_ABI_EPOCH);
    assert(ppf_abi_revision() == PPF_ABI_REVISION);
    assert(strcmp(ppf_abi_tag(), PPF_ABI_TAG) == 0);
    assert(strcmp(ppf_abi_contract_sha256(),
                  PPF_ABI_CONTRACT_SHA256) == 0);
    assert(ppf_abi_info_size() == sizeof(ppf_abi_info_t));
    assert(ppf_get_abi_info(NULL, sizeof(info)) == -1);
    assert(ppf_get_abi_info(&info, 3u * sizeof(uint32_t)) == -1);
    assert(ppf_get_abi_info((ppf_abi_info_t *)prefix,
                            sizeof(prefix)) == 1);
    assert(prefix[0] == sizeof(ppf_abi_info_t));
    assert(prefix[1] == PPF_ABI_INFO_VERSION);
    assert(prefix[2] == PPF_ABI_EPOCH);
    assert(prefix[3] == PPF_ABI_REVISION);

    assert(ppf_get_abi_info(&info, sizeof(info)) == 0);
    assert(info.struct_size == sizeof(info));
    assert(info.info_version == PPF_ABI_INFO_VERSION);
    assert(info.abi_epoch == PPF_ABI_EPOCH);
    assert(info.abi_revision == PPF_ABI_REVISION);
    assert(info.pointer_bits == sizeof(void *) * CHAR_BIT);
    assert(info.size_t_bits == sizeof(size_t) * CHAR_BIT);
    assert(info.limb_bits == sizeof(mp_limb_t) * CHAR_BIT);

    CHECK_SIZE_ALIGN(info, ppf_allocator_t,
                     sizeof_ppf_allocator_t, alignof_ppf_allocator_t);
    CHECK_SIZE_ALIGN(info, ppf_arena_mark_t,
                     sizeof_ppf_arena_mark_t, alignof_ppf_arena_mark_t);
    CHECK_SIZE_ALIGN(info, ppf_context_t,
                     sizeof_ppf_context_t, alignof_ppf_context_t);
    CHECK_SIZE_ALIGN(info, ppf_t, sizeof_ppf_t, alignof_ppf_t);
    CHECK_SIZE_ALIGN(info, ppf_product_plan_t,
                     sizeof_ppf_product_plan_t,
                     alignof_ppf_product_plan_t);
    CHECK_SIZE_ALIGN(info, ppf_constant_stats_t,
                     sizeof_ppf_constant_stats_t,
                     alignof_ppf_constant_stats_t);
    CHECK_SIZE_ALIGN(info, ppf_abi_info_t,
                     sizeof_ppf_abi_info_t, alignof_ppf_abi_info_t);

    assert(info.offsetof_context_default_precision ==
           offsetof(ppf_context_t, default_precision));
    assert(info.offsetof_context_emin == offsetof(ppf_context_t, emin));
    assert(info.offsetof_context_emax == offsetof(ppf_context_t, emax));
    assert(info.offsetof_context_default_rounding ==
           offsetof(ppf_context_t, default_rounding));
    assert(info.offsetof_context_flags == offsetof(ppf_context_t, flags));
    assert(info.offsetof_context_allocator ==
           offsetof(ppf_context_t, allocator));
    assert(info.offsetof_context_arena_head ==
           offsetof(ppf_context_t, arena_head));
    assert(info.offsetof_context_arena_current ==
           offsetof(ppf_context_t, arena_current));
    assert(info.offsetof_context_arena_block_bytes ==
           offsetof(ppf_context_t, arena_block_bytes));

    assert(info.offsetof_number_limbs == offsetof(ppf_t, limbs));
    assert(info.offsetof_number_allocated_limbs ==
           offsetof(ppf_t, allocated_limbs));
    assert(info.offsetof_number_used_limbs == offsetof(ppf_t, used_limbs));
    assert(info.offsetof_number_precision == offsetof(ppf_t, precision));
    assert(info.offsetof_number_exponent == offsetof(ppf_t, exponent));
    assert(info.offsetof_number_sign == offsetof(ppf_t, sign));
    assert(info.offsetof_number_kind == offsetof(ppf_t, kind));
    assert(info.offsetof_number_allocator == offsetof(ppf_t, allocator));
    assert(info.offsetof_number_storage_flags ==
           offsetof(ppf_t, storage_flags));

    puts("ABI runtime contract passed");
    return 0;
}
