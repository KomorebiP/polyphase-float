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
#include <time.h>

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int compare_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double median(double *values, int count) {
    qsort(values, (size_t)count, sizeof(*values), compare_double);
    return count & 1 ? values[count / 2]
                     : 0.5 * (values[count / 2 - 1] + values[count / 2]);
}

static int reference_asprintf(char **text, const char *format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = mpfr_vasprintf(text, format, ap);
    va_end(ap);
    return rc;
}

static char *ppf_out_buffer(const ppf_t *x, size_t *size) {
    char *buffer = NULL;
    FILE *stream = open_memstream(&buffer, size);
    size_t count;
    if (!stream) return NULL;
    count = ppf_out_str(stream, 10, 128, x, PPF_RNDN, NULL);
    if (fclose(stream) != 0 || count != *size) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

static char *mpfr_out_buffer(mpfr_srcptr x, size_t *size) {
    char *buffer = NULL;
    FILE *stream = open_memstream(&buffer, size);
    size_t count;
    if (!stream) return NULL;
    count = mpfr_out_str(stream, 10, 128, x, MPFR_RNDN);
    if (fclose(stream) != 0 || count != *size) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

static int ppf_roundtrip(const ppf_t *source, ppf_t *target) {
    char *bytes = NULL;
    size_t size = 0;
    FILE *out = open_memstream(&bytes, &size);
    FILE *in;
    int rc;
    if (!out) return -1;
    rc = ppf_fpif_export(out, source);
    if (fclose(out) != 0 || rc != 0) { free(bytes); return -1; }
    in = fmemopen(bytes, size, "rb");
    if (!in) { free(bytes); return -1; }
    rc = ppf_fpif_import(target, in, NULL);
    fclose(in);
    free(bytes);
    return rc;
}

static int mpfr_roundtrip(mpfr_ptr source, mpfr_ptr target) {
    char *bytes = NULL;
    size_t size = 0;
    FILE *out = open_memstream(&bytes, &size);
    FILE *in;
    int rc;
    if (!out) return -1;
    rc = mpfr_fpif_export(out, source);
    if (fclose(out) != 0 || rc != 0) { free(bytes); return -1; }
    in = fmemopen(bytes, size, "rb");
    if (!in) { free(bytes); return -1; }
    rc = mpfr_fpif_import(target, in);
    fclose(in);
    free(bytes);
    return rc;
}

static void print_row(unsigned long bits, const char *operation,
                      double ppf_time, double mpfr_time) {
    double speedup = ppf_time > 0.0 ? mpfr_time / ppf_time : 0.0;
    printf("%lu,%s,%.9f,%.9f,%.6f\n",
           bits, operation, ppf_time, mpfr_time, speedup);
}

int main(int argc, char **argv) {
    unsigned long bits = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000000ul;
    int repetitions = argc > 2 ? atoi(argv[2]) : 9;
    ppf_t p, p_import;
    mpfr_t m, m_import, check;
    mpz_t z;
    double *pt, *mt;
    char *a = NULL, *b = NULL;
    int na, nb;
    size_t as = 0, bs = 0;

    if (bits < 2 || repetitions < 1) return 2;
    pt = calloc((size_t)repetitions, sizeof(*pt));
    mt = calloc((size_t)repetitions, sizeof(*mt));
    if (!pt || !mt) return 2;
    if (ppf_init2(&p, bits) != 0 || ppf_init2(&p_import, 17) != 0) return 2;
    mpfr_init2(m, bits); mpfr_init2(m_import, 17); mpfr_init2(check, bits);
    mpz_init(z);
    mpz_set_ui(z, 1u);
    mpz_mul_2exp(z, z, bits - 1u);
    for (unsigned long k = 1; k < bits; k += 97u)
        mpz_setbit(z, k);
    mpz_add_ui(z, z, 12345u);
    if (ppf_set_z_2exp(&p, z, -(ppf_exp_t)(bits / 3u), PPF_RNDN, NULL) ==
        PPF_TERNARY_ERROR) return 2;
    (void)mpfr_set_z_2exp(m, z, -(mpfr_exp_t)(bits / 3u), MPFR_RNDN);

    a = ppf_out_buffer(&p, &as);
    b = mpfr_out_buffer(m, &bs);
    if (!a || !b || as != bs || memcmp(a, b, as) != 0) return 3;
    free(a); free(b); a = b = NULL;
    na = ppf_asprintf(&a, "%+.128Rg", &p);
    nb = reference_asprintf(&b, "%+.128Rg", m);
    if (na != nb || na < 0 || memcmp(a, b, (size_t)na + 1u) != 0) return 3;
    ppf_free_str(a); mpfr_free_str(b); a = b = NULL;
    if (ppf_roundtrip(&p, &p_import) != 0 || !ppf_equal_p(&p, &p_import)) return 3;
    if (mpfr_roundtrip(m, m_import) != 0 || mpfr_cmp(m, m_import) != 0) return 3;

    printf("bits,operation,ppf_median_s,mpfr_median_s,speedup_mpfr_over_ppf\n");

    for (int i = 0; i < repetitions; ++i) {
        double t0 = now_seconds();
        a = ppf_out_buffer(&p, &as);
        pt[i] = now_seconds() - t0;
        free(a);
        t0 = now_seconds();
        b = mpfr_out_buffer(m, &bs);
        mt[i] = now_seconds() - t0;
        free(b);
    }
    print_row(bits, "out_str_128_digits", median(pt, repetitions),
              median(mt, repetitions));

    for (int i = 0; i < repetitions; ++i) {
        double t0 = now_seconds();
        na = ppf_asprintf(&a, "%+.128Rg", &p);
        pt[i] = now_seconds() - t0;
        if (na < 0) return 3;
        ppf_free_str(a);
        t0 = now_seconds();
        nb = reference_asprintf(&b, "%+.128Rg", m);
        mt[i] = now_seconds() - t0;
        if (nb < 0) return 3;
        mpfr_free_str(b);
    }
    print_row(bits, "asprintf_128_significant", median(pt, repetitions),
              median(mt, repetitions));

    for (int i = 0; i < repetitions; ++i) {
        double t0 = now_seconds();
        if (ppf_roundtrip(&p, &p_import) != 0) return 3;
        pt[i] = now_seconds() - t0;
        t0 = now_seconds();
        if (mpfr_roundtrip(m, m_import) != 0) return 3;
        mt[i] = now_seconds() - t0;
    }
    print_row(bits, "portable_interchange_roundtrip", median(pt, repetitions),
              median(mt, repetitions));

    (void)ppf_get_mpfr(check, &p, MPFR_RNDN);
    if (mpfr_cmp(check, m) != 0) return 3;
    free(pt); free(mt);
    mpz_clear(z);
    ppf_clear(&p); ppf_clear(&p_import);
    mpfr_clears(m, m_import, check, (mpfr_ptr)0);
    return 0;
}
