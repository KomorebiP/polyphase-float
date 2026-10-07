/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float.h"
#include "polyphase_float_mpfr.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static int sign_of(int value) { return value > 0 ? 1 : (value < 0 ? -1 : 0); }

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_nan_p(a) || mpfr_nan_p(b))
        return mpfr_nan_p(a) && mpfr_nan_p(b);
    if (mpfr_inf_p(a) || mpfr_inf_p(b))
        return mpfr_inf_p(a) && mpfr_inf_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    if (!mpfr_equal_p(a, b)) return 0;
    if (mpfr_zero_p(a)) return mpfr_signbit(a) == mpfr_signbit(b);
    return 1;
}

static void compare_flags(ppf_context_t *ctx) {
    CHECK(((ppf_context_flags(ctx) & PPF_FLAG_UNDERFLOW) != 0) ==
          (mpfr_underflow_p() != 0));
    CHECK(((ppf_context_flags(ctx) & PPF_FLAG_OVERFLOW) != 0) ==
          (mpfr_overflow_p() != 0));
    CHECK(((ppf_context_flags(ctx) & PPF_FLAG_DIVBY0) != 0) ==
          (mpfr_divby0_p() != 0));
    CHECK(((ppf_context_flags(ctx) & PPF_FLAG_NAN) != 0) ==
          (mpfr_nanflag_p() != 0));
    CHECK(((ppf_context_flags(ctx) & PPF_FLAG_INEXACT) != 0) ==
          (mpfr_inexflag_p() != 0));
    CHECK(((ppf_context_flags(ctx) & PPF_FLAG_ERANGE) != 0) ==
          (mpfr_erangeflag_p() != 0));
}

static void random_regular(gmp_randstate_t rs, mpfr_ptr x, int allow_negative) {
    mpfr_urandomb(x, rs);
    if (mpfr_zero_p(x)) mpfr_set_ui(x, 1, MPFR_RNDN);
    mpfr_mul_2si(x, x, (long)gmp_urandomm_ui(rs, 401) - 200, MPFR_RNDN);
    if (allow_negative && gmp_urandomm_ui(rs, 2)) mpfr_neg(x, x, MPFR_RNDN);
}

/* mpfr_root is deprecated by the official MPFR header. Its only semantic
   difference from mpfr_rootn_ui is that a nonzero degree preserves the sign
   of a zero operand. Build the reference result without calling the
   deprecated declaration so strict validation builds remain warning-free. */
static int legacy_root_reference(mpfr_ptr rop, mpfr_srcptr op,
                                 unsigned long degree, mpfr_rnd_t rnd) {
    if (degree != 0 && mpfr_zero_p(op))
        return mpfr_set(rop, op, rnd);
    return mpfr_rootn_ui(rop, op, degree, rnd);
}

static void compare_one(ppf_t *got, ppf_t *a, mpfr_ptr mr, mpfr_ptr ma,
                        mpfr_ptr out, ppf_context_t *ctx,
                        int op, long degree, mpfr_rnd_t rnd) {
    int mt = 0, pt = 0;
    mpfr_clear_flags();
    ppf_context_flags_clear(ctx, PPF_FLAGS_ALL);
    switch (op) {
        case 0:
            mt = mpfr_sqrt(mr, ma, rnd);
            pt = ppf_sqrt(got, a, ppf_rounding_from_mpfr(rnd), ctx);
            break;
        case 1:
            mt = mpfr_rec_sqrt(mr, ma, rnd);
            pt = ppf_rec_sqrt(got, a, ppf_rounding_from_mpfr(rnd), ctx);
            break;
        case 2:
            mt = mpfr_cbrt(mr, ma, rnd);
            pt = ppf_cbrt(got, a, ppf_rounding_from_mpfr(rnd), ctx);
            break;
        case 3:
            mt = mpfr_rootn_ui(mr, ma, (unsigned long)degree, rnd);
            pt = ppf_rootn_ui(got, a, (unsigned long)degree,
                              ppf_rounding_from_mpfr(rnd), ctx);
            break;
        case 4:
            mt = mpfr_rootn_si(mr, ma, degree, rnd);
            pt = ppf_rootn_si(got, a, degree,
                              ppf_rounding_from_mpfr(rnd), ctx);
            break;
        case 5:
            mt = legacy_root_reference(mr, ma, (unsigned long)degree, rnd);
            pt = ppf_root(got, a, (unsigned long)degree,
                          ppf_rounding_from_mpfr(rnd), ctx);
            break;
        default:
            CHECK(0);
    }
    CHECK(pt != PPF_TERNARY_ERROR);
    (void)ppf_get_mpfr(out, got, MPFR_RNDN);
    if (!same_value(mr, out)) {
        fprintf(stderr, "root mismatch op=%d degree=%ld rnd=%d\n", op, degree, (int)rnd);
        mpfr_printf("input=%Ra\nmpfr=%Ra\nppf =%Ra\n", ma, mr, out);
        exit(1);
    }
    CHECK(sign_of(mt) == sign_of(pt));
    compare_flags(ctx);
}

