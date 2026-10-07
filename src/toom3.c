/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "ppf_openmp_compat.h"

#include "ppf_internal.h"

/* Direct-limb signed-magnitude arithmetic used only by the rank-5 Toom-3
   product graph.  All buffers are thread-local and reused between calls. */
typedef struct {
    mp_limb_t *value;
    size_t capacity;
    mp_size_t used;
    int sign;
    _Atomic int *failed;
} ppf_slimb_t;

enum {
    PPF_TOOM_ZERO = 0,
    PPF_TOOM_ONE = 1,
    PPF_TOOM_MINUS_ONE = 2,
    PPF_TOOM_TWO = 3,
    PPF_TOOM_INFINITY = 4,
    PPF_TOOM_POINTS = 5
};

typedef struct {
    _Atomic int failed;
    ppf_slimb_t ea[PPF_TOOM_POINTS];
    ppf_slimb_t eb[PPF_TOOM_POINTS];
    ppf_slimb_t product[PPF_TOOM_POINTS];
    ppf_slimb_t coefficient[PPF_TOOM_POINTS];
    ppf_slimb_t scratch[10];
} ppf_toom3_limb_ctx_t;

static _Thread_local ppf_toom3_limb_ctx_t g_toom3_limb_tls[2];

#ifndef PPF_TOOM3_PARALLEL_EVAL_MIN_LIMBS
#define PPF_TOOM3_PARALLEL_EVAL_MIN_LIMBS 40000UL
#endif
#ifndef PPF_TOOM3_SHORT_INTERP_MAX_LIMBS
#define PPF_TOOM3_SHORT_INTERP_MAX_LIMBS 100000UL
#endif

static void toom3_bind_failure(ppf_toom3_limb_ctx_t *ctx) {
    size_t i;
    atomic_store(&ctx->failed, 0);
    for (i = 0; i < PPF_TOOM_POINTS; ++i) {
        ctx->ea[i].failed = &ctx->failed;
        ctx->eb[i].failed = &ctx->failed;
        ctx->product[i].failed = &ctx->failed;
        ctx->coefficient[i].failed = &ctx->failed;
    }
    for (i = 0; i < sizeof(ctx->scratch) / sizeof(ctx->scratch[0]); ++i)
        ctx->scratch[i].failed = &ctx->failed;
}

static mp_size_t slim_trim(mp_srcptr value, mp_size_t used) {
    while (used > 0 && value[used - 1] == 0) --used;
    return used;
}

static int slim_reserve(ppf_slimb_t *value, size_t needed) {
    size_t next;
    mp_limb_t *replacement;
    if (needed <= value->capacity) return 0;
    next = value->capacity ? value->capacity : 8U;
    while (next < needed) {
        if (next > SIZE_MAX / 2U) { next = needed; break; }
        next *= 2U;
    }
    if (next > SIZE_MAX / sizeof(*replacement)) {
        if (value->failed) atomic_store(value->failed, 1);
        return -1;
    }
    replacement = realloc(value->value, next * sizeof(*replacement));
    if (!replacement) {
        if (value->failed) atomic_store(value->failed, 1);
        return -1;
    }
    value->value = replacement;
    value->capacity = next;
    return 0;
}

static void slim_clear(ppf_slimb_t *value) {
    free(value->value);
    memset(value, 0, sizeof(*value));
}

static void slim_zero(ppf_slimb_t *value) {
    value->used = 0;
    value->sign = 0;
}

static ppf_slimb_t slim_view(mp_srcptr value, mp_size_t used, int sign) {
    ppf_slimb_t out;
    out.value = (mp_ptr)value;
    out.capacity = 0;
    out.used = slim_trim(value, used);
    out.sign = out.used ? (sign < 0 ? -1 : 1) : 0;
    out.failed = NULL;
    return out;
}

static void slim_copy(ppf_slimb_t *out, const ppf_slimb_t *in) {
    if (!in->used || !in->sign) {
        slim_zero(out);
        return;
    }
    if (slim_reserve(out, (size_t)in->used) != 0) { slim_zero(out); return; }
    memcpy(out->value, in->value, (size_t)in->used * sizeof(*out->value));
    out->used = in->used;
    out->sign = in->sign;
}

