/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(COND) do { \
    if (!(COND)) { \
        fprintf(stderr, "alias matrix failure at %s:%d: %s\n", \
                __FILE__, __LINE__, #COND); \
        exit(1); \
    } \
} while (0)

static const ppf_rnd_t all_rounds[] = {
    PPF_RNDN, PPF_RNDZ, PPF_RNDU, PPF_RNDD, PPF_RNDA, PPF_RNDF
};

static int same_value(const ppf_t *a, const ppf_t *b) {
    if (ppf_get_prec(a) != ppf_get_prec(b)) return 0;
    if (ppf_nan_p(a) || ppf_nan_p(b))
        return ppf_nan_p(a) && ppf_nan_p(b) && ppf_signbit(a) == ppf_signbit(b);
    if (ppf_inf_p(a) || ppf_inf_p(b))
        return ppf_inf_p(a) && ppf_inf_p(b) && ppf_signbit(a) == ppf_signbit(b);
    if (ppf_zero_p(a) || ppf_zero_p(b))
        return ppf_zero_p(a) && ppf_zero_p(b) && ppf_signbit(a) == ppf_signbit(b);
    return ppf_cmp(a, b, NULL) == 0;
}

static int same_representation(const ppf_t *a, const ppf_t *b) {
    size_t bytes;
    if (a->kind != b->kind || a->sign != b->sign ||
        a->exponent != b->exponent || a->precision != b->precision ||
        a->allocated_limbs != b->allocated_limbs ||
        a->used_limbs != b->used_limbs ||
        a->storage_flags != b->storage_flags)
        return 0;
    bytes = a->allocated_limbs * sizeof(*a->limbs);
    return bytes == 0 || memcmp(a->limbs, b->limbs, bytes) == 0;
}

static void init_context(ppf_context_t *ctx) {
    CHECK(ppf_context_init(ctx) == 0);
    CHECK(ppf_context_set_range(ctx, PPF_EMIN_MIN, PPF_EMAX_MAX) == 0);
}

static void init_value(ppf_t *x, ppf_prec_t precision, const char *text,
                       ppf_context_t *ctx) {
    CHECK(ppf_init2_ctx(x, precision, ctx) == 0);
    CHECK(ppf_set_str(x, text, 10, PPF_RNDN, ctx) == 0);
}

static void copy_value(ppf_t *dst, const ppf_t *src, ppf_context_t *ctx) {
    CHECK(ppf_init2_ctx(dst, src->precision, ctx) == 0);
    CHECK(ppf_set(dst, src, PPF_RNDN, ctx) != PPF_TERNARY_ERROR);
}

static void compare_run(const char *name, const char *pattern, ppf_rnd_t rnd,
                        int expected_ternary, int alias_ternary,
                        ppf_flags_t expected_flags, ppf_flags_t alias_flags,
                        const ppf_t *expected, const ppf_t *alias) {
    if (expected_ternary != alias_ternary || expected_flags != alias_flags ||
        !same_value(expected, alias)) {
        fprintf(stderr,
                "%s alias pattern %s failed at rounding %d: "
                "ternary %d/%d flags 0x%x/0x%x\n",
                name, pattern, (int)rnd, expected_ternary, alias_ternary,
                (unsigned)expected_flags, (unsigned)alias_flags);
        exit(1);
    }
}

typedef int (*unary_fn)(ppf_t *, const ppf_t *, ppf_rnd_t, ppf_context_t *);
typedef int (*binary_fn)(ppf_t *, const ppf_t *, const ppf_t *,
                         ppf_rnd_t, ppf_context_t *);
typedef int (*ternary_fn)(ppf_t *, const ppf_t *, const ppf_t *, const ppf_t *,
                          ppf_rnd_t, ppf_context_t *);
typedef int (*quaternary_fn)(ppf_t *, const ppf_t *, const ppf_t *,
                             const ppf_t *, const ppf_t *, ppf_rnd_t,
                             ppf_context_t *);

static void check_unary(const char *name, unary_fn fn, const char *value) {
    ppf_context_t ctx;
    ppf_t source, expected, alias;
    init_context(&ctx);
    init_value(&source, 113, value, &ctx);
    for (size_t i = 0; i < sizeof(all_rounds) / sizeof(all_rounds[0]); ++i) {
        ppf_rnd_t rnd = all_rounds[i];
        ppf_flags_t ef, af;
        int et, at;
        CHECK(ppf_init2_ctx(&expected, source.precision, &ctx) == 0);
        copy_value(&alias, &source, &ctx);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        et = fn(&expected, &source, rnd, &ctx);
        ef = ppf_context_flags(&ctx);
        if (et == PPF_TERNARY_ERROR) { fprintf(stderr, "%s out-of-place failed rnd=%d\n", name, (int)rnd); exit(1); }
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        at = fn(&alias, &alias, rnd, &ctx);
        af = ppf_context_flags(&ctx);
        if (at == PPF_TERNARY_ERROR) { fprintf(stderr, "%s alias failed rnd=%d\n", name, (int)rnd); exit(1); }
        compare_run(name, "dst=src", rnd, et, at, ef, af,
                    &expected, &alias);
        ppf_clear(&expected);
        ppf_clear(&alias);
    }
    ppf_clear(&source);
    ppf_context_clear(&ctx);
}

static void check_binary(const char *name, binary_fn fn,
                         const char *av, const char *bv) {
    ppf_context_t ctx;
    ppf_t a, b;
    init_context(&ctx);
    init_value(&a, 113, av, &ctx);
    init_value(&b, 79, bv, &ctx);
    for (size_t r = 0; r < sizeof(all_rounds) / sizeof(all_rounds[0]); ++r) {
        ppf_rnd_t rnd = all_rounds[r];
        for (unsigned which = 0; which < 2; ++which) {
            ppf_t aa, bb, expected;
            ppf_flags_t ef, af;
            int et, at;
            copy_value(&aa, &a, &ctx);
            copy_value(&bb, &b, &ctx);
            CHECK(ppf_init2_ctx(&expected, which == 0 ? a.precision : b.precision,
                                &ctx) == 0);
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            et = fn(&expected, &a, &b, rnd, &ctx);
            ef = ppf_context_flags(&ctx);
            CHECK(et != PPF_TERNARY_ERROR);
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            at = which == 0 ? fn(&aa, &aa, &bb, rnd, &ctx)
                            : fn(&bb, &aa, &bb, rnd, &ctx);
            af = ppf_context_flags(&ctx);
            CHECK(at != PPF_TERNARY_ERROR);
            compare_run(name, which == 0 ? "dst=src1" : "dst=src2", rnd,
                        et, at, ef, af, &expected, which == 0 ? &aa : &bb);
            ppf_clear(&aa);
            ppf_clear(&bb);
            ppf_clear(&expected);
        }
    }
    for (size_t r = 0; r < sizeof(all_rounds) / sizeof(all_rounds[0]); ++r) {
        ppf_t source, expected; ppf_flags_t ef, af; int et, at;
        copy_value(&source, &a, &ctx);
        CHECK(ppf_init2_ctx(&expected, source.precision, &ctx) == 0);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        et = fn(&expected, &a, &a, all_rounds[r], &ctx); ef = ppf_context_flags(&ctx);
        CHECK(et != PPF_TERNARY_ERROR);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        at = fn(&source, &source, &source, all_rounds[r], &ctx); af = ppf_context_flags(&ctx);
        CHECK(at != PPF_TERNARY_ERROR);
        compare_run(name, "dst=src1=src2", all_rounds[r], et, at, ef, af, &expected, &source);
        ppf_clear(&source); ppf_clear(&expected);
    }
    ppf_clear(&a);
    ppf_clear(&b);
    ppf_context_clear(&ctx);
}

static void check_ternary(const char *name, ternary_fn fn,
                          const char *av, const char *bv, const char *cv) {
    ppf_context_t ctx;
    ppf_t a, b, c;
    const ppf_prec_t precisions[3] = {113, 97, 83};
    init_context(&ctx);
    init_value(&a, precisions[0], av, &ctx);
    init_value(&b, precisions[1], bv, &ctx);
    init_value(&c, precisions[2], cv, &ctx);
    for (size_t r = 0; r < sizeof(all_rounds) / sizeof(all_rounds[0]); ++r) {
        ppf_rnd_t rnd = all_rounds[r];
        for (unsigned which = 0; which < 3; ++which) {
            ppf_t aa, bb, cc, expected;
            ppf_t *alias;
            ppf_flags_t ef, af;
            int et, at;
            copy_value(&aa, &a, &ctx);
            copy_value(&bb, &b, &ctx);
            copy_value(&cc, &c, &ctx);
            alias = which == 0 ? &aa : (which == 1 ? &bb : &cc);
            CHECK(ppf_init2_ctx(&expected, precisions[which], &ctx) == 0);
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            et = fn(&expected, &a, &b, &c, rnd, &ctx);
            ef = ppf_context_flags(&ctx);
            CHECK(et != PPF_TERNARY_ERROR);
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            at = fn(alias, &aa, &bb, &cc, rnd, &ctx);
            af = ppf_context_flags(&ctx);
            CHECK(at != PPF_TERNARY_ERROR);
            compare_run(name, which == 0 ? "dst=src1" :
                              (which == 1 ? "dst=src2" : "dst=src3"),
                        rnd, et, at, ef, af, &expected, alias);
            ppf_clear(&aa); ppf_clear(&bb); ppf_clear(&cc);
            ppf_clear(&expected);
        }
    }
    for (size_t r = 0; r < sizeof(all_rounds) / sizeof(all_rounds[0]); ++r) {
        ppf_t source, expected; ppf_flags_t ef, af; int et, at;
        copy_value(&source, &a, &ctx); CHECK(ppf_init2_ctx(&expected, source.precision, &ctx) == 0);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        et = fn(&expected, &a, &a, &a, all_rounds[r], &ctx); ef = ppf_context_flags(&ctx);
        CHECK(et != PPF_TERNARY_ERROR);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        at = fn(&source, &source, &source, &source, all_rounds[r], &ctx); af = ppf_context_flags(&ctx);
        CHECK(at != PPF_TERNARY_ERROR);
        compare_run(name, "dst=src1=src2=src3", all_rounds[r], et, at, ef, af, &expected, &source);
        ppf_clear(&source); ppf_clear(&expected);
    }
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&c);
    ppf_context_clear(&ctx);
}

