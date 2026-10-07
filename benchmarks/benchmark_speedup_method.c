/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float_mpfr.h"
#include "benchmark_samples.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ppf_openmp_compat.h"

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static void make_value(gmp_randstate_t state, mpz_t z,
                       mpfr_ptr m, ppf_t *p, ppf_context_t *ctx,
                       unsigned long bits, unsigned seed_shift) {
    mpz_urandomb(z, state, bits);
    mpz_setbit(z, bits - 1u);
    if (seed_shift & 1u) mpz_neg(z, z);
    mpfr_set_z_2exp(m, z, -(long)(bits / 3u + seed_shift), MPFR_RNDN);
    if (ppf_import_mpfr_exact(p, m, ctx) != 0) {
        fprintf(stderr, "input import failed\n");
        exit(2);
    }
}

static int same_value(mpfr_srcptr a, mpfr_srcptr b) {
    if (mpfr_zero_p(a) || mpfr_zero_p(b)) {
        return mpfr_zero_p(a) && mpfr_zero_p(b) &&
               mpfr_signbit(a) == mpfr_signbit(b);
    }
    return mpfr_equal_p(a, b) != 0;
}

static int legacy_add(ppf_t *out, const ppf_t *a, const ppf_t *b,
                      ppf_context_t *ctx) {
    mpz_t za, zb, tmp;
    ppf_exp_t ea, eb, common;
    uint64_t da, db;
    int rc;
    mpz_inits(za, zb, tmp, NULL);
    ea = ppf_get_z_2exp(za, a);
    eb = ppf_get_z_2exp(zb, b);
    common = ea < eb ? ea : eb;
    da = (uint64_t)ea - (uint64_t)common;
    db = (uint64_t)eb - (uint64_t)common;
    if (da > ULONG_MAX || db > ULONG_MAX) {
        mpz_clears(za, zb, tmp, NULL);
        return PPF_TERNARY_ERROR;
    }
    if (da) mpz_mul_2exp(za, za, (mp_bitcnt_t)da);
    if (db) mpz_mul_2exp(zb, zb, (mp_bitcnt_t)db);
    mpz_add(tmp, za, zb);
    rc = ppf_set_z_2exp(out, tmp, common, PPF_RNDN, ctx);
    mpz_clears(za, zb, tmp, NULL);
    return rc;
}

static int legacy_scale(ppf_t *out, const ppf_t *a,
                        ppf_context_t *ctx) {
    mpz_t z;
    ppf_exp_t e;
    int rc;
    mpz_init(z);
    e = ppf_get_z_2exp(z, a);
    if ((137 > 0 && e > INT64_MAX - 137) ||
        (137 < 0 && e < INT64_MIN - 137)) {
        mpz_clear(z);
        return PPF_TERNARY_ERROR;
    }
    e += 137;
    rc = ppf_set_z_2exp(out, z, e, PPF_RNDN, ctx);
    mpz_clear(z);
    return rc;
}

static int legacy_mul_inner(ppf_t *out, const ppf_t *a, const ppf_t *b,
                            unsigned long workers, ppf_context_t *ctx) {
    size_t n = a->used_limbs, h;
    mpz_t a0, a1, b0, b1, sa, sb, z0, z1, z2, product, tmp;
    ppf_exp_t ea, eb, base;
    int rc;
    if (a->used_limbs != b->used_limbs || workers < 3 || n < 2)
        return ppf_mul(out, a, b, PPF_RNDN, ctx);
    h = (n + 1u) / 2u;
    if (n <= h) return ppf_mul(out, a, b, PPF_RNDN, ctx);
    mpz_roinit_n(a0, a->limbs, (mp_size_t)h);
    mpz_roinit_n(a1, a->limbs + h, (mp_size_t)(n - h));
    mpz_roinit_n(b0, b->limbs, (mp_size_t)h);
    mpz_roinit_n(b1, b->limbs + h, (mp_size_t)(n - h));
    mpz_inits(sa, sb, z0, z1, z2, product, tmp, NULL);
    mpz_add(sa, a0, a1);
    mpz_add(sb, b0, b1);
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel sections num_threads(3))
    {
PPF_OMP_PRAGMA(omp section)
        { mpz_mul(z0, a0, b0); }
PPF_OMP_PRAGMA(omp section)
        { mpz_mul(z2, a1, b1); }
PPF_OMP_PRAGMA(omp section)
        { mpz_mul(z1, sa, sb); }
    }
