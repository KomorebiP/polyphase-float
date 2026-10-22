/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _GNU_SOURCE
#include "polyphase_float.h"
#include "polyphase_float_mpfr.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>


static int call_ppf_vasprintf(char **text, const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vasprintf(text, format, ap);
    va_end(ap);
    return rc;
}

static int call_ppf_vsnprintf(char *buffer, size_t size,
                              const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vsnprintf(buffer, size, format, ap);
    va_end(ap);
    return rc;
}

static int call_ppf_vsprintf(char *buffer, const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vsprintf(buffer, format, ap);
    va_end(ap);
    return rc;
}

static int call_ppf_vfprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vfprintf(stream, format, ap);
    va_end(ap);
    return rc;
}

static int call_ppf_vprintf(const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = ppf_vprintf(format, ap);
    va_end(ap);
    return rc;
}

static int reference_asprintf(char **text, const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = mpfr_vasprintf(text, format, ap);
    va_end(ap);
    return rc;
}

static void check_one(const char *format, ppf_t *p, mpfr_ptr m) {
    char *a = NULL, *b = NULL;
    int na = ppf_asprintf(&a, format, p);
    int nb = reference_asprintf(&b, format, m);
    if (na != nb || na < 0 || memcmp(a, b, (size_t)na + 1u) != 0) {
        fprintf(stderr, "format mismatch %s\nPPF(%d): %s\nMPFR(%d): %s\n",
                format, na, a ? a : "(null)", nb, b ? b : "(null)");
        abort();
    }
    free(a); mpfr_free_str(b);
}

static void test_ppf_formats(void) {
    static const char *formats[] = {
        "%Rb", "%.8Rb", "%Ra", "%.8Ra", "%Re", "%.6Re",
        "%Rf", "%.6Rf", "%Rg", "%.6Rg", "%+020.6Rf",
        "%RNf", "%RZf", "%#.0Ra", "%#.0Rb", "%#.0Rg",
        "% 18.5Re", "%-18.5Rg"
    };
    static const char *values[] = {
        "3.1416", "-0.03125", "8", "0", "-0", "@Inf@", "-@Inf@"
    };
    ppf_t p;
    mpfr_t m;
    assert(ppf_init2(&p, 100) == 0);
    mpfr_init2(m, 100);
    for (size_t v = 0; v < sizeof(values)/sizeof(values[0]); ++v) {
        assert(ppf_set_str(&p, values[v], 0, PPF_RNDN, NULL) == 0);
        assert(mpfr_set_str(m, values[v], 0, MPFR_RNDN) == 0);
        for (size_t i = 0; i < sizeof(formats)/sizeof(formats[0]); ++i)
            check_one(formats[i], &p, m);
    }
    ppf_clear(&p); mpfr_clear(m);
}

static void test_mixed_format(void) {
    ppf_t p;
    mpfr_t m;
    mpz_t z;
    char *a = NULL, *b = NULL;
    int na, nb;
    assert(ppf_init2(&p, 100) == 0);
    mpfr_init2(m, 100);
    mpz_init_set_ui(z, 123456);
    assert(ppf_set_str(&p, "3.1416", 10, PPF_RNDN, NULL) == 0);
    assert(mpfr_set_str(m, "3.1416", 10, MPFR_RNDN) == 0);
    na = ppf_asprintf(&a, "x=%+.6Rf z=%#Zx p=%Pu s=%s i=%d",
                      &p, z, (ppf_prec_t)100, "ok", 7);
    nb = reference_asprintf(&b, "x=%+.6Rf z=%#Zx p=%Pu s=%s i=%d",
                            m, z, (mpfr_prec_t)100, "ok", 7);
    assert(na == nb && na >= 0 && memcmp(a, b, (size_t)na + 1u) == 0);
    free(a); mpfr_free_str(b);

    a = b = NULL;
    na = ppf_asprintf(&a, "%*.*R*f", 20, 8, PPF_RNDD, &p);
    nb = reference_asprintf(&b, "%*.*R*f", 20, 8, MPFR_RNDD, m);
    assert(na == nb && memcmp(a, b, (size_t)na + 1u) == 0);
    free(a); mpfr_free_str(b);
    mpz_clear(z); ppf_clear(&p); mpfr_clear(m);
}