static void check_quaternary(const char *name, quaternary_fn fn,
                             const char *av, const char *bv,
                             const char *cv, const char *dv) {
    ppf_context_t ctx;
    ppf_t a, b, c, d;
    const ppf_prec_t precisions[4] = {113, 101, 89, 73};
    init_context(&ctx);
    init_value(&a, precisions[0], av, &ctx);
    init_value(&b, precisions[1], bv, &ctx);
    init_value(&c, precisions[2], cv, &ctx);
    init_value(&d, precisions[3], dv, &ctx);
    for (size_t r = 0; r < sizeof(all_rounds) / sizeof(all_rounds[0]); ++r) {
        ppf_rnd_t rnd = all_rounds[r];
        for (unsigned which = 0; which < 4; ++which) {
            ppf_t aa, bb, cc, dd, expected;
            ppf_t *alias;
            ppf_flags_t ef, af;
            int et, at;
            copy_value(&aa, &a, &ctx); copy_value(&bb, &b, &ctx);
            copy_value(&cc, &c, &ctx); copy_value(&dd, &d, &ctx);
            alias = which == 0 ? &aa : which == 1 ? &bb :
                    which == 2 ? &cc : &dd;
            CHECK(ppf_init2_ctx(&expected, precisions[which], &ctx) == 0);
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            et = fn(&expected, &a, &b, &c, &d, rnd, &ctx);
            ef = ppf_context_flags(&ctx);
            CHECK(et != PPF_TERNARY_ERROR);
            ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            at = fn(alias, &aa, &bb, &cc, &dd, rnd, &ctx);
            af = ppf_context_flags(&ctx);
            CHECK(at != PPF_TERNARY_ERROR);
            compare_run(name, which == 0 ? "dst=src1" : which == 1 ?
                        "dst=src2" : which == 2 ? "dst=src3" : "dst=src4",
                        rnd, et, at, ef, af, &expected, alias);
            ppf_clear(&aa); ppf_clear(&bb); ppf_clear(&cc); ppf_clear(&dd);
            ppf_clear(&expected);
        }
    }
    for (size_t r = 0; r < sizeof(all_rounds) / sizeof(all_rounds[0]); ++r) {
        ppf_t source, expected; ppf_flags_t ef, af; int et, at;
        copy_value(&source, &a, &ctx); CHECK(ppf_init2_ctx(&expected, source.precision, &ctx) == 0);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        et = fn(&expected, &a, &a, &a, &a, all_rounds[r], &ctx); ef = ppf_context_flags(&ctx);
        CHECK(et != PPF_TERNARY_ERROR);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        at = fn(&source, &source, &source, &source, &source, all_rounds[r], &ctx); af = ppf_context_flags(&ctx);
        CHECK(at != PPF_TERNARY_ERROR);
        compare_run(name, "dst=src1=src2=src3=src4", all_rounds[r], et, at, ef, af, &expected, &source);
        ppf_clear(&source); ppf_clear(&expected);
    }
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&c); ppf_clear(&d);
    ppf_context_clear(&ctx);
}

