/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

/* Internal implementation-name remapping for transactional wrappers. */
#ifndef PPF_TRANSACTIONAL_SPECIAL_NAMES_H
#define PPF_TRANSACTIONAL_SPECIAL_NAMES_H

#define ppf_eint ppf_eint_uncommitted_internal
#define ppf_li2 ppf_li2_uncommitted_internal
#define ppf_gamma ppf_gamma_uncommitted_internal
#define ppf_gamma_inc ppf_gamma_inc_uncommitted_internal
#define ppf_lngamma ppf_lngamma_uncommitted_internal
#define ppf_lgamma ppf_lgamma_uncommitted_internal
#define ppf_digamma ppf_digamma_uncommitted_internal
#define ppf_beta ppf_beta_uncommitted_internal
#define ppf_zeta ppf_zeta_uncommitted_internal
#define ppf_zeta_ui ppf_zeta_ui_uncommitted_internal
#define ppf_erf ppf_erf_uncommitted_internal
#define ppf_erfc ppf_erfc_uncommitted_internal
#define ppf_j0 ppf_j0_uncommitted_internal
#define ppf_j1 ppf_j1_uncommitted_internal
#define ppf_jn ppf_jn_uncommitted_internal
#define ppf_y0 ppf_y0_uncommitted_internal
#define ppf_y1 ppf_y1_uncommitted_internal
#define ppf_yn ppf_yn_uncommitted_internal
#define ppf_agm ppf_agm_uncommitted_internal
#define ppf_ai ppf_ai_uncommitted_internal

#endif