static void randomized_roots(void) {
    gmp_randstate_t rs;
    ppf_context_t ctx;
    ppf_t a, got;
    mpfr_t ma, mr, out;
    const unsigned long degrees[] = {1,2,3,4,5,7,11,17};
    const long signed_degrees[] = {-7,-5,-3,-2,-1,1,2,3,5,9};

    gmp_randinit_mt(rs);
    gmp_randseed_ui(rs, 0x524f4f5453UL);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&a, 53, &ctx) == 0);
    CHECK(ppf_init2_ctx(&got, 53, &ctx) == 0);
    mpfr_inits2(53, ma, mr, out, (mpfr_ptr)0);

    for (int iter = 0; iter < 180; ++iter) {
        mpfr_prec_t pa = 20 + gmp_urandomm_ui(rs, 220);
        mpfr_prec_t pr = 2 + gmp_urandomm_ui(rs, 180);
        mpfr_set_prec(ma, pa);
        mpfr_set_prec(mr, pr);
        mpfr_set_prec(out, pr);
        CHECK(ppf_set_prec(&a, pa) == 0);
        CHECK(ppf_set_prec(&got, pr) == 0);
        random_regular(rs, ma, 1);
        CHECK(ppf_import_mpfr_exact(&a, ma, &ctx) == 0);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            compare_one(&got, &a, mr, ma, out, &ctx, 2, 3, (mpfr_rnd_t)r);
            for (size_t j = 0; j < sizeof(degrees)/sizeof(degrees[0]); ++j)
                compare_one(&got, &a, mr, ma, out, &ctx, 3,
                            (long)degrees[j], (mpfr_rnd_t)r);
            for (size_t j = 0; j < sizeof(signed_degrees)/sizeof(signed_degrees[0]); ++j)
                compare_one(&got, &a, mr, ma, out, &ctx, 4,
                            signed_degrees[j], (mpfr_rnd_t)r);
        }
    }

    mpfr_clears(ma, mr, out, (mpfr_ptr)0);
    ppf_clear(&a); ppf_clear(&got); ppf_context_clear(&ctx);
    gmp_randclear(rs);
}

static void compare_hypot(void) {
    gmp_randstate_t rs;
    ppf_context_t ctx;
    ppf_t a,b,got;
    mpfr_t ma,mb,mr,out;
    gmp_randinit_mt(rs); gmp_randseed_ui(rs, 0x4859504f54UL);
    CHECK(ppf_context_init(&ctx)==0);
    CHECK(ppf_init2_ctx(&a,53,&ctx)==0);
    CHECK(ppf_init2_ctx(&b,53,&ctx)==0);
    CHECK(ppf_init2_ctx(&got,53,&ctx)==0);
    mpfr_inits2(53,ma,mb,mr,out,(mpfr_ptr)0);
    for(int iter=0;iter<160;++iter){
        mpfr_prec_t pa=20+gmp_urandomm_ui(rs,200);
        mpfr_prec_t pb=20+gmp_urandomm_ui(rs,200);
        mpfr_prec_t pr=2+gmp_urandomm_ui(rs,180);
        mpfr_set_prec(ma,pa);mpfr_set_prec(mb,pb);mpfr_set_prec(mr,pr);mpfr_set_prec(out,pr);
        CHECK(ppf_set_prec(&a,pa)==0);CHECK(ppf_set_prec(&b,pb)==0);CHECK(ppf_set_prec(&got,pr)==0);
        random_regular(rs,ma,1);random_regular(rs,mb,1);
        CHECK(ppf_import_mpfr_exact(&a,ma,&ctx)==0);CHECK(ppf_import_mpfr_exact(&b,mb,&ctx)==0);
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){
            int mt,pt;mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            mt=mpfr_hypot(mr,ma,mb,(mpfr_rnd_t)r);
            pt=ppf_hypot(&got,&a,&b,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&got,MPFR_RNDN);
            if(!same_value(mr,out)){mpfr_printf("hypot mismatch %Ra %Ra -> %Ra %Ra\n",ma,mb,mr,out);exit(1);}
            CHECK(sign_of(mt)==sign_of(pt));compare_flags(&ctx);
        }
    }
    mpfr_clears(ma,mb,mr,out,(mpfr_ptr)0);ppf_clear(&a);ppf_clear(&b);ppf_clear(&got);ppf_context_clear(&ctx);gmp_randclear(rs);
}