/* Fixed-rounding and scalar adapters. */
#define WRAP_FIXED(NAME) \
    static int wrap_##NAME(ppf_t *d, const ppf_t *s, ppf_rnd_t r, \
                           ppf_context_t *c) { (void)r; return NAME(d, s, c); }
WRAP_FIXED(ppf_ceil)
WRAP_FIXED(ppf_floor)
WRAP_FIXED(ppf_round)
WRAP_FIXED(ppf_roundeven)
WRAP_FIXED(ppf_trunc)

static int wrap_setsign(ppf_t *d, const ppf_t *s, ppf_rnd_t r,
                        ppf_context_t *c) { return ppf_setsign(d, s, 1, r, c); }
static int wrap_fmod_ui(ppf_t *d, const ppf_t *s, ppf_rnd_t r,
                        ppf_context_t *c) { return ppf_fmod_ui(d, s, 3UL, r, c); }
static int wrap_mul_2exp(ppf_t *d, const ppf_t *s, ppf_rnd_t r,
                         ppf_context_t *c) { return ppf_mul_2exp(d, s, 7UL, r, c); }
static int wrap_div_2exp(ppf_t *d, const ppf_t *s, ppf_rnd_t r,
                         ppf_context_t *c) { return ppf_div_2exp(d, s, 7UL, r, c); }

#define WRAP_SCALAR_UI(NAME, VALUE) \
    static int wrap_##NAME(ppf_t *d, const ppf_t *s, ppf_rnd_t r, \
                           ppf_context_t *c) { return NAME(d, s, VALUE, r, c); }
#define WRAP_SCALAR_SI(NAME, VALUE) \
    static int wrap_##NAME(ppf_t *d, const ppf_t *s, ppf_rnd_t r, \
                           ppf_context_t *c) { return NAME(d, s, VALUE, r, c); }
#define WRAP_LEFT_UI(NAME, VALUE) \
    static int wrap_##NAME(ppf_t *d, const ppf_t *s, ppf_rnd_t r, \
                           ppf_context_t *c) { return NAME(d, VALUE, s, r, c); }
#define WRAP_LEFT_SI(NAME, VALUE) \
    static int wrap_##NAME(ppf_t *d, const ppf_t *s, ppf_rnd_t r, \
                           ppf_context_t *c) { return NAME(d, VALUE, s, r, c); }
#define WRAP_SCALAR_D(NAME, VALUE) \
    static int wrap_##NAME(ppf_t *d, const ppf_t *s, ppf_rnd_t r, \
                           ppf_context_t *c) { return NAME(d, s, VALUE, r, c); }
#define WRAP_LEFT_D(NAME, VALUE) \
    static int wrap_##NAME(ppf_t *d, const ppf_t *s, ppf_rnd_t r, \
                           ppf_context_t *c) { return NAME(d, VALUE, s, r, c); }

WRAP_SCALAR_UI(ppf_add_ui, 7UL)
WRAP_SCALAR_SI(ppf_add_si, -3L)
WRAP_SCALAR_D(ppf_add_d, 0.75)
WRAP_LEFT_UI(ppf_ui_sub, 9UL)
WRAP_SCALAR_UI(ppf_sub_ui, 7UL)
WRAP_LEFT_SI(ppf_si_sub, 9L)
WRAP_SCALAR_SI(ppf_sub_si, -3L)
WRAP_LEFT_D(ppf_d_sub, 9.25)
WRAP_SCALAR_D(ppf_sub_d, 0.75)
WRAP_SCALAR_UI(ppf_mul_ui, 7UL)
WRAP_SCALAR_SI(ppf_mul_si, -3L)
WRAP_SCALAR_D(ppf_mul_d, 0.75)
WRAP_LEFT_UI(ppf_ui_div, 9UL)
WRAP_SCALAR_UI(ppf_div_ui, 7UL)
WRAP_LEFT_SI(ppf_si_div, 9L)
WRAP_SCALAR_SI(ppf_div_si, -3L)
WRAP_LEFT_D(ppf_d_div, 9.25)
WRAP_SCALAR_D(ppf_div_d, 0.75)
WRAP_SCALAR_UI(ppf_mul_2ui, 7UL)
WRAP_SCALAR_SI(ppf_mul_2si, -3L)
WRAP_SCALAR_UI(ppf_div_2ui, 7UL)
WRAP_SCALAR_SI(ppf_div_2si, -3L)
WRAP_SCALAR_UI(ppf_pow_ui, 5UL)
WRAP_SCALAR_SI(ppf_pow_si, -3L)
WRAP_SCALAR_UI(ppf_pow_uj, 5UL)
WRAP_SCALAR_SI(ppf_pow_sj, -3L)
WRAP_SCALAR_SI(ppf_pown, -3L)
WRAP_SCALAR_SI(ppf_compound_si, 4L)
WRAP_SCALAR_UI(ppf_cosu, 7UL)
WRAP_SCALAR_UI(ppf_sinu, 7UL)
WRAP_SCALAR_UI(ppf_tanu, 7UL)
WRAP_SCALAR_UI(ppf_acosu, 7UL)
WRAP_SCALAR_UI(ppf_asinu, 7UL)
WRAP_SCALAR_UI(ppf_atanu, 7UL)

