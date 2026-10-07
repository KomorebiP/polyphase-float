/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static int sign_of(int x) { return (x > 0) - (x < 0); }

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b)) return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_zero_p(a) && mpfr_zero_p(b))
        return mpfr_signbit(a) == mpfr_signbit(b);
    return mpfr_equal_p(a, b);
}

static void export_ppf(mpfr_ptr out, const ppf_t *x) {
    (void)ppf_get_mpfr(out, x, MPFR_RNDN);
}

static void compare_flags(const ppf_context_t *ctx) {
    ppf_flags_t f = ppf_context_flags(ctx);
    CHECK((mpfr_inexflag_p() != 0) == ((f & PPF_FLAG_INEXACT) != 0));
    CHECK((mpfr_underflow_p() != 0) == ((f & PPF_FLAG_UNDERFLOW) != 0));
    CHECK((mpfr_overflow_p() != 0) == ((f & PPF_FLAG_OVERFLOW) != 0));
    CHECK((mpfr_divby0_p() != 0) == ((f & PPF_FLAG_DIVBY0) != 0));
    CHECK((mpfr_nanflag_p() != 0) == ((f & PPF_FLAG_NAN) != 0));
    CHECK((mpfr_erangeflag_p() != 0) == ((f & PPF_FLAG_ERANGE) != 0));
}

static void clear_flags(ppf_context_t *ctx) {
    mpfr_clear_flags();
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
}

static void random_operand(gmp_randstate_t rs, mpfr_ptr m, ppf_t *p,
                           ppf_context_t *ctx, mpfr_prec_t precision) {
    mpz_t z;
    unsigned bits = 1u + (unsigned)gmp_urandomm_ui(rs, precision + 31);
    long e = (long)gmp_urandomm_ui(rs, 1001) - 500;
    mpz_init(z);
    mpz_urandomb(z, rs, bits);
    if (mpz_sgn(z) == 0) mpz_set_ui(z, 1);
    if (gmp_urandomb_ui(rs, 1)) mpz_neg(z, z);
    (void)mpfr_set_z_2exp(m, z, e, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(p, m, ctx) == 0);
    mpz_clear(z);
}

static void check_result(const ppf_t *got, mpfr_srcptr ref,
                         int pt, int mt, ppf_context_t *ctx) {
    mpfr_t converted;
    mpfr_init2(converted, mpfr_get_prec(ref));
    export_ppf(converted, got);
    if (!same_value(converted, ref)) {
        mpfr_printf("reference=%Ra\nconverted=%Ra\n", ref, converted);
        exit(1);
    }
    CHECK(sign_of(pt) == sign_of(mt));
    compare_flags(ctx);
    mpfr_clear(converted);
}

