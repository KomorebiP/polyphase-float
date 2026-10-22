/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "polyphase_float_mpfr.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"CHECK FAILED %s:%d: %s\n",__FILE__,__LINE__,#c);exit(1);}}while(0)
static int sgn(int x){return(x>0)-(x<0);}static int same(mpfr_srcptr a,mpfr_srcptr b){if(mpfr_nan_p(a)||mpfr_nan_p(b))return mpfr_nan_p(a)&&mpfr_nan_p(b);if(mpfr_zero_p(a)||mpfr_zero_p(b))return mpfr_zero_p(a)&&mpfr_zero_p(b)&&mpfr_signbit(a)==mpfr_signbit(b);return mpfr_equal_p(a,b)!=0;}
static void flags(ppf_context_t*c){ppf_flags_t f=ppf_context_flags(c);CHECK((mpfr_inexflag_p()!=0)==((f&PPF_FLAG_INEXACT)!=0));CHECK((mpfr_underflow_p()!=0)==((f&PPF_FLAG_UNDERFLOW)!=0));CHECK((mpfr_overflow_p()!=0)==((f&PPF_FLAG_OVERFLOW)!=0));}
static void set_exact(mpfr_ptr m,ppf_t*p,long z,long e,ppf_context_t*c){mpz_t q;mpz_init_set_si(q,z);(void)mpfr_set_z_2exp(m,q,e,MPFR_RNDN);(void)ppf_set_z_2exp(p,q,e,PPF_RNDN,c);mpz_clear(q);}
/* A nonzero division remainder can carry the rounded two-bit significand
   across emax. Exercise that fallback with separate and aliased outputs. */
static void division_carry_boundary(void) {
    const struct { long az, ae, bz, be; int alias; } rows[] = {
        {7, 0, 15, -10, 0}, {3, 0, 7, -10, 1},
        {11, -3, 3, -10, 2}, {7, 0, 1, -6, 0}
    };
    ppf_context_t c;
    CHECK(ppf_context_init(&c) == 0);
    CHECK(ppf_context_set_range(&c, -9, 9) == 0);
    CHECK(mpfr_set_emin(-9) == 0);
    CHECK(mpfr_set_emax(9) == 0);
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        ppf_t a, b, result;
        mpfr_t ma, mb, expected, actual;
        int ap = rows[i].alias == 1 ? 2 : 8;
        int bp = rows[i].alias == 2 ? 2 : 8;
        CHECK(ppf_init2_ctx(&a, (ppf_prec_t)ap, &c) == 0);
        CHECK(ppf_init2_ctx(&b, (ppf_prec_t)bp, &c) == 0);
        CHECK(ppf_init2_ctx(&result, 2, &c) == 0);
        mpfr_init2(ma, ap); mpfr_init2(mb, bp);
        mpfr_init2(expected, 2); mpfr_init2(actual, 2);
        for (int sign = -1; sign <= 1; sign += 2) {
            for (int r = MPFR_RNDN; r <= MPFR_RNDA; ++r) {
                ppf_t *destination = rows[i].alias == 1 ? &a :
                                     rows[i].alias == 2 ? &b : &result;
                set_exact(ma, &a, sign * rows[i].az, rows[i].ae, &c);
                set_exact(mb, &b, rows[i].bz, rows[i].be, &c);
                mpfr_clear_flags();
                ppf_context_flags_clear(&c, PPF_FLAGS_ALL);
                int mt = mpfr_div(expected, ma, mb, (mpfr_rnd_t)r);
                int pt = ppf_div(destination, &a, &b,
                                 ppf_rounding_from_mpfr((mpfr_rnd_t)r), &c);
                CHECK(pt != PPF_TERNARY_ERROR);
                CHECK(ppf_validate(destination));
                flags(&c);
                CHECK((ppf_context_flags(&c) & PPF_FLAG_ERANGE) == 0);
                CHECK(sgn(mt) == sgn(pt));
                CHECK(ppf_get_mpfr(actual, destination, MPFR_RNDN) == 0);
                CHECK(same(expected, actual));
            }
        }
        mpfr_clears(ma, mb, expected, actual, (mpfr_ptr)0);
        ppf_clear(&a); ppf_clear(&b); ppf_clear(&result);
    }
    ppf_context_clear(&c);
}
int main(void){mpfr_exp_t oe=mpfr_get_emin(),ox=mpfr_get_emax();ppf_context_t c;ppf_t a,b,g;mpfr_t ma,mb,mr,out;CHECK(mpfr_set_emin(-4)==0);CHECK(mpfr_set_emax(4)==0);CHECK(ppf_context_init(&c)==0);CHECK(ppf_context_set_range(&c,-4,4)==0);CHECK(ppf_init2_ctx(&a,8,&c)==0);CHECK(ppf_init2_ctx(&b,8,&c)==0);CHECK(ppf_init2_ctx(&g,8,&c)==0);mpfr_inits2(8,ma,mb,mr,out,(mpfr_ptr)0);
    struct row{long az,ae,bz,be;int op;}rows[]={{7,0,7,0,0},{1,-4,1,-4,0},{7,0,7,0,1},{1,-4,7,0,1},{1,-4,7,0,2},{7,0,1,-4,2}};
    for(size_t i=0;i<sizeof(rows)/sizeof(rows[0]);++i){set_exact(ma,&a,rows[i].az,rows[i].ae,&c);set_exact(mb,&b,rows[i].bz,rows[i].be,&c);for(int r=MPFR_RNDN;r<=MPFR_RNDA;++r){int mt,pt;mpfr_clear_flags();ppf_context_flags_clear(&c,PPF_FLAGS_ALL);if(rows[i].op==0){mt=mpfr_mul(mr,ma,mb,(mpfr_rnd_t)r);pt=ppf_mul(&g,&a,&b,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&c);}else if(rows[i].op==1){mt=mpfr_add(mr,ma,mb,(mpfr_rnd_t)r);pt=ppf_add(&g,&a,&b,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&c);}else{mt=mpfr_div(mr,ma,mb,(mpfr_rnd_t)r);pt=ppf_div(&g,&a,&b,ppf_rounding_from_mpfr((mpfr_rnd_t)r),&c);}CHECK(pt!=PPF_TERNARY_ERROR);ppf_get_mpfr(out,&g,MPFR_RNDN);if(!same(mr,out)){mpfr_printf("range mismatch row=%zu rnd=%d ref=%Ra got=%Ra\n",i,r,mr,out);exit(1);}CHECK(sgn(mt)==sgn(pt));flags(&c);}}
    mpfr_clears(ma,mb,mr,out,(mpfr_ptr)0);ppf_clear(&a);ppf_clear(&b);ppf_clear(&g);ppf_context_clear(&c);division_carry_boundary();CHECK(mpfr_set_emin(oe)==0);CHECK(mpfr_set_emax(ox)==0);puts("ARITHMETIC RANGE TESTS PASSED");return 0;}