static int wrap_add_z(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpz_t z; int t; mpz_init_set_si(z, -11); t = ppf_add_z(d, s, z, r, c); mpz_clear(z); return t;
}
static int wrap_add_q(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpq_t q; int t; mpq_init(q); mpq_set_si(q, 7, 3); t = ppf_add_q(d, s, q, r, c); mpq_clear(q); return t;
}
static int wrap_z_sub(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpz_t z; int t; mpz_init_set_si(z, 11); t = ppf_z_sub(d, z, s, r, c); mpz_clear(z); return t;
}
static int wrap_sub_z(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpz_t z; int t; mpz_init_set_si(z, -11); t = ppf_sub_z(d, s, z, r, c); mpz_clear(z); return t;
}
static int wrap_sub_q(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpq_t q; int t; mpq_init(q); mpq_set_si(q, 7, 3); t = ppf_sub_q(d, s, q, r, c); mpq_clear(q); return t;
}
static int wrap_mul_z(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpz_t z; int t; mpz_init_set_si(z, -11); t = ppf_mul_z(d, s, z, r, c); mpz_clear(z); return t;
}
static int wrap_mul_q(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpq_t q; int t; mpq_init(q); mpq_set_si(q, 7, 3); t = ppf_mul_q(d, s, q, r, c); mpq_clear(q); return t;
}
static int wrap_div_z(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpz_t z; int t; mpz_init_set_si(z, 11); t = ppf_div_z(d, s, z, r, c); mpz_clear(z); return t;
}
static int wrap_div_q(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpq_t q; int t; mpq_init(q); mpq_set_si(q, 7, 3); t = ppf_div_q(d, s, q, r, c); mpq_clear(q); return t;
}
static int wrap_sqr_threads(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_sqr_threads(d, s, r, 3UL, c);
}
static int wrap_sqr_algorithm_threads(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_sqr_algorithm_threads(d, s, r, 3UL, PPF_PRODUCT_KARATSUBA3, c);
}
static int wrap_rec_sqrt_threads(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_rec_sqrt_threads(d, s, r, 3UL, c);
}
static int wrap_rec_sqrt_algorithm_threads(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_rec_sqrt_algorithm_threads(d, s, r, 3UL, PPF_PRODUCT_KARATSUBA3, c);
}
static int wrap_rootn_ui(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_rootn_ui(d, s, 3UL, r, c);
}
static int wrap_rootn_si(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_rootn_si(d, s, -3L, r, c);
}
static int wrap_root(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_root(d, s, 5UL, r, c);
}
static int wrap_div_with_plan(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    ppf_t denominator; ppf_divisor_plan_t *plan = NULL; int t;
    init_value(&denominator, d->precision, "3.25", c);
    CHECK(ppf_divisor_plan_create(&plan, &denominator, d->precision, c) == 0);
    t = ppf_div_with_plan(d, s, plan, r, c);
    ppf_divisor_plan_destroy(plan); ppf_clear(&denominator); return t;
}
static int wrap_pow_z(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    mpz_t z; int t; mpz_init_set_si(z, -3); t = ppf_pow_z(d, s, z, r, c); mpz_clear(z); return t;
}
static int wrap_ui_pow(ppf_t *d, const ppf_t *s, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_ui_pow(d, 3UL, s, r, c);
}

static int wrap_mul_threads(ppf_t *d, const ppf_t *a, const ppf_t *b,
                            ppf_rnd_t r, ppf_context_t *c) {
    return ppf_mul_threads(d, a, b, r, 3UL, c);
}
static int wrap_mul_algorithm_threads(ppf_t *d, const ppf_t *a, const ppf_t *b,
                                      ppf_rnd_t r, ppf_context_t *c) {
    return ppf_mul_algorithm_threads(d, a, b, r, 3UL,
                                     PPF_PRODUCT_KARATSUBA3, c);
}
static int wrap_atan2u(ppf_t *d, const ppf_t *a, const ppf_t *b,
                       ppf_rnd_t r, ppf_context_t *c) {
    return ppf_atan2u(d, a, b, 7UL, r, c);
}

static int wrap_fma_threads(ppf_t *d, const ppf_t *a, const ppf_t *b,
                            const ppf_t *cc, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_fma_threads(d, a, b, cc, r, 3UL, c);
}
static int wrap_fma_algorithm_threads(ppf_t *d, const ppf_t *a, const ppf_t *b,
                                      const ppf_t *cc, ppf_rnd_t r,
                                      ppf_context_t *c) {
    return ppf_fma_algorithm_threads(d, a, b, cc, r, 3UL,
                                     PPF_PRODUCT_KARATSUBA3, c);
}
static int wrap_fms_threads(ppf_t *d, const ppf_t *a, const ppf_t *b,
                            const ppf_t *cc, ppf_rnd_t r, ppf_context_t *c) {
    return ppf_fms_threads(d, a, b, cc, r, 3UL, c);
}
static int wrap_fms_algorithm_threads(ppf_t *d, const ppf_t *a, const ppf_t *b,
                                      const ppf_t *cc, ppf_rnd_t r,
                                      ppf_context_t *c) {
    return ppf_fms_algorithm_threads(d, a, b, cc, r, 3UL,
                                     PPF_PRODUCT_KARATSUBA3, c);
}

static void check_reldiff(void) {
    ppf_context_t ctx; ppf_t a, b, expected, aa, bb;
    init_context(&ctx); init_value(&a, 113, "5.75", &ctx);
    init_value(&b, 79, "2.25", &ctx);
    for (size_t r = 0; r < sizeof(all_rounds)/sizeof(all_rounds[0]); ++r) {
        ppf_flags_t ef, af;
        CHECK(ppf_init2_ctx(&expected, a.precision, &ctx) == 0);
        copy_value(&aa, &a, &ctx); copy_value(&bb, &b, &ctx);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        ppf_reldiff(&expected, &a, &b, all_rounds[r], &ctx); ef=ppf_context_flags(&ctx);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        ppf_reldiff(&aa, &aa, &bb, all_rounds[r], &ctx); af=ppf_context_flags(&ctx);
        CHECK(ef==af && same_value(&expected,&aa));
        ppf_clear(&expected); ppf_clear(&aa); ppf_clear(&bb);
        CHECK(ppf_init2_ctx(&expected, b.precision, &ctx) == 0);
        copy_value(&aa, &a, &ctx); copy_value(&bb, &b, &ctx);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        ppf_reldiff(&expected, &a, &b, all_rounds[r], &ctx); ef=ppf_context_flags(&ctx);
        ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
        ppf_reldiff(&bb, &aa, &bb, all_rounds[r], &ctx); af=ppf_context_flags(&ctx);
        CHECK(ef==af && same_value(&expected,&bb));
        ppf_clear(&expected); ppf_clear(&aa); ppf_clear(&bb);
    }
    ppf_clear(&a); ppf_clear(&b); ppf_context_clear(&ctx);
}