static void comparison_tests(gmp_randstate_t rs) {
    ppf_context_t ctx;
    ppf_t a, b;
    mpfr_t ma, mb;
    mpz_t z;
    mpq_t q;
    mpf_t f;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&b, 53, &ctx) == 0);
    mpfr_inits2(53, ma, mb, (mpfr_ptr)0);
    mpz_init(z); mpq_init(q); mpf_init2(f, 160);
    for (int i = 0; i < 900; ++i) {
        mpfr_prec_t pa = 2 + gmp_urandomm_ui(rs, 350);
        mpfr_prec_t pb = 2 + gmp_urandomm_ui(rs, 350);
        mpfr_set_prec(ma, pa); mpfr_set_prec(mb, pb);
        random_operand(rs, ma, &a, &ctx, pa);
        random_operand(rs, mb, &b, &ctx, pb);
        clear_flags(&ctx);
        CHECK(sign_of(ppf_cmp(&a, &b, &ctx)) == sign_of(mpfr_cmp(ma, mb)));
        compare_flags(&ctx);
        clear_flags(&ctx);
        CHECK(sign_of(ppf_cmpabs(&a, &b, &ctx)) == sign_of(mpfr_cmpabs(ma, mb)));
        compare_flags(&ctx);
        CHECK(!!ppf_greater_p(&a, &b) == !!mpfr_greater_p(ma, mb));
        CHECK(!!ppf_greaterequal_p(&a, &b) == !!mpfr_greaterequal_p(ma, mb));
        CHECK(!!ppf_less_p(&a, &b) == !!mpfr_less_p(ma, mb));
        CHECK(!!ppf_lessequal_p(&a, &b) == !!mpfr_lessequal_p(ma, mb));
        CHECK(!!ppf_lessgreater_p(&a, &b) == !!mpfr_lessgreater_p(ma, mb));
        CHECK(!!ppf_unordered_p(&a, &b) == !!mpfr_unordered_p(ma, mb));
        CHECK(!!ppf_total_order_p(&a, &b) == !!mpfr_total_order_p(ma, mb));

        {
            unsigned long u = gmp_urandomb_ui(rs, 63);
            long s = (long)gmp_urandomb_ui(rs, 62);
            if (gmp_urandomb_ui(rs, 1)) s = -s;
            long e = (long)gmp_urandomm_ui(rs, 401) - 200;
            CHECK(sign_of(ppf_cmp_ui(&a, u, &ctx)) == sign_of(mpfr_cmp_ui(ma, u)));
            CHECK(sign_of(ppf_cmp_si(&a, s, &ctx)) == sign_of(mpfr_cmp_si(ma, s)));
            CHECK(sign_of(ppf_cmp_ui_2exp(&a, u, e, &ctx)) == sign_of(mpfr_cmp_ui_2exp(ma, u, e)));
            CHECK(sign_of(ppf_cmp_si_2exp(&a, s, e, &ctx)) == sign_of(mpfr_cmp_si_2exp(ma, s, e)));
            CHECK(sign_of(ppf_cmpabs_ui(&a, u, &ctx)) == sign_of(mpfr_cmpabs_ui(ma, u)));
        }
        mpz_urandomb(z, rs, 1 + gmp_urandomm_ui(rs, 280));
        if (gmp_urandomb_ui(rs, 1)) mpz_neg(z, z);
        CHECK(sign_of(ppf_cmp_z(&a, z, &ctx)) == sign_of(mpfr_cmp_z(ma, z)));
        {
            mpz_t den;
            mpz_init(den);
            mpz_urandomb(den, rs, 1 + gmp_urandomm_ui(rs, 180));
            if (mpz_sgn(den) == 0) mpz_set_ui(den, 1);
            mpq_set_num(q, z); mpq_set_den(q, den); mpq_canonicalize(q);
            CHECK(sign_of(ppf_cmp_q(&a, q, &ctx)) == sign_of(mpfr_cmp_q(ma, q)));
            mpf_set_q(f, q);
            CHECK(sign_of(ppf_cmp_f(&a, f, &ctx)) == sign_of(mpfr_cmp_f(ma, f)));
            mpz_clear(den);
        }
    }
    /* Signed special ordering. */
    ppf_set_nan(&a, -1); ppf_set_inf(&b, -1);
    mpfr_set_nan(ma); (void)mpfr_setsign(ma, ma, 1, MPFR_RNDN); mpfr_set_inf(mb, -1);
    CHECK(ppf_total_order_p(&a, &b) == mpfr_total_order_p(ma, mb));
    ppf_set_zero(&a, -1); ppf_set_zero(&b, 1);
    mpfr_set_zero(ma, -1); mpfr_set_zero(mb, 1);
    CHECK(ppf_total_order_p(&a, &b) == mpfr_total_order_p(ma, mb));

    mpf_clear(f); mpq_clear(q); mpz_clear(z);
    mpfr_clears(ma, mb, (mpfr_ptr)0);
    ppf_clear(&a); ppf_clear(&b); ppf_context_clear(&ctx);
}

static void rounding_tests(gmp_randstate_t rs) {
    ppf_context_t ctx;
    ppf_t x, got, iout, fout;
    mpfr_t mx, ref, refi, reff;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&iout, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&fout, 53, &ctx) == 0);
    mpfr_inits2(53, mx, ref, refi, reff, (mpfr_ptr)0);

    for (int iter = 0; iter < 1000; ++iter) {
        mpfr_prec_t px = 2 + gmp_urandomm_ui(rs, 400);
        mpfr_prec_t pr = 2 + gmp_urandomm_ui(rs, 220);
        mpfr_set_prec(mx, px); mpfr_set_prec(ref, pr);
        mpfr_set_prec(refi, pr); mpfr_set_prec(reff, pr);
        random_operand(rs, mx, &x, &ctx, px);
        CHECK(ppf_set_prec(&got, pr) == 0);
        CHECK(ppf_set_prec(&iout, pr) == 0);
        CHECK(ppf_set_prec(&fout, pr) == 0);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            ppf_rnd_t prnd = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
            int pt, mt;
            clear_flags(&ctx);
            mt = mpfr_rint(ref, mx, (mpfr_rnd_t)r);
            pt = ppf_rint(&got, &x, prnd, &ctx);
            check_result(&got, ref, pt, mt, &ctx);

#define CHECK_FIXED(MF, PF) do { \
            clear_flags(&ctx); mt = MF(ref, mx); pt = PF(&got, &x, &ctx); \
            check_result(&got, ref, pt, mt, &ctx); \
        } while (0)
            CHECK_FIXED(mpfr_ceil, ppf_ceil);
            CHECK_FIXED(mpfr_floor, ppf_floor);
            CHECK_FIXED(mpfr_round, ppf_round);
            CHECK_FIXED(mpfr_roundeven, ppf_roundeven);
            CHECK_FIXED(mpfr_trunc, ppf_trunc);