static void large_newton_recsqrt(void) {
    gmp_randstate_t rs;
    ppf_context_t ctx;
    ppf_t input, one, threaded, alias;
    mpfr_t minput, reference, converted;
    mpz_t z;
    const mpfr_prec_t precisions[] = {32768, 65537, 100000};

    gmp_randinit_mt(rs);
    gmp_randseed_ui(rs, 0x4e4557544f4eUL);
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_init2_ctx(&input, 32768, &ctx) == 0);
    CHECK(ppf_init2_ctx(&one, 32768, &ctx) == 0);
    CHECK(ppf_init2_ctx(&threaded, 32768, &ctx) == 0);
    CHECK(ppf_init2_ctx(&alias, 32768, &ctx) == 0);
    mpfr_inits2(32768, minput, reference, converted, (mpfr_ptr)0);
    mpz_init(z);

    for (size_t pi = 0; pi < sizeof(precisions)/sizeof(precisions[0]); ++pi) {
        mpfr_prec_t p = precisions[pi];
        mpfr_set_prec(minput, p); mpfr_set_prec(reference, p);
        mpfr_set_prec(converted, p);
        CHECK(ppf_set_prec(&input, p) == 0);
        CHECK(ppf_set_prec(&one, p) == 0);
        CHECK(ppf_set_prec(&threaded, p) == 0);
        CHECK(ppf_set_prec(&alias, p) == 0);
        for (int iter = 0; iter < 6; ++iter) {
            mpz_urandomb(z, rs, (mp_bitcnt_t)p);
            mpz_setbit(z, (mp_bitcnt_t)p - 1u);
            mpfr_set_z(minput, z, MPFR_RNDN);
            mpfr_mul_2si(minput, minput,
                         (long)gmp_urandomm_ui(rs, 401) - 200,
                         MPFR_RNDN);
            CHECK(ppf_import_mpfr_exact(&input, minput, &ctx) == 0);
            for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
                int mt, pt;
                mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
                mt = mpfr_rec_sqrt(reference, minput, (mpfr_rnd_t)r);
                pt = ppf_rec_sqrt(&one, &input,
                                  ppf_rounding_from_mpfr((mpfr_rnd_t)r), &ctx);
                CHECK(pt != PPF_TERNARY_ERROR);
                ppf_get_mpfr(converted, &one, MPFR_RNDN);
                CHECK(same_value(reference, converted));
                CHECK(sign_of(mt) == sign_of(pt));
                compare_flags(&ctx);

                mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
                mt = mpfr_rec_sqrt(reference, minput, (mpfr_rnd_t)r);
                pt = ppf_rec_sqrt_threads(
                    &threaded, &input,
                    ppf_rounding_from_mpfr((mpfr_rnd_t)r), 3, &ctx);
                CHECK(pt != PPF_TERNARY_ERROR);
                ppf_get_mpfr(converted, &threaded, MPFR_RNDN);
                CHECK(same_value(reference, converted));
                CHECK(sign_of(mt) == sign_of(pt));
                compare_flags(&ctx);
            }
            CHECK(ppf_set(&alias, &input, PPF_RNDN, &ctx) != PPF_TERNARY_ERROR);
            CHECK(ppf_rec_sqrt_threads(&alias, &alias, PPF_RNDN, 3, &ctx) !=
                  PPF_TERNARY_ERROR);
            mpfr_rec_sqrt(reference, minput, MPFR_RNDN);
            ppf_get_mpfr(converted, &alias, MPFR_RNDN);
            CHECK(same_value(reference, converted));
        }
    }

    /* An odd stored exponent can make the final p-bit result coincide with
       the center of a much narrower work interval.  The value is settled by
       endpoint rounding, while the ternary direction is proved exactly from
       result^2 * input relative to one. */
    {
        const mpfr_prec_t p = 100000;
        mpfr_set_prec(minput, p); mpfr_set_prec(reference, p);
        mpfr_set_prec(converted, p);
        CHECK(ppf_set_prec(&input, p) == 0);
        CHECK(ppf_set_prec(&threaded, p) == 0);
        mpz_set_ui(z, 1); mpz_mul_2exp(z, z, (mp_bitcnt_t)p - 1u);
        for (unsigned k = 0; k < 47; ++k) {
            mp_bitcnt_t pos = (mp_bitcnt_t)(((uint64_t)(k + 3) *
                ((uint64_t)p / 54u + 1u) + 28u) % ((uint64_t)p - 1u));
            mpz_setbit(z, pos);
        }
        mpfr_set_z(minput, z, MPFR_RNDN);
        mpfr_mul_2si(minput, minput, -7L, MPFR_RNDN);
        CHECK(ppf_import_mpfr_exact(&input, minput, &ctx) == 0);
        CHECK((input.exponent & 1) != 0);
        for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
            int mt, pt;
            mpfr_clear_flags(); ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
            mt = mpfr_rec_sqrt(reference, minput, (mpfr_rnd_t)r);
            pt = ppf_rec_sqrt_threads(&threaded, &input,
                ppf_rounding_from_mpfr((mpfr_rnd_t)r), 3, &ctx);
            CHECK(pt != PPF_TERNARY_ERROR);
            ppf_get_mpfr(converted, &threaded, MPFR_RNDN);
            CHECK(same_value(reference, converted));
            CHECK(sign_of(mt) == sign_of(pt));
            compare_flags(&ctx);
        }
    }

    /* One million bits forces the large-product path without making the
       ordinary test suite prohibitively long. */
    {
        const mpfr_prec_t p = 1000000;
        mpfr_set_prec(minput, p); mpfr_set_prec(reference, p);
        mpfr_set_prec(converted, p);
        CHECK(ppf_set_prec(&input, p) == 0);
        CHECK(ppf_set_prec(&threaded, p) == 0);
        mpz_urandomb(z, rs, (mp_bitcnt_t)p);
        mpz_setbit(z, (mp_bitcnt_t)p - 1u);
        mpfr_set_z(minput, z, MPFR_RNDN);
        CHECK(ppf_import_mpfr_exact(&input, minput, &ctx) == 0);
        CHECK(ppf_rec_sqrt_threads(&threaded, &input, PPF_RNDN, 3, &ctx) !=
              PPF_TERNARY_ERROR);
        mpfr_rec_sqrt(reference, minput, MPFR_RNDN);
        ppf_get_mpfr(converted, &threaded, MPFR_RNDN);
        CHECK(same_value(reference, converted));
    }

    mpz_clear(z);
    mpfr_clears(minput, reference, converted, (mpfr_ptr)0);
    ppf_clear(&input); ppf_clear(&one); ppf_clear(&threaded); ppf_clear(&alias);
    ppf_context_clear(&ctx); gmp_randclear(rs);
}