static int slim_cmp_abs(const ppf_slimb_t *a, const ppf_slimb_t *b) {
    if (a->used != b->used) return a->used > b->used ? 1 : -1;
    if (!a->used) return 0;
    return mpn_cmp(a->value, b->value, a->used);
}

static void slim_add(ppf_slimb_t *out,
                     const ppf_slimb_t *a,
                     const ppf_slimb_t *b) {
    if (!a->sign || !a->used) { slim_copy(out, b); return; }
    if (!b->sign || !b->used) { slim_copy(out, a); return; }
    if (a->sign == b->sign) {
        const ppf_slimb_t *large = a->used >= b->used ? a : b;
        const ppf_slimb_t *small = a->used >= b->used ? b : a;
        if (slim_reserve(out, (size_t)large->used + 1U) != 0) { slim_zero(out); return; }
        mp_limb_t carry = mpn_add(out->value,
                                  large->value, large->used,
                                  small->value, small->used);
        out->used = large->used;
        if (carry) out->value[out->used++] = carry;
        out->sign = a->sign;
        return;
    }
    int cmp = slim_cmp_abs(a, b);
    if (cmp == 0) { slim_zero(out); return; }
    const ppf_slimb_t *large = cmp > 0 ? a : b;
    const ppf_slimb_t *small = cmp > 0 ? b : a;
    if (slim_reserve(out, (size_t)large->used) != 0) { slim_zero(out); return; }
    mp_limb_t borrow = mpn_sub(out->value,
                               large->value, large->used,
                               small->value, small->used);
    if (borrow) {
        fprintf(stderr, "direct-limb Toom-3 signed subtraction underflow\n");
        abort();
    }
    out->used = slim_trim(out->value, large->used);
    out->sign = out->used ? large->sign : 0;
}

static void slim_sub(ppf_slimb_t *out,
                     const ppf_slimb_t *a,
                     const ppf_slimb_t *b) {
    ppf_slimb_t negative = *b;
    negative.sign = -negative.sign;
    slim_add(out, a, &negative);
}

static void slim_mul_small(ppf_slimb_t *out,
                           const ppf_slimb_t *in,
                           mp_limb_t multiplier) {
    if (!in->sign || !in->used || multiplier == 0) {
        slim_zero(out);
        return;
    }
    if (multiplier == 1) { slim_copy(out, in); return; }
    if (slim_reserve(out, (size_t)in->used + 1U) != 0) { slim_zero(out); return; }
    mp_limb_t carry = mpn_mul_1(out->value, in->value, in->used, multiplier);
    out->used = in->used;
    if (carry) out->value[out->used++] = carry;
    out->sign = in->sign;
}

static void slim_divexact_small(ppf_slimb_t *out,
                                const ppf_slimb_t *in,
                                mp_limb_t divisor) {
    if (!in->sign || !in->used) { slim_zero(out); return; }
    if (divisor < 2) { slim_copy(out, in); return; }
#if defined(PPF_TOOM3_VERIFY_EXACT_DIVISION)
    if (mpn_mod_1(in->value, in->used, divisor) != 0) {
        fprintf(stderr, "direct-limb Toom-3 non-exact interpolation division\n");
        abort();
    }
#endif
    if (slim_reserve(out, (size_t)in->used) != 0) { slim_zero(out); return; }
    if (divisor == 2) {
        (void)mpn_rshift(out->value, in->value, in->used, 1);
    } else {
        mpn_divexact_1(out->value, in->value, in->used, divisor);
    }
    out->used = slim_trim(out->value, in->used);
    out->sign = out->used ? in->sign : 0;
}