#else
    mpz_mul(z0, a0, b0);
    mpz_mul(z2, a1, b1);
    mpz_mul(z1, sa, sb);
#endif
    mpz_sub(z1, z1, z0);
    mpz_sub(z1, z1, z2);
    mpz_set(product, z0);
    mpz_mul_2exp(tmp, z1, (mp_bitcnt_t)(h * GMP_NUMB_BITS));
    mpz_add(product, product, tmp);
    mpz_mul_2exp(tmp, z2, (mp_bitcnt_t)(2u * h * GMP_NUMB_BITS));
    mpz_add(product, product, tmp);
    if (a->sign * b->sign < 0) mpz_neg(product, product);
    ea = a->exponent - (ppf_exp_t)a->precision;
    eb = b->exponent - (ppf_exp_t)b->precision;
    base = ea + eb;
    rc = ppf_set_z_2exp(out, product, base, PPF_RNDN, ctx);
    mpz_clears(sa, sb, z0, z1, z2, product, tmp, NULL);
    return rc;
}

typedef struct {
    ppf_context_t context;
    ppf_t a, b, c, out;
    mpfr_t ma, mb, mc, mout;
} outer_job_t;

static int run_ppf_mul_outer(outer_job_t *jobs, size_t count,
                             unsigned long workers) {
#ifndef _OPENMP
    (void)workers;
#endif
    int failed = 0;
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel for num_threads((int)workers) schedule(static) reduction(|:failed))
#endif
    for (size_t i = 0; i < count; ++i) {
        int rc = ppf_mul(&jobs[i].out, &jobs[i].a, &jobs[i].b,
                         PPF_RNDN, &jobs[i].context);
        if (rc == PPF_TERNARY_ERROR) failed = 1;
    }
    return failed ? -1 : 0;
}

static void run_mpfr_mul_outer(outer_job_t *jobs, size_t count,
                               unsigned long workers) {
#ifndef _OPENMP
    (void)workers;
#endif
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel for num_threads((int)workers) schedule(static))
#endif
    for (size_t i = 0; i < count; ++i)
        mpfr_mul(jobs[i].mout, jobs[i].ma, jobs[i].mb, MPFR_RNDN);
}


static int run_ppf_fma_outer(outer_job_t *jobs, size_t count,
                             unsigned long workers) {
#ifndef _OPENMP
    (void)workers;
#endif
    int failed = 0;
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel for num_threads((int)workers) schedule(static) reduction(|:failed))
#endif
    for (size_t i = 0; i < count; ++i) {
        int rc = ppf_fma(&jobs[i].out, &jobs[i].a, &jobs[i].b,
                         &jobs[i].c, PPF_RNDN, &jobs[i].context);
        if (rc == PPF_TERNARY_ERROR) failed = 1;
    }
    return failed ? -1 : 0;
}

static void run_mpfr_fma_outer(outer_job_t *jobs, size_t count,
                               unsigned long workers) {
#ifndef _OPENMP
    (void)workers;
#endif
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel for num_threads((int)workers) schedule(static))
#endif
    for (size_t i = 0; i < count; ++i)
        mpfr_fma(jobs[i].mout, jobs[i].ma, jobs[i].mb,
                 jobs[i].mc, MPFR_RNDN);
}

static void run_mpfr_div_outer(mpfr_ptr outputs[], mpfr_ptr inputs[],
                               size_t count, mpfr_srcptr denominator,
                               unsigned long workers) {
#ifndef _OPENMP
    (void)workers;
#endif
#ifdef _OPENMP
PPF_OMP_PRAGMA(omp parallel for num_threads((int)workers) schedule(static))
#endif
    for (size_t i = 0; i < count; ++i)
        mpfr_div(outputs[i], inputs[i], denominator, MPFR_RNDN);
}

