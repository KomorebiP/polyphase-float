/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef PPC_MPFR_MINIMAL_H
#define PPC_MPFR_MINIMAL_H

/* Test-only ABI declarations for environments that contain libmpfr.so but not
   the development header. Production builds should install libmpfr-dev and use
   the official <mpfr.h>. */
#include <gmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#if defined(__GNUC__) || defined(__clang__)
#define PPF_MPFR_DEPRECATED __attribute__((__deprecated__))
#else
#define PPF_MPFR_DEPRECATED
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* MPFR's default ABI selects signed precision and exponent types with the
   same width as GMP's mp_size_t/mp_exp_t configuration.  Using the GMP
   typedefs here keeps this test-only declaration layer aligned with the
   official header on both int-sized and long-sized GMP builds. */
typedef mp_size_t mpfr_prec_t;
typedef mp_exp_t mpfr_exp_t;
typedef enum {
    MPFR_RNDN = 0,
    MPFR_RNDZ = 1,
    MPFR_RNDU = 2,
    MPFR_RNDD = 3,
    MPFR_RNDA = 4,
    MPFR_RNDF = 5
} mpfr_rnd_t;

typedef enum {
    MPFR_FREE_LOCAL_CACHE = 1,
    MPFR_FREE_GLOBAL_CACHE = 2
} mpfr_free_cache_t;

typedef struct {
    mpfr_prec_t _mpfr_prec;
    int _mpfr_sign;
    mpfr_exp_t _mpfr_exp;
    mp_limb_t *_mpfr_d;
} __mpfr_struct;
typedef __mpfr_struct mpfr_t[1];
typedef __mpfr_struct *mpfr_ptr;
typedef const __mpfr_struct *mpfr_srcptr;