static void slim_mul(ppf_slimb_t *out,
                     const ppf_slimb_t *a,
                     const ppf_slimb_t *b,
                     int square) {
    if (!a->sign || !a->used || !b->sign || !b->used) {
        slim_zero(out);
        return;
    }
    if (slim_reserve(out, (size_t)a->used + (size_t)b->used) != 0) { slim_zero(out); return; }
    if (square && a == b) {
        mpn_sqr(out->value, a->value, a->used);
    } else if (a->used >= b->used) {
        mpn_mul(out->value, a->value, a->used, b->value, b->used);
    } else {
        mpn_mul(out->value, b->value, b->used, a->value, a->used);
    }
    out->used = slim_trim(out->value, a->used + b->used);
    out->sign = out->used ? a->sign * b->sign : 0;
}

static int slim_add_at(mp_ptr destination, mp_size_t destination_size,
                       const ppf_slimb_t *source, mp_size_t offset) {
    if (!source->used || !source->sign) return 0;
    if (source->sign < 0 || offset < 0 || offset > destination_size ||
        source->used > destination_size - offset) return -1;
    mp_limb_t carry = mpn_add_n(destination + offset,
                                destination + offset,
                                source->value, source->used);
    mp_size_t position = offset + source->used;
    while (carry && position < destination_size) {
        destination[position] += 1;
        carry = destination[position] == 0;
        ++position;
    }
    return carry ? -1 : 0;
}

static void toom3_limb_ctx_clear(ppf_toom3_limb_ctx_t *ctx) {
    for (int i = 0; i < PPF_TOOM_POINTS; ++i) {
        slim_clear(&ctx->ea[i]);
        slim_clear(&ctx->eb[i]);
        slim_clear(&ctx->product[i]);
        slim_clear(&ctx->coefficient[i]);
    }
    for (size_t i = 0; i < sizeof(ctx->scratch)/sizeof(ctx->scratch[0]); ++i)
        slim_clear(&ctx->scratch[i]);
    memset(ctx, 0, sizeof(*ctx));
}

void ppf_toom3_limb_cleanup_thread(void) {
    toom3_limb_ctx_clear(&g_toom3_limb_tls[0]);
    toom3_limb_ctx_clear(&g_toom3_limb_tls[1]);
}

static void toom3_limb_eval_point(ppf_slimb_t *out,
                                  mp_srcptr input, mp_size_t n,
                                  mp_size_t k, int point,
                                  ppf_slimb_t *scratch0,
                                  ppf_slimb_t *scratch1,
                                  ppf_slimb_t *scratch2) {
    mp_size_t top = n - 2 * k;
    ppf_slimb_t x0 = slim_view(input, k, 1);
    ppf_slimb_t x1 = slim_view(input + k, k, 1);
    ppf_slimb_t x2 = slim_view(input + 2 * k, top, 1);
    switch (point) {
        case PPF_TOOM_ZERO:
            slim_copy(out, &x0);
            break;
        case PPF_TOOM_INFINITY:
            slim_copy(out, &x2);
            break;
        case PPF_TOOM_ONE:
            slim_add(scratch0, &x0, &x1);
            slim_add(out, scratch0, &x2);
            break;
        case PPF_TOOM_MINUS_ONE:
            slim_sub(scratch0, &x0, &x1);
            slim_add(out, scratch0, &x2);
            break;
        case PPF_TOOM_TWO:
            slim_mul_small(scratch0, &x1, 2);
            slim_mul_small(scratch1, &x2, 4);
            slim_add(scratch2, &x0, scratch0);
            slim_add(out, scratch2, scratch1);
            break;
        default:
            slim_zero(out);
            break;
    }
}

static mp_size_t toom3_limb_prepare(mp_size_t n) {
    return (n + 2) / 3;
}

static void toom3_limb_prepare_serial(ppf_toom3_limb_ctx_t *ctx,
                                      mp_srcptr a, mp_srcptr b,
                                      mp_size_t n, mp_size_t k,
                                      int square) {
    for (int point = 0; point < PPF_TOOM_POINTS; ++point) {
        toom3_limb_eval_point(&ctx->ea[point], a, n, k, point,
                              &ctx->scratch[0], &ctx->scratch[1],
                              &ctx->scratch[2]);
        if (!square) {
            toom3_limb_eval_point(&ctx->eb[point], b, n, k, point,
                                  &ctx->scratch[3], &ctx->scratch[4],
                                  &ctx->scratch[5]);
        }
    }
}

