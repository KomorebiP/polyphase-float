/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <stdlib.h>

int ppf_rounding_valid(ppf_rnd_t rnd) {
    return rnd >= PPF_RNDN && rnd <= PPF_RNDF;
}

int ppf_precision_valid(ppf_prec_t precision) {
    if (precision < PPF_PREC_MIN) return 0;
    if (precision > (ppf_prec_t)(SIZE_MAX / 2)) return 0;
    return ppf_limbs_for_precision(precision) != 0;
}

size_t ppf_limbs_for_precision(ppf_prec_t precision) {
    ppf_prec_t add = (ppf_prec_t)PPF_LIMB_BITS - 1;
    ppf_prec_t n;
    if (precision < PPF_PREC_MIN || precision > UINT64_MAX - add) return 0;
    n = (precision + add) / PPF_LIMB_BITS;
    if (n == 0 || n > SIZE_MAX / sizeof(mp_limb_t)) return 0;
    return (size_t)n;
}

int ppf_exp_add(ppf_exp_t a, ppf_exp_t b, ppf_exp_t *out) {
    if (!out) return -1;
    if ((b > 0 && a > INT64_MAX - b) ||
        (b < 0 && a < INT64_MIN - b)) return -1;
    *out = a + b;
    return 0;
}

int ppf_exp_sub_prec(ppf_exp_t a, ppf_prec_t b, ppf_exp_t *out) {
    if (b > (ppf_prec_t)INT64_MAX) return -1;
    return ppf_exp_add(a, -(ppf_exp_t)b, out);
}

static int allocator_valid(const ppf_allocator_t *a) {
    return a && a->malloc_fn && a->realloc_fn && a->free_fn;
}

int ppf_init2_ctx(ppf_t *x, ppf_prec_t precision,
                  ppf_context_t *ctx) {
    size_t limbs, bytes;
    if (!x || !ctx || !allocator_valid(&ctx->allocator) ||
        !ppf_precision_valid(precision)) return -1;
    memset(x, 0, sizeof(*x));
    x->allocator = ctx->allocator;
    x->precision = precision;
    x->sign = 1;
    x->kind = PPF_KIND_NAN;
    x->storage_flags = PPF_STORAGE_OWNED;
    limbs = ppf_limbs_for_precision(precision);
    bytes = limbs * sizeof(mp_limb_t);
    x->limbs = (mp_limb_t *)ppf_context_malloc_internal(ctx, bytes);
    if (!x->limbs) {
        memset(x, 0, sizeof(*x));
        return -1;
    }
    memset(x->limbs, 0, bytes);
    x->allocated_limbs = limbs;
    return 0;
}

int ppf_init2(ppf_t *x, ppf_prec_t precision) {
    ppf_context_t *ctx = ppf_default_context();
    return ctx ? ppf_init2_ctx(x, precision, ctx) : -1;
}

int ppf_init(ppf_t *x) {
    ppf_context_t *ctx = ppf_default_context();
    return ctx ? ppf_init2_ctx(x, ctx->default_precision, ctx) : -1;
}

void ppf_clear(ppf_t *x) {
    if (!x) return;
    if (x->limbs && !(x->storage_flags & PPF_STORAGE_EXTERNAL) &&
        allocator_valid(&x->allocator)) {
        x->allocator.free_fn(x->allocator.opaque, x->limbs,
                             x->allocated_limbs * sizeof(mp_limb_t));
    }
    memset(x, 0, sizeof(*x));
}

int ppf_reserve(ppf_t *x, ppf_prec_t precision) {
    size_t need, old_bytes, new_bytes;
    mp_limb_t *next;
    if (!x || !allocator_valid(&x->allocator) ||
        !ppf_precision_valid(precision)) return -1;
    need = ppf_limbs_for_precision(precision);
    if (need <= x->allocated_limbs) return 0;
    if (x->storage_flags & PPF_STORAGE_EXTERNAL) return -1;
    old_bytes = x->allocated_limbs * sizeof(mp_limb_t);
    new_bytes = need * sizeof(mp_limb_t);
    next = (mp_limb_t *)x->allocator.realloc_fn(
        x->allocator.opaque, x->limbs, old_bytes, new_bytes);
    if (!next) {
        ppf_note_allocation_failure_internal();
        return -1;
    }
    memset(next + x->allocated_limbs, 0,
           (need - x->allocated_limbs) * sizeof(mp_limb_t));
    x->limbs = next;
    x->allocated_limbs = need;
    return 0;
}

int ppf_set_prec(ppf_t *x, ppf_prec_t precision) {
    if (!x || (x->storage_flags & PPF_STORAGE_EXTERNAL) ||
        ppf_reserve(x, precision) != 0) return -1;
    x->precision = precision;
    x->used_limbs = 0;
    x->exponent = 0;
    x->sign = 1;
    x->kind = PPF_KIND_NAN;
    memset(x->limbs, 0, x->allocated_limbs * sizeof(mp_limb_t));
    return 0;
}

