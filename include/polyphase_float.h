/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef POLYPHASE_FLOAT_H
#define POLYPHASE_FLOAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <stdarg.h>
#include <gmp.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(PPF_BUILD_SHARED)
#  ifdef PPF_BUILDING_LIBRARY
#    define PPF_API __declspec(dllexport)
#  else
#    define PPF_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(PPF_BUILD_SHARED)
#  define PPF_API __attribute__((visibility("default")))
#else
#  define PPF_API
#endif

#define PPF_VERSION_MAJOR 0
#define PPF_VERSION_MINOR 18
#define PPF_VERSION_PATCHLEVEL 0
#define PPF_VERSION_STRING "0.18.0"
#define PPF_VERSION_NUM(major, minor, patchlevel) \
    (((long)(major) << 16) | ((long)(minor) << 8) | (long)(patchlevel))
#define PPF_VERSION PPF_VERSION_NUM(PPF_VERSION_MAJOR, PPF_VERSION_MINOR, \
                                    PPF_VERSION_PATCHLEVEL)

#define PPF_ABI_EPOCH 1
#define PPF_ABI_REVISION 0
#define PPF_ABI_TAG "polyphase-float-abi-1"
#define PPF_ABI_INFO_VERSION 1
#define PPF_ABI_CONTRACT_SHA256 \
    "100c993384c2cc04193c4cd1d5b3c49c9f1823257d2e7b306b3115fae6929423"
#define PPF_SEMANTIC_CONTRACT_VERSION 1
#define PPF_SEMANTIC_CONTRACT_SHA256 \
    "c2934870bc76396e14c3b51643ec5c3b31ad1b21a93495f746ff4a20feffa444"
#define PPF_ALIASING_CONTRACT_VERSION 1
#define PPF_ALIASING_CONTRACT_SHA256 \
    "e55f826ecd61b67c91c2e9b7e1ca3efc35fb831ac923fc34aad65148d5ea41c5"


#if defined(__SIZEOF_FLOAT128__) && (__SIZEOF_FLOAT128__ == 16)
#  define PPF_HAVE_FLOAT128 1
__extension__ typedef __float128 ppf_float128_t;
#else
#  define PPF_HAVE_FLOAT128 0
#endif

#if defined(__DECIMAL_BID_FORMAT__) && defined(__DEC64_MANT_DIG__) && \
    defined(__DEC128_MANT_DIG__)
#  define PPF_HAVE_DECIMAL_FLOAT 1
__extension__ typedef _Decimal64 ppf_decimal64_t;
__extension__ typedef _Decimal128 ppf_decimal128_t;
#else
#  define PPF_HAVE_DECIMAL_FLOAT 0
#endif

typedef uint64_t ppf_prec_t;
typedef int64_t ppf_exp_t;
typedef uint32_t ppf_flags_t;

enum { PPF_PREC_MIN = 2 };
#define PPF_PREC_DEFAULT ((ppf_prec_t)53)
#define PPF_EMIN_DEFAULT ((ppf_exp_t)-1073741823)
#define PPF_EMAX_DEFAULT ((ppf_exp_t) 1073741823)
#define PPF_EMIN_MIN ((ppf_exp_t)(INT64_MIN / 4))
#define PPF_EMAX_MAX ((ppf_exp_t)(INT64_MAX / 4))

typedef enum {
    PPF_RND_DEFAULT = -1,
    PPF_RNDN = 0,
    PPF_RNDZ = 1,
    PPF_RNDU = 2,
    PPF_RNDD = 3,
    PPF_RNDA = 4,
    PPF_RNDF = 5
} ppf_rnd_t;

typedef enum {
    PPF_KIND_REGULAR = 0,
    PPF_KIND_ZERO = 1,
    PPF_KIND_INF = 2,
    PPF_KIND_NAN = 3
} ppf_kind_t;

enum {
    PPF_FLAG_UNDERFLOW = 1u << 0,
    PPF_FLAG_OVERFLOW  = 1u << 1,
    PPF_FLAG_DIVBY0    = 1u << 2,
    PPF_FLAG_NAN       = 1u << 3,
    PPF_FLAG_INEXACT   = 1u << 4,
    PPF_FLAG_ERANGE    = 1u << 5,
    /* A request to the configured Polyphase allocator failed. */
    PPF_FLAG_ALLOC     = 1u << 6,
    PPF_FLAGS_ALL      = (1u << 7) - 1u
};

typedef void *(*ppf_malloc_fn)(void *opaque, size_t size);
typedef void *(*ppf_realloc_fn)(void *opaque, void *ptr,
                                size_t old_size, size_t new_size);
typedef void (*ppf_free_fn)(void *opaque, void *ptr, size_t size);

typedef struct {
    ppf_malloc_fn malloc_fn;
    ppf_realloc_fn realloc_fn;
    ppf_free_fn free_fn;
    void *opaque;
} ppf_allocator_t;

typedef struct {
    void *block;
    size_t used;
} ppf_arena_mark_t;

typedef struct {
    ppf_prec_t default_precision;
    ppf_exp_t emin;
    ppf_exp_t emax;
    ppf_rnd_t default_rounding;
    ppf_flags_t flags;
    ppf_allocator_t allocator;
    void *arena_head;
    void *arena_current;
    size_t arena_block_bytes;
} ppf_context_t;

typedef struct {
    mp_limb_t *limbs;
    size_t allocated_limbs;
    size_t used_limbs;
    ppf_prec_t precision;
    ppf_exp_t exponent;
    int sign;
    ppf_kind_t kind;
    ppf_allocator_t allocator;
    uint32_t storage_flags;
} ppf_t;

enum {
    PPF_STORAGE_OWNED = 0u,
    PPF_STORAGE_EXTERNAL = 1u << 0
};

#define PPF_TERNARY_ERROR INT_MIN

typedef enum {
    PPF_FREE_LOCAL_CACHE = 1u << 0,
    PPF_FREE_GLOBAL_CACHE = 1u << 1
} ppf_free_cache_t;

enum {
    PPF_NAN_KIND = 0,
    PPF_INF_KIND = 1,
    PPF_ZERO_KIND = 2,
    PPF_REGULAR_KIND = 3
};

typedef enum {
    PPF_INTERVAL_ERROR = -1,
    PPF_INTERVAL_UNDECIDED = 0,
    PPF_INTERVAL_DECIDED = 1
} ppf_interval_status_t;