static void toom3_limb_multiply_prepared_point(
    ppf_toom3_limb_ctx_t *ctx,
    mp_srcptr a, mp_srcptr b,
    mp_size_t n, mp_size_t k,
    int square, int point
) {
    (void)a; (void)b; (void)n; (void)k;
    slim_mul(&ctx->product[point], &ctx->ea[point],
             square ? &ctx->ea[point] : &ctx->eb[point], square);
}

static void toom3_limb_compute_point(ppf_toom3_limb_ctx_t *ctx,
                                     mp_srcptr a, mp_srcptr b,
                                     mp_size_t n, mp_size_t k,
                                     int square, int point) {
    /* The three nontrivial evaluation points own disjoint reusable scratch
       slots, so point evaluation can occur inside the product tasks without
       allocation or synchronization. */
    int ia = 0, ib = 3;
    if (point == PPF_TOOM_ONE) { ia = 6; ib = 7; }
    else if (point == PPF_TOOM_MINUS_ONE) { ia = 8; ib = 9; }
    toom3_limb_eval_point(&ctx->ea[point], a, n, k, point,
                          &ctx->scratch[ia], &ctx->scratch[ia + (point == PPF_TOOM_TWO)],
                          &ctx->scratch[ia + 2 * (point == PPF_TOOM_TWO)]);
    if (!square) {
        toom3_limb_eval_point(&ctx->eb[point], b, n, k, point,
                              &ctx->scratch[ib], &ctx->scratch[ib + (point == PPF_TOOM_TWO)],
                              &ctx->scratch[ib + 2 * (point == PPF_TOOM_TWO)]);
    }
    slim_mul(&ctx->product[point], &ctx->ea[point],
             square ? &ctx->ea[point] : &ctx->eb[point], square);
}

static int toom3_limb_reconstruct(mp_ptr out, mp_size_t n, mp_size_t k,
                                  ppf_slimb_t coefficient[PPF_TOOM_POINTS]) {
    memset(out, 0, (size_t)(2 * n) * sizeof(*out));
    for (mp_size_t i = 0; i < PPF_TOOM_POINTS; ++i) {
        if (coefficient[i].sign < 0 ||
            slim_add_at(out, 2 * n, &coefficient[i], i * k) != 0) {
            return -1;
        }
    }
    return 0;
}

/* Twelve-pass interpolation.  With
       v(t)=c0+c1*t+c2*t^2+c3*t^3+c4*t^4,
   define
       a=(v2-vm1)/3,
       b=(v1-vm1)/2,
       d=v1-v0.
   Then
       c3=(a-d)/2-2*c4,
       c2=d-b-c4,
       c1=b-c3.
   All divisions are exact. */
static int toom3_limb_finish_short(mp_ptr out, mp_size_t n, mp_size_t k,
                                   ppf_toom3_limb_ctx_t *ctx) {
    ppf_slimb_t *v = ctx->product;
    ppf_slimb_t *c = ctx->coefficient;
    ppf_slimb_t *t = ctx->scratch;
    slim_copy(&c[0], &v[PPF_TOOM_ZERO]);
    slim_copy(&c[4], &v[PPF_TOOM_INFINITY]);

    slim_sub(&t[0], &v[PPF_TOOM_TWO], &v[PPF_TOOM_MINUS_ONE]);
    slim_divexact_small(&t[1], &t[0], 3);                 /* a */
    slim_sub(&t[2], &v[PPF_TOOM_ONE], &v[PPF_TOOM_MINUS_ONE]);
    slim_divexact_small(&t[3], &t[2], 2);                 /* b */
    slim_sub(&t[4], &v[PPF_TOOM_ONE], &v[PPF_TOOM_ZERO]);/* d */

    slim_sub(&t[5], &t[1], &t[4]);
    slim_divexact_small(&t[6], &t[5], 2);
    slim_mul_small(&t[7], &c[4], 2);
    slim_sub(&c[3], &t[6], &t[7]);

    slim_sub(&t[8], &t[4], &t[3]);
    slim_sub(&c[2], &t[8], &c[4]);
    slim_sub(&c[1], &t[3], &c[3]);
    return toom3_limb_reconstruct(out, n, k, c);
}