int main(int argc, char **argv) {
    const char *method;
    unsigned long bits, workers, jobs_count;
    int inner_reps;
    gmp_randstate_t state;
    mpz_t z;
    ppf_context_t context;
    ppf_t a, b, c, out, dot_a[8], dot_b[8], dot_out[8];
    const ppf_t *dot_ap[8], *dot_bp[8], *plan_nums[8];
    ppf_t *plan_outs[8];
    mpfr_t ma, mb, mc, mout, expected, dot_ma[8], dot_mb[8],
           plan_mout[8], dot_expected;
    mpfr_ptr dot_map[8], dot_mbp[8], plan_moutp[8];
    ppf_divisor_plan_t *plan = NULL;
    outer_job_t *outer = NULL;
    int ternaries[8] = {0};
    int ppf_rc = 0, mpfr_rc = 0, verified = 1;
    double started, elapsed, *samples;
    size_t dot_length = 8;
    int dot_initialized = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: %s METHOD BITS [WORKERS] [JOBS]\n", argv[0]);
        return 2;
    }
    method = argv[1];
    bits = strtoul(argv[2], NULL, 10);
    workers = argc > 3 ? strtoul(argv[3], NULL, 10) : 4UL;
    jobs_count = argc > 4 ? strtoul(argv[4], NULL, 10) : workers;
    inner_reps = argc > 5 ? atoi(argv[5]) : 5;
    if (!bits || !workers || !jobs_count || jobs_count > 64 || inner_reps < 1)
        return 2;
    samples = calloc((size_t)inner_reps, sizeof(*samples));
    if (!samples) return 2;

    gmp_randinit_mt(state);
    gmp_randseed_ui(state, 0x5350454544555053UL);
    mpz_init(z);
    if (ppf_context_init(&context) != 0) return 2;
    if (ppf_init2_ctx(&a, bits, &context) ||
        ppf_init2_ctx(&b, bits, &context) ||
        ppf_init2_ctx(&c, bits, &context) ||
        ppf_init2_ctx(&out, bits, &context)) return 2;
    mpfr_inits2(bits, ma, mb, mc, mout, expected, dot_expected,
                (mpfr_ptr)0);
    make_value(state, z, ma, &a, &context, bits, 3);
    make_value(state, z, mb, &b, &context, bits, 7);
    make_value(state, z, mc, &c, &context, bits, 11);
    if (mpfr_sgn(ma) < 0) {
        mpfr_neg(ma, ma, MPFR_RNDN);
        ppf_import_mpfr_exact(&a, ma, &context);
    }
    if (mpfr_sgn(mb) < 0) {
        mpfr_neg(mb, mb, MPFR_RNDN);
        ppf_import_mpfr_exact(&b, mb, &context);
    }

    if (strstr(method, "dot") || strstr(method, "plan") ||
        strstr(method, "div_outer")) {
        dot_initialized = 1;
        for (size_t i = 0; i < dot_length; ++i) {
            ppf_init2_ctx(&dot_a[i], bits, &context);
            ppf_init2_ctx(&dot_b[i], bits, &context);
            ppf_init2_ctx(&dot_out[i], bits, &context);
            mpfr_init2(dot_ma[i], bits);
            mpfr_init2(dot_mb[i], bits);
            mpfr_init2(plan_mout[i], bits);
            make_value(state, z, dot_ma[i], &dot_a[i], &context,
                       bits, (unsigned)(17 + i));
            make_value(state, z, dot_mb[i], &dot_b[i], &context,
                       bits, (unsigned)(31 + i));
            dot_ap[i] = &dot_a[i]; dot_bp[i] = &dot_b[i];
            dot_map[i] = dot_ma[i]; dot_mbp[i] = dot_mb[i];
            plan_moutp[i] = plan_mout[i];
            plan_nums[i] = &dot_a[i]; plan_outs[i] = &dot_out[i];
        }
    }

    if (strstr(method, "outer")) {
        outer = calloc(jobs_count, sizeof(*outer));
        if (!outer) return 2;
        for (size_t i = 0; i < jobs_count; ++i) {
            ppf_context_init(&outer[i].context);
            ppf_init2_ctx(&outer[i].a, bits, &outer[i].context);
            ppf_init2_ctx(&outer[i].b, bits, &outer[i].context);
            ppf_init2_ctx(&outer[i].c, bits, &outer[i].context);
            ppf_init2_ctx(&outer[i].out, bits, &outer[i].context);
            mpfr_inits2(bits, outer[i].ma, outer[i].mb, outer[i].mc,
                        outer[i].mout, (mpfr_ptr)0);
            make_value(state, z, outer[i].ma, &outer[i].a,
                       &outer[i].context, bits, (unsigned)(71 + 2*i));
            make_value(state, z, outer[i].mb, &outer[i].b,
                       &outer[i].context, bits, (unsigned)(72 + 3*i));
            make_value(state, z, outer[i].mc, &outer[i].c,
                       &outer[i].context, bits, (unsigned)(73 + 3*i));
        }
    }
    if (!strcmp(method, "ppf_plan_batch") ||
        !strcmp(method, "ppf_plan_build")) {
        if (ppf_divisor_plan_create(&plan, &b, bits, &context) != 0)
            return 3;
    }