static void check_frexp_alias(void) {
    ppf_context_t ctx; ppf_t source, expected, alias; ppf_exp_t ee, ae;
    init_context(&ctx); init_value(&source, 113, "13.625", &ctx);
    for (size_t r=0;r<sizeof(all_rounds)/sizeof(all_rounds[0]);++r) {
        int et,at; ppf_flags_t ef,af;
        CHECK(ppf_init2_ctx(&expected,source.precision,&ctx)==0);
        copy_value(&alias,&source,&ctx);
        ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
        et=ppf_frexp(&ee,&expected,&source,all_rounds[r],&ctx);ef=ppf_context_flags(&ctx);
        ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
        at=ppf_frexp(&ae,&alias,&alias,all_rounds[r],&ctx);af=ppf_context_flags(&ctx);
        CHECK(et==at&&ee==ae&&ef==af&&same_value(&expected,&alias));
        ppf_clear(&expected);ppf_clear(&alias);
    }
    ppf_clear(&source);ppf_context_clear(&ctx);
}

static void check_remainder_outputs(const char *name, int nearest) {
    ppf_context_t ctx; ppf_t x,y;
    init_context(&ctx); init_value(&x,113,"11.75",&ctx); init_value(&y,79,"3.25",&ctx);
    for(size_t r=0;r<sizeof(all_rounds)/sizeof(all_rounds[0]);++r){
        for(unsigned which=0;which<2;++which){
            ppf_t xx,yy,expected; long eq=0,aq=0; int et,at; ppf_flags_t ef,af;
            copy_value(&xx,&x,&ctx);copy_value(&yy,&y,&ctx);
            CHECK(ppf_init2_ctx(&expected,which?y.precision:x.precision,&ctx)==0);
            ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            et=nearest?ppf_remquo(&expected,&eq,&x,&y,all_rounds[r],&ctx):
                       ppf_fmodquo(&expected,&eq,&x,&y,all_rounds[r],&ctx);
            ef=ppf_context_flags(&ctx);
            ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            at=nearest?ppf_remquo(which?&yy:&xx,&aq,&xx,&yy,all_rounds[r],&ctx):
                       ppf_fmodquo(which?&yy:&xx,&aq,&xx,&yy,all_rounds[r],&ctx);
            af=ppf_context_flags(&ctx);
            CHECK(et==at&&eq==aq&&ef==af&&same_value(&expected,which?&yy:&xx));
            ppf_clear(&xx);ppf_clear(&yy);ppf_clear(&expected);
        }
    }
    ppf_clear(&x);ppf_clear(&y);ppf_context_clear(&ctx);(void)name;
}

static void check_dot_threads_alias(void) {
    ppf_context_t ctx; ppf_t a[3],b[3];
    const char *av[]={"1.25","2.5","-0.75"};
    const char *bv[]={"0.5","-1.25","3.0"};
    init_context(&ctx);
    for(unsigned i=0;i<3;++i){init_value(&a[i],97-i*8,av[i],&ctx);init_value(&b[i],89-i*8,bv[i],&ctx);}
    for(size_t r=0;r<sizeof(all_rounds)/sizeof(all_rounds[0]);++r){
        for(unsigned side=0;side<2;++side)for(unsigned idx=0;idx<3;++idx){
            ppf_t ac[3],bc[3],expected;const ppf_t*ap[3];const ppf_t*bp[3];ppf_t*alias;
            int et,at;ppf_flags_t ef,af;
            for(unsigned i=0;i<3;++i){copy_value(&ac[i],&a[i],&ctx);copy_value(&bc[i],&b[i],&ctx);ap[i]=&ac[i];bp[i]=&bc[i];}
            alias=side?&bc[idx]:&ac[idx];CHECK(ppf_init2_ctx(&expected,alias->precision,&ctx)==0);
            {
                const ppf_t*oa[3]={&a[0],&a[1],&a[2]};const ppf_t*ob[3]={&b[0],&b[1],&b[2]};
                ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);et=ppf_dot_threads(&expected,oa,ob,3,all_rounds[r],3,&ctx);ef=ppf_context_flags(&ctx);
            }
            ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);at=ppf_dot_threads(alias,ap,bp,3,all_rounds[r],3,&ctx);af=ppf_context_flags(&ctx);
            CHECK(et==at&&ef==af&&same_value(&expected,alias));
            for(unsigned i=0;i<3;++i){ppf_clear(&ac[i]);ppf_clear(&bc[i]);}ppf_clear(&expected);
        }
    }
    for(unsigned i=0;i<3;++i){ppf_clear(&a[i]);ppf_clear(&b[i]);}ppf_context_clear(&ctx);
}