static void test_snprintf_and_n(void) {
    ppf_t p, count;
    mpfr_t m, mcount;
    char a[7], b[7];
    int na, nb;
    char *sa = NULL, *sb = NULL;
    assert(ppf_init2(&p, 80) == 0);
    assert(ppf_init2(&count, 80) == 0);
    mpfr_init2(m, 80); mpfr_init2(mcount, 80);
    assert(ppf_set_str(&p, "3.1415926535", 10, PPF_RNDN, NULL) == 0);
    assert(mpfr_set_str(m, "3.1415926535", 10, MPFR_RNDN) == 0);
    na = ppf_snprintf(a, sizeof(a), "%.12Rf", &p);
    {
        va_list unused;
        (void)unused;
    }
    /* The minimal ABI exposes the v-form, so use the ordinary MPFR stream
       formatter through an allocated string for the reference truncation. */
    nb = reference_asprintf(&sb, "%.12Rf", m);
    assert(na == nb);
    memcpy(b, sb, sizeof(b)-1u); b[sizeof(b)-1u] = 0;
    assert(memcmp(a, b, sizeof(a)) == 0);
    mpfr_free_str(sb); sb = NULL;

    na = ppf_asprintf(&sa, "abc%RnXYZ", &count);
    nb = reference_asprintf(&sb, "abc%RnXYZ", mcount);
    assert(na == nb && memcmp(sa, sb, (size_t)na + 1u) == 0);
    assert(ppf_get_ui(&count, PPF_RNDN, NULL) == 3u);
    assert(mpfr_get_ui(mcount, MPFR_RNDN) == 3u);
    free(sa); mpfr_free_str(sb);
    ppf_clear(&p); ppf_clear(&count); mpfr_clears(m, mcount, (mpfr_ptr)0);
}


static void test_flag_preservation(void) {
    ppf_t x;
    char *text = NULL;
    assert(ppf_init2(&x, 80) == 0);
    assert(ppf_set_str(&x, "1.23456789", 10, PPF_RNDN, NULL) == 0);
    ppf_clear_flags();
    ppf_set_underflow();
    assert(ppf_asprintf(&text, "%.2Rg", &x) >= 0);
    assert(strcmp(text, "1.2") == 0);
    assert(ppf_flags_test(PPF_FLAGS_ALL) == PPF_FLAG_UNDERFLOW);
    free(text);
    ppf_clear(&x);
}

static void test_embedded_zero(void) {
    char *s = NULL;
    int n = ppf_asprintf(&s, "A%cB", 0);
    assert(n == 3);
    assert(s[0] == 'A' && s[1] == 0 && s[2] == 'B' && s[3] == 0);
    free(s);
}


static uint64_t format_rng_state = UINT64_C(0x12345678abcdef);

static uint64_t format_rng(void) {
    format_rng_state ^= format_rng_state << 13;
    format_rng_state ^= format_rng_state >> 7;
    format_rng_state ^= format_rng_state << 17;
    return format_rng_state;
}

static void test_random_differential(void) {
    static const char *formats[] = {
        "%Ra", "%#.0Ra", "%.0Ra", "%.1Ra", "%.13Ra",
        "%RA", "%#.0RA", "%Rb", "%#.0Rb", "%.0Rb", "%.7Rb",
        "%Re", "%.0Re", "%.1Re", "%.17Re", "%RE",
        "%Rf", "%.0Rf", "%.1Rf", "%.17Rf", "%RF",
        "%Rg", "%#.0Rg", "%.0Rg", "%.1Rg", "%.8Rg", "%RG",
        "%+020.6Rf", "%-20.6Re", "% 20.8Rg",
        "%RZf", "%RUf", "%RDf", "%RYf", "%RNf"
    };
    for (unsigned trial = 0; trial < 200u; ++trial) {
        union { uint64_t bits; double value; } sample;
        ppf_t p;
        mpfr_t m;
        sample.bits = format_rng();
        if (!isfinite(sample.value)) { --trial; continue; }
        assert(ppf_init2(&p, 113) == 0);
        mpfr_init2(m, 113);
        assert(ppf_set_d(&p, sample.value, PPF_RNDN, NULL) !=
               PPF_TERNARY_ERROR);
        (void)mpfr_set_d(m, sample.value, MPFR_RNDN);
        for (size_t i = 0; i < sizeof(formats)/sizeof(formats[0]); ++i)
            check_one(formats[i], &p, m);
        ppf_clear(&p);
        mpfr_clear(m);
    }
}

