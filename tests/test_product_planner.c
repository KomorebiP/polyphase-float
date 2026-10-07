/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message) {
    fprintf(stderr, "product planner test failure: %s\n", message);
    exit(2);
}

static int same_ternary(int a, int b) {
    return (a > 0) == (b > 0) && (a < 0) == (b < 0);
}

static void check_one(gmp_randstate_t state, unsigned long input_bits,
                      unsigned long output_bits, int square,
                      ppf_rnd_t prnd, mpfr_rnd_t mrnd) {
    ppf_context_t ctx;
    ppf_t a, b, full, rank3, rank5, window;
    mpfr_t ma, mb, reference, exported;
    mpz_t za, zb;
    int tf, t3, t5, tw, tm;

    if (ppf_context_init(&ctx) != 0) fail("context init");
    if (ppf_init2_ctx(&a, input_bits, &ctx) != 0 ||
        ppf_init2_ctx(&b, input_bits, &ctx) != 0 ||
        ppf_init2_ctx(&full, output_bits, &ctx) != 0 ||
        ppf_init2_ctx(&rank3, output_bits, &ctx) != 0 ||
        ppf_init2_ctx(&rank5, output_bits, &ctx) != 0 ||
        ppf_init2_ctx(&window, output_bits, &ctx) != 0)
        fail("ppf init");
    mpfr_init2(ma, input_bits);
    mpfr_init2(mb, input_bits);
    mpfr_init2(reference, output_bits);
    mpfr_init2(exported, output_bits);
    mpz_inits(za, zb, NULL);

    mpz_urandomb(za, state, input_bits);
    mpz_urandomb(zb, state, input_bits);
    mpz_setbit(za, input_bits - 1);
    mpz_setbit(zb, input_bits - 1);
    if ((mpz_get_ui(za) & 1u) != 0) mpz_neg(za, za);
    if ((mpz_get_ui(zb) & 2u) != 0) mpz_neg(zb, zb);
    if (ppf_set_z(&a, za, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_z(&b, zb, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        fail("input assignment");
    mpfr_set_z(ma, za, MPFR_RNDN);
    mpfr_set_z(mb, zb, MPFR_RNDN);

    if (square) {
        tm = mpfr_sqr(reference, ma, mrnd);
        tf = ppf_sqr_algorithm_threads(&full, &a, prnd, 1,
                                       PPF_PRODUCT_FULL, &ctx);
        t3 = ppf_sqr_algorithm_threads(&rank3, &a, prnd, 3,
                                       PPF_PRODUCT_KARATSUBA3, &ctx);
        t5 = ppf_sqr_algorithm_threads(&rank5, &a, prnd, 5,
                                       PPF_PRODUCT_TOOM3, &ctx);
        tw = ppf_sqr_algorithm_threads(&window, &a, prnd, 3,
                                       PPF_PRODUCT_WINDOW, &ctx);
    } else {
        tm = mpfr_mul(reference, ma, mb, mrnd);
        tf = ppf_mul_algorithm_threads(&full, &a, &b, prnd, 1,
                                       PPF_PRODUCT_FULL, &ctx);
        t3 = ppf_mul_algorithm_threads(&rank3, &a, &b, prnd, 3,
                                       PPF_PRODUCT_KARATSUBA3, &ctx);
        t5 = ppf_mul_algorithm_threads(&rank5, &a, &b, prnd, 5,
                                       PPF_PRODUCT_TOOM3, &ctx);
        tw = ppf_mul_algorithm_threads(&window, &a, &b, prnd, 3,
                                       PPF_PRODUCT_WINDOW, &ctx);
    }
    if (tf == PPF_TERNARY_ERROR || t3 == PPF_TERNARY_ERROR ||
        t5 == PPF_TERNARY_ERROR || tw == PPF_TERNARY_ERROR)
        fail("algorithm returned error");
    if (!same_ternary(tf, tm) || !same_ternary(t3, tm) ||
        !same_ternary(t5, tm) || !same_ternary(tw, tm))
        fail("ternary mismatch");

#define CHECK_VALUE(name, object) do { \
    ppf_get_mpfr(exported, &(object), MPFR_RNDN); \
    if (!mpfr_equal_p(exported, reference)) fail(name); \
} while (0)
    CHECK_VALUE("full value mismatch", full);
    CHECK_VALUE("rank3 value mismatch", rank3);
    CHECK_VALUE("rank5 value mismatch", rank5);
    CHECK_VALUE("window value mismatch", window);
#undef CHECK_VALUE

    /* Destination/input aliasing is checked on a separate copy. */
    if (!square) {
        ppf_t alias;
        int ta;
        if (ppf_init2_ctx(&alias, input_bits, &ctx) != 0) fail("alias init");
        if (ppf_set(&alias, &a, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
            fail("alias copy");
        ppf_prec_round(&alias, output_bits, PPF_RNDN, &ctx);
        /* Recreate at input precision because precision is part of the object. */
        ppf_clear(&alias);
        if (ppf_init2_ctx(&alias, input_bits, &ctx) != 0 ||
            ppf_set(&alias, &a, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
            fail("alias recreate");
        ppf_set_prec(&alias, output_bits);
        if (ppf_set(&alias, &a, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
            fail("alias source reset");
        /* This alias case is meaningful only when precisions match. */
        if (output_bits == input_bits) {
            ta = ppf_mul_algorithm_threads(&alias, &alias, &b, prnd, 5,
                                           PPF_PRODUCT_TOOM3, &ctx);
            if (!same_ternary(ta, tm)) fail("alias ternary");
            ppf_get_mpfr(exported, &alias, MPFR_RNDN);
            if (!mpfr_equal_p(exported, reference)) fail("alias value");
        }
        ppf_clear(&alias);
    }

    mpz_clears(za, zb, NULL);
    mpfr_clears(ma, mb, reference, exported, (mpfr_ptr)0);
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&full);
    ppf_clear(&rank3); ppf_clear(&rank5); ppf_clear(&window);
    ppf_context_clear(&ctx);
}


static void check_fma_rank(gmp_randstate_t state, unsigned long bits,
                           ppf_rnd_t prnd, mpfr_rnd_t mrnd) {
    ppf_context_t ctx;
    ppf_t a, b, c, full, rank3, rank5, automatic, alias;
    mpfr_t ma, mb, mc, reference, exported;
    mpz_t za, zb, zc;
    int tf, t3, t5, taut, ta, tm;
    if (ppf_context_init(&ctx) != 0) fail("fma context");
#define INIT_PPF(x) if (ppf_init2_ctx(&(x), bits, &ctx) != 0) fail("fma ppf init")
    INIT_PPF(a); INIT_PPF(b); INIT_PPF(c); INIT_PPF(full);
    INIT_PPF(rank3); INIT_PPF(rank5); INIT_PPF(automatic); INIT_PPF(alias);
#undef INIT_PPF
    mpfr_inits2(bits, ma, mb, mc, reference, exported, (mpfr_ptr)0);
    mpz_inits(za, zb, zc, NULL);
    mpz_urandomb(za, state, bits); mpz_setbit(za, bits - 1);
    mpz_urandomb(zb, state, bits); mpz_setbit(zb, bits - 1);
    mpz_urandomb(zc, state, bits); mpz_setbit(zc, bits - 1);
    if (mpz_tstbit(za, 1)) mpz_neg(za, za);
    if (mpz_tstbit(zb, 2)) mpz_neg(zb, zb);
    if (mpz_tstbit(zc, 3)) mpz_neg(zc, zc);
    if (ppf_set_z(&a, za, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_z(&b, zb, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_z(&c, zc, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&alias, &a, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        fail("fma assignment");
    mpfr_set_z(ma, za, MPFR_RNDN);
    mpfr_set_z(mb, zb, MPFR_RNDN);
    mpfr_set_z(mc, zc, MPFR_RNDN);
    tm = mpfr_fma(reference, ma, mb, mc, mrnd);
    tf = ppf_fma_algorithm_threads(&full, &a, &b, &c, prnd, 1,
                                   PPF_PRODUCT_FULL, &ctx);
    t3 = ppf_fma_algorithm_threads(&rank3, &a, &b, &c, prnd, 3,
                                   PPF_PRODUCT_KARATSUBA3, &ctx);
    t5 = ppf_fma_algorithm_threads(&rank5, &a, &b, &c, prnd, 5,
                                   PPF_PRODUCT_TOOM3, &ctx);
    taut = ppf_fma_algorithm_threads(&automatic, &a, &b, &c, prnd, 5,
                                     PPF_PRODUCT_AUTO, &ctx);
    if (tf == PPF_TERNARY_ERROR || t3 == PPF_TERNARY_ERROR ||
        t5 == PPF_TERNARY_ERROR || taut == PPF_TERNARY_ERROR ||
        !same_ternary(tf, tm) || !same_ternary(t3, tm) ||
        !same_ternary(t5, tm) || !same_ternary(taut, tm))
        fail("fma ternary");
#define CHECK_FMA(object, label) do { \
    if (ppf_get_mpfr(exported, &(object), MPFR_RNDN) == PPF_TERNARY_ERROR || \
        !mpfr_equal_p(exported, reference)) fail(label); \
} while (0)
    CHECK_FMA(full, "fma full value");
    CHECK_FMA(rank3, "fma rank3 value");
    CHECK_FMA(rank5, "fma rank5 value");
    CHECK_FMA(automatic, "fma auto value");
    ta = ppf_fma_algorithm_threads(&alias, &alias, &b, &c, prnd, 5,
                                   PPF_PRODUCT_TOOM3, &ctx);
    if (!same_ternary(ta, tm)) fail("fma alias ternary");
    CHECK_FMA(alias, "fma alias value");
#undef CHECK_FMA
    mpz_clears(za, zb, zc, NULL);
    mpfr_clears(ma, mb, mc, reference, exported, (mpfr_ptr)0);
    ppf_clear(&a); ppf_clear(&b); ppf_clear(&c); ppf_clear(&full);
    ppf_clear(&rank3); ppf_clear(&rank5); ppf_clear(&automatic);
    ppf_clear(&alias); ppf_context_clear(&ctx);
}

static void check_recsqrt_rank(gmp_randstate_t state,
                                unsigned long bits) {
    ppf_context_t ctx;
    ppf_t input, rank3, rank5, automatic, alias;
    mpfr_t source, reference, exported;
    mpz_t z;
    int tr3, tr5, taut, tm;

    if (ppf_context_init(&ctx) != 0) fail("recsqrt context");
    if (ppf_init2_ctx(&input, bits, &ctx) != 0 ||
        ppf_init2_ctx(&rank3, bits, &ctx) != 0 ||
        ppf_init2_ctx(&rank5, bits, &ctx) != 0 ||
        ppf_init2_ctx(&automatic, bits, &ctx) != 0 ||
        ppf_init2_ctx(&alias, bits, &ctx) != 0)
        fail("recsqrt ppf init");
    mpfr_init2(source, bits);
    mpfr_init2(reference, bits);
    mpfr_init2(exported, bits);
    mpz_init(z);
    mpz_urandomb(z, state, bits);
    mpz_setbit(z, bits - 1);
    if (ppf_set_z(&input, z, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set(&alias, &input, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        fail("recsqrt assignment");
    mpfr_set_z(source, z, MPFR_RNDN);
    tm = mpfr_rec_sqrt(reference, source, MPFR_RNDN);
    tr3 = ppf_rec_sqrt_algorithm_threads(
        &rank3, &input, PPF_RNDN, 3, PPF_PRODUCT_KARATSUBA3, &ctx);
    tr5 = ppf_rec_sqrt_algorithm_threads(
        &rank5, &input, PPF_RNDN, 5, PPF_PRODUCT_TOOM3, &ctx);
    taut = ppf_rec_sqrt_algorithm_threads(
        &automatic, &input, PPF_RNDN, 5, PPF_PRODUCT_AUTO, &ctx);
    if (tr3 == PPF_TERNARY_ERROR || tr5 == PPF_TERNARY_ERROR ||
        taut == PPF_TERNARY_ERROR ||
        !same_ternary(tr3, tm) || !same_ternary(tr5, tm) ||
        !same_ternary(taut, tm))
        fail("recsqrt ternary");
#define CHECK_RECSQRT(object, label) do { \
    if (ppf_get_mpfr(exported, &(object), MPFR_RNDN) == PPF_TERNARY_ERROR || \
        !mpfr_equal_p(exported, reference)) fail(label); \
} while (0)
    CHECK_RECSQRT(rank3, "recsqrt rank3 value");
    CHECK_RECSQRT(rank5, "recsqrt rank5 value");
    CHECK_RECSQRT(automatic, "recsqrt auto value");
    tr5 = ppf_rec_sqrt_algorithm_threads(
        &alias, &alias, PPF_RNDN, 5, PPF_PRODUCT_TOOM3, &ctx);
    if (!same_ternary(tr5, tm)) fail("recsqrt alias ternary");
    CHECK_RECSQRT(alias, "recsqrt alias value");
#undef CHECK_RECSQRT
    mpz_clear(z);
    mpfr_clears(source, reference, exported, (mpfr_ptr)0);
    ppf_clear(&input); ppf_clear(&rank3); ppf_clear(&rank5);
    ppf_clear(&automatic); ppf_clear(&alias);
    ppf_context_clear(&ctx);
}

int main(void) {
    gmp_randstate_t state;
    const ppf_rnd_t prnd[] = {PPF_RNDN, PPF_RNDZ, PPF_RNDU,
                              PPF_RNDD, PPF_RNDA};
    const mpfr_rnd_t mrnd[] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU,
                               MPFR_RNDD, MPFR_RNDA};
    gmp_randinit_mt(state);
    gmp_randseed_ui(state, 0x5A17D00Du);

    for (unsigned i = 0; i < 80; ++i) {
        unsigned long in = 256u + (unsigned long)(i * 997u) % 20000u;
        unsigned long out = 64u + (unsigned long)(i * 619u) % in;
        for (unsigned r = 0; r < 5; ++r) {
            check_one(state, in, out, 0, prnd[r], mrnd[r]);
            check_one(state, in, out, 1, prnd[r], mrnd[r]);
            if (i < 20) check_fma_rank(state, in, prnd[r], mrnd[r]);
        }
    }
    check_one(state, 1000000UL, 1000000UL, 0, PPF_RNDN, MPFR_RNDN);
    check_one(state, 1000000UL, 1000000UL, 1, PPF_RNDN, MPFR_RNDN);
    check_recsqrt_rank(state, 1000000UL);
    check_fma_rank(state, 1000000UL, PPF_RNDN, MPFR_RNDN);

    gmp_randclear(state);
    puts("RANK-ADAPTIVE PRODUCT DIFFERENTIAL TESTS PASSED");
    return 0;
}