ppf_prec_t ppf_get_prec(const ppf_t *x) {
    return x ? x->precision : 0;
}

void ppf_swap(ppf_t *a, ppf_t *b) {
    ppf_t t;
    if (!a || !b || a == b) return;
    t = *a;
    *a = *b;
    *b = t;
}

size_t ppf_limb_count(const ppf_t *x) {
    return x ? ppf_limbs_for_precision(x->precision) : 0;
}

const mp_limb_t *ppf_limbs_read(const ppf_t *x) {
    return x && ppf_regular_p(x) ? x->limbs : NULL;
}

mp_limb_t *ppf_limbs_write(ppf_t *x) {
    size_t n;
    if (!x || !x->limbs) return NULL;
    n = ppf_limbs_for_precision(x->precision);
    if (!n || x->allocated_limbs < n) return NULL;
    memset(x->limbs, 0, n * sizeof(mp_limb_t));
    x->kind = PPF_KIND_NAN;
    x->sign = 1;
    x->exponent = 0;
    x->used_limbs = 0;
    return x->limbs;
}

int ppf_commit_regular(ppf_t *x, int sign, ppf_exp_t exponent,
                       ppf_context_t *ctx) {
    size_t n, top_limb;
    unsigned top_bit;
    mp_limb_t high_mask;
    ctx = ppf_resolve_context(ctx);
    if (!x || !ctx || !x->limbs || exponent < ctx->emin || exponent > ctx->emax) {
        if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
        return -1;
    }
    n = ppf_limbs_for_precision(x->precision);
    if (!n || x->allocated_limbs < n) return -1;
    top_limb = (size_t)((x->precision - 1) / PPF_LIMB_BITS);
    top_bit = (unsigned)((x->precision - 1) % PPF_LIMB_BITS);
    if (((x->limbs[top_limb] >> top_bit) & 1u) == 0) return -1;
    if (top_bit + 1 < PPF_LIMB_BITS) {
        high_mask = ~((((mp_limb_t)1 << (top_bit + 1)) - 1));
        if ((x->limbs[top_limb] & high_mask) != 0) return -1;
    }
    x->used_limbs = n;
    x->kind = PPF_KIND_REGULAR;
    x->sign = sign < 0 ? -1 : 1;
    x->exponent = exponent;
    return ppf_validate(x) ? 0 : -1;
}

void ppf_set_zero(ppf_t *x, int sign) {
    if (!x) return;
    x->kind = PPF_KIND_ZERO;
    x->sign = sign < 0 ? -1 : 1;
    x->exponent = 0;
    x->used_limbs = 0;
}

void ppf_set_inf(ppf_t *x, int sign) {
    if (!x) return;
    x->kind = PPF_KIND_INF;
    x->sign = sign < 0 ? -1 : 1;
    x->exponent = 0;
    x->used_limbs = 0;
}

void ppf_set_nan(ppf_t *x, int sign) {
    if (!x) return;
    x->kind = PPF_KIND_NAN;
    x->sign = sign < 0 ? -1 : 1;
    x->exponent = 0;
    x->used_limbs = 0;
}

int ppf_nan_p(const ppf_t *x) { return x && x->kind == PPF_KIND_NAN; }
int ppf_inf_p(const ppf_t *x) { return x && x->kind == PPF_KIND_INF; }
int ppf_zero_p(const ppf_t *x) { return x && x->kind == PPF_KIND_ZERO; }
int ppf_regular_p(const ppf_t *x) { return x && x->kind == PPF_KIND_REGULAR; }
int ppf_number_p(const ppf_t *x) {
    return x && (x->kind == PPF_KIND_REGULAR || x->kind == PPF_KIND_ZERO);
}
int ppf_signbit(const ppf_t *x) { return x && x->sign < 0; }

int ppf_sgn(const ppf_t *x, ppf_context_t *ctx) {
    if (!x) return 0;
    if (ppf_nan_p(x)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return 0;
    }
    if (ppf_zero_p(x)) return 0;
    return x->sign < 0 ? -1 : 1;
}

void ppf_zero_unused_bits(ppf_t *x) {
    unsigned rem;
    mp_limb_t mask;
    if (!x || !x->limbs || x->used_limbs == 0) return;
    rem = (unsigned)(x->precision % PPF_LIMB_BITS);
    if (rem == 0) return;
    mask = (((mp_limb_t)1 << (rem - 1)) - 1) * 2 + 1;
    x->limbs[x->used_limbs - 1] &= mask;
}