static void test_native_gmp_and_wrappers(void) {
    ppf_t p;
    mpz_t z;
    mpq_t q;
    mpf_t f;
    mp_limb_t limb = 123u;
    mp_limb_t limbs[2] = { 0x1234u, 0x5678u };
    char *a = NULL, *b = NULL;
    char sa[256], sb[256], sc[256];
    char *stream_text = NULL;
    size_t stream_size = 0;
    FILE *stream;
    int na, nb;

    assert(ppf_init2(&p, 100) == 0);
    assert(ppf_set_str(&p, "3.1416", 10, PPF_RNDN, NULL) == 0);
    mpz_init_set_ui(z, 12345u);
    mpq_init(q); mpq_set_ui(q, 7u, 3u);
    mpf_init2(f, 128); mpf_set_d(f, 1.25);

    na = ppf_asprintf(&a,
        "std=%F z=%#Zx q=%Qd f=%.3Ff m=%Mu n=%Nx p=%Pu x=%.4Rf",
        1.5, z, q, f, limb, limbs, (mp_size_t)2,
        (ppf_prec_t)100, &p);
    assert(na > 0);
    nb = call_ppf_vasprintf(&b,
        "std=%F z=%#Zx q=%Qd f=%.3Ff m=%Mu n=%Nx p=%Pu x=%.4Rf",
        1.5, z, q, f, limb, limbs, (mp_size_t)2,
        (ppf_prec_t)100, &p);
    assert(na == nb && memcmp(a, b, (size_t)na + 1u) == 0);

    assert(ppf_sprintf(sa, "%.8Rf/%d", &p, 7) > 0);
    assert(call_ppf_vsprintf(sb, "%.8Rf/%d", &p, 7) > 0);
    assert(strcmp(sa, sb) == 0);
    assert(ppf_snprintf(sc, sizeof(sc), "%.8Rf/%d", &p, 7) ==
           (int)strlen(sa));
    assert(strcmp(sa, sc) == 0);
    memset(sc, 0x55, sizeof(sc));
    assert(call_ppf_vsnprintf(sc, 5u, "%.8Rf/%d", &p, 7) ==
           (int)strlen(sa));
    assert(sc[4] == '\0' && memcmp(sc, sa, 4u) == 0);
    assert(ppf_snprintf(NULL, 0u, "%.8Rf/%d", &p, 7) ==
           (int)strlen(sa));

    stream = open_memstream(&stream_text, &stream_size);
    assert(stream != NULL);
    assert(ppf_fprintf(stream, "%.8Rf/%d", &p, 7) == (int)strlen(sa));
    assert(fclose(stream) == 0);
    assert(stream_size == strlen(sa) && memcmp(stream_text, sa, stream_size) == 0);
    free(stream_text); stream_text = NULL; stream_size = 0;

    stream = open_memstream(&stream_text, &stream_size);
    assert(stream != NULL);
    assert(call_ppf_vfprintf(stream, "%.8Rf/%d", &p, 7) ==
           (int)strlen(sa));
    assert(fclose(stream) == 0);
    assert(stream_size == strlen(sa) && memcmp(stream_text, sa, stream_size) == 0);
    free(stream_text);

    /* Exercise the stdout wrappers without producing user-visible output. */
    assert(ppf_printf("") == 0);
    assert(call_ppf_vprintf("") == 0);

    free(a); free(b);
    mpf_clear(f); mpq_clear(q); mpz_clear(z); ppf_clear(&p);
}

int main(void) {
    test_ppf_formats();
    test_mixed_format();
    test_snprintf_and_n();
    test_embedded_zero();
    test_flag_preservation();
    test_native_gmp_and_wrappers();
    test_random_differential();
    puts("formatted-output tests passed");
    return 0;
}