#undef CHECK_FIXED

#define CHECK_DOUBLE(MF, PF) do { \
            clear_flags(&ctx); mt = MF(ref, mx, (mpfr_rnd_t)r); \
            pt = PF(&got, &x, prnd, &ctx); \
            check_result(&got, ref, pt, mt, &ctx); \
        } while (0)
            CHECK_DOUBLE(mpfr_rint_ceil, ppf_rint_ceil);
            CHECK_DOUBLE(mpfr_rint_floor, ppf_rint_floor);
            CHECK_DOUBLE(mpfr_rint_round, ppf_rint_round);
            CHECK_DOUBLE(mpfr_rint_roundeven, ppf_rint_roundeven);
            CHECK_DOUBLE(mpfr_rint_trunc, ppf_rint_trunc);
#undef CHECK_DOUBLE

            clear_flags(&ctx);
            mt = mpfr_frac(ref, mx, (mpfr_rnd_t)r);
            pt = ppf_frac(&got, &x, prnd, &ctx);
            check_result(&got, ref, pt, mt, &ctx);

            clear_flags(&ctx);
            mt = mpfr_modf(refi, reff, mx, (mpfr_rnd_t)r);
            pt = ppf_modf(&iout, &fout, &x, prnd, &ctx);
            {
                mpfr_t gi, gf;
                mpfr_init2(gi, pr); mpfr_init2(gf, pr);
                export_ppf(gi, &iout); export_ppf(gf, &fout);
                CHECK(same_value(gi, refi)); CHECK(same_value(gf, reff));
                CHECK(pt == mt);
                compare_flags(&ctx);
                mpfr_clear(gi); mpfr_clear(gf);
            }
        }
        CHECK(ppf_integer_p(&x) == mpfr_integer_p(mx));
        CHECK(ppf_min_prec(&x) == (ppf_prec_t)mpfr_min_prec(mx));
    }

    mpfr_clears(mx, ref, refi, reff, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&got); ppf_clear(&iout); ppf_clear(&fout);
    ppf_context_clear(&ctx);
}

static void neighbor_and_extrema_tests(gmp_randstate_t rs) {
    ppf_context_t ctx;
    ppf_t x, y, got;
    mpfr_t mx, my, ref, out;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&y, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, mx, my, ref, out, (mpfr_ptr)0);
    CHECK(mpfr_set_emin((mpfr_exp_t)ctx.emin) == 0);
    CHECK(mpfr_set_emax((mpfr_exp_t)ctx.emax) == 0);

    for (int iter = 0; iter < 600; ++iter) {
        mpfr_prec_t p = 2 + gmp_urandomm_ui(rs, 250);
        mpfr_set_prec(mx, p); mpfr_set_prec(my, p);
        mpfr_set_prec(ref, p); mpfr_set_prec(out, p);
        random_operand(rs, mx, &x, &ctx, p);
        random_operand(rs, my, &y, &ctx, p);
        CHECK(ppf_set_prec(&got, p) == 0);

        (void)ppf_set(&got, &x, PPF_RNDN, &ctx);
        mpfr_set(ref, mx, MPFR_RNDN);
        ppf_nextabove(&got, &ctx); mpfr_nextabove(ref);
        export_ppf(out, &got); CHECK(same_value(out, ref));

        (void)ppf_set(&got, &x, PPF_RNDN, &ctx);
        mpfr_set(ref, mx, MPFR_RNDN);
        ppf_nextbelow(&got, &ctx); mpfr_nextbelow(ref);
        export_ppf(out, &got); CHECK(same_value(out, ref));

        (void)ppf_set(&got, &x, PPF_RNDN, &ctx);
        mpfr_set(ref, mx, MPFR_RNDN);
        ppf_nexttoward(&got, &y, &ctx); mpfr_nexttoward(ref, my);
        export_ppf(out, &got); CHECK(same_value(out, ref));

        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            int pt, mt;
            clear_flags(&ctx);
            mt = mpfr_min(ref, mx, my, (mpfr_rnd_t)r);
            pt = ppf_min(&got, &x, &y, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
            check_result(&got, ref, pt, mt, &ctx);
            clear_flags(&ctx);
            mt = mpfr_max(ref, mx, my, (mpfr_rnd_t)r);
            pt = ppf_max(&got, &x, &y, ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
            check_result(&got, ref, pt, mt, &ctx);
        }
    }
    mpfr_clears(mx, my, ref, out, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&y); ppf_clear(&got); ppf_context_clear(&ctx);
}