/* Older interpolation retained for its better large-cache behavior on some
   GMP/CPU combinations. */
static int toom3_limb_finish_long(mp_ptr out, mp_size_t n, mp_size_t k,
                                  ppf_toom3_limb_ctx_t *ctx) {
    ppf_slimb_t *v = ctx->product;
    ppf_slimb_t *c = ctx->coefficient;
    ppf_slimb_t *t = ctx->scratch;
    slim_copy(&c[0], &v[PPF_TOOM_ZERO]);
    slim_copy(&c[4], &v[PPF_TOOM_INFINITY]);

    slim_add(&t[0], &v[PPF_TOOM_ONE], &v[PPF_TOOM_MINUS_ONE]);
    slim_divexact_small(&t[1], &t[0], 2);
    slim_sub(&t[2], &t[1], &c[0]);
    slim_sub(&c[2], &t[2], &c[4]);

    slim_sub(&t[0], &v[PPF_TOOM_ONE], &v[PPF_TOOM_MINUS_ONE]);
    slim_divexact_small(&t[3], &t[0], 2);

    slim_mul_small(&t[0], &c[2], 4);
    slim_sub(&t[1], &v[PPF_TOOM_TWO], &c[0]);
    slim_sub(&t[2], &t[1], &t[0]);
    slim_mul_small(&t[0], &c[4], 16);
    slim_sub(&t[1], &t[2], &t[0]);
    slim_divexact_small(&t[4], &t[1], 2);

    slim_sub(&t[0], &t[4], &t[3]);
    slim_divexact_small(&c[3], &t[0], 3);
    slim_sub(&c[1], &t[3], &c[3]);
    return toom3_limb_reconstruct(out, n, k, c);
}

static int toom3_limb_finish(mp_ptr out, mp_size_t n, mp_size_t k,
                             ppf_toom3_limb_ctx_t *ctx,
                             unsigned long short_interp_max_limbs) {
    if ((unsigned long)n <= short_interp_max_limbs)
        return toom3_limb_finish_short(out, n, k, ctx);
    return toom3_limb_finish_long(out, n, k, ctx);
}

unsigned long ppf_toom3_limb_product_internal(
    mp_ptr out, mp_srcptr a, mp_srcptr b, mp_size_t n,
    unsigned long workers, int square
) {
    ppf_toom3_limb_ctx_t *ctx;
    ppf_thread_state_touch();
    ctx = &g_toom3_limb_tls[0];
    mp_size_t k = toom3_limb_prepare(n);
    unsigned long team = workers < 5UL ? workers : 5UL;
    if (team < 1UL) team = 1UL;
    int parallel_evaluation;
    toom3_bind_failure(ctx);
    parallel_evaluation = !square ||
        (unsigned long)n >= PPF_TOOM3_PARALLEL_EVAL_MIN_LIMBS;
    if (!parallel_evaluation) {
        toom3_limb_prepare_serial(ctx, a, b, n, k, square);
PPF_OMP_PRAGMA(omp parallel sections num_threads(team))
        {
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_multiply_prepared_point(ctx, a, b, n, k, square, PPF_TOOM_TWO); }
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_multiply_prepared_point(ctx, a, b, n, k, square, PPF_TOOM_ONE); }
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_multiply_prepared_point(ctx, a, b, n, k, square, PPF_TOOM_MINUS_ONE); }
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_multiply_prepared_point(ctx, a, b, n, k, square, PPF_TOOM_ZERO); }
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_multiply_prepared_point(ctx, a, b, n, k, square, PPF_TOOM_INFINITY); }
        }
    } else {
PPF_OMP_PRAGMA(omp parallel sections num_threads(team))
        {
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_compute_point(ctx, a, b, n, k, square, PPF_TOOM_TWO); }
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_compute_point(ctx, a, b, n, k, square, PPF_TOOM_ONE); }
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_compute_point(ctx, a, b, n, k, square, PPF_TOOM_MINUS_ONE); }
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_compute_point(ctx, a, b, n, k, square, PPF_TOOM_ZERO); }
PPF_OMP_PRAGMA(omp section)
            { toom3_limb_compute_point(ctx, a, b, n, k, square, PPF_TOOM_INFINITY); }
        }
    }
    if (atomic_load(&ctx->failed) ||
        toom3_limb_finish(out, n, k, ctx,
                          PPF_TOOM3_SHORT_INTERP_MAX_LIMBS) != 0) {
        if (square) mpn_sqr(out, a, n);
        else mpn_mul(out, a, n, b, n);
        return 0UL;
    }
    return 5UL;
}