/* Immutable reusable-denominator object. */
typedef struct ppf_divisor_plan ppf_divisor_plan_t;

typedef enum {
    PPF_PRODUCT_AUTO = 0,
    PPF_PRODUCT_FULL = 1,
    PPF_PRODUCT_KARATSUBA3 = 2,
    PPF_PRODUCT_TOOM3 = 3,
    PPF_PRODUCT_WINDOW = 4
} ppf_product_algorithm_t;

typedef enum {
    PPF_PRODUCT_OP_MULTIPLY = 1,
    PPF_PRODUCT_OP_SQUARE = 2,
    PPF_PRODUCT_OP_FMA = 3,
    PPF_PRODUCT_OP_REC_SQRT = 4
} ppf_product_operation_t;

typedef struct {
    ppf_product_algorithm_t algorithm;
    unsigned long requested_workers;
    unsigned long effective_workers;
    int profile_rule_matched;
} ppf_product_plan_t;


typedef enum {
    PPF_CONSTANT_PI = 1,
    PPF_CONSTANT_LOG2 = 2,
    PPF_CONSTANT_EULER = 3,
    PPF_CONSTANT_CATALAN = 4
} ppf_constant_id_t;

typedef struct {
    ppf_constant_id_t constant_id;
    ppf_prec_t requested_precision;
    ppf_prec_t computed_precision;
    ppf_prec_t cached_precision;
    unsigned long workers;
    unsigned long attempts;
    unsigned long terms;
    unsigned long packets;
    unsigned long guard_bits;
    unsigned long interval_width_bits;
    ppf_exp_t interval_exponent;
    int cache_hit;
    int ternary;
    double compute_seconds;
    double rounding_seconds;
    double total_seconds;
} ppf_constant_stats_t;

enum {
    PPF_ABI_ENDIAN_UNKNOWN = 0,
    PPF_ABI_ENDIAN_LITTLE = 1,
    PPF_ABI_ENDIAN_BIG = 2
};

enum {
    PPF_ABI_FEATURE_FLOAT128 = 1u << 0,
    PPF_ABI_FEATURE_DECIMAL_FLOAT = 1u << 1,
    PPF_ABI_FEATURE_TLS = 1u << 2,
    PPF_ABI_FEATURE_THREAD_EXIT_CLEANUP = 1u << 3
};

/* Fixed-width runtime description of the public C ABI.  Existing fields and
   their order are frozen for ABI epoch 1.  Later revisions may use reserved
   fields but may not resize this structure within the epoch. */
typedef struct {
    uint32_t struct_size;
    uint32_t info_version;
    uint32_t abi_epoch;
    uint32_t abi_revision;
    uint32_t endian;
    uint32_t pointer_bits;
    uint32_t size_t_bits;
    uint32_t limb_bits;
    uint32_t feature_flags;
    uint32_t reserved32[7];

    uint64_t sizeof_ppf_allocator_t;
    uint64_t alignof_ppf_allocator_t;
    uint64_t sizeof_ppf_arena_mark_t;
    uint64_t alignof_ppf_arena_mark_t;
    uint64_t sizeof_ppf_context_t;
    uint64_t alignof_ppf_context_t;
    uint64_t sizeof_ppf_t;
    uint64_t alignof_ppf_t;
    uint64_t sizeof_ppf_product_plan_t;
    uint64_t alignof_ppf_product_plan_t;
    uint64_t sizeof_ppf_constant_stats_t;
    uint64_t alignof_ppf_constant_stats_t;
    uint64_t sizeof_ppf_abi_info_t;
    uint64_t alignof_ppf_abi_info_t;

    uint64_t offsetof_context_default_precision;
    uint64_t offsetof_context_emin;
    uint64_t offsetof_context_emax;
    uint64_t offsetof_context_default_rounding;
    uint64_t offsetof_context_flags;
    uint64_t offsetof_context_allocator;
    uint64_t offsetof_context_arena_head;
    uint64_t offsetof_context_arena_current;
    uint64_t offsetof_context_arena_block_bytes;

    uint64_t offsetof_number_limbs;
    uint64_t offsetof_number_allocated_limbs;
    uint64_t offsetof_number_used_limbs;
    uint64_t offsetof_number_precision;
    uint64_t offsetof_number_exponent;
    uint64_t offsetof_number_sign;
    uint64_t offsetof_number_kind;
    uint64_t offsetof_number_allocator;
    uint64_t offsetof_number_storage_flags;

    uint64_t reserved64[8];
} ppf_abi_info_t;

/* Context, flags, allocator, and scratch arena. */
PPF_API ppf_allocator_t ppf_default_allocator(void);
PPF_API int ppf_context_init(ppf_context_t *ctx);
PPF_API int ppf_context_init_custom(ppf_context_t *ctx,
                                    const ppf_allocator_t *allocator);
PPF_API int ppf_context_clone(ppf_context_t *dst,
                              const ppf_context_t *src);
PPF_API void ppf_context_clear(ppf_context_t *ctx);
PPF_API ppf_context_t *ppf_default_context(void);
PPF_API void ppf_default_context_cleanup(void);
/* Release all heap-backed state owned by the calling thread.
   POSIX thread exit performs the same cleanup automatically when the
   thread-state destructor could be registered successfully. */
PPF_API void ppf_thread_cleanup(void);
PPF_API int ppf_context_set_default_precision(ppf_context_t *ctx,
                                              ppf_prec_t precision);
PPF_API int ppf_context_set_range(ppf_context_t *ctx,
                                  ppf_exp_t emin, ppf_exp_t emax);
PPF_API int ppf_context_set_rounding(ppf_context_t *ctx, ppf_rnd_t rnd);
PPF_API ppf_flags_t ppf_context_flags(const ppf_context_t *ctx);
PPF_API void ppf_context_flags_clear(ppf_context_t *ctx, ppf_flags_t mask);
PPF_API void ppf_context_flags_set(ppf_context_t *ctx, ppf_flags_t mask);
PPF_API ppf_flags_t ppf_context_flags_save(const ppf_context_t *ctx);
PPF_API void ppf_context_flags_restore(ppf_context_t *ctx,
                                       ppf_flags_t flags,
                                       ppf_flags_t mask);
