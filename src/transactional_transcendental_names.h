/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

/* Internal implementation-name remapping for transactional wrappers. */
#ifndef PPF_TRANSACTIONAL_TRANSCENDENTAL_NAMES_H
#define PPF_TRANSACTIONAL_TRANSCENDENTAL_NAMES_H

#define ppf_log ppf_log_uncommitted_internal
#define ppf_log_ui ppf_log_ui_uncommitted_internal
#define ppf_log2 ppf_log2_uncommitted_internal
#define ppf_log10 ppf_log10_uncommitted_internal
#define ppf_log1p ppf_log1p_uncommitted_internal
#define ppf_log2p1 ppf_log2p1_uncommitted_internal
#define ppf_log10p1 ppf_log10p1_uncommitted_internal
#define ppf_exp ppf_exp_uncommitted_internal
#define ppf_exp2 ppf_exp2_uncommitted_internal
#define ppf_exp10 ppf_exp10_uncommitted_internal
#define ppf_expm1 ppf_expm1_uncommitted_internal
#define ppf_exp2m1 ppf_exp2m1_uncommitted_internal
#define ppf_exp10m1 ppf_exp10m1_uncommitted_internal
#define ppf_pow ppf_pow_uncommitted_internal
#define ppf_powr ppf_powr_uncommitted_internal
#define ppf_pow_ui ppf_pow_ui_uncommitted_internal
#define ppf_pow_si ppf_pow_si_uncommitted_internal
#define ppf_pow_uj ppf_pow_uj_uncommitted_internal
#define ppf_pow_sj ppf_pow_sj_uncommitted_internal
#define ppf_pown ppf_pown_uncommitted_internal
#define ppf_pow_z ppf_pow_z_uncommitted_internal
#define ppf_ui_pow_ui ppf_ui_pow_ui_uncommitted_internal
#define ppf_ui_pow ppf_ui_pow_uncommitted_internal
#define ppf_compound_si ppf_compound_si_uncommitted_internal

#endif
