/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _GNU_SOURCE
#include "polyphase_float.h"
#include "polyphase_float_mpfr.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

static char *capture_ppf_out(const ppf_t *x, int base, size_t digits,
                             ppf_rnd_t rnd, size_t *count) {
    char *buf = NULL;
    size_t size = 0;
    FILE *f = open_memstream(&buf, &size);
    assert(f != NULL);
    *count = ppf_out_str(f, base, digits, x, rnd, NULL);
    assert(fclose(f) == 0);
    assert(*count == size);
    return buf;
}

static char *capture_mpfr_out(mpfr_srcptr x, int base, size_t digits,
                              mpfr_rnd_t rnd, size_t *count) {
    char *buf = NULL;
    size_t size = 0;
    FILE *f = open_memstream(&buf, &size);
    assert(f != NULL);
    *count = mpfr_out_str(f, base, digits, x, rnd);
    assert(fclose(f) == 0);
    assert(*count == size);
    return buf;
}

static void check_out_case(const char *text, int base, size_t digits,
                           ppf_rnd_t prnd, mpfr_rnd_t mrnd) {
    ppf_t p;
    mpfr_t m;
    char *a, *b;
    size_t na, nb;
    assert(ppf_init2(&p, 113) == 0);
    mpfr_init2(m, 113);
    assert(ppf_set_str(&p, text, 0, PPF_RNDN, NULL) == 0);
    assert(mpfr_set_str(m, text, 0, MPFR_RNDN) == 0);
    a = capture_ppf_out(&p, base, digits, prnd, &na);
    b = capture_mpfr_out(m, base, digits, mrnd, &nb);
    if (na != nb || memcmp(a, b, na) != 0) {
        fprintf(stderr, "out_str mismatch text=%s base=%d digits=%zu\nPPF:  %s\nMPFR: %s\n",
                text, base, digits, a, b);
        abort();
    }
    free(a); free(b);
    ppf_clear(&p); mpfr_clear(m);
}

static void test_out_str(void) {
    const char *values[] = {
        "3.1415926535897932384626433832795028841971",
        "-0.03125", "0", "-0", "@Inf@", "-@Inf@"
    };
    const int bases[] = {2, 10, 16, 37};
    const size_t digits[] = {0, 1, 7, 29};
    for (size_t i = 0; i < sizeof(values)/sizeof(values[0]); ++i)
        for (size_t j = 0; j < sizeof(bases)/sizeof(bases[0]); ++j)
            for (size_t k = 0; k < sizeof(digits)/sizeof(digits[0]); ++k)
                check_out_case(values[i], bases[j], digits[k],
                               PPF_RNDN, MPFR_RNDN);
    {
        ppf_t x; char *buf = NULL; size_t size = 0, count;
        FILE *f;
        assert(ppf_init2(&x, 80) == 0);
        assert(ppf_set_str(&x, "3.14", 10, PPF_RNDN, NULL) == 0);
        f = open_memstream(&buf, &size); assert(f);
        count = ppf_out_str(f, -16, 6, &x, PPF_RNDN, NULL);
        assert(fclose(f) == 0 && count == size);
        assert(strchr(buf, '@') != NULL);
        assert(strchr(buf, 'A') || strchr(buf, 'B') || strchr(buf, 'C') ||
               strchr(buf, 'D') || strchr(buf, 'E') || strchr(buf, 'F'));
        free(buf); ppf_clear(&x);
    }
}


static uint64_t io_rng_state = UINT64_C(0xfedcba987654321);

static uint64_t io_rng(void) {
    io_rng_state ^= io_rng_state << 13;
    io_rng_state ^= io_rng_state >> 7;
    io_rng_state ^= io_rng_state << 17;
    return io_rng_state;
}

static void test_out_str_random(void) {
    static const int bases[] = {2, 10, 16, 37};
    static const size_t digit_counts[] = {0, 1, 7, 29};
    static const ppf_rnd_t prounds[] = {
        PPF_RNDN, PPF_RNDZ, PPF_RNDU, PPF_RNDD, PPF_RNDA
    };
    static const mpfr_rnd_t mrounds[] = {
        MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA
    };
    for (unsigned trial = 0; trial < 24u; ++trial) {
        union { uint64_t bits; double value; } sample;
        ppf_t p;
        mpfr_t m;
        sample.bits = io_rng();
        if (!isfinite(sample.value)) { --trial; continue; }
        assert(ppf_init2(&p, 113) == 0);
        mpfr_init2(m, 113);
        assert(ppf_set_d(&p, sample.value, PPF_RNDN, NULL) !=
               PPF_TERNARY_ERROR);
        (void)mpfr_set_d(m, sample.value, MPFR_RNDN);
        for (size_t b = 0; b < sizeof(bases)/sizeof(bases[0]); ++b) {
            for (size_t d = 0; d < sizeof(digit_counts)/sizeof(digit_counts[0]); ++d) {
                for (size_t r = 0; r < sizeof(prounds)/sizeof(prounds[0]); ++r) {
                    char *a, *c;
                    size_t na, nc;
                    a = capture_ppf_out(&p, bases[b], digit_counts[d],
                                        prounds[r], &na);
                    c = capture_mpfr_out(m, bases[b], digit_counts[d],
                                         mrounds[r], &nc);
                    if (na != nc || memcmp(a, c, na) != 0) {
                        fprintf(stderr,
                            "random out_str mismatch trial=%u base=%d digits=%zu rnd=%zu\nPPF:  %s\nMPFR: %s\n",
                            trial, bases[b], digit_counts[d], r, a, c);
                        abort();
                    }
                    free(a); free(c);
                }
            }
        }
        ppf_clear(&p); mpfr_clear(m);
    }
}