static void check_multi_output(void) {
    ppf_context_t ctx; ppf_t x, i, f, s, c, snapshot;
    init_context(&ctx); init_value(&x,113,"3.625",&ctx);
    CHECK(ppf_init2_ctx(&i,113,&ctx)==0);CHECK(ppf_init2_ctx(&f,97,&ctx)==0);
    CHECK(ppf_modf(&i,&f,&x,PPF_RNDN,&ctx)!=PPF_TERNARY_ERROR);
    {
        ppf_t xa,fo,ie,fe; int t1,t2;
        copy_value(&xa,&x,&ctx);CHECK(ppf_init2_ctx(&fo,97,&ctx)==0);
        CHECK(ppf_init2_ctx(&ie,113,&ctx)==0);CHECK(ppf_init2_ctx(&fe,97,&ctx)==0);
        t1=ppf_modf(&ie,&fe,&x,PPF_RNDN,&ctx);t2=ppf_modf(&xa,&fo,&xa,PPF_RNDN,&ctx);
        CHECK(t1==t2&&same_value(&ie,&xa)&&same_value(&fe,&fo));
        ppf_clear(&xa);ppf_clear(&fo);ppf_clear(&ie);ppf_clear(&fe);
    }
    {
        ppf_t xa,io,ie,fe; int t1,t2;
        copy_value(&xa,&x,&ctx);CHECK(ppf_init2_ctx(&io,113,&ctx)==0);
        CHECK(ppf_init2_ctx(&ie,113,&ctx)==0);CHECK(ppf_init2_ctx(&fe,113,&ctx)==0);
        t1=ppf_modf(&ie,&fe,&x,PPF_RNDN,&ctx);t2=ppf_modf(&io,&xa,&xa,PPF_RNDN,&ctx);
        CHECK(t1==t2&&same_value(&ie,&io)&&same_value(&fe,&xa));
        ppf_clear(&xa);ppf_clear(&io);ppf_clear(&ie);ppf_clear(&fe);
    }
    copy_value(&snapshot,&i,&ctx);
    CHECK(ppf_modf(&i,&i,&x,PPF_RNDN,&ctx)==PPF_TERNARY_ERROR);
    CHECK(same_representation(&i,&snapshot)); ppf_clear(&snapshot);

    CHECK(ppf_init2_ctx(&s,113,&ctx)==0);CHECK(ppf_init2_ctx(&c,97,&ctx)==0);
    {
        ppf_t xa,se,ce,co;int e,a;
        init_value(&xa,113,"0.375",&ctx);CHECK(ppf_init2_ctx(&se,113,&ctx)==0);CHECK(ppf_init2_ctx(&ce,97,&ctx)==0);CHECK(ppf_init2_ctx(&co,97,&ctx)==0);
        e=ppf_sin_cos(&se,&ce,&xa,PPF_RNDN,&ctx);a=ppf_sin_cos(&xa,&co,&xa,PPF_RNDN,&ctx);
        CHECK(e==a&&same_value(&se,&xa)&&same_value(&ce,&co));
        ppf_clear(&xa);ppf_clear(&se);ppf_clear(&ce);ppf_clear(&co);
    }
    init_value(&snapshot,113,"0.375",&ctx);
    { ppf_t before; copy_value(&before,&snapshot,&ctx);
      CHECK(ppf_sin_cos(&snapshot,&snapshot,&x,PPF_RNDN,&ctx)==PPF_TERNARY_ERROR);
      CHECK(same_representation(&snapshot,&before)); ppf_clear(&before); }
    ppf_clear(&snapshot);
    {
        ppf_t xa,se,ce,so;int e,a;
        init_value(&xa,113,"0.375",&ctx);CHECK(ppf_init2_ctx(&se,113,&ctx)==0);CHECK(ppf_init2_ctx(&ce,113,&ctx)==0);CHECK(ppf_init2_ctx(&so,113,&ctx)==0);
        e=ppf_sin_cos(&se,&ce,&xa,PPF_RNDN,&ctx);a=ppf_sin_cos(&so,&xa,&xa,PPF_RNDN,&ctx);
        CHECK(e==a&&same_value(&se,&so)&&same_value(&ce,&xa));
        ppf_clear(&xa);ppf_clear(&se);ppf_clear(&ce);ppf_clear(&so);
    }
    {
        ppf_t xa,se,ce,co;int e,a;
        init_value(&xa,113,"0.375",&ctx);CHECK(ppf_init2_ctx(&se,113,&ctx)==0);CHECK(ppf_init2_ctx(&ce,97,&ctx)==0);CHECK(ppf_init2_ctx(&co,97,&ctx)==0);
        e=ppf_sinh_cosh(&se,&ce,&xa,PPF_RNDN,&ctx);a=ppf_sinh_cosh(&xa,&co,&xa,PPF_RNDN,&ctx);
        CHECK(e==a&&same_value(&se,&xa)&&same_value(&ce,&co));
        ppf_clear(&xa);ppf_clear(&se);ppf_clear(&ce);ppf_clear(&co);
    }
    {
        ppf_t xa,se,ce,so;int e,a;
        init_value(&xa,113,"0.375",&ctx);CHECK(ppf_init2_ctx(&se,113,&ctx)==0);CHECK(ppf_init2_ctx(&ce,113,&ctx)==0);CHECK(ppf_init2_ctx(&so,113,&ctx)==0);
        e=ppf_sinh_cosh(&se,&ce,&xa,PPF_RNDN,&ctx);a=ppf_sinh_cosh(&so,&xa,&xa,PPF_RNDN,&ctx);
        CHECK(e==a&&same_value(&se,&so)&&same_value(&ce,&xa));
        ppf_clear(&xa);ppf_clear(&se);ppf_clear(&ce);ppf_clear(&so);
    }
    init_value(&snapshot,113,"0.375",&ctx);
    { ppf_t before; copy_value(&before,&snapshot,&ctx);
      CHECK(ppf_sinh_cosh(&snapshot,&snapshot,&x,PPF_RNDN,&ctx)==PPF_TERNARY_ERROR);
      CHECK(same_representation(&snapshot,&before)); ppf_clear(&before); }
    ppf_clear(&snapshot);
    ppf_clear(&x);ppf_clear(&i);ppf_clear(&f);ppf_clear(&s);ppf_clear(&c);ppf_context_clear(&ctx);
}