int ppf_store_normalized_mpz(ppf_t *x, mpz_srcptr magnitude,
                             ppf_exp_t exponent, int sign) {
    size_t count = 0, need;
    if (!x || !magnitude || mpz_sgn(magnitude) <= 0) return -1;
    if ((ppf_prec_t)mpz_sizeinbase(magnitude, 2) != x->precision) return -1;
    need = ppf_limbs_for_precision(x->precision);
    if (need == 0 || ppf_reserve(x, x->precision) != 0) return -1;
    memset(x->limbs, 0, need * sizeof(mp_limb_t));
    mpz_export(x->limbs, &count, -1, sizeof(mp_limb_t), 0, 0, magnitude);
    if (count > need) return -1;
    x->used_limbs = need;
    x->exponent = exponent;
    x->sign = sign < 0 ? -1 : 1;
    x->kind = PPF_KIND_REGULAR;
    ppf_zero_unused_bits(x);
    return 0;
}

int ppf_set_max_finite(ppf_t *x, int sign, const ppf_context_t *ctx) {
    mpz_t z;
    int rc;
    if (!x || !ctx) return -1;
    mpz_init(z);
    mpz_set_ui(z, 1);
    mpz_mul_2exp(z, z, x->precision);
    mpz_sub_ui(z, z, 1);
    rc = ppf_store_normalized_mpz(x, z, ctx->emax, sign);
    mpz_clear(z);
    return rc;
}

int ppf_set_min_normal(ppf_t *x, int sign, const ppf_context_t *ctx) {
    mpz_t z;
    int rc;
    if (!x || !ctx) return -1;
    mpz_init_set_ui(z, 1);
    mpz_mul_2exp(z, z, x->precision - 1);
    rc = ppf_store_normalized_mpz(x, z, ctx->emin, sign);
    mpz_clear(z);
    return rc;
}

ppf_exp_t ppf_get_z_2exp(mpz_ptr value, const ppf_t *op) {
    ppf_exp_t out = 0;
    if (!value || !op || !ppf_regular_p(op) ||
        ppf_exp_sub_prec(op->exponent, op->precision, &out) != 0) {
        if (value) mpz_set_ui(value, 0);
        return 0;
    }
    mpz_import(value, op->used_limbs, -1, sizeof(mp_limb_t), 0, 0,
               op->limbs);
    if (op->sign < 0) mpz_neg(value, value);
    return out;
}

int ppf_validate(const ppf_t *x) {
    size_t need;
    ppf_prec_t top_bit;
    size_t limb_index;
    unsigned bit_index;
    if (!x || !ppf_precision_valid(x->precision) ||
        !allocator_valid(&x->allocator) || !x->limbs ||
        x->allocated_limbs < ppf_limbs_for_precision(x->precision) ||
        (x->sign != 1 && x->sign != -1) ||
        x->kind < PPF_KIND_REGULAR || x->kind > PPF_KIND_NAN)
        return 0;
    if (!ppf_regular_p(x)) return x->used_limbs == 0;
    need = ppf_limbs_for_precision(x->precision);
    if (x->used_limbs != need) return 0;
    top_bit = x->precision - 1;
    limb_index = (size_t)(top_bit / PPF_LIMB_BITS);
    bit_index = (unsigned)(top_bit % PPF_LIMB_BITS);
    if (((x->limbs[limb_index] >> bit_index) & 1u) == 0) return 0;
    if (bit_index + 1 < PPF_LIMB_BITS &&
        (x->limbs[limb_index] >> (bit_index + 1)) != 0) return 0;
    return 1;
}

const char *ppf_rounding_name(ppf_rnd_t rnd) {
    switch (rnd) {
        case PPF_RND_DEFAULT: return "PPF_RND_DEFAULT";
        case PPF_RNDN: return "PPF_RNDN";
        case PPF_RNDZ: return "PPF_RNDZ";
        case PPF_RNDU: return "PPF_RNDU";
        case PPF_RNDD: return "PPF_RNDD";
        case PPF_RNDA: return "PPF_RNDA";
        case PPF_RNDF: return "PPF_RNDF";
        default: return "PPF_RND_INVALID";
    }
}

const char *ppf_kind_name(ppf_kind_t kind) {
    switch (kind) {
        case PPF_KIND_REGULAR: return "regular";
        case PPF_KIND_ZERO: return "zero";
        case PPF_KIND_INF: return "infinity";
        case PPF_KIND_NAN: return "nan";
        default: return "invalid";
    }
}

void ppf_dump_file(FILE *stream, const ppf_t *x) {
    mpz_t z;
    ppf_exp_t e;
    if (!stream) stream = stderr;
    if (!x) {
        fprintf(stream, "ppf(null)\n");
        return;
    }
    fprintf(stream, "ppf(kind=%s sign=%c precision=%llu exponent=%lld",
            ppf_kind_name(x->kind), x->sign < 0 ? '-' : '+',
            (unsigned long long)x->precision, (long long)x->exponent);
    if (ppf_regular_p(x)) {
        mpz_init(z);
        e = ppf_get_z_2exp(z, x);
        fprintf(stream, " value=");
        mpz_out_str(stream, 16, z);
        fprintf(stream, "*2^%lld", (long long)e);
        mpz_clear(z);
    }
    fprintf(stream, ")\n");
}

void ppf_dump(const ppf_t *x) {
    ppf_dump_file(stdout, x);
}