static void test_inp_str(void) {
    const char input[] = " \t -0x1.921fb54442d18p+1 trailing";
    FILE *pf = fmemopen((void *)input, sizeof(input)-1, "r");
    FILE *mf = fmemopen((void *)input, sizeof(input)-1, "r");
    ppf_t p;
    mpfr_t m, got;
    size_t pn, mn;
    assert(pf && mf);
    assert(ppf_init2(&p, 80) == 0);
    mpfr_init2(m, 80); mpfr_init2(got, 80);
    pn = ppf_inp_str(&p, pf, 0, PPF_RNDN, NULL);
    mn = mpfr_inp_str(m, mf, 0, MPFR_RNDN);
    assert(pn == mn && pn != 0);
    assert(ppf_get_mpfr(got, &p, MPFR_RNDN) == 0);
    assert(mpfr_cmp(got, m) == 0);
    assert(fgetc(pf) == ' ');
    assert(fgetc(mf) == ' ');
    fclose(pf); fclose(mf);
    ppf_clear(&p); mpfr_clears(m, got, (mpfr_ptr)0);
}

static void roundtrip_one(ppf_t *source) {
    FILE *f = tmpfile();
    ppf_t target;
    assert(f != NULL);
    assert(ppf_init2(&target, 17) == 0);
    assert(ppf_fpif_export(f, source) == 0);
    rewind(f);
    assert(ppf_fpif_import(&target, f, NULL) == 0);
    assert(target.precision == source->precision);
    assert(target.kind == source->kind);
    assert(target.sign == source->sign);
    assert(ppf_nan_p(source) ? ppf_nan_p(&target) : ppf_equal_p(&target, source));
    fclose(f);
    ppf_clear(&target);
}

static void test_fpif(void) {
    ppf_t x;
    FILE *f;
    unsigned char *bytes;
    long size;
    ppf_t unchanged, snapshot;
    assert(ppf_init2(&x, 257) == 0);
    assert(ppf_set_str(&x, "-3.1415926535897932384626433832795028841971",
                       10, PPF_RNDN, NULL) == 0);
    roundtrip_one(&x);
    ppf_set_zero(&x, -1); roundtrip_one(&x);
    ppf_set_inf(&x, -1); roundtrip_one(&x);
    ppf_set_nan(&x, -1); roundtrip_one(&x);

    assert(ppf_set_ui(&x, 17, PPF_RNDN, NULL) == 0);
    f = tmpfile(); assert(f);
    assert(ppf_fpif_export(f, &x) == 0);
    assert(fseek(f, 0, SEEK_END) == 0);
    size = ftell(f); assert(size > 40);
    rewind(f);
    bytes = malloc((size_t)size); assert(bytes);
    assert(fread(bytes, 1, (size_t)size, f) == (size_t)size);
    fclose(f);
    bytes[size/2] ^= 0x40;
    f = fmemopen(bytes, (size_t)size, "r"); assert(f);
    assert(ppf_init2(&unchanged, 64) == 0);
    assert(ppf_init2(&snapshot, 64) == 0);
    assert(ppf_set_ui(&unchanged, 123, PPF_RNDN, NULL) == 0);
    assert(ppf_set(&snapshot, &unchanged, PPF_RNDN, NULL) == 0);
    assert(ppf_fpif_import(&unchanged, f, NULL) != 0);
    assert(ppf_equal_p(&unchanged, &snapshot));
    fclose(f); free(bytes);
    ppf_clear(&unchanged); ppf_clear(&snapshot); ppf_clear(&x);
}


static void test_fpif_random(void) {
    static const ppf_prec_t precisions[] = {2, 3, 17, 53, 113, 257};
    mpz_t z;
    mpz_init(z);
    for (unsigned trial = 0; trial < 120u; ++trial) {
        ppf_prec_t precision = precisions[trial %
            (sizeof(precisions)/sizeof(precisions[0]))];
        ppf_t source;
        ppf_exp_t exact_exp = (ppf_exp_t)((int)(io_rng() % 401u) - 200);
        assert(ppf_init2(&source, precision) == 0);
        mpz_set_ui(z, 1u);
        mpz_mul_2exp(z, z, (mp_bitcnt_t)(precision - 1u));
        for (ppf_prec_t bit = 0; bit + 1u < precision; ++bit)
            if ((io_rng() >> (bit & 63u)) & 1u)
                mpz_setbit(z, (mp_bitcnt_t)bit);
        if (io_rng() & 1u) mpz_neg(z, z);
        assert(ppf_set_z_2exp(&source, z, exact_exp, PPF_RNDN, NULL) == 0);
        roundtrip_one(&source);
        ppf_clear(&source);
    }
    mpz_clear(z);
}

static void test_dump(void) {
    ppf_t x;
    char *buf = NULL;
    size_t size = 0;
    FILE *f;
    assert(ppf_init2(&x, 53) == 0);
    assert(ppf_set_d(&x, 1.5, PPF_RNDN, NULL) == 0);
    f = open_memstream(&buf, &size); assert(f);
    ppf_dump_file(f, &x);
    assert(fclose(f) == 0);
    assert(size != 0 && strstr(buf, "ppf(kind=regular") != NULL);
    free(buf); ppf_clear(&x);
}

int main(void) {
    test_out_str();
    test_out_str_random();
    test_inp_str();
    test_fpif();
    test_fpif_random();
    test_dump();
    puts("I/O tests passed");
    return 0;
}
