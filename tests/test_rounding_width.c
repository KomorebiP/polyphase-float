/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

/* Compile the private rounding helper from its production source.  A public
   ppf_set_z_2exp witness needs over 512 MiB of integer storage on LP64; calling
   the actual helper lets this width regression run with small GMP integers.
   Link the static core: this translation unit supplies its round.c symbols,
   so the archive's duplicate round.c member is not selected.  No test hook or
   additional exported library symbol is needed. */
#include "../src/round.c"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); \
} } while (0)

static void interval_range_flags(void) {
    ppf_context_t ctx;
    ppf_t value;
    mpz_t lower, upper;
    int ternary = 99;
    CHECK(ppf_context_init(&ctx) == 0);
    CHECK(ppf_context_set_range(&ctx, 0, 10) == 0);
    CHECK(ppf_init2_ctx(&value, 2, &ctx) == 0);
    CHECK(ppf_set_si(&value, -1, PPF_RNDN, &ctx) == 0);
    mpz_inits(lower, upper, NULL);
    mpz_set_ui(lower, 19);
    mpz_set_ui(upper, 31);
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    ppf_context_flags_set(&ctx, PPF_FLAG_ERANGE);
    /* Both endpoints round upward to 1/2, but only the lower underflows.
       The interval therefore cannot certify the exact result's flags. */
    CHECK(ppf_set_dyadic_interval(&value, lower, upper, -6,
              PPF_RNDN, &ternary, &ctx) == PPF_INTERVAL_UNDECIDED);
    CHECK(ternary == 99);
    CHECK(ctx.flags == PPF_FLAG_ERANGE);
    CHECK(value.sign == -1 && value.exponent == 1 && value.limbs[0] == 2);
    CHECK(ppf_set_dyadic_interval(&value, upper, upper, -6,
              PPF_RNDN, &ternary, &ctx) == PPF_INTERVAL_DECIDED);
    CHECK(ternary == 1);
    CHECK(ctx.flags == (PPF_FLAG_ERANGE | PPF_FLAG_INEXACT));
    ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
    CHECK(ppf_set_dyadic_interval(&value, lower, lower, -6,
              PPF_RNDN, &ternary, &ctx) == PPF_INTERVAL_DECIDED);
    CHECK(ternary == 1);
    CHECK(ctx.flags == (PPF_FLAG_UNDERFLOW | PPF_FLAG_INEXACT));
    mpz_clears(lower, upper, NULL);
    ppf_clear(&value);
    ppf_context_clear(&ctx);
}

int main(void) {
    mpz_t q, r;
    unsigned long checks = 0;
    mpz_inits(q, r, NULL);
#if ULONG_MAX > UINT_MAX
    /* Before the fix, this shift narrowed to 2 and incorrectly rounded up.
       The exact comparison is 3 < 2^(shift-1), independent of the quotient. */
    volatile mp_bitcnt_t wide_shift = (mp_bitcnt_t)UINT_MAX + 3UL;
    mpz_set_ui(q, 3);
    mpz_set_ui(r, 3);
    CHECK(rounded_increment(PPF_RNDN, 1, q, r, wide_shift) == 0);
    ++checks;
    CHECK(rounded_increment(PPF_RNDN, -1, q, r, ULONG_MAX) == 0);
    ++checks;
#endif
    for (unsigned shift = 1; shift <= 10; ++shift) {
        unsigned denominator = 1u << shift;
        for (unsigned quotient = 0; quotient < 4; ++quotient) {
            mpz_set_ui(q, quotient);
            for (unsigned remainder = 0; remainder < denominator; ++remainder) {
                mpz_set_ui(r, remainder);
                for (int sign = -1; sign <= 1; sign += 2) {
                    for (int mode = PPF_RNDN; mode <= PPF_RNDF; ++mode) {
                        int expected = 0;
                        if (remainder) {
                            switch (mode) {
                                case PPF_RNDN:
                                    expected = 2u * remainder > denominator ||
                                        (2u * remainder == denominator &&
                                         (quotient & 1u));
                                    break;
                                case PPF_RNDU: expected = sign > 0; break;
                                case PPF_RNDD: expected = sign < 0; break;
                                case PPF_RNDA: expected = 1; break;
                                default: break;
                            }
                        }
                        CHECK(rounded_increment((ppf_rnd_t)mode, sign,
                                                 q, r, shift) == expected);
                        ++checks;
                    }
                }
            }
        }
    }
    mpz_clears(q, r, NULL);
    interval_range_flags();
    printf("rounding width passed: %lu exact helper checks\n", checks);
    return 0;
}