void mpfr_init2(mpfr_ptr, mpfr_prec_t);
void mpfr_init(mpfr_ptr);
void mpfr_inits(mpfr_ptr, ...);
void mpfr_set_default_prec(mpfr_prec_t);
mpfr_prec_t mpfr_get_default_prec(void);
void mpfr_inits2(mpfr_prec_t, mpfr_ptr, ...);
void mpfr_clears(mpfr_ptr, ...);
void mpfr_clear(mpfr_ptr);
size_t __gmpfr_out_str(FILE *, int, size_t, mpfr_srcptr, mpfr_rnd_t);
#define mpfr_out_str __gmpfr_out_str
size_t __gmpfr_inp_str(mpfr_ptr, FILE *, int, mpfr_rnd_t);
#define mpfr_inp_str __gmpfr_inp_str
int __gmpfr_fpif_export(FILE *, mpfr_ptr);
#define mpfr_fpif_export __gmpfr_fpif_export
int __gmpfr_fpif_import(mpfr_ptr, FILE *);
#define mpfr_fpif_import __gmpfr_fpif_import
int __gmpfr_fprintf(FILE *, const char *, ...);
#define mpfr_fprintf __gmpfr_fprintf
int __gmpfr_vfprintf(FILE *, const char *, va_list);
#define mpfr_vfprintf __gmpfr_vfprintf
int __gmpfr_vasprintf(char **, const char *, va_list);
#define mpfr_vasprintf __gmpfr_vasprintf
int __gmpfr_vsnprintf(char *, size_t, const char *, va_list);
#define mpfr_vsnprintf __gmpfr_vsnprintf
void mpfr_free_str(char *);
mpfr_prec_t mpfr_get_prec(mpfr_srcptr);
mpfr_exp_t mpfr_get_exp(mpfr_srcptr);
int mpfr_set_exp(mpfr_ptr, mpfr_exp_t);
void mpfr_set_prec(mpfr_ptr, mpfr_prec_t);
int mpfr_set_z_2exp(mpfr_ptr, mpz_srcptr, mpfr_exp_t, mpfr_rnd_t);
int mpfr_set(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_set_ui(mpfr_ptr, unsigned long, mpfr_rnd_t);
int mpfr_set_si(mpfr_ptr, long, mpfr_rnd_t);
int __gmpfr_set_uj(mpfr_ptr, uintmax_t, mpfr_rnd_t);
#define mpfr_set_uj __gmpfr_set_uj
int __gmpfr_set_sj(mpfr_ptr, intmax_t, mpfr_rnd_t);
#define mpfr_set_sj __gmpfr_set_sj
int mpfr_set_flt(mpfr_ptr, float, mpfr_rnd_t);
int mpfr_set_d(mpfr_ptr, double, mpfr_rnd_t);
int mpfr_set_ld(mpfr_ptr, long double, mpfr_rnd_t);
int mpfr_set_z(mpfr_ptr, mpz_srcptr, mpfr_rnd_t);
int mpfr_set_q(mpfr_ptr, mpq_srcptr, mpfr_rnd_t);
int mpfr_set_f(mpfr_ptr, mpf_srcptr, mpfr_rnd_t);
int mpfr_set_ui_2exp(mpfr_ptr, unsigned long, mpfr_exp_t, mpfr_rnd_t);
int mpfr_set_si_2exp(mpfr_ptr, long, mpfr_exp_t, mpfr_rnd_t);
int __gmpfr_set_uj_2exp(mpfr_ptr, uintmax_t, intmax_t, mpfr_rnd_t);
#define mpfr_set_uj_2exp __gmpfr_set_uj_2exp
int __gmpfr_set_sj_2exp(mpfr_ptr, intmax_t, intmax_t, mpfr_rnd_t);
#define mpfr_set_sj_2exp __gmpfr_set_sj_2exp
int mpfr_set_str(mpfr_ptr, const char *, int, mpfr_rnd_t);
int mpfr_strtofr(mpfr_ptr, const char *, char **, int, mpfr_rnd_t);
int mpfr_neg(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
void mpfr_set_zero(mpfr_ptr, int);
void mpfr_set_inf(mpfr_ptr, int);
void mpfr_set_nan(mpfr_ptr);
int mpfr_const_pi(mpfr_ptr, mpfr_rnd_t);
int mpfr_const_log2(mpfr_ptr, mpfr_rnd_t);
int mpfr_add(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_add_ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_add_si(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
int mpfr_add_d(mpfr_ptr, mpfr_srcptr, double, mpfr_rnd_t);
int mpfr_add_z(mpfr_ptr, mpfr_srcptr, mpz_srcptr, mpfr_rnd_t);
int mpfr_add_q(mpfr_ptr, mpfr_srcptr, mpq_srcptr, mpfr_rnd_t);
int mpfr_sub(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_ui_sub(mpfr_ptr, unsigned long, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sub_ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_si_sub(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sub_si(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
int mpfr_d_sub(mpfr_ptr, double, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sub_d(mpfr_ptr, mpfr_srcptr, double, mpfr_rnd_t);
int mpfr_z_sub(mpfr_ptr, mpz_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sub_z(mpfr_ptr, mpfr_srcptr, mpz_srcptr, mpfr_rnd_t);
int mpfr_sub_q(mpfr_ptr, mpfr_srcptr, mpq_srcptr, mpfr_rnd_t);
int mpfr_mul(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sqr(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_div(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_ui_div(mpfr_ptr, unsigned long, mpfr_srcptr, mpfr_rnd_t);
int mpfr_div_ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_si_div(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
int mpfr_div_si(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
int mpfr_d_div(mpfr_ptr, double, mpfr_srcptr, mpfr_rnd_t);
int mpfr_div_d(mpfr_ptr, mpfr_srcptr, double, mpfr_rnd_t);
int mpfr_div_z(mpfr_ptr, mpfr_srcptr, mpz_srcptr, mpfr_rnd_t);
int mpfr_div_q(mpfr_ptr, mpfr_srcptr, mpq_srcptr, mpfr_rnd_t);
int mpfr_mul_ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_mul_si(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
int mpfr_mul_d(mpfr_ptr, mpfr_srcptr, double, mpfr_rnd_t);
int mpfr_mul_z(mpfr_ptr, mpfr_srcptr, mpz_srcptr, mpfr_rnd_t);
int mpfr_mul_q(mpfr_ptr, mpfr_srcptr, mpq_srcptr, mpfr_rnd_t);
int mpfr_fma(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_fms(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_fmma(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_fmms(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_fac_ui(mpfr_ptr, unsigned long, mpfr_rnd_t);
int mpfr_sum(mpfr_ptr, const mpfr_ptr *, unsigned long, mpfr_rnd_t);
int mpfr_dot(mpfr_ptr, const mpfr_ptr *, const mpfr_ptr *, unsigned long, mpfr_rnd_t);
int mpfr_mul_2ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_mul_2si(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
int mpfr_div_2ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_div_2si(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
int mpfr_mul_2exp(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_div_2exp(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_sqrt(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sqrt_ui(mpfr_ptr, unsigned long, mpfr_rnd_t);
int mpfr_rec_sqrt(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_cbrt(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_rootn_ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_rootn_si(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
int mpfr_root(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t) PPF_MPFR_DEPRECATED;
int mpfr_hypot(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_log(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_log_ui(mpfr_ptr, unsigned long, mpfr_rnd_t);
int mpfr_log2(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_log10(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_log1p(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_log2p1(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_log10p1(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_exp(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_exp2(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_exp10(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_expm1(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_exp2m1(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_exp10m1(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_pow(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_powr(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_pow_ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_pow_si(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
int mpfr_pown(mpfr_ptr, mpfr_srcptr, intmax_t, mpfr_rnd_t);
int mpfr_pow_z(mpfr_ptr, mpfr_srcptr, mpz_srcptr, mpfr_rnd_t);
int mpfr_ui_pow_ui(mpfr_ptr, unsigned long, unsigned long, mpfr_rnd_t);
int mpfr_ui_pow(mpfr_ptr, unsigned long, mpfr_srcptr, mpfr_rnd_t);
int mpfr_compound_si(mpfr_ptr, mpfr_srcptr, long, mpfr_rnd_t);
int mpfr_eint(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_li2(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_gamma(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_gamma_inc(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_lngamma(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_lgamma(mpfr_ptr, int *, mpfr_srcptr, mpfr_rnd_t);
int mpfr_digamma(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_beta(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_zeta(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_zeta_ui(mpfr_ptr, unsigned long, mpfr_rnd_t);
int mpfr_erf(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_erfc(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_j0(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_j1(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_jn(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
int mpfr_y0(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_y1(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_yn(mpfr_ptr, long, mpfr_srcptr, mpfr_rnd_t);
int mpfr_agm(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_ai(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_const_euler(mpfr_ptr, mpfr_rnd_t);
int mpfr_const_catalan(mpfr_ptr, mpfr_rnd_t);
int mpfr_can_round(mpfr_srcptr, mpfr_exp_t, mpfr_rnd_t, mpfr_rnd_t, mpfr_prec_t);
int mpfr_urandomb(mpfr_ptr, gmp_randstate_t);
int mpfr_urandom(mpfr_ptr, gmp_randstate_t, mpfr_rnd_t);
int mpfr_nrandom(mpfr_ptr, gmp_randstate_t, mpfr_rnd_t);
int mpfr_grandom(mpfr_ptr, mpfr_ptr, gmp_randstate_t, mpfr_rnd_t) PPF_MPFR_DEPRECATED;
int mpfr_erandom(mpfr_ptr, gmp_randstate_t, mpfr_rnd_t);
const char *mpfr_get_version(void);
const char *mpfr_get_patches(void);
int mpfr_buildopt_tls_p(void);
int mpfr_buildopt_float128_p(void);
int mpfr_buildopt_decimal_p(void);
int mpfr_buildopt_gmpinternals_p(void);
int mpfr_buildopt_sharedcache_p(void);
const char *mpfr_buildopt_tune_case(void);
void mpfr_free_cache(void);
void mpfr_free_cache2(mpfr_free_cache_t);
void mpfr_free_pool(void);
int mpfr_mp_memory_cleanup(void);
int mpfr_eq(mpfr_srcptr, mpfr_srcptr, unsigned long);
void mpfr_reldiff(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);

int mpfr_abs(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_setsign(mpfr_ptr, mpfr_srcptr, int, mpfr_rnd_t);
int mpfr_copysign(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_cmp_ui(mpfr_srcptr, unsigned long);
int mpfr_cmp_si(mpfr_srcptr, long);
int mpfr_cmp_ui_2exp(mpfr_srcptr, unsigned long, mpfr_exp_t);
int mpfr_cmp_si_2exp(mpfr_srcptr, long, mpfr_exp_t);
int mpfr_cmpabs(mpfr_srcptr, mpfr_srcptr);
int mpfr_cmpabs_ui(mpfr_srcptr, unsigned long);
int mpfr_greater_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_greaterequal_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_less_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_lessequal_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_lessgreater_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_unordered_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_total_order_p(mpfr_srcptr, mpfr_srcptr);

int mpfr_cmp_d(mpfr_srcptr, double);
int mpfr_cmp_ld(mpfr_srcptr, long double);
int mpfr_cmp_z(mpfr_srcptr, mpz_srcptr);
int mpfr_cmp_q(mpfr_srcptr, mpq_srcptr);
int mpfr_cmp_f(mpfr_srcptr, mpf_srcptr);
int mpfr_cmp_ui_2exp(mpfr_srcptr, unsigned long, mpfr_exp_t);
int mpfr_cmp_si_2exp(mpfr_srcptr, long, mpfr_exp_t);
int mpfr_cmpabs(mpfr_srcptr, mpfr_srcptr);
int mpfr_cmpabs_ui(mpfr_srcptr, unsigned long);
int mpfr_greater_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_greaterequal_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_less_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_lessequal_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_lessgreater_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_unordered_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_copysign(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_ceil(mpfr_ptr, mpfr_srcptr);
int mpfr_floor(mpfr_ptr, mpfr_srcptr);
int mpfr_round(mpfr_ptr, mpfr_srcptr);
int mpfr_roundeven(mpfr_ptr, mpfr_srcptr);
int mpfr_trunc(mpfr_ptr, mpfr_srcptr);
int mpfr_fmod(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_fmod_ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_fmodquo(mpfr_ptr, long *, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_remainder(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_remquo(mpfr_ptr, long *, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
void mpfr_nexttoward(mpfr_ptr, mpfr_srcptr);
int mpfr_max(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_dim(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_rint(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_ceil(mpfr_ptr, mpfr_srcptr);
int mpfr_floor(mpfr_ptr, mpfr_srcptr);
int mpfr_round(mpfr_ptr, mpfr_srcptr);
int mpfr_roundeven(mpfr_ptr, mpfr_srcptr);
int mpfr_trunc(mpfr_ptr, mpfr_srcptr);
int mpfr_rint_ceil(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_rint_floor(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_rint_round(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_rint_roundeven(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_rint_trunc(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_frac(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_modf(mpfr_ptr, mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_integer_p(mpfr_srcptr);
mpfr_prec_t mpfr_min_prec(mpfr_srcptr);
void mpfr_nexttoward(mpfr_ptr, mpfr_srcptr);
int mpfr_min(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_max(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_urandomb(mpfr_ptr, gmp_randstate_t);
int mpfr_equal_p(mpfr_srcptr, mpfr_srcptr);
int mpfr_number_p(mpfr_srcptr);
int mpfr_regular_p(mpfr_srcptr);
int mpfr_zero_p(mpfr_srcptr);
int mpfr_nan_p(mpfr_srcptr);
int mpfr_inf_p(mpfr_srcptr);
int mpfr_signbit(mpfr_srcptr);
int mpfr_sgn(mpfr_srcptr);
int mpfr_cmp(mpfr_srcptr, mpfr_srcptr);
void mpfr_nextbelow(mpfr_ptr);
void mpfr_nextabove(mpfr_ptr);
int mpfr_cmp_z(mpfr_srcptr, mpz_srcptr);
int mpfr_cmp_q(mpfr_srcptr, mpq_srcptr);
int mpfr_cmp_f(mpfr_srcptr, mpf_srcptr);
int mpfr_cmp_d(mpfr_srcptr, double);
int mpfr_cmp_ld(mpfr_srcptr, long double);
int mpfr_fmod(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_fmod_ui(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_fmodquo(mpfr_ptr, long *, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_remainder(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_remquo(mpfr_ptr, long *, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
mpfr_exp_t mpfr_get_z_2exp(mpz_ptr, mpfr_srcptr);
float mpfr_get_flt(mpfr_srcptr, mpfr_rnd_t);
double mpfr_get_d(mpfr_srcptr, mpfr_rnd_t);
long double mpfr_get_ld(mpfr_srcptr, mpfr_rnd_t);
long mpfr_get_si(mpfr_srcptr, mpfr_rnd_t);
unsigned long mpfr_get_ui(mpfr_srcptr, mpfr_rnd_t);
intmax_t __gmpfr_mpfr_get_sj(mpfr_srcptr, mpfr_rnd_t);
#define mpfr_get_sj __gmpfr_mpfr_get_sj
uintmax_t __gmpfr_mpfr_get_uj(mpfr_srcptr, mpfr_rnd_t);
#define mpfr_get_uj __gmpfr_mpfr_get_uj
double mpfr_get_d_2exp(long *, mpfr_srcptr, mpfr_rnd_t);
long double mpfr_get_ld_2exp(long *, mpfr_srcptr, mpfr_rnd_t);
int mpfr_frexp(mpfr_exp_t *, mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_get_z(mpz_ptr, mpfr_srcptr, mpfr_rnd_t);
void mpfr_get_q(mpq_ptr, mpfr_srcptr);
int mpfr_get_f(mpf_ptr, mpfr_srcptr, mpfr_rnd_t);
size_t mpfr_get_str_ndigits(int, mpfr_prec_t);
int mpfr_fits_ulong_p(mpfr_srcptr, mpfr_rnd_t);
int mpfr_fits_slong_p(mpfr_srcptr, mpfr_rnd_t);
int mpfr_fits_uint_p(mpfr_srcptr, mpfr_rnd_t);
int mpfr_fits_sint_p(mpfr_srcptr, mpfr_rnd_t);
int mpfr_fits_ushort_p(mpfr_srcptr, mpfr_rnd_t);
int mpfr_fits_sshort_p(mpfr_srcptr, mpfr_rnd_t);
int mpfr_fits_uintmax_p(mpfr_srcptr, mpfr_rnd_t);
int mpfr_fits_intmax_p(mpfr_srcptr, mpfr_rnd_t);
char *mpfr_get_str(char *, mpfr_exp_t *, int, size_t, mpfr_srcptr, mpfr_rnd_t);
void mpfr_free_str(char *);
void mpfr_free_cache(void);
void mpfr_free_cache2(mpfr_free_cache_t);
int mpfr_mp_memory_cleanup(void);
void mpfr_clear_flags(void);
void mpfr_set_underflow(void);
void mpfr_set_overflow(void);
void mpfr_set_divby0(void);
void mpfr_set_nanflag(void);
void mpfr_set_inexflag(void);
void mpfr_set_erangeflag(void);
int mpfr_inexflag_p(void);
int mpfr_divby0_p(void);
int mpfr_nanflag_p(void);
int mpfr_erangeflag_p(void);
int mpfr_overflow_p(void);
int mpfr_underflow_p(void);
mpfr_exp_t mpfr_get_emin(void);
mpfr_exp_t mpfr_get_emax(void);
int mpfr_set_emin(mpfr_exp_t);
int mpfr_set_emax(mpfr_exp_t);
mpfr_exp_t mpfr_get_emin_min(void);
mpfr_exp_t mpfr_get_emin_max(void);
mpfr_exp_t mpfr_get_emax_min(void);
mpfr_exp_t mpfr_get_emax_max(void);
int mpfr_check_range(mpfr_ptr, int, mpfr_rnd_t);
int mpfr_subnormalize(mpfr_ptr, int, mpfr_rnd_t);
void mpfr_clear_underflow(void);
void mpfr_clear_overflow(void);
void mpfr_clear_divby0(void);
void mpfr_clear_nanflag(void);
void mpfr_clear_inexflag(void);
void mpfr_clear_erangeflag(void);
int mpfr_buildopt_tls_p(void);
int mpfr_buildopt_sharedcache_p(void);
const char *mpfr_get_version(void);
const char *mpfr_print_rnd_mode(mpfr_rnd_t);
int mpfr_printf(const char *, ...);

int mpfr_cos(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sin(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_tan(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_cosu(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_sinu(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_tanu(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_cospi(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sinpi(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_tanpi(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sin_cos(mpfr_ptr, mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sec(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_csc(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_cot(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_acos(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_asin(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_atan(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_acosu(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_asinu(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_atanu(mpfr_ptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_acospi(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_asinpi(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_atanpi(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_atan2(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_atan2u(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, unsigned long, mpfr_rnd_t);
int mpfr_atan2pi(mpfr_ptr, mpfr_srcptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_cosh(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sinh(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_tanh(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sinh_cosh(mpfr_ptr, mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_sech(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_csch(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_coth(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_acosh(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_asinh(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);
int mpfr_atanh(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);

#ifdef MPFR_WANT_FLOAT128
# ifndef mpfr_float128
#  define mpfr_float128 _Float128
# endif
__extension__ int mpfr_set_float128(mpfr_ptr, mpfr_float128, mpfr_rnd_t);
__extension__ mpfr_float128 mpfr_get_float128(mpfr_srcptr, mpfr_rnd_t);
#endif
#ifdef MPFR_WANT_DECIMAL_FLOATS
__extension__ int mpfr_set_decimal64(mpfr_ptr, _Decimal64, mpfr_rnd_t);
__extension__ int mpfr_set_decimal128(mpfr_ptr, _Decimal128, mpfr_rnd_t);
__extension__ _Decimal64 mpfr_get_decimal64(mpfr_srcptr, mpfr_rnd_t);
__extension__ _Decimal128 mpfr_get_decimal128(mpfr_srcptr, mpfr_rnd_t);
#endif

#ifdef __cplusplus
}
#endif

#undef PPF_MPFR_DEPRECATED

#endif