#define RUN_ONCE() do { \
    if (!strcmp(method,"ppf_add")) ppf_rc=ppf_add(&out,&a,&b,PPF_RNDN,&context); \
    else if (!strcmp(method,"legacy_add")) ppf_rc=legacy_add(&out,&a,&b,&context); \
    else if (!strcmp(method,"mpfr_add")) mpfr_rc=mpfr_add(mout,ma,mb,MPFR_RNDN); \
    else if (!strcmp(method,"ppf_mul_1t")) ppf_rc=ppf_mul(&out,&a,&b,PPF_RNDN,&context); \
    else if (!strcmp(method,"ppf_mul_inner")) ppf_rc=ppf_mul_threads(&out,&a,&b,PPF_RNDN,workers,&context); \
    else if (!strcmp(method,"legacy_mul_inner")) ppf_rc=legacy_mul_inner(&out,&a,&b,workers,&context); \
    else if (!strcmp(method,"mpfr_mul_1t")) mpfr_rc=mpfr_mul(mout,ma,mb,MPFR_RNDN); \
    else if (!strcmp(method,"ppf_sqr_1t")) ppf_rc=ppf_sqr(&out,&a,PPF_RNDN,&context); \
    else if (!strcmp(method,"ppf_sqr_inner")) ppf_rc=ppf_sqr_threads(&out,&a,PPF_RNDN,workers,&context); \
    else if (!strcmp(method,"mpfr_sqr_1t")) mpfr_rc=mpfr_sqr(mout,ma,MPFR_RNDN); \
    else if (!strcmp(method,"ppf_scale")) ppf_rc=ppf_mul_2si(&out,&a,137,PPF_RNDN,&context); \
    else if (!strcmp(method,"legacy_scale")) ppf_rc=legacy_scale(&out,&a,&context); \
    else if (!strcmp(method,"mpfr_scale")) mpfr_rc=mpfr_mul_2si(mout,ma,137,MPFR_RNDN); \
    else if (!strcmp(method,"ppf_div")) ppf_rc=ppf_div(&out,&a,&b,PPF_RNDN,&context); \
    else if (!strcmp(method,"mpfr_div")) mpfr_rc=mpfr_div(mout,ma,mb,MPFR_RNDN); \
    else if (!strcmp(method,"ppf_sqrt")) ppf_rc=ppf_sqrt(&out,&a,PPF_RNDN,&context); \
    else if (!strcmp(method,"mpfr_sqrt")) mpfr_rc=mpfr_sqrt(mout,ma,MPFR_RNDN); \
    else if (!strcmp(method,"ppf_hypot")) ppf_rc=ppf_hypot(&out,&a,&b,PPF_RNDN,&context); \
    else if (!strcmp(method,"mpfr_hypot")) mpfr_rc=mpfr_hypot(mout,ma,mb,MPFR_RNDN); \
    else if (!strcmp(method,"ppf_fma")) ppf_rc=ppf_fma(&out,&a,&b,&c,PPF_RNDN,&context); \
    else if (!strcmp(method,"mpfr_fma")) mpfr_rc=mpfr_fma(mout,ma,mb,mc,MPFR_RNDN); \
    else if (!strcmp(method,"ppf_dot_1t")) ppf_rc=ppf_dot(&out,dot_ap,dot_bp,dot_length,PPF_RNDN,&context); \
    else if (!strcmp(method,"ppf_dot_inner")) ppf_rc=ppf_dot_threads(&out,dot_ap,dot_bp,dot_length,PPF_RNDN,workers,&context); \
    else if (!strcmp(method,"mpfr_dot_1t")) mpfr_rc=mpfr_dot(mout,dot_map,dot_mbp,(unsigned long)dot_length,MPFR_RNDN); \
    else if (!strcmp(method,"ppf_mul_outer")) { if(run_ppf_mul_outer(outer,jobs_count,workers)) ppf_rc=PPF_TERNARY_ERROR; } \
    else if (!strcmp(method,"mpfr_mul_outer")) run_mpfr_mul_outer(outer,jobs_count,workers); \
    else if (!strcmp(method,"ppf_fma_outer")) { if(run_ppf_fma_outer(outer,jobs_count,workers)) ppf_rc=PPF_TERNARY_ERROR; } \
    else if (!strcmp(method,"mpfr_fma_outer")) run_mpfr_fma_outer(outer,jobs_count,workers); \
    else if (!strcmp(method,"ppf_plan_batch")) { if(ppf_div_with_plan_many(plan_outs,plan_nums,dot_length,plan,PPF_RNDN,workers,ternaries,&context)) ppf_rc=PPF_TERNARY_ERROR; } \
    else if (!strcmp(method,"mpfr_div_outer")) run_mpfr_div_outer(plan_moutp,dot_map,dot_length,mb,workers); \
    else { fprintf(stderr,"unknown method %s\\n",method); return 2; } \
} while(0)

    /* Compute expected values without timing. */
    if (strstr(method,"add")) mpfr_add(expected, ma, mb, MPFR_RNDN);
    else if (strstr(method,"mul") && !strstr(method,"outer"))
        mpfr_mul(expected, ma, mb, MPFR_RNDN);
    else if (!strcmp(method,"ppf_sqr_1t") || !strcmp(method,"ppf_sqr_inner") ||
             !strcmp(method,"mpfr_sqr_1t"))
        mpfr_sqr(expected, ma, MPFR_RNDN);
    else if (strstr(method,"scale")) mpfr_mul_2si(expected, ma, 137, MPFR_RNDN);
    else if (!strcmp(method,"ppf_div") || !strcmp(method,"mpfr_div"))
        mpfr_div(expected, ma, mb, MPFR_RNDN);
    else if (strstr(method,"sqrt") && !strstr(method,"hypot"))
        mpfr_sqrt(expected, ma, MPFR_RNDN);
    else if (strstr(method,"hypot")) mpfr_hypot(expected, ma, mb, MPFR_RNDN);
    else if (strstr(method,"fma")) mpfr_fma(expected, ma, mb, mc, MPFR_RNDN);
    else if (strstr(method,"dot"))
        mpfr_dot(dot_expected, dot_map, dot_mbp,
                 (unsigned long)dot_length, MPFR_RNDN);

    for (int warm = 0; warm < 3; ++warm) RUN_ONCE();
    for (int rep = 0; rep < inner_reps; ++rep) {
        started = now_seconds();
        RUN_ONCE();
        samples[rep] = now_seconds() - started;
    }
    if (ppf_benchmark_median(samples, (size_t)inner_reps, &elapsed) != 0) {
        fprintf(stderr, "unable to compute benchmark median\n");
        return 5;
    }

    if (!strncmp(method,"ppf_",4) || !strncmp(method,"legacy_",7)) {
        if (ppf_rc == PPF_TERNARY_ERROR) verified = 0;
        if (!strstr(method,"outer") && !strstr(method,"plan_batch")) {
            ppf_get_mpfr(mout, &out, MPFR_RNDN);
            if (strstr(method,"dot")) {
                if (!same_value(mout, dot_expected)) verified = 0;
            } else if (!same_value(mout, expected)) verified = 0;
        }
    }
    if (!strcmp(method,"ppf_mul_outer") || !strcmp(method,"mpfr_mul_outer") ||
        !strcmp(method,"ppf_fma_outer") || !strcmp(method,"mpfr_fma_outer")) {
        int is_fma = strstr(method, "fma") != NULL;
        for (size_t i=0;i<jobs_count;++i) {
            mpfr_t check;
            mpfr_init2(check,bits);
            if (is_fma)
                mpfr_fma(check,outer[i].ma,outer[i].mb,outer[i].mc,MPFR_RNDN);
            else
                mpfr_mul(check,outer[i].ma,outer[i].mb,MPFR_RNDN);
            if (!strncmp(method,"ppf_",4))
                ppf_get_mpfr(outer[i].mout,&outer[i].out,MPFR_RNDN);
            if (!same_value(outer[i].mout,check)) verified=0;
            mpfr_clear(check);
        }
    }
    if (!strcmp(method,"ppf_plan_batch")) {
        for(size_t i=0;i<dot_length;++i){
            mpfr_div(expected,dot_ma[i],mb,MPFR_RNDN);
            ppf_get_mpfr(mout,&dot_out[i],MPFR_RNDN);
            if(!same_value(mout,expected))verified=0;
        }
    }
    if (!verified) {
        fprintf(stderr,"verification failed for %s\n",method);
        if (!strstr(method,"outer") && !strstr(method,"plan_batch")) {
            mpfr_printf("actual=%.40Re\nexpected=%.40Re\n", mout, expected);
        }
        return 4;
    }
    (void)mpfr_rc;
    printf("{\"schema_version\":2,\"method\":\"%s\",\"bits\":%lu,"
           "\"workers\":%lu,\"jobs\":%lu,\"warmups\":3,"
           "\"sample_statistic\":\"median\",\"seconds\":%.12f,"
           "\"sample_count\":%d,\"samples_seconds\":",
           method, bits, workers, jobs_count, elapsed, inner_reps);
    ppf_benchmark_print_samples_json(samples, (size_t)inner_reps);
    printf(",\"verified\":true}\n");

    if (plan) ppf_divisor_plan_destroy(plan);
    if (outer) {
        for(size_t i=0;i<jobs_count;++i){
            ppf_clear(&outer[i].a);ppf_clear(&outer[i].b);
            ppf_clear(&outer[i].c);ppf_clear(&outer[i].out);
            ppf_context_clear(&outer[i].context);
            mpfr_clears(outer[i].ma,outer[i].mb,outer[i].mc,
                        outer[i].mout,(mpfr_ptr)0);
        }
        free(outer);
    }
    if (dot_initialized) {
        for(size_t i=0;i<dot_length;++i){
            ppf_clear(&dot_a[i]);ppf_clear(&dot_b[i]);ppf_clear(&dot_out[i]);
            mpfr_clear(dot_ma[i]);mpfr_clear(dot_mb[i]);mpfr_clear(plan_mout[i]);
        }
    }
    ppf_clear(&a);ppf_clear(&b);ppf_clear(&c);ppf_clear(&out);
    ppf_context_clear(&context);
    mpfr_clears(ma,mb,mc,mout,expected,dot_expected,(mpfr_ptr)0);
    mpz_clear(z);gmp_randclear(state);free(samples);
    return 0;
}