PPF_API void *ppf_arena_alloc(ppf_context_t *ctx, size_t bytes,
                              size_t alignment);
PPF_API ppf_arena_mark_t ppf_arena_mark(const ppf_context_t *ctx);
PPF_API void ppf_arena_release(ppf_context_t *ctx, ppf_arena_mark_t mark);
PPF_API void ppf_arena_reset(ppf_context_t *ctx);

/* Public exponent-range and exception controls.  The short forms use the
   calling thread's default context; explicit-context programs may continue
   to use ppf_context_set_range and ppf_context_flags_* directly. */
PPF_API ppf_exp_t ppf_get_emin(void);
PPF_API ppf_exp_t ppf_get_emax(void);
PPF_API int ppf_set_emin(ppf_exp_t exponent);
PPF_API int ppf_set_emax(ppf_exp_t exponent);
PPF_API ppf_exp_t ppf_get_emin_min(void);
PPF_API ppf_exp_t ppf_get_emin_max(void);
PPF_API ppf_exp_t ppf_get_emax_min(void);
PPF_API ppf_exp_t ppf_get_emax_max(void);
PPF_API int ppf_check_range(ppf_t *x, int ternary,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_subnormalize(ppf_t *x, int ternary,
                             ppf_rnd_t rnd, ppf_context_t *ctx);

PPF_API void ppf_clear_underflow(void);
PPF_API void ppf_clear_overflow(void);
PPF_API void ppf_clear_divby0(void);
PPF_API void ppf_clear_nanflag(void);
PPF_API void ppf_clear_inexflag(void);
PPF_API void ppf_clear_erangeflag(void);
PPF_API void ppf_clear_allocflag(void);
PPF_API void ppf_clear_flags(void);
PPF_API void ppf_set_underflow(void);
PPF_API void ppf_set_overflow(void);
PPF_API void ppf_set_divby0(void);
PPF_API void ppf_set_nanflag(void);
PPF_API void ppf_set_inexflag(void);
PPF_API void ppf_set_erangeflag(void);
PPF_API void ppf_set_allocflag(void);
PPF_API int ppf_underflow_p(void);
PPF_API int ppf_overflow_p(void);
PPF_API int ppf_divby0_p(void);
PPF_API int ppf_nanflag_p(void);
PPF_API int ppf_inexflag_p(void);
PPF_API int ppf_erangeflag_p(void);
PPF_API int ppf_allocflag_p(void);
PPF_API void ppf_flags_clear(ppf_flags_t mask);
PPF_API void ppf_flags_set(ppf_flags_t mask);
PPF_API ppf_flags_t ppf_flags_test(ppf_flags_t mask);
PPF_API ppf_flags_t ppf_flags_save(void);
PPF_API void ppf_flags_restore(ppf_flags_t flags, ppf_flags_t mask);

/* Initialization and precision. Variadic lists end with (ppf_t *)0. */
PPF_API int ppf_init(ppf_t *x);
PPF_API int ppf_init2(ppf_t *x, ppf_prec_t precision);
PPF_API int ppf_init2_ctx(ppf_t *x, ppf_prec_t precision,
                          ppf_context_t *ctx);
PPF_API int ppf_inits2(ppf_prec_t precision, ppf_t *first, ...);
PPF_API int ppf_inits(ppf_t *first, ...);
PPF_API void ppf_clear(ppf_t *x);
PPF_API void ppf_clears(ppf_t *first, ...);
PPF_API int ppf_reserve(ppf_t *x, ppf_prec_t precision);
PPF_API int ppf_set_prec(ppf_t *x, ppf_prec_t precision);
PPF_API int ppf_prec_round(ppf_t *x, ppf_prec_t precision,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API ppf_prec_t ppf_get_prec(const ppf_t *x);
PPF_API int ppf_set_default_prec(ppf_prec_t precision);
PPF_API ppf_prec_t ppf_get_default_prec(void);
PPF_API void ppf_swap(ppf_t *a, ppf_t *b);
PPF_API void ppf_set_prec_raw(ppf_t *x, ppf_prec_t precision);

/* Controlled direct significand access. */
PPF_API size_t ppf_limb_count(const ppf_t *x);
PPF_API const mp_limb_t *ppf_limbs_read(const ppf_t *x);
PPF_API mp_limb_t *ppf_limbs_write(ppf_t *x);
PPF_API int ppf_commit_regular(ppf_t *x, int sign, ppf_exp_t exponent,
                               ppf_context_t *ctx);

/* Special values and classification. */
PPF_API void ppf_set_zero(ppf_t *x, int sign);
PPF_API void ppf_set_inf(ppf_t *x, int sign);
PPF_API void ppf_set_nan(ppf_t *x, int sign);
PPF_API int ppf_nan_p(const ppf_t *x);
PPF_API int ppf_inf_p(const ppf_t *x);
PPF_API int ppf_zero_p(const ppf_t *x);
PPF_API int ppf_regular_p(const ppf_t *x);
PPF_API int ppf_number_p(const ppf_t *x);
PPF_API int ppf_signbit(const ppf_t *x);
PPF_API int ppf_sgn(const ppf_t *x, ppf_context_t *ctx);
PPF_API int ppf_setsign(ppf_t *rop, const ppf_t *op, int signbit,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_copysign(ppf_t *rop, const ppf_t *op1, const ppf_t *op2,
                         ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_default_rounding_mode(ppf_rnd_t rnd);
PPF_API ppf_rnd_t ppf_get_default_rounding_mode(void);
PPF_API int ppf_can_round(const ppf_t *approximation, ppf_exp_t error_bits,
                          ppf_rnd_t source_rounding,
                          ppf_rnd_t target_rounding,
                          ppf_prec_t target_precision);
PPF_API const char *ppf_print_rnd_mode(ppf_rnd_t rnd);
PPF_API int ppf_round_nearest_away_finish(ppf_t *rop,
                                          const ppf_t *extended_result,
                                          int extended_ternary,
                                          ppf_context_t *ctx);

#if defined(__GNUC__) || defined(__clang__)
#  define ppf_round_nearest_away_ctx(foo, rop, ctx, ...) \
    __extension__ ({ \
        ppf_t _ppf_rna_tmp; \
        ppf_t *_ppf_rna_rop = (rop); \
        ppf_context_t *_ppf_rna_ctx_arg = (ctx); \
        ppf_context_t *_ppf_rna_ctx = _ppf_rna_ctx_arg; \
        int _ppf_rna_inex = PPF_TERNARY_ERROR; \
        int _ppf_rna_result = PPF_TERNARY_ERROR; \
        if (_ppf_rna_ctx == NULL) _ppf_rna_ctx = ppf_default_context(); \
        if (_ppf_rna_ctx != NULL && _ppf_rna_rop != NULL && \
            ppf_init2_ctx(&_ppf_rna_tmp, _ppf_rna_rop->precision + 1u, \
                          _ppf_rna_ctx) == 0) { \
            _ppf_rna_inex = (foo)(&_ppf_rna_tmp, __VA_ARGS__, \
                                  PPF_RNDN, _ppf_rna_ctx); \
            _ppf_rna_result = ppf_round_nearest_away_finish( \
                _ppf_rna_rop, &_ppf_rna_tmp, _ppf_rna_inex, _ppf_rna_ctx); \
            ppf_clear(&_ppf_rna_tmp); \
        } \
        _ppf_rna_result; \
    })
#  define ppf_round_nearest_away(foo, rop, ...) \
    ppf_round_nearest_away_ctx((foo), (rop), NULL, __VA_ARGS__)
#endif

/* Assignment. */
PPF_API int ppf_set(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_ui(ppf_t *rop, unsigned long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_si(ppf_t *rop, long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_uj(ppf_t *rop, uintmax_t value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_sj(ppf_t *rop, intmax_t value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_flt(ppf_t *rop, float value,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_d(ppf_t *rop, double value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_ld(ppf_t *rop, long double value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
#if PPF_HAVE_FLOAT128
PPF_API int ppf_set_float128(ppf_t *rop, ppf_float128_t value,
                             ppf_rnd_t rnd, ppf_context_t *ctx);
#endif
#if PPF_HAVE_DECIMAL_FLOAT
PPF_API int ppf_set_decimal64(ppf_t *rop, ppf_decimal64_t value,
                              ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_decimal128(ppf_t *rop, ppf_decimal128_t value,
                               ppf_rnd_t rnd, ppf_context_t *ctx);
#endif
PPF_API int ppf_set_z(ppf_t *rop, mpz_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_q(ppf_t *rop, mpq_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_f(ppf_t *rop, mpf_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_ui_2exp(ppf_t *rop, unsigned long value,
                            ppf_exp_t exponent,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_si_2exp(ppf_t *rop, long value,
                            ppf_exp_t exponent,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_uj_2exp(ppf_t *rop, uintmax_t value,
                            ppf_exp_t exponent,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_sj_2exp(ppf_t *rop, intmax_t value,
                            ppf_exp_t exponent,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_z_2exp(ppf_t *rop, mpz_srcptr value,
                           ppf_exp_t exponent,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_set_str(ppf_t *rop, const char *text, int base,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_strtofr(ppf_t *rop, const char *text, char **endptr,
                        int base, ppf_rnd_t rnd, ppf_context_t *ctx);

/* Combined initialization and assignment. */
PPF_API int ppf_init_set(ppf_t *rop, const ppf_t *op,
                         ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_ui(ppf_t *rop, unsigned long value,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_si(ppf_t *rop, long value,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_uj(ppf_t *rop, uintmax_t value,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_sj(ppf_t *rop, intmax_t value,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_flt(ppf_t *rop, float value,
                             ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_d(ppf_t *rop, double value,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_ld(ppf_t *rop, long double value,
                            ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_z(ppf_t *rop, mpz_srcptr value,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_q(ppf_t *rop, mpq_srcptr value,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_f(ppf_t *rop, mpf_srcptr value,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_init_set_str(ppf_t *rop, const char *text, int base,
                             ppf_rnd_t rnd, ppf_context_t *ctx);

/* Conversion and fit tests. */
PPF_API float ppf_get_flt(const ppf_t *op, ppf_rnd_t rnd,
                          ppf_context_t *ctx);
PPF_API double ppf_get_d(const ppf_t *op, ppf_rnd_t rnd,
                         ppf_context_t *ctx);
PPF_API long double ppf_get_ld(const ppf_t *op, ppf_rnd_t rnd,
                               ppf_context_t *ctx);
#if PPF_HAVE_FLOAT128
PPF_API ppf_float128_t ppf_get_float128(const ppf_t *op, ppf_rnd_t rnd,
                                        ppf_context_t *ctx);
#endif
#if PPF_HAVE_DECIMAL_FLOAT
PPF_API ppf_decimal64_t ppf_get_decimal64(const ppf_t *op, ppf_rnd_t rnd,
                                          ppf_context_t *ctx);
PPF_API ppf_decimal128_t ppf_get_decimal128(const ppf_t *op, ppf_rnd_t rnd,
                                            ppf_context_t *ctx);
#endif
PPF_API long ppf_get_si(const ppf_t *op, ppf_rnd_t rnd,
                        ppf_context_t *ctx);
PPF_API unsigned long ppf_get_ui(const ppf_t *op, ppf_rnd_t rnd,
                                 ppf_context_t *ctx);
PPF_API intmax_t ppf_get_sj(const ppf_t *op, ppf_rnd_t rnd,
                            ppf_context_t *ctx);
PPF_API uintmax_t ppf_get_uj(const ppf_t *op, ppf_rnd_t rnd,
                             ppf_context_t *ctx);
PPF_API double ppf_get_d_2exp(long *exponent, const ppf_t *op,
                              ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API long double ppf_get_ld_2exp(long *exponent, const ppf_t *op,
                                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_frexp(ppf_exp_t *exponent, ppf_t *mantissa,
                      const ppf_t *op, ppf_rnd_t rnd,
                      ppf_context_t *ctx);
PPF_API ppf_exp_t ppf_get_z_2exp(mpz_ptr value, const ppf_t *op);
PPF_API int ppf_get_z(mpz_ptr value, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API void ppf_get_q(mpq_ptr value, const ppf_t *op,
                       ppf_context_t *ctx);
PPF_API int ppf_get_f(mpf_ptr value, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API size_t ppf_get_str_ndigits(int base, ppf_prec_t precision);
PPF_API char *ppf_get_str(char *buffer, ppf_exp_t *exponent,
                          int base, size_t digits, const ppf_t *op,
                          ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API void ppf_free_str(char *text);

/* Stream, portable interchange, and formatted output.  A NULL stream means
   stdin for input and stdout for output. */
PPF_API size_t ppf_out_str(FILE *stream, int base, size_t digits,
                           const ppf_t *op, ppf_rnd_t rnd,
                           ppf_context_t *ctx);
PPF_API size_t ppf_inp_str(ppf_t *rop, FILE *stream, int base,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_fpif_export(FILE *stream, const ppf_t *op);
PPF_API int ppf_fpif_import(ppf_t *op, FILE *stream, ppf_context_t *ctx);

PPF_API int ppf_fprintf(FILE *stream, const char *format, ...);
PPF_API int ppf_vfprintf(FILE *stream, const char *format, va_list ap);
PPF_API int ppf_printf(const char *format, ...);
PPF_API int ppf_vprintf(const char *format, va_list ap);
PPF_API int ppf_sprintf(char *buffer, const char *format, ...);
PPF_API int ppf_vsprintf(char *buffer, const char *format, va_list ap);
PPF_API int ppf_snprintf(char *buffer, size_t size,
                         const char *format, ...);
PPF_API int ppf_vsnprintf(char *buffer, size_t size,
                          const char *format, va_list ap);
PPF_API int ppf_asprintf(char **text, const char *format, ...);
PPF_API int ppf_vasprintf(char **text, const char *format, va_list ap);
PPF_API int ppf_fits_ulong_p(const ppf_t *op, ppf_rnd_t rnd);
PPF_API int ppf_fits_slong_p(const ppf_t *op, ppf_rnd_t rnd);
PPF_API int ppf_fits_uint_p(const ppf_t *op, ppf_rnd_t rnd);
PPF_API int ppf_fits_sint_p(const ppf_t *op, ppf_rnd_t rnd);
PPF_API int ppf_fits_ushort_p(const ppf_t *op, ppf_rnd_t rnd);
PPF_API int ppf_fits_sshort_p(const ppf_t *op, ppf_rnd_t rnd);
PPF_API int ppf_fits_uintmax_p(const ppf_t *op, ppf_rnd_t rnd);
PPF_API int ppf_fits_intmax_p(const ppf_t *op, ppf_rnd_t rnd);


/* Extended comparison, sign, and ordering operations. */
PPF_API int ppf_cmp_ui(const ppf_t *op, unsigned long value,
                       ppf_context_t *ctx);
PPF_API int ppf_cmp_si(const ppf_t *op, long value,
                       ppf_context_t *ctx);
PPF_API int ppf_cmp_d(const ppf_t *op, double value,
                      ppf_context_t *ctx);
PPF_API int ppf_cmp_ld(const ppf_t *op, long double value,
                       ppf_context_t *ctx);
PPF_API int ppf_cmp_z(const ppf_t *op, mpz_srcptr value,
                      ppf_context_t *ctx);
PPF_API int ppf_cmp_q(const ppf_t *op, mpq_srcptr value,
                      ppf_context_t *ctx);
PPF_API int ppf_cmp_f(const ppf_t *op, mpf_srcptr value,
                      ppf_context_t *ctx);
PPF_API int ppf_cmp_ui_2exp(const ppf_t *op, unsigned long value,
                            ppf_exp_t exponent, ppf_context_t *ctx);
PPF_API int ppf_cmp_si_2exp(const ppf_t *op, long value,
                            ppf_exp_t exponent, ppf_context_t *ctx);
PPF_API int ppf_cmpabs(const ppf_t *a, const ppf_t *b,
                       ppf_context_t *ctx);
PPF_API int ppf_cmpabs_ui(const ppf_t *op, unsigned long value,
                          ppf_context_t *ctx);
PPF_API int ppf_greater_p(const ppf_t *a, const ppf_t *b);
PPF_API int ppf_greaterequal_p(const ppf_t *a, const ppf_t *b);
PPF_API int ppf_less_p(const ppf_t *a, const ppf_t *b);
PPF_API int ppf_lessequal_p(const ppf_t *a, const ppf_t *b);
PPF_API int ppf_lessgreater_p(const ppf_t *a, const ppf_t *b);
PPF_API int ppf_unordered_p(const ppf_t *a, const ppf_t *b);
PPF_API int ppf_total_order_p(const ppf_t *a, const ppf_t *b);
PPF_API int ppf_neg(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_abs(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);

/* Integer rounding, fractional decomposition, and remainders. */
PPF_API int ppf_rint(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_ceil(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
PPF_API int ppf_floor(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
PPF_API int ppf_round(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
PPF_API int ppf_roundeven(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
PPF_API int ppf_trunc(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
PPF_API int ppf_rint_ceil(ppf_t *rop, const ppf_t *op,
                          ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_rint_floor(ppf_t *rop, const ppf_t *op,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_rint_round(ppf_t *rop, const ppf_t *op,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_rint_roundeven(ppf_t *rop, const ppf_t *op,
                               ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_rint_trunc(ppf_t *rop, const ppf_t *op,
                           ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_frac(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_modf(ppf_t *integer_part, ppf_t *fractional_part,
                     const ppf_t *op, ppf_rnd_t rnd,
                     ppf_context_t *ctx);
PPF_API int ppf_fmod(ppf_t *rop, const ppf_t *x, const ppf_t *y,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_fmod_ui(ppf_t *rop, const ppf_t *x, unsigned long y,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_fmodquo(ppf_t *rop, long *quotient_bits,
                        const ppf_t *x, const ppf_t *y,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_remainder(ppf_t *rop, const ppf_t *x, const ppf_t *y,
                          ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_remquo(ppf_t *rop, long *quotient_bits,
                       const ppf_t *x, const ppf_t *y,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_integer_p(const ppf_t *op);
PPF_API ppf_prec_t ppf_min_prec(const ppf_t *op);

/* Neighboring values, selection, and positive difference. */
PPF_API void ppf_nexttoward(ppf_t *x, const ppf_t *y,
                            ppf_context_t *ctx);
PPF_API void ppf_nextabove(ppf_t *x, ppf_context_t *ctx);
PPF_API void ppf_nextbelow(ppf_t *x, ppf_context_t *ctx);
PPF_API int ppf_min(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_max(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_dim(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API ppf_exp_t ppf_get_exp(const ppf_t *x, ppf_context_t *ctx);
PPF_API int ppf_set_exp(ppf_t *x, ppf_exp_t exponent,
                        ppf_context_t *ctx);

/* Random distributions, build information, memory/cache management,
   MPF compatibility, and external-significand storage. */
PPF_API int ppf_urandomb(ppf_t *rop, gmp_randstate_t state);
PPF_API int ppf_urandom(ppf_t *rop, gmp_randstate_t state,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_nrandom(ppf_t *rop, gmp_randstate_t state,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_grandom(ppf_t *rop1, ppf_t *rop2,
                        gmp_randstate_t state, ppf_rnd_t rnd,
                        ppf_context_t *ctx);
PPF_API int ppf_erandom(ppf_t *rop, gmp_randstate_t state,
                        ppf_rnd_t rnd, ppf_context_t *ctx);

PPF_API const char *ppf_get_version(void);
PPF_API uint32_t ppf_abi_epoch(void);
PPF_API uint32_t ppf_abi_revision(void);
PPF_API const char *ppf_abi_tag(void);
PPF_API const char *ppf_abi_contract_sha256(void);
PPF_API size_t ppf_abi_info_size(void);
PPF_API int ppf_get_abi_info(ppf_abi_info_t *info, size_t info_size);

PPF_API const char *ppf_get_patches(void);
PPF_API int ppf_buildopt_tls_p(void);
PPF_API int ppf_buildopt_thread_cleanup_p(void);
PPF_API int ppf_buildopt_float128_p(void);
PPF_API int ppf_buildopt_decimal_p(void);
PPF_API int ppf_buildopt_gmpinternals_p(void);
PPF_API int ppf_buildopt_sharedcache_p(void);
PPF_API const char *ppf_buildopt_tune_case(void);

PPF_API void ppf_free_cache(void);
PPF_API void ppf_free_cache2(ppf_free_cache_t way);
PPF_API void ppf_free_pool(void);
PPF_API int ppf_mp_memory_cleanup(void);

PPF_API int ppf_eq(const ppf_t *op1, const ppf_t *op2,
                   unsigned long bits);
PPF_API void ppf_reldiff(ppf_t *rop, const ppf_t *op1,
                         const ppf_t *op2, ppf_rnd_t rnd,
                         ppf_context_t *ctx);
PPF_API int ppf_mul_2exp(ppf_t *rop, const ppf_t *op,
                         unsigned long exponent, ppf_rnd_t rnd,
                         ppf_context_t *ctx);
PPF_API int ppf_div_2exp(ppf_t *rop, const ppf_t *op,
                         unsigned long exponent, ppf_rnd_t rnd,
                         ppf_context_t *ctx);

PPF_API size_t ppf_custom_get_size(ppf_prec_t precision);
PPF_API void ppf_custom_init(void *significand, ppf_prec_t precision);
PPF_API int ppf_custom_init_set(ppf_t *x, int kind, ppf_exp_t exponent,
                                ppf_prec_t precision, void *significand);
PPF_API int ppf_custom_get_kind(const ppf_t *x);
PPF_API void *ppf_custom_get_significand(const ppf_t *x);
PPF_API ppf_exp_t ppf_custom_get_exp(const ppf_t *x);
PPF_API void ppf_custom_move(ppf_t *x, void *new_position);

/* Independent arithmetic. */
PPF_API int ppf_add(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_add_ui(ppf_t *rop, const ppf_t *op, unsigned long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_add_si(ppf_t *rop, const ppf_t *op, long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_add_d(ppf_t *rop, const ppf_t *op, double value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_add_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_add_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sub(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_ui_sub(ppf_t *rop, unsigned long value, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sub_ui(ppf_t *rop, const ppf_t *op, unsigned long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_si_sub(ppf_t *rop, long value, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sub_si(ppf_t *rop, const ppf_t *op, long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_d_sub(ppf_t *rop, double value, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sub_d(ppf_t *rop, const ppf_t *op, double value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_z_sub(ppf_t *rop, mpz_srcptr value, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sub_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sub_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_mul(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_mul_ui(ppf_t *rop, const ppf_t *op, unsigned long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_mul_si(ppf_t *rop, const ppf_t *op, long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_mul_d(ppf_t *rop, const ppf_t *op, double value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_mul_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_mul_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_mul_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                            ppf_rnd_t rnd, unsigned long workers,
                            ppf_context_t *ctx);
PPF_API int ppf_sqr(ppf_t *rop, const ppf_t *a,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sqr_threads(ppf_t *rop, const ppf_t *a,
                            ppf_rnd_t rnd, unsigned long workers,
                            ppf_context_t *ctx);
PPF_API int ppf_mul_algorithm_threads(
    ppf_t *rop, const ppf_t *a, const ppf_t *b,
    ppf_rnd_t rnd, unsigned long workers,
    ppf_product_algorithm_t algorithm, ppf_context_t *ctx);
PPF_API int ppf_sqr_algorithm_threads(
    ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd,
    unsigned long workers, ppf_product_algorithm_t algorithm,
    ppf_context_t *ctx);
PPF_API ppf_product_algorithm_t ppf_plan_product_algorithm(
    const ppf_t *a, const ppf_t *b, int square,
    unsigned long workers);
PPF_API int ppf_plan_product(
    ppf_product_plan_t *plan, const ppf_t *a, const ppf_t *b,
    ppf_product_operation_t operation, unsigned long workers);
PPF_API const char *ppf_product_profile_name(void);
PPF_API const char *ppf_product_profile_sha256(void);
PPF_API int ppf_div(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_ui_div(ppf_t *rop, unsigned long value, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_div_ui(ppf_t *rop, const ppf_t *op, unsigned long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_si_div(ppf_t *rop, long value, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_div_si(ppf_t *rop, const ppf_t *op, long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_d_div(ppf_t *rop, double value, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_div_d(ppf_t *rop, const ppf_t *op, double value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_div_z(ppf_t *rop, const ppf_t *op, mpz_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_div_q(ppf_t *rop, const ppf_t *op, mpq_srcptr value,
                      ppf_rnd_t rnd, ppf_context_t *ctx);

/* Root family. */
PPF_API int ppf_sqrt(ppf_t *rop, const ppf_t *a,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sqrt_ui(ppf_t *rop, unsigned long value,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_rec_sqrt(ppf_t *rop, const ppf_t *a,
                         ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_rec_sqrt_threads(ppf_t *rop, const ppf_t *a,
                                 ppf_rnd_t rnd, unsigned long workers,
                                 ppf_context_t *ctx);
PPF_API int ppf_rec_sqrt_algorithm_threads(
    ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd,
    unsigned long workers, ppf_product_algorithm_t algorithm,
    ppf_context_t *ctx);
PPF_API int ppf_cbrt(ppf_t *rop, const ppf_t *a,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_rootn_ui(ppf_t *rop, const ppf_t *a, unsigned long degree,
                         ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_rootn_si(ppf_t *rop, const ppf_t *a, long degree,
                         ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_root(ppf_t *rop, const ppf_t *a, unsigned long degree,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_hypot(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                      ppf_rnd_t rnd, ppf_context_t *ctx);

/* Reusable-denominator division.  A plan owns an exact copy of the
   denominator and may also own a portable reciprocal. */
PPF_API int ppf_divisor_plan_create(
    ppf_divisor_plan_t **out, const ppf_t *denominator,
    ppf_prec_t destination_precision, ppf_context_t *ctx);
PPF_API int ppf_divisor_plan_create_ex(
    ppf_divisor_plan_t **out, const ppf_t *denominator,
    ppf_prec_t destination_precision, unsigned long expected_applications,
    ppf_context_t *ctx);
PPF_API void ppf_divisor_plan_destroy(ppf_divisor_plan_t *plan);
PPF_API ppf_prec_t ppf_divisor_plan_precision(
    const ppf_divisor_plan_t *plan);
PPF_API unsigned long ppf_divisor_plan_applications(
    const ppf_divisor_plan_t *plan);
PPF_API int ppf_div_with_plan(
    ppf_t *rop, const ppf_t *numerator, const ppf_divisor_plan_t *plan,
    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_div_with_plan_many(
    ppf_t *outputs[], const ppf_t *const numerators[], size_t length,
    const ppf_divisor_plan_t *plan, ppf_rnd_t rnd, unsigned long workers,
    int ternaries[], ppf_context_t *ctx);

PPF_API int ppf_fma(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    const ppf_t *c, ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_fma_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                            const ppf_t *c, ppf_rnd_t rnd,
                            unsigned long workers, ppf_context_t *ctx);
PPF_API int ppf_fma_algorithm_threads(
    ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c,
    ppf_rnd_t rnd, unsigned long workers,
    ppf_product_algorithm_t algorithm, ppf_context_t *ctx);
PPF_API int ppf_fms(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                    const ppf_t *c, ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_fmma(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                     const ppf_t *c, const ppf_t *d,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_fmms(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                     const ppf_t *c, const ppf_t *d,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_fms_threads(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                            const ppf_t *c, ppf_rnd_t rnd,
                            unsigned long workers, ppf_context_t *ctx);
PPF_API int ppf_fms_algorithm_threads(
    ppf_t *rop, const ppf_t *a, const ppf_t *b, const ppf_t *c,
    ppf_rnd_t rnd, unsigned long workers,
    ppf_product_algorithm_t algorithm, ppf_context_t *ctx);
PPF_API int ppf_mul_2ui(ppf_t *rop, const ppf_t *a, unsigned long shift,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_mul_2si(ppf_t *rop, const ppf_t *a, long shift,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_div_2ui(ppf_t *rop, const ppf_t *a, unsigned long shift,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_div_2si(ppf_t *rop, const ppf_t *a, long shift,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_fac_ui(ppf_t *rop, unsigned long value,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sum(ppf_t *rop, const ppf_t *const values[], size_t length,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_dot(ppf_t *rop, const ppf_t *const a[],
                    const ppf_t *const b[], size_t length,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_dot_threads(ppf_t *rop, const ppf_t *const a[],
                            const ppf_t *const b[], size_t length,
                            ppf_rnd_t rnd, unsigned long workers,
                            ppf_context_t *ctx);


/* Correctly rounded logarithms, exponentials, and powers. */
PPF_API int ppf_log(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_log_ui(ppf_t *rop, unsigned long op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_log2(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_log10(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_log1p(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_log2p1(ppf_t *rop, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_log10p1(ppf_t *rop, const ppf_t *op,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_exp(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_exp2(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_exp10(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_expm1(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_exp2m1(ppf_t *rop, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_exp10m1(ppf_t *rop, const ppf_t *op,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_pow(ppf_t *rop, const ppf_t *base, const ppf_t *exponent,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_powr(ppf_t *rop, const ppf_t *base, const ppf_t *exponent,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_pow_ui(ppf_t *rop, const ppf_t *base, unsigned long exponent,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_pow_si(ppf_t *rop, const ppf_t *base, long exponent,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_pow_uj(ppf_t *rop, const ppf_t *base, uintmax_t exponent,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_pow_sj(ppf_t *rop, const ppf_t *base, intmax_t exponent,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_pown(ppf_t *rop, const ppf_t *base, intmax_t exponent,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_pow_z(ppf_t *rop, const ppf_t *base, mpz_srcptr exponent,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_ui_pow_ui(ppf_t *rop, unsigned long base,
                          unsigned long exponent, ppf_rnd_t rnd,
                          ppf_context_t *ctx);
PPF_API int ppf_ui_pow(ppf_t *rop, unsigned long base,
                       const ppf_t *exponent, ppf_rnd_t rnd,
                       ppf_context_t *ctx);
PPF_API int ppf_compound_si(ppf_t *rop, const ppf_t *op, long exponent,
                            ppf_rnd_t rnd, ppf_context_t *ctx);


/* Correctly rounded trigonometric and hyperbolic functions. */
PPF_API int ppf_cos(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API void ppf_set_trig_threads(unsigned long workers);
PPF_API unsigned long ppf_get_trig_threads(void);
PPF_API int ppf_sin(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_tan(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_cosu(ppf_t *rop, const ppf_t *op, unsigned long u,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sinu(ppf_t *rop, const ppf_t *op, unsigned long u,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_tanu(ppf_t *rop, const ppf_t *op, unsigned long u,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_cospi(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sinpi(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_tanpi(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sin_cos(ppf_t *sop, ppf_t *cop, const ppf_t *op,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sec(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_csc(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_cot(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_acos(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_asin(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_atan(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_acosu(ppf_t *rop, const ppf_t *op, unsigned long u,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_asinu(ppf_t *rop, const ppf_t *op, unsigned long u,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_atanu(ppf_t *rop, const ppf_t *op, unsigned long u,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_acospi(ppf_t *rop, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_asinpi(ppf_t *rop, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_atanpi(ppf_t *rop, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_atan2(ppf_t *rop, const ppf_t *y, const ppf_t *x,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_atan2u(ppf_t *rop, const ppf_t *y, const ppf_t *x,
                       unsigned long u, ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_atan2pi(ppf_t *rop, const ppf_t *y, const ppf_t *x,
                        ppf_rnd_t rnd, ppf_context_t *ctx);

PPF_API int ppf_cosh(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sinh(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_tanh(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sinh_cosh(ppf_t *sop, ppf_t *cop, const ppf_t *op,
                          ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_sech(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_csch(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_coth(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_acosh(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_asinh(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_atanh(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);

/* Special functions. */
PPF_API int ppf_eint(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_li2(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_gamma(ppf_t *rop, const ppf_t *op,
                      ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_gamma_inc(ppf_t *rop, const ppf_t *op,
                          const ppf_t *op2, ppf_rnd_t rnd,
                          ppf_context_t *ctx);
PPF_API int ppf_lngamma(ppf_t *rop, const ppf_t *op,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_lgamma(ppf_t *rop, int *signp, const ppf_t *op,
                       ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_digamma(ppf_t *rop, const ppf_t *op,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_beta(ppf_t *rop, const ppf_t *op1, const ppf_t *op2,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_zeta(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_zeta_ui(ppf_t *rop, unsigned long op,
                        ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_erf(ppf_t *rop, const ppf_t *op,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_erfc(ppf_t *rop, const ppf_t *op,
                     ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_j0(ppf_t *rop, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_j1(ppf_t *rop, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_jn(ppf_t *rop, long n, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_y0(ppf_t *rop, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_y1(ppf_t *rop, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_yn(ppf_t *rop, long n, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_agm(ppf_t *rop, const ppf_t *op1, const ppf_t *op2,
                    ppf_rnd_t rnd, ppf_context_t *ctx);
PPF_API int ppf_ai(ppf_t *rop, const ppf_t *op,
                   ppf_rnd_t rnd, ppf_context_t *ctx);


/* Independently certified constants and reusable dyadic-interval caches. */
PPF_API int ppf_const_pi(ppf_t *rop, ppf_rnd_t rnd,
                          ppf_context_t *ctx);
PPF_API int ppf_const_pi_threads(ppf_t *rop, ppf_rnd_t rnd,
                                  unsigned long workers,
                                  ppf_context_t *ctx);
PPF_API int ppf_const_pi_uncached(ppf_t *rop, ppf_rnd_t rnd,
                                   unsigned long workers,
                                   ppf_context_t *ctx);
PPF_API int ppf_const_log2(ppf_t *rop, ppf_rnd_t rnd,
                            ppf_context_t *ctx);
PPF_API int ppf_const_log2_threads(ppf_t *rop, ppf_rnd_t rnd,
                                    unsigned long workers,
                                    ppf_context_t *ctx);
PPF_API int ppf_const_log2_uncached(ppf_t *rop, ppf_rnd_t rnd,
                                     unsigned long workers,
                                     ppf_context_t *ctx);
PPF_API int ppf_const_euler(ppf_t *rop, ppf_rnd_t rnd,
                            ppf_context_t *ctx);
PPF_API int ppf_const_euler_threads(ppf_t *rop, ppf_rnd_t rnd,
                                    unsigned long workers,
                                    ppf_context_t *ctx);
PPF_API int ppf_const_euler_uncached(ppf_t *rop, ppf_rnd_t rnd,
                                     unsigned long workers,
                                     ppf_context_t *ctx);
PPF_API int ppf_const_catalan(ppf_t *rop, ppf_rnd_t rnd,
                              ppf_context_t *ctx);
PPF_API int ppf_const_catalan_threads(ppf_t *rop, ppf_rnd_t rnd,
                                      unsigned long workers,
                                      ppf_context_t *ctx);
PPF_API int ppf_const_catalan_uncached(ppf_t *rop, ppf_rnd_t rnd,
                                       unsigned long workers,
                                       ppf_context_t *ctx);
PPF_API int ppf_precompute_pi(ppf_prec_t precision,
                              unsigned long workers);
PPF_API int ppf_precompute_log2(ppf_prec_t precision,
                                unsigned long workers);
PPF_API int ppf_precompute_euler(ppf_prec_t precision,
                                 unsigned long workers);
PPF_API int ppf_precompute_catalan(ppf_prec_t precision,
                                   unsigned long workers);
PPF_API int ppf_precompute_constant(ppf_constant_id_t id,
                                    ppf_prec_t precision,
                                    unsigned long workers);
PPF_API int ppf_get_constant_interval(ppf_constant_id_t id,
                                      mpz_ptr lower, mpz_ptr upper,
                                      ppf_exp_t *exponent,
                                      ppf_prec_t precision,
                                      unsigned long workers);
PPF_API void ppf_clear_constant_cache(ppf_constant_id_t id);
PPF_API void ppf_free_constant_caches(void);
PPF_API ppf_prec_t ppf_constant_cache_precision(ppf_constant_id_t id);
PPF_API void ppf_set_constant_threads(unsigned long workers);
PPF_API unsigned long ppf_get_constant_threads(void);
PPF_API int ppf_get_last_constant_stats(ppf_constant_stats_t *out);

/* Certified interval assignment. */
PPF_API ppf_interval_status_t ppf_set_dyadic_interval(
    ppf_t *rop, mpz_srcptr lower, mpz_srcptr upper,
    ppf_exp_t exponent, ppf_rnd_t rnd, int *ternary,
    ppf_context_t *ctx);

/* Comparison, validation, and diagnostics. */
PPF_API int ppf_cmp(const ppf_t *a, const ppf_t *b, ppf_context_t *ctx);
PPF_API int ppf_equal_p(const ppf_t *a, const ppf_t *b);
PPF_API int ppf_validate(const ppf_t *x);
PPF_API void ppf_dump(const ppf_t *x);
PPF_API void ppf_dump_file(FILE *stream, const ppf_t *x);
PPF_API const char *ppf_rounding_name(ppf_rnd_t rnd);
PPF_API const char *ppf_kind_name(ppf_kind_t kind);

#ifdef __cplusplus
}
#endif
#endif