static void special_cases(void) {
    ppf_context_t ctx; ppf_t a, got; mpfr_t ma, mr, out;
    CHECK(ppf_context_init(&ctx)==0);CHECK(ppf_init2_ctx(&a,80,&ctx)==0);CHECK(ppf_init2_ctx(&got,80,&ctx)==0);
    mpfr_inits2(80,ma,mr,out,(mpfr_ptr)0);
    for(int k=0;k<7;++k){
        if(k==0){mpfr_set_zero(ma,1);ppf_set_zero(&a,1);}
        else if(k==1){mpfr_set_zero(ma,-1);ppf_set_zero(&a,-1);}
        else if(k==2){mpfr_set_inf(ma,1);ppf_set_inf(&a,1);}
        else if(k==3){mpfr_set_inf(ma,-1);ppf_set_inf(&a,-1);}
        else if(k==4){mpfr_set_nan(ma);ppf_set_nan(&a,1);}
        else if(k==5){mpfr_set_si(ma,-8,MPFR_RNDN);ppf_set_si(&a,-8,PPF_RNDN,&ctx);}
        else {mpfr_set_ui(ma,16,MPFR_RNDN);ppf_set_ui(&a,16,PPF_RNDN,&ctx);}
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){
            compare_one(&got,&a,mr,ma,out,&ctx,0,2,(mpfr_rnd_t)r);
            compare_one(&got,&a,mr,ma,out,&ctx,1,-2,(mpfr_rnd_t)r);
            compare_one(&got,&a,mr,ma,out,&ctx,3,4,(mpfr_rnd_t)r);
            compare_one(&got,&a,mr,ma,out,&ctx,4,-3,(mpfr_rnd_t)r);
            compare_one(&got,&a,mr,ma,out,&ctx,5,2,(mpfr_rnd_t)r);
        }
    }
    for(unsigned long n=0;n<100;++n){
        for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){int mt,pt;mpfr_clear_flags();ppf_context_flags_clear(&ctx,PPF_FLAGS_ALL);
            mt=mpfr_sqrt_ui(mr,n,(mpfr_rnd_t)r);pt=ppf_sqrt_ui(&got,n,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&ctx);
            CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&got,MPFR_RNDN);CHECK(same_value(mr,out));CHECK(sign_of(mt)==sign_of(pt));compare_flags(&ctx);}
    }
    mpfr_clears(ma,mr,out,(mpfr_ptr)0);ppf_clear(&a);ppf_clear(&got);ppf_context_clear(&ctx);
}

int main(void) {
    randomized_roots();
    compare_hypot();
    large_newton_recsqrt();
    special_cases();
    puts("INDEPENDENT ROOT FAMILY TESTS PASSED");
    return 0;
}