static void remainder_tests(gmp_randstate_t rs) {
    ppf_context_t ctx;
    ppf_t x, y, got;
    mpfr_t mx, my, ref, out;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&x, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&y, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, mx, my, ref, out, (mpfr_ptr)0);

    for (int iter = 0; iter < 1000; ++iter) {
        mpfr_prec_t px = 2 + gmp_urandomm_ui(rs, 380);
        mpfr_prec_t py = 2 + gmp_urandomm_ui(rs, 300);
        mpfr_prec_t pr = 2 + gmp_urandomm_ui(rs, 260);
        mpfr_set_prec(mx, px); mpfr_set_prec(my, py);
        mpfr_set_prec(ref, pr); mpfr_set_prec(out, pr);
        random_operand(rs, mx, &x, &ctx, px);
        random_operand(rs, my, &y, &ctx, py);
        if (mpfr_zero_p(my)) { mpfr_set_ui(my, 3, MPFR_RNDN); CHECK(ppf_import_mpfr_exact(&y, my, &ctx) == 0); }
        CHECK(ppf_set_prec(&got, pr) == 0);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            ppf_rnd_t prnd = ppf_rounding_from_mpfr((mpfr_rnd_t)r);
            int pt, mt;
            long pq = 0, mq = 0;
            clear_flags(&ctx);
            mt = mpfr_fmod(ref, mx, my, (mpfr_rnd_t)r);
            pt = ppf_fmod(&got, &x, &y, prnd, &ctx);
            check_result(&got, ref, pt, mt, &ctx);

            clear_flags(&ctx);
            mt = mpfr_fmodquo(ref, &mq, mx, my, (mpfr_rnd_t)r);
            pt = ppf_fmodquo(&got, &pq, &x, &y, prnd, &ctx);
            check_result(&got, ref, pt, mt, &ctx);
            CHECK(pq == mq);

            clear_flags(&ctx);
            mt = mpfr_remainder(ref, mx, my, (mpfr_rnd_t)r);
            pt = ppf_remainder(&got, &x, &y, prnd, &ctx);
            check_result(&got, ref, pt, mt, &ctx);

            clear_flags(&ctx);
            mt = mpfr_remquo(ref, &mq, mx, my, (mpfr_rnd_t)r);
            pt = ppf_remquo(&got, &pq, &x, &y, prnd, &ctx);
            check_result(&got, ref, pt, mt, &ctx);
            CHECK(pq == mq);
        }
    }

    /* Very large phase gap: must avoid materializing the quotient. */
    mpfr_set_prec(mx, 127); mpfr_set_prec(my, 83); mpfr_set_prec(ref, 97); mpfr_set_prec(out, 97);
    mpfr_set_ui_2exp(mx, 17, 1000000, MPFR_RNDN);
    mpfr_set_ui(my, 19, MPFR_RNDN);
    CHECK(ppf_import_mpfr_exact(&x, mx, &ctx) == 0);
    CHECK(ppf_import_mpfr_exact(&y, my, &ctx) == 0);
    CHECK(ppf_set_prec(&got, 97) == 0);
    {
        long pq = 0, mq = 0;
        int pt = ppf_fmodquo(&got, &pq, &x, &y, PPF_RNDN, &ctx);
        int mt = mpfr_fmodquo(ref, &mq, mx, my, MPFR_RNDN);
        export_ppf(out, &got); CHECK(same_value(out, ref));
        CHECK(sign_of(pt) == sign_of(mt)); CHECK(pq == mq);
    }

    /* Special flags. */
    ppf_set_inf(&x, 1); ppf_set_ui(&y, 3, PPF_RNDN, &ctx);
    mpfr_set_inf(mx, 1); mpfr_set_ui(my, 3, MPFR_RNDN);
    clear_flags(&ctx);
    (void)ppf_fmod(&got, &x, &y, PPF_RNDN, &ctx);
    (void)mpfr_fmod(ref, mx, my, MPFR_RNDN);
    CHECK(ppf_nan_p(&got)); compare_flags(&ctx);

    mpfr_clears(mx, my, ref, out, (mpfr_ptr)0);
    ppf_clear(&x); ppf_clear(&y); ppf_clear(&got); ppf_context_clear(&ctx);
}

int main(void) {
    gmp_randstate_t rs;
    gmp_randinit_mt(rs);
    gmp_randseed_ui(rs, 0x5554494c495459UL);
    comparison_tests(rs);
    rounding_tests(rs);
    neighbor_and_extrema_tests(rs);
    remainder_tests(rs);
    gmp_randclear(rs);
    puts("INDEPENDENT UTILITY AND REMAINDER TESTS PASSED");
    return 0;
}