static void check_init_set_distinctness(void) {
    ppf_context_t ctx; ppf_t source, destination, before;
    init_context(&ctx);
    init_value(&source, 97, "-3.625", &ctx);
    copy_value(&before, &source, &ctx);
    memset(&destination, 0, sizeof(destination));
    CHECK(ppf_init_set(&destination, &source, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
    CHECK(ppf_cmp(&destination, &source, &ctx) == 0);
    CHECK(same_representation(&source, &before));
    ppf_clear(&destination); ppf_clear(&before); ppf_clear(&source); ppf_context_clear(&ctx);
}

static void check_round_nearest_away_distinctness(void) {
    ppf_context_t ctx; ppf_t rop, extended, before;
    init_context(&ctx);
    CHECK(ppf_init2_ctx(&rop, 53, &ctx) == 0);
    init_value(&extended, 54, "1.375", &ctx);
    CHECK(ppf_round_nearest_away_finish(&rop, &extended, 1, &ctx) != PPF_TERNARY_ERROR);
    copy_value(&before, &extended, &ctx);
    CHECK(ppf_round_nearest_away_finish(&extended, &extended, 1, &ctx) == PPF_TERNARY_ERROR);
    CHECK(same_representation(&extended, &before));
    ppf_clear(&before); ppf_clear(&extended); ppf_clear(&rop); ppf_context_clear(&ctx);
}

static void check_grandom_policy(void) {
    ppf_context_t ctx;ppf_t x,before;gmp_randstate_t st;
    init_context(&ctx);init_value(&x,97,"1.25",&ctx);copy_value(&before,&x,&ctx);
    gmp_randinit_default(st);gmp_randseed_ui(st,12345UL);
    CHECK(ppf_grandom(&x,&x,st,PPF_RNDN,&ctx)==PPF_TERNARY_ERROR);
    CHECK(same_representation(&x,&before));
    gmp_randclear(st);ppf_clear(&x);ppf_clear(&before);ppf_context_clear(&ctx);
}

static void check_constant_interval_policy(void) {
    mpz_t both,before,lo,hi;ppf_exp_t e;
    mpz_init_set_ui(both,123);mpz_init_set(before,both);mpz_inits(lo,hi,NULL);
    CHECK(ppf_get_constant_interval(PPF_CONSTANT_PI,lo,hi,&e,64,1)==0);
    CHECK(mpz_cmp(lo,hi)<=0);
    CHECK(ppf_get_constant_interval(PPF_CONSTANT_PI,both,both,&e,64,1)!=0);
    CHECK(mpz_cmp(both,before)==0);
    mpz_clears(both,before,lo,hi,NULL);
}

static void check_div_many_policy(void) {
    ppf_context_t ctx;ppf_t den,n0,n1,n2,o0,o1,o2,b0,b1;ppf_divisor_plan_t*plan=NULL;int ts[3];
    init_context(&ctx);init_value(&den,97,"3.25",&ctx);init_value(&n0,97,"7.5",&ctx);init_value(&n1,97,"9.25",&ctx);init_value(&n2,97,"11.75",&ctx);
    CHECK(ppf_divisor_plan_create(&plan,&den,97,&ctx)==0);
    copy_value(&o0,&n0,&ctx);copy_value(&o1,&n1,&ctx);copy_value(&o2,&n2,&ctx);
    {ppf_t*out[3]={&o0,&o1,&o2};const ppf_t*in[3]={&o0,&o1,&o2};CHECK(ppf_div_with_plan_many(out,in,3,plan,PPF_RNDN,3,ts,&ctx)==0);}
    copy_value(&b0,&n0,&ctx);copy_value(&b1,&n1,&ctx);
    {ppf_t*out[2]={&b1,&o2};const ppf_t*in[2]={&b0,&b1};ppf_t snap;copy_value(&snap,&b1,&ctx);CHECK(ppf_div_with_plan_many(out,in,2,plan,PPF_RNDN,2,ts,&ctx)!=0);CHECK(same_representation(&b1,&snap));ppf_clear(&snap);}
    ppf_clear(&den);ppf_clear(&n0);ppf_clear(&n1);ppf_clear(&n2);ppf_clear(&o0);ppf_clear(&o1);ppf_clear(&o2);ppf_clear(&b0);ppf_clear(&b1);ppf_divisor_plan_destroy(plan);ppf_context_clear(&ctx);
}

struct unary_case { const char *name; unary_fn fn; const char *value; };

static const struct unary_case unary_cases[] = {
    {"ppf_setsign", wrap_setsign, "-3.625"},
    {"ppf_sqr", ppf_sqr, "1.375"}, {"ppf_sqrt", ppf_sqrt, "2.25"},
    {"ppf_neg", ppf_neg, "-3.625"}, {"ppf_abs", ppf_abs, "-3.625"},
    {"ppf_rint", ppf_rint, "3.625"},
    {"ppf_ceil", wrap_ppf_ceil, "-3.625"},
    {"ppf_floor", wrap_ppf_floor, "-3.625"},
    {"ppf_round", wrap_ppf_round, "-3.625"},
    {"ppf_roundeven", wrap_ppf_roundeven, "-2.5"},
    {"ppf_trunc", wrap_ppf_trunc, "-3.625"},
    {"ppf_rint_ceil", ppf_rint_ceil, "-3.625"},
    {"ppf_rint_floor", ppf_rint_floor, "-3.625"},
    {"ppf_rint_round", ppf_rint_round, "-3.625"},
    {"ppf_rint_roundeven", ppf_rint_roundeven, "-2.5"},
    {"ppf_rint_trunc", ppf_rint_trunc, "-3.625"},
    {"ppf_frac", ppf_frac, "-3.625"},
    {"ppf_fmod_ui", wrap_fmod_ui, "11.75"},
    {"ppf_mul_2exp", wrap_mul_2exp, "1.375"},
    {"ppf_div_2exp", wrap_div_2exp, "1.375"},
    {"ppf_add_ui", wrap_ppf_add_ui, "1.375"},
    {"ppf_add_si", wrap_ppf_add_si, "1.375"},
    {"ppf_add_d", wrap_ppf_add_d, "1.375"},
    {"ppf_add_z", wrap_add_z, "1.375"}, {"ppf_add_q", wrap_add_q, "1.375"},
    {"ppf_ui_sub", wrap_ppf_ui_sub, "1.375"},
    {"ppf_sub_ui", wrap_ppf_sub_ui, "1.375"},
    {"ppf_si_sub", wrap_ppf_si_sub, "1.375"},
    {"ppf_sub_si", wrap_ppf_sub_si, "1.375"},
    {"ppf_d_sub", wrap_ppf_d_sub, "1.375"},
    {"ppf_sub_d", wrap_ppf_sub_d, "1.375"},
    {"ppf_z_sub", wrap_z_sub, "1.375"}, {"ppf_sub_z", wrap_sub_z, "1.375"},
    {"ppf_sub_q", wrap_sub_q, "1.375"},
    {"ppf_mul_ui", wrap_ppf_mul_ui, "1.375"},
    {"ppf_mul_si", wrap_ppf_mul_si, "1.375"},
    {"ppf_mul_d", wrap_ppf_mul_d, "1.375"},
    {"ppf_mul_z", wrap_mul_z, "1.375"}, {"ppf_mul_q", wrap_mul_q, "1.375"},
    {"ppf_sqr_threads", wrap_sqr_threads, "1.375"},
    {"ppf_sqr_algorithm_threads", wrap_sqr_algorithm_threads, "1.375"},
    {"ppf_ui_div", wrap_ppf_ui_div, "1.375"},
    {"ppf_div_ui", wrap_ppf_div_ui, "1.375"},
    {"ppf_si_div", wrap_ppf_si_div, "1.375"},
    {"ppf_div_si", wrap_ppf_div_si, "1.375"},
    {"ppf_d_div", wrap_ppf_d_div, "1.375"},
    {"ppf_div_d", wrap_ppf_div_d, "1.375"},
    {"ppf_div_z", wrap_div_z, "1.375"}, {"ppf_div_q", wrap_div_q, "1.375"},
    {"ppf_rec_sqrt", ppf_rec_sqrt, "2.25"},
    {"ppf_rec_sqrt_threads", wrap_rec_sqrt_threads, "2.25"},
    {"ppf_rec_sqrt_algorithm_threads", wrap_rec_sqrt_algorithm_threads, "2.25"},
    {"ppf_cbrt", ppf_cbrt, "2.25"}, {"ppf_rootn_ui", wrap_rootn_ui, "2.25"},
    {"ppf_rootn_si", wrap_rootn_si, "2.25"}, {"ppf_root", wrap_root, "2.25"},
    {"ppf_div_with_plan", wrap_div_with_plan, "11.75"},
    {"ppf_mul_2ui", wrap_ppf_mul_2ui, "1.375"},
    {"ppf_mul_2si", wrap_ppf_mul_2si, "1.375"},
    {"ppf_div_2ui", wrap_ppf_div_2ui, "1.375"},
    {"ppf_div_2si", wrap_ppf_div_2si, "1.375"},
    {"ppf_log", ppf_log, "1.375"}, {"ppf_log2", ppf_log2, "1.375"},
    {"ppf_log10", ppf_log10, "1.375"}, {"ppf_log1p", ppf_log1p, "0.375"},
    {"ppf_log2p1", ppf_log2p1, "0.375"}, {"ppf_log10p1", ppf_log10p1, "0.375"},
    {"ppf_exp", ppf_exp, "0.375"}, {"ppf_exp2", ppf_exp2, "0.375"},
    {"ppf_exp10", ppf_exp10, "0.125"}, {"ppf_expm1", ppf_expm1, "0.375"},
    {"ppf_exp2m1", ppf_exp2m1, "0.375"}, {"ppf_exp10m1", ppf_exp10m1, "0.125"},
    {"ppf_pow_ui", wrap_ppf_pow_ui, "1.375"},
    {"ppf_pow_si", wrap_ppf_pow_si, "1.375"},
    {"ppf_pow_uj", wrap_ppf_pow_uj, "1.375"},
    {"ppf_pow_sj", wrap_ppf_pow_sj, "1.375"},
    {"ppf_pown", wrap_ppf_pown, "1.375"}, {"ppf_pow_z", wrap_pow_z, "1.375"},
    {"ppf_ui_pow", wrap_ui_pow, "1.375"}, {"ppf_compound_si", wrap_ppf_compound_si, "0.125"},
    {"ppf_cosu", wrap_ppf_cosu, "0.375"}, {"ppf_sinu", wrap_ppf_sinu, "0.375"},
    {"ppf_tanu", wrap_ppf_tanu, "0.375"}, {"ppf_cospi", ppf_cospi, "0.375"},
    {"ppf_sinpi", ppf_sinpi, "0.375"}, {"ppf_tanpi", ppf_tanpi, "0.375"},
    {"ppf_sec", ppf_sec, "0.375"}, {"ppf_csc", ppf_csc, "0.375"},
    {"ppf_cot", ppf_cot, "0.375"}, {"ppf_acosu", wrap_ppf_acosu, "0.375"},
    {"ppf_asinu", wrap_ppf_asinu, "0.375"}, {"ppf_atanu", wrap_ppf_atanu, "0.375"},
    {"ppf_acospi", ppf_acospi, "0.375"}, {"ppf_asinpi", ppf_asinpi, "0.375"},
    {"ppf_atanpi", ppf_atanpi, "0.375"}, {"ppf_sech", ppf_sech, "0.375"},
    {"ppf_csch", ppf_csch, "0.375"}, {"ppf_coth", ppf_coth, "0.375"}
};

int main(void) {
    for (size_t i = 0; i < sizeof(unary_cases)/sizeof(unary_cases[0]); ++i)
        check_unary(unary_cases[i].name, unary_cases[i].fn, unary_cases[i].value);

    check_frexp_alias();
    check_binary("ppf_add", ppf_add, "1.375", "2.25");
    check_binary("ppf_sub", ppf_sub, "1.375", "2.25");
    check_binary("ppf_mul", ppf_mul, "1.375", "2.25");
    check_binary("ppf_div", ppf_div, "1.375", "2.25");
    check_binary("ppf_copysign", ppf_copysign, "1.375", "-2.25");
    check_binary("ppf_fmod", ppf_fmod, "11.75", "3.25");
    check_binary("ppf_remainder", ppf_remainder, "11.75", "3.25");
    check_binary("ppf_min", ppf_min, "1.375", "2.25");
    check_binary("ppf_max", ppf_max, "1.375", "2.25");
    check_binary("ppf_dim", ppf_dim, "3.375", "2.25");
    check_reldiff();
    check_binary("ppf_mul_threads", wrap_mul_threads, "1.375", "2.25");
    check_binary("ppf_mul_algorithm_threads", wrap_mul_algorithm_threads, "1.375", "2.25");
    check_binary("ppf_hypot", ppf_hypot, "3.0", "4.0");
    check_binary("ppf_pow", ppf_pow, "1.375", "2.25");
    check_binary("ppf_powr", ppf_powr, "1.375", "2.25");
    check_binary("ppf_atan2u", wrap_atan2u, "0.75", "1.25");
    check_binary("ppf_atan2pi", ppf_atan2pi, "0.75", "1.25");

    check_ternary("ppf_fma", ppf_fma, "1.375", "2.25", "-0.625");
    check_ternary("ppf_fms", ppf_fms, "1.375", "2.25", "-0.625");
    check_ternary("ppf_fma_threads", wrap_fma_threads, "1.375", "2.25", "-0.625");
    check_ternary("ppf_fma_algorithm_threads", wrap_fma_algorithm_threads, "1.375", "2.25", "-0.625");
    check_ternary("ppf_fms_threads", wrap_fms_threads, "1.375", "2.25", "-0.625");
    check_ternary("ppf_fms_algorithm_threads", wrap_fms_algorithm_threads, "1.375", "2.25", "-0.625");
    check_quaternary("ppf_fmma", ppf_fmma, "1.375", "2.25", "-0.625", "1.125");
    check_quaternary("ppf_fmms", ppf_fmms, "1.375", "2.25", "-0.625", "1.125");

    check_remainder_outputs("ppf_fmodquo", 0);
    check_remainder_outputs("ppf_remquo", 1);
    check_dot_threads_alias();
    check_multi_output();
    check_init_set_distinctness();
    check_round_nearest_away_distinctness();
    check_grandom_policy();
    check_constant_interval_policy();
    check_div_many_policy();

    puts("systematic aliasing matrix passed");
    return 0;
}