unsigned long ppf_toom3_limb_two_product_internal(
    mp_ptr first, mp_srcptr a, mp_srcptr b, mp_size_t first_size,
    mp_ptr second, mp_srcptr c, mp_srcptr d, mp_size_t second_size,
    unsigned long workers
) {
    ppf_toom3_limb_ctx_t *x;
    ppf_toom3_limb_ctx_t *y;
    ppf_thread_state_touch();
    x = &g_toom3_limb_tls[0];
    y = &g_toom3_limb_tls[1];
    mp_size_t kx = toom3_limb_prepare(first_size);
    mp_size_t ky = toom3_limb_prepare(second_size);
    unsigned long team = workers < 10UL ? workers : 10UL;
    if (team < 1UL) team = 1UL;
    toom3_bind_failure(x);
    toom3_bind_failure(y);
PPF_OMP_PRAGMA(omp parallel num_threads(team))
    {
PPF_OMP_PRAGMA(omp single nowait)
        {
PPF_OMP_PRAGMA(omp task shared(x))
            { toom3_limb_compute_point(x, a, b, first_size, kx, 0, PPF_TOOM_TWO); }
PPF_OMP_PRAGMA(omp task shared(y))
            { toom3_limb_compute_point(y, c, d, second_size, ky, 0, PPF_TOOM_TWO); }
PPF_OMP_PRAGMA(omp task shared(x))
            { toom3_limb_compute_point(x, a, b, first_size, kx, 0, PPF_TOOM_ONE); }
PPF_OMP_PRAGMA(omp task shared(y))
            { toom3_limb_compute_point(y, c, d, second_size, ky, 0, PPF_TOOM_ONE); }
PPF_OMP_PRAGMA(omp task shared(x))
            { toom3_limb_compute_point(x, a, b, first_size, kx, 0, PPF_TOOM_MINUS_ONE); }
PPF_OMP_PRAGMA(omp task shared(y))
            { toom3_limb_compute_point(y, c, d, second_size, ky, 0, PPF_TOOM_MINUS_ONE); }
PPF_OMP_PRAGMA(omp task shared(x))
            { toom3_limb_compute_point(x, a, b, first_size, kx, 0, PPF_TOOM_ZERO); }
PPF_OMP_PRAGMA(omp task shared(y))
            { toom3_limb_compute_point(y, c, d, second_size, ky, 0, PPF_TOOM_ZERO); }
PPF_OMP_PRAGMA(omp task shared(x))
            { toom3_limb_compute_point(x, a, b, first_size, kx, 0, PPF_TOOM_INFINITY); }
PPF_OMP_PRAGMA(omp task shared(y))
            { toom3_limb_compute_point(y, c, d, second_size, ky, 0, PPF_TOOM_INFINITY); }
PPF_OMP_PRAGMA(omp taskwait)
        }
    }
    int fx, fy;
    fx = atomic_load(&x->failed) ? -1 :
        toom3_limb_finish(first, first_size, kx, x,
                          PPF_TOOM3_SHORT_INTERP_MAX_LIMBS);
    fy = atomic_load(&y->failed) ? -1 :
        toom3_limb_finish(second, second_size, ky, y,
                          PPF_TOOM3_SHORT_INTERP_MAX_LIMBS);
    if (fx != 0) mpn_mul(first, a, first_size, b, first_size);
    if (fy != 0) mpn_mul(second, c, second_size, d, second_size);
    return fx == 0 && fy == 0 ? 10UL : 0UL;
}
