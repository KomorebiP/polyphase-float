/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ppf_internal.h"

#include <stdlib.h>
#include <math.h>
#include "ppf_openmp_compat.h"

#ifndef PPF_ROOT_GUARD_BITS
#define PPF_ROOT_GUARD_BITS 112u
#endif
#ifndef PPF_RECIP_ROOT_GUARD_BITS
#define PPF_RECIP_ROOT_GUARD_BITS 48u
#endif
#ifndef PPF_ROOT_ATTEMPTS
#define PPF_ROOT_ATTEMPTS 10u
#endif

#if defined(__SIZEOF_INT128__)
__extension__ typedef __int128 ppf_i128_t;
__extension__ typedef unsigned __int128 ppf_u128_t;
#endif

static int root_error(ppf_t *rop, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, 1);
    if (ctx) ppf_raise(ctx, PPF_FLAG_ERANGE);
    return PPF_TERNARY_ERROR;
}

static int root_memory_or_error(ppf_t *rop, ppf_context_t *ctx) {
    if (ctx && (ppf_context_flags(ctx) & PPF_FLAG_ALLOC) != 0)
        return ppf_allocation_error(rop, ctx);
    return root_error(rop, ctx);
}

static int root_invalid(ppf_t *rop, int sign, ppf_context_t *ctx) {
    if (rop) ppf_set_nan(rop, sign);
    if (ctx) ppf_raise(ctx, PPF_FLAG_NAN);
    return 0;
}

static int euclidean_remainder_exp(ppf_exp_t value, unsigned long degree,
                                   unsigned long *remainder) {
#if defined(__SIZEOF_INT128__)
    ppf_i128_t v = (ppf_i128_t)value;
    ppf_i128_t n = (ppf_i128_t)degree;
    ppf_i128_t r;
    if (!remainder || degree == 0 || n <= 0) return -1;
    r = v % n;
    if (r < 0) r += n;
    if (r < 0 || r > (ppf_i128_t)ULONG_MAX) return -1;
    *remainder = (unsigned long)r;
    return 0;
#else
    if (!remainder || degree == 0 || degree > (unsigned long)INT64_MAX)
        return -1;
    {
        ppf_exp_t n = (ppf_exp_t)degree;
        ppf_exp_t r = value % n;
        if (r < 0) r += n;
        *remainder = (unsigned long)r;
    }
    return 0;
#endif
}

static int exact_divided_exponent(ppf_exp_t value, unsigned long degree,
                                  unsigned long remainder,
                                  ppf_exp_t *quotient) {
#if defined(__SIZEOF_INT128__)
    ppf_i128_t adjusted, q;
    if (!quotient || degree == 0) return -1;
    adjusted = (ppf_i128_t)value - (ppf_i128_t)remainder;
    q = adjusted / (ppf_i128_t)degree;
    if (q < (ppf_i128_t)INT64_MIN || q > (ppf_i128_t)INT64_MAX)
        return -1;
    *quotient = (ppf_exp_t)q;
    return 0;
#else
    ppf_exp_t adjusted;
    if (!quotient || degree == 0 || degree > (unsigned long)INT64_MAX)
        return -1;
    if (ppf_exp_add(value, -(ppf_exp_t)remainder, &adjusted) != 0)
        return -1;
    *quotient = adjusted / (ppf_exp_t)degree;
    return 0;
#endif
}

static int root_interval_from_exact(ppf_t *rop, mpz_srcptr magnitude,
                                    ppf_exp_t exponent,
                                    unsigned long degree, int result_sign,
                                    ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t adjusted, radicand, root, lower, upper;
    size_t bits, approximate_root_bits;
    ppf_prec_t desired;
    unsigned long exp_remainder;
    ppf_exp_t exponent_quotient, base;
    uint64_t shift;
    int ternary = 0;
    ppf_interval_status_t status;

    if (!rop || !magnitude || mpz_sgn(magnitude) <= 0 || degree == 0)
        return root_error(rop, ctx);
    if (degree == 1) {
        mpz_t signed_value;
        int rc;
        mpz_init_set(signed_value, magnitude);
        if (result_sign < 0) mpz_neg(signed_value, signed_value);
        rc = ppf_set_z_2exp(rop, signed_value, exponent, rnd, ctx);
        mpz_clear(signed_value);
        return rc;
    }
    if (euclidean_remainder_exp(exponent, degree, &exp_remainder) != 0 ||
        exact_divided_exponent(exponent, degree, exp_remainder,
                               &exponent_quotient) != 0)
        return root_error(rop, ctx);

    mpz_inits(adjusted, radicand, root, lower, upper, NULL);
    mpz_set(adjusted, magnitude);
    if (exp_remainder != 0)
        mpz_mul_2exp(adjusted, adjusted, (mp_bitcnt_t)exp_remainder);
    bits = mpz_sizeinbase(adjusted, 2);
    approximate_root_bits = (bits - 1u) / (size_t)degree + 1u;
    desired = rop->precision + PPF_ROOT_GUARD_BITS;
    if (desired < rop->precision) {
        mpz_clears(adjusted, radicand, root, lower, upper, NULL);
        return root_error(rop, ctx);
    }
    shift = desired > approximate_root_bits
        ? (uint64_t)(desired - approximate_root_bits) : 0u;

    for (unsigned attempt = 0; attempt < PPF_ROOT_ATTEMPTS; ++attempt) {
#if defined(__SIZEOF_INT128__)
        ppf_u128_t power = (ppf_u128_t)degree * (ppf_u128_t)shift;
        if (power > (ppf_u128_t)ULONG_MAX) {
            mpz_clears(adjusted, radicand, root, lower, upper, NULL);
            return root_error(rop, ctx);
        }
        mpz_mul_2exp(radicand, adjusted, (mp_bitcnt_t)power);
#else
        if (shift != 0 && degree > ULONG_MAX / shift) {
            mpz_clears(adjusted, radicand, root, lower, upper, NULL);
            return root_error(rop, ctx);
        }
        mpz_mul_2exp(radicand, adjusted,
                     (mp_bitcnt_t)(degree * (unsigned long)shift));
#endif
        {
            int exact_root = mpz_root(root, radicand, degree);
            if (shift > (uint64_t)INT64_MAX ||
                ppf_exp_add(exponent_quotient, -(ppf_exp_t)shift, &base) != 0) {
                mpz_clears(adjusted, radicand, root, lower, upper, NULL);
                return root_error(rop, ctx);
            }
            if (exact_root) {
                int rc;
                if (result_sign < 0) mpz_neg(root, root);
                rc = ppf_set_z_2exp(rop, root, base, rnd, ctx);
                mpz_clears(adjusted, radicand, root, lower, upper, NULL);
                return rc;
            }
        }

        if (result_sign > 0) {
            mpz_set(lower, root);
            mpz_add_ui(upper, root, 1);
        } else {
            mpz_neg(upper, root);
            mpz_sub_ui(lower, upper, 1);
        }
        status = ppf_set_dyadic_interval(rop, lower, upper, base,
                                         rnd, &ternary, ctx);
        if (status == PPF_INTERVAL_DECIDED) {
            mpz_clears(adjusted, radicand, root, lower, upper, NULL);
            return ternary;
        }
        shift += 64u;
    }
    mpz_clears(adjusted, radicand, root, lower, upper, NULL);
    return root_error(rop, ctx);
}

static int reciprocal_root_interval_from_exact(
    ppf_t *rop, mpz_srcptr magnitude, ppf_exp_t exponent,
    unsigned long degree, int result_sign,
    ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t radicand, root, numerator, quotient, division_remainder;
    mpz_t lower, upper, temporary, denominator_plus_one, correction;
    size_t magnitude_bits;
    ppf_prec_t desired;
    unsigned long exponent_remainder;
    ppf_exp_t exponent_quotient;
    int ternary = 0;
    ppf_interval_status_t status;

    if (!rop || !magnitude || mpz_sgn(magnitude) <= 0 || degree == 0)
        return root_error(rop, ctx);
    magnitude_bits = mpz_sizeinbase(magnitude, 2);
    desired = rop->precision + PPF_RECIP_ROOT_GUARD_BITS;
    if (desired < rop->precision ||
        euclidean_remainder_exp(exponent, degree, &exponent_remainder) != 0 ||
        exact_divided_exponent(exponent, degree, exponent_remainder,
                               &exponent_quotient) != 0)
        return root_error(rop, ctx);

    mpz_inits(radicand, root, numerator, quotient, division_remainder,
              lower, upper, temporary, denominator_plus_one, correction,
              NULL);

    for (unsigned attempt = 0; attempt < PPF_ROOT_ATTEMPTS; ++attempt) {
        uint64_t root_shift, root_shift_quotient, root_bits, numerator_power;
        unsigned long shift_mod, adjustment;
        ppf_exp_t quotient_scale, interval_exponent;
        int exact_root;
#if defined(__SIZEOF_INT128__)
        ppf_u128_t target = (ppf_u128_t)degree *
                            (ppf_u128_t)(desired + 64u * attempt);
        ppf_i128_t scale128;
        if (target > (ppf_u128_t)magnitude_bits)
            target -= (ppf_u128_t)magnitude_bits;
        else
            target = 0;
        if (target > (ppf_u128_t)UINT64_MAX) goto fail;
        root_shift = (uint64_t)target;
#else
        uint64_t work = desired + 64u * attempt;
        if (work < desired || degree > UINT64_MAX / work) goto fail;
        root_shift = (uint64_t)degree * work;
        root_shift = root_shift > magnitude_bits
            ? root_shift - (uint64_t)magnitude_bits : 0u;
#endif
        shift_mod = (unsigned long)(root_shift % degree);
        adjustment = exponent_remainder >= shift_mod
            ? exponent_remainder - shift_mod
            : degree - (shift_mod - exponent_remainder);
        if (root_shift > UINT64_MAX - adjustment) goto fail;
        root_shift += adjustment;
        if (root_shift > (uint64_t)ULONG_MAX) goto fail;

        mpz_mul_2exp(radicand, magnitude, (mp_bitcnt_t)root_shift);
        exact_root = mpz_root(root, radicand, degree);
        root_bits = (uint64_t)mpz_sizeinbase(root, 2);
        if (root_bits > UINT64_MAX - (desired + 64u * attempt)) goto fail;
        numerator_power = root_bits + desired + 64u * attempt;
        if (numerator_power > (uint64_t)ULONG_MAX) goto fail;

        root_shift_quotient =
            (root_shift - (uint64_t)exponent_remainder) / degree;
#if defined(__SIZEOF_INT128__)
        scale128 = (ppf_i128_t)numerator_power -
                   ((ppf_i128_t)root_shift_quotient -
                    (ppf_i128_t)exponent_quotient);
        if (scale128 < (ppf_i128_t)INT64_MIN ||
            scale128 > (ppf_i128_t)INT64_MAX)
            goto fail;
        quotient_scale = (ppf_exp_t)scale128;
#else
        if (root_shift_quotient > (uint64_t)INT64_MAX) goto fail;
        if (ppf_exp_add((ppf_exp_t)root_shift_quotient,
                        -exponent_quotient, &interval_exponent) != 0 ||
            ppf_exp_add((ppf_exp_t)numerator_power,
                        -interval_exponent, &quotient_scale) != 0)
            goto fail;
#endif
        if (quotient_scale == INT64_MIN) goto fail;
        interval_exponent = -quotient_scale;

        mpz_set_ui(numerator, 0);
        mpz_setbit(numerator, (mp_bitcnt_t)numerator_power);
        mpz_fdiv_qr(quotient, division_remainder, numerator, root);

        mpz_set(lower, quotient);
        mpz_set(upper, quotient);
        if (mpz_sgn(division_remainder) != 0)
            mpz_add_ui(upper, upper, 1);

        /* If the integer root was truncated, the true root lies in
           [root, root+1).  Derive floor(2^N/(root+1)) from the quotient and
           remainder of 2^N/root, avoiding a second full-size division. */
        if (!exact_root) {
            mpz_add_ui(denominator_plus_one, root, 1);
            if (mpz_cmp(division_remainder, quotient) < 0) {
                mpz_sub(temporary, quotient, division_remainder);
                mpz_cdiv_q(correction, temporary, denominator_plus_one);
                mpz_sub(lower, lower, correction);
            }
        }

        if (result_sign < 0) {
            mpz_set(temporary, lower);
            mpz_neg(lower, upper);
            mpz_neg(upper, temporary);
        }
        status = ppf_set_dyadic_interval(rop, lower, upper,
                                         interval_exponent, rnd,
                                         &ternary, ctx);
        if (status == PPF_INTERVAL_DECIDED) {
            mpz_clears(radicand, root, numerator, quotient,
                       division_remainder, lower, upper, temporary,
                       denominator_plus_one, correction, NULL);
            return ternary;
        }
    }

fail:
    mpz_clears(radicand, root, numerator, quotient, division_remainder,
               lower, upper, temporary, denominator_plus_one, correction,
               NULL);
    return root_error(rop, ctx);
}


#ifndef PPF_RECSQRT_NEWTON_MIN_BITS
#define PPF_RECSQRT_NEWTON_MIN_BITS 32768u
#endif
#ifndef PPF_RECSQRT_INPUT_GUARD_BITS
#define PPF_RECSQRT_INPUT_GUARD_BITS 12u
#endif
#ifndef PPF_RECSQRT_CORRECTION_GUARD_BITS
#define PPF_RECSQRT_CORRECTION_GUARD_BITS 8u
#endif
#ifndef PPF_RECSQRT_RECURSION_SLACK
#define PPF_RECSQRT_RECURSION_SLACK 10u
#endif
#ifndef PPF_RECSQRT_WORK_GUARD_BITS
#define PPF_RECSQRT_WORK_GUARD_BITS 12u
#endif
#ifndef PPF_RECSQRT_PARALLEL_MIN_LIMBS
#define PPF_RECSQRT_PARALLEL_MIN_LIMBS 4096u
#endif

static void recsqrt_round_signed_pow2(mpz_t out, mpz_t magnitude,
                                      mpz_srcptr value,
                                      mp_bitcnt_t shift) {
    int sign = mpz_sgn(value);
    if (shift == 0) { mpz_set(out, value); return; }
    mpz_abs(magnitude, value);
    mpz_fdiv_q_2exp(out, magnitude, shift);
    if (mpz_tstbit(magnitude, shift - 1)) {
        int sticky = shift > 1 && mpz_scan1(magnitude, 0) < shift - 1;
        if (sticky || mpz_odd_p(out)) mpz_add_ui(out, out, 1);
    }
    if (sign < 0) mpz_neg(out, out);
}

static int recsqrt_truncate_input(mpz_t out, mp_bitcnt_t *out_d,
                                  mpz_srcptr magnitude, mp_bitcnt_t d,
                                  unsigned long target) {
    size_t bits = mpz_sizeinbase(magnitude, 2);
    size_t keep = (size_t)target + PPF_RECSQRT_INPUT_GUARD_BITS;
    if (bits > keep) {
        mp_bitcnt_t cut = (mp_bitcnt_t)(bits - keep);
        if (d < cut) return -1;
        mpz_fdiv_q_2exp(out, magnitude, cut);
        *out_d = d - cut;
    } else {
        mpz_set(out, magnitude);
        *out_d = d;
    }
    return 0;
}

/* Exact three-product Karatsuba for the large products in the fused inverse
   square-root refinement.  It is used only for balanced operands and one
   active OpenMP level; all reconstruction is exact. */
static void recsqrt_parallel_mul(mpz_t out, mpz_srcptr a, mpz_srcptr b,
                                 unsigned long workers,
                                 ppf_product_algorithm_t algorithm) {
    size_t an = (size_t)mpz_size(a), bn = (size_t)mpz_size(b);
    size_t n = an > bn ? an : bn;
    size_t split = (n + 1u) / 2u;
    int sign = mpz_sgn(a) * mpz_sgn(b);
#ifndef PPF_RECSQRT_TOOM3_MIN_LIMBS
#define PPF_RECSQRT_TOOM3_MIN_LIMBS 60000u
#endif
    /* Rank selection is explicit for tuning and conservative in AUTO.
       The five-channel path is most useful once its child products are large
       enough to amortize direct-limb evaluation and interpolation. */
    if (algorithm == PPF_PRODUCT_FULL) {
        mpz_mul(out, a, b);
        return;
    }
    if (algorithm == PPF_PRODUCT_TOOM3 &&
        workers >= 5 && !omp_in_parallel() && an == bn &&
        n >= PPF_RECSQRT_PARALLEL_MIN_LIMBS) {
        mp_ptr target = mpz_limbs_write(out, (mp_size_t)(2u * n));
        size_t used;
        (void)ppf_toom3_limb_product_internal(
            target, mpz_limbs_read(a), mpz_limbs_read(b),
            (mp_size_t)n, workers, a == b);
        used = 2u * n;
        while (used && target[used - 1] == 0) --used;
        mpz_limbs_finish(out, sign < 0 ? -(mp_size_t)used : (mp_size_t)used);
        return;
    }
    if (workers < 3 || omp_in_parallel() ||
        n < PPF_RECSQRT_PARALLEL_MIN_LIMBS ||
        an <= split || bn <= split) {
        mpz_mul(out, a, b);
        return;
    }
    {
        mp_srcptr ap = mpz_limbs_read(a), bp = mpz_limbs_read(b);
        mpz_t al, ah, bl, bh, z0, z1, z2, sa, sb, temp;
        mpz_roinit_n(al, ap, (mp_size_t)split);
        mpz_roinit_n(ah, ap + split, (mp_size_t)(an - split));
        mpz_roinit_n(bl, bp, (mp_size_t)split);
        mpz_roinit_n(bh, bp + split, (mp_size_t)(bn - split));
        mpz_inits(z0, z1, z2, sa, sb, temp, NULL);
PPF_OMP_PRAGMA(omp parallel sections num_threads(3))
        {
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(z0, al, bl); }
PPF_OMP_PRAGMA(omp section)
            { mpz_mul(z2, ah, bh); }
PPF_OMP_PRAGMA(omp section)
            {
                mpz_add(sa, al, ah);
                mpz_add(sb, bl, bh);
                mpz_mul(z1, sa, sb);
            }
        }
        mpz_sub(z1, z1, z0);
        mpz_sub(z1, z1, z2);
        mpz_mul_2exp(out, z2,
                     (mp_bitcnt_t)(2u * split * GMP_NUMB_BITS));
        mpz_mul_2exp(temp, z1,
                     (mp_bitcnt_t)(split * GMP_NUMB_BITS));
        mpz_add(out, out, temp);
        mpz_add(out, out, z0);
        if (sign < 0) mpz_neg(out, out);
        mpz_clears(z0, z1, z2, sa, sb, temp, NULL);
    }
}

/* Return an integer X enclosing A^(-1/2) at precision p.  Recursive Newton
   refinement uses X <- X + X(1-AX^2)/2.  Only the high residual bits capable
   of changing the p-bit result are retained; the omitted correction is less
   than 2^-g of one output ulp. */
static int recsqrt_fixed_truncated(mpz_t result, mpz_srcptr magnitude,
                                   mp_bitcnt_t d, unsigned long p,
                                   unsigned long workers,
                                   ppf_product_algorithm_t algorithm) {
    enum { BASE_BITS = 40 };
    unsigned long levels[64], current = p;
    size_t count = 0;
    mpz_t truncated, x, x0, square, product, low_window, residual;
    mpz_t correction, delta, abs_correction;
    mp_bitcnt_t td;

    while (current > BASE_BITS) {
        if (count >= sizeof(levels) / sizeof(levels[0])) return -1;
        levels[count++] = current;
        current = (current + PPF_RECSQRT_RECURSION_SLACK + 1u) / 2u;
    }
    mpz_inits(truncated, x, x0, square, product, low_window, residual,
              correction, delta, abs_correction, NULL);
    if (recsqrt_truncate_input(truncated, &td, magnitude, d, current) != 0)
        goto fail;
    {
        signed long e2 = 0;
        double fraction = mpz_get_d_2exp(&e2, truncated);
        long de = e2 - (long)td;
        long double A = ldexpl((long double)fraction, (int)de);
        long double y = 1.0L / sqrtl(A);
        long double scaled = ldexpl(y, (int)current);
        mpz_set_ui(x, (unsigned long long)floorl(scaled + 0.5L));
    }

    while (count != 0) {
        unsigned long target = levels[--count], h = current;
        mp_bitcnt_t full_shift, cut, kept_shift, threshold_pos;
        int low_nonzero;
        if (h > (ULONG_MAX - 1u) / 3u ||
            target > ULONG_MAX - PPF_RECSQRT_CORRECTION_GUARD_BITS - 8u)
            goto fail;
        if (recsqrt_truncate_input(truncated, &td, magnitude, d, target) != 0)
            goto fail;
        mpz_mul_2exp(x0, x, (mp_bitcnt_t)(target - h));
        recsqrt_parallel_mul(square, x, x, workers, algorithm);
        recsqrt_parallel_mul(product, truncated, square, workers, algorithm);

        if (td > (mp_bitcnt_t)(~(mp_bitcnt_t)0) -
                 (mp_bitcnt_t)(3u * h + 1u - target))
            goto fail;
        full_shift = td + (mp_bitcnt_t)(3u * h + 1u - target);
        cut = full_shift > (mp_bitcnt_t)(h + PPF_RECSQRT_CORRECTION_GUARD_BITS)
            ? full_shift -
              (mp_bitcnt_t)(h + PPF_RECSQRT_CORRECTION_GUARD_BITS) : 0;
        kept_shift = full_shift - cut;
        threshold_pos = td + (mp_bitcnt_t)(2u * h);
        low_nonzero = cut != 0 && !mpz_divisible_2exp_p(product, cut);
        /* Compute floor((2^threshold-product)/2^cut) exactly.  The former
           window extractor copied only the low h-bit slice and could miss the
           distant threshold bit for one exponent parity.  Keeping the full
           quotient here is still much smaller than the discarded full
           correction product, and the final truncation error is below 2^-g
           output ulp by construction. */
        mpz_fdiv_q_2exp(low_window, product, cut);
        mpz_set_ui(residual, 0);
        mpz_setbit(residual, threshold_pos - cut);
        mpz_sub(residual, residual, low_window);
        if (low_nonzero) mpz_sub_ui(residual, residual, 1);
        recsqrt_parallel_mul(correction, x, residual, workers, algorithm);
        recsqrt_round_signed_pow2(delta, abs_correction, correction,
                                  kept_shift);
        mpz_add(x, x0, delta);
        current = target;
    }
    mpz_set(result, x);
    mpz_clears(truncated, x, x0, square, product, low_window, residual,
               correction, delta, abs_correction, NULL);
    return 0;
fail:
    mpz_clears(truncated, x, x0, square, product, low_window, residual,
               correction, delta, abs_correction, NULL);
    return -1;
}


static int recsqrt_objects_same(const ppf_t *a, const ppf_t *b) {
    if (!a || !b || a->kind != b->kind || a->sign != b->sign ||
        a->precision != b->precision || a->exponent != b->exponent ||
        a->used_limbs != b->used_limbs)
        return 0;
    if (a->used_limbs == 0) return 1;
    return memcmp(a->limbs, b->limbs,
                  a->used_limbs * sizeof(*a->limbs)) == 0;
}

/* Compare a positive finite candidate c with 1/sqrt(a) exactly by testing
   c^2*a against one.  This is used only when a narrow interval settles the
   rounded value but straddles that value, so the generic interval layer cannot
   determine the ternary direction. */
static int recsqrt_exact_ternary(const ppf_t *candidate, const ppf_t *a) {
    mpz_t square, product;
    size_t bits;
#if defined(__SIZEOF_INT128__)
    ppf_i128_t exponent;
#else
    ppf_exp_t ce, ae, exponent;
#endif
    int cmp;
    if (ppf_inf_p(candidate)) return 1;
    if (ppf_zero_p(candidate)) return -1;
    if (!ppf_regular_p(candidate) || candidate->sign < 0 ||
        !ppf_regular_p(a) || a->sign < 0)
        return PPF_TERNARY_ERROR;
    {
        mpz_t cv, av;
        mpz_roinit_n(cv, candidate->limbs,
                     (mp_size_t)candidate->used_limbs);
        mpz_roinit_n(av, a->limbs, (mp_size_t)a->used_limbs);
        mpz_inits(square, product, NULL);
        mpz_mul(square, cv, cv);
        mpz_mul(product, square, av);
    }
#if defined(__SIZEOF_INT128__)
    exponent = 2 * ((ppf_i128_t)candidate->exponent -
                    (ppf_i128_t)candidate->precision) +
               ((ppf_i128_t)a->exponent - (ppf_i128_t)a->precision);
#else
    if (ppf_exp_sub_prec(candidate->exponent, candidate->precision, &ce) != 0 ||
        ppf_exp_sub_prec(a->exponent, a->precision, &ae) != 0 ||
        ppf_exp_add(ce, ce, &exponent) != 0 ||
        ppf_exp_add(exponent, ae, &exponent) != 0) {
        mpz_clears(square, product, NULL);
        return PPF_TERNARY_ERROR;
    }
#endif
    bits = mpz_sizeinbase(product, 2);
#if defined(__SIZEOF_INT128__)
    if (exponent >= 0) {
        cmp = (exponent == 0 && mpz_cmp_ui(product, 1) == 0) ? 0 : 1;
    } else {
        ppf_i128_t k128 = -exponent;
        if (k128 > (ppf_i128_t)SIZE_MAX) cmp = -1;
        else {
            size_t k = (size_t)k128;
            if (bits - 1u < k) cmp = -1;
            else if (bits - 1u > k) cmp = 1;
            else cmp = (mpz_scan1(product, 0) == (mp_bitcnt_t)k) ? 0 : 1;
        }
    }
#else
    if (exponent >= 0) {
        cmp = (exponent == 0 && mpz_cmp_ui(product, 1) == 0) ? 0 : 1;
    } else {
        uint64_t k64 = (uint64_t)(-(exponent + 1)) + 1u;
        if (k64 > SIZE_MAX) cmp = -1;
        else {
            size_t k = (size_t)k64;
            if (bits - 1u < k) cmp = -1;
            else if (bits - 1u > k) cmp = 1;
            else cmp = (mpz_scan1(product, 0) == (mp_bitcnt_t)k) ? 0 : 1;
        }
    }
#endif
    mpz_clears(square, product, NULL);
    return cmp;
}


static ppf_interval_status_t recsqrt_resolve_interval(
    ppf_t *rop, mpz_srcptr lower, mpz_srcptr upper, ppf_exp_t exponent,
    const ppf_t *input, ppf_rnd_t rnd, int *ternary, ppf_context_t *ctx) {
    ppf_interval_status_t status;
    ppf_t lo, hi;
    ppf_context_t lo_ctx, hi_ctx;
    ppf_flags_t original;
    int tlo, thi, exact_t;
    memset(&lo, 0, sizeof(lo)); memset(&hi, 0, sizeof(hi));
    status = ppf_set_dyadic_interval(rop, lower, upper, exponent,
                                     rnd, ternary, ctx);
    if (status != PPF_INTERVAL_UNDECIDED) return status;
    original = ctx->flags;
    if (ppf_context_clone(&lo_ctx, ctx) != 0) return PPF_INTERVAL_ERROR;
    if (ppf_context_clone(&hi_ctx, ctx) != 0) {
        ppf_context_clear(&lo_ctx); return PPF_INTERVAL_ERROR;
    }
    lo_ctx.flags = hi_ctx.flags = 0;
    if (ppf_init2_ctx(&lo, rop->precision, &lo_ctx) != 0 ||
        ppf_init2_ctx(&hi, rop->precision, &hi_ctx) != 0) {
        ctx->flags = original | lo_ctx.flags | hi_ctx.flags;
        ppf_clear(&lo); ppf_clear(&hi);
        ppf_context_clear(&lo_ctx); ppf_context_clear(&hi_ctx);
        return PPF_INTERVAL_ERROR;
    }
    tlo = ppf_set_z_2exp(&lo, lower, exponent, rnd, &lo_ctx);
    thi = ppf_set_z_2exp(&hi, upper, exponent, rnd, &hi_ctx);
    (void)tlo; (void)thi;
    if (!recsqrt_objects_same(&lo, &hi)) {
        ppf_t *selected = NULL;
        ppf_context_t *selected_ctx = NULL;
        int relation;
        /* For positive reciprocal roots, a narrow interval that crosses one
           representable boundary can still settle every directed mode. */
        if (rnd == PPF_RNDD || rnd == PPF_RNDZ) {
            relation = recsqrt_exact_ternary(&hi, input);
            if (relation == PPF_TERNARY_ERROR) goto interval_error;
            selected = relation <= 0 ? &hi : &lo;
            selected_ctx = relation <= 0 ? &hi_ctx : &lo_ctx;
        } else if (rnd == PPF_RNDU || rnd == PPF_RNDA) {
            relation = recsqrt_exact_ternary(&lo, input);
            if (relation == PPF_TERNARY_ERROR) goto interval_error;
            selected = relation >= 0 ? &lo : &hi;
            selected_ctx = relation >= 0 ? &lo_ctx : &hi_ctx;
        } else if (rnd == PPF_RNDF) {
            selected = &lo;
            selected_ctx = &lo_ctx;
        } else {
            ppf_clear(&lo); ppf_clear(&hi);
            ppf_context_clear(&lo_ctx); ppf_context_clear(&hi_ctx);
            return PPF_INTERVAL_UNDECIDED;
        }
        exact_t = recsqrt_exact_ternary(selected, input);
        if (exact_t == PPF_TERNARY_ERROR) goto interval_error;
        ppf_swap(rop, selected);
        ctx->flags = original | selected_ctx->flags;
    } else {
        exact_t = recsqrt_exact_ternary(&lo, input);
        if (exact_t == PPF_TERNARY_ERROR) goto interval_error;
        ppf_swap(rop, &lo);
        ctx->flags = original | lo_ctx.flags | hi_ctx.flags;
    }
    if (exact_t == 0) ctx->flags &= ~PPF_FLAG_INEXACT;
    else ctx->flags |= PPF_FLAG_INEXACT;
    if (ternary) *ternary = exact_t;
    ppf_clear(&lo); ppf_clear(&hi);
    ppf_context_clear(&lo_ctx); ppf_context_clear(&hi_ctx);
    return PPF_INTERVAL_DECIDED;

interval_error:
    ctx->flags = original | lo_ctx.flags | hi_ctx.flags;
    ppf_clear(&lo); ppf_clear(&hi);
    ppf_context_clear(&lo_ctx); ppf_context_clear(&hi_ctx);
    return PPF_INTERVAL_ERROR;
}

static int recsqrt_newton_positive(ppf_t *rop, const ppf_t *a,
                                   ppf_rnd_t rnd, unsigned long workers,
                                   ppf_product_algorithm_t algorithm,
                                   ppf_context_t *ctx) {
    mpz_t magnitude_view, approximation, lower, upper;
    ppf_exp_t result_exponent, interval_exponent;
    int parity, ternary = 0;
    ppf_interval_status_t status;
#if defined(__SIZEOF_INT128__)
    ppf_i128_t re;
#endif
    mpz_roinit_n(magnitude_view, a->limbs, (mp_size_t)a->used_limbs);
    mpz_inits(approximation, lower, upper, NULL);
    parity = 1 - ((uint64_t)a->exponent & 1u);
#if defined(__SIZEOF_INT128__)
    re = -((ppf_i128_t)a->exponent - 1 - parity) / 2;
    if (re < (ppf_i128_t)INT64_MIN || re > (ppf_i128_t)INT64_MAX) goto fail;
    result_exponent = (ppf_exp_t)re;
#else
    if (a->exponent == INT64_MIN) goto fail;
    result_exponent = -(a->exponent - 1 - parity) / 2;
#endif
    for (unsigned attempt = 0; attempt < PPF_ROOT_ATTEMPTS; ++attempt) {
        unsigned long work;
        if (rop->precision > (ppf_prec_t)ULONG_MAX -
                             PPF_RECSQRT_WORK_GUARD_BITS - 64u * attempt)
            goto fail;
        work = (unsigned long)rop->precision +
               PPF_RECSQRT_WORK_GUARD_BITS + 64u * attempt;
        if (recsqrt_fixed_truncated(
                approximation, magnitude_view,
                (mp_bitcnt_t)(a->precision - 1u - (ppf_prec_t)parity),
                work, workers, algorithm) != 0)
            goto fail;
        mpz_sub_ui(lower, approximation, 2);
        mpz_add_ui(upper, approximation, 2);
        if (ppf_exp_add(result_exponent, -(ppf_exp_t)work,
                        &interval_exponent) != 0)
            goto fail;
        status = recsqrt_resolve_interval(rop, lower, upper,
                                            interval_exponent, a, rnd,
                                            &ternary, ctx);
        if (status == PPF_INTERVAL_DECIDED) {
            mpz_clears(approximation, lower, upper, NULL);
            return ternary;
        }
    }
fail:
    mpz_clears(approximation, lower, upper, NULL);
    return PPF_TERNARY_ERROR;
}

static int rootn_dispatch(ppf_t *rop, const ppf_t *a,
                          unsigned long degree, int reciprocal,
                          ppf_rnd_t rnd, ppf_context_t *ctx,
                          int sqrt_zero_rule) {
    mpz_t z;
    ppf_exp_t exponent;
    int odd, result_sign, rc;

    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !ctx || !ppf_rounding_valid(rnd))
        return root_error(rop, ctx);
    if (degree == 0) return root_invalid(rop, 1, ctx);
    odd = (degree & 1u) != 0;
    if (ppf_nan_p(a)) return root_invalid(rop, a->sign, ctx);
    if (ppf_zero_p(a)) {
        result_sign = odd ? a->sign : 1;
        if (sqrt_zero_rule && degree == 2 && !reciprocal)
            result_sign = a->sign;
        if (reciprocal) {
            ppf_set_inf(rop, result_sign);
            ppf_raise(ctx, PPF_FLAG_DIVBY0);
        } else {
            ppf_set_zero(rop, result_sign);
        }
        return 0;
    }
    if (ppf_inf_p(a)) {
        if (a->sign < 0 && !odd) return root_invalid(rop, 1, ctx);
        result_sign = a->sign < 0 ? -1 : 1;
        if (reciprocal) ppf_set_zero(rop, result_sign);
        else ppf_set_inf(rop, result_sign);
        return 0;
    }
    if (a->sign < 0 && !odd) return root_invalid(rop, 1, ctx);
    result_sign = a->sign < 0 ? -1 : 1;
    if (ppf_exp_sub_prec(a->exponent, a->precision, &exponent) != 0)
        return root_error(rop, ctx);
    mpz_roinit_n(z, a->limbs, (mp_size_t)a->used_limbs);
    if (reciprocal)
        rc = reciprocal_root_interval_from_exact(rop, z, exponent, degree,
                                                  result_sign, rnd, ctx);
    else
        rc = root_interval_from_exact(rop, z, exponent, degree,
                                      result_sign, rnd, ctx);
    return rc;
}

int ppf_rootn_ui(ppf_t *rop, const ppf_t *a, unsigned long degree,
                 ppf_rnd_t rnd, ppf_context_t *ctx) {
    return rootn_dispatch(rop, a, degree, 0, rnd, ctx, 0);
}

int ppf_rootn_si(ppf_t *rop, const ppf_t *a, long degree,
                 ppf_rnd_t rnd, ppf_context_t *ctx) {
    unsigned long magnitude;
    if (degree >= 0)
        return ppf_rootn_ui(rop, a, (unsigned long)degree, rnd, ctx);
    magnitude = (unsigned long)(-(degree + 1L)) + 1UL;
    return rootn_dispatch(rop, a, magnitude, 1, rnd, ctx, 0);
}

int ppf_root(ppf_t *rop, const ppf_t *a, unsigned long degree,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    if (a && ppf_zero_p(a)) {
        if (degree == 0) return root_invalid(rop, 1, ppf_resolve_context(ctx));
        ppf_set_zero(rop, a->sign); /* deprecated MPFR root convention */
        return 0;
    }
    return ppf_rootn_ui(rop, a, degree, rnd, ctx);
}

int ppf_sqrt_ui(ppf_t *rop, unsigned long value,
                ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_t temp;
    ppf_prec_t precision = sizeof(unsigned long) * CHAR_BIT;
    int rc;
    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx) return root_error(rop, ctx);
    if (precision < PPF_PREC_MIN) precision = PPF_PREC_MIN;
    if (ppf_init2_ctx(&temp, precision, ctx) != 0)
        return root_memory_or_error(rop, ctx);
    if (ppf_set_ui(&temp, value, PPF_RNDN, ctx) == PPF_TERNARY_ERROR) {
        ppf_clear(&temp);
        return root_error(rop, ctx);
    }
    rc = ppf_sqrt(rop, &temp, rnd, ctx);
    ppf_clear(&temp);
    return rc;
}

int ppf_rec_sqrt_algorithm_threads(
    ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd,
    unsigned long workers, ppf_product_algorithm_t algorithm,
    ppf_context_t *ctx) {
    ppf_product_plan_t plan;
    ppf_t temporary;
    int rc;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !ctx || !ppf_rounding_valid(rnd) ||
        algorithm < PPF_PRODUCT_AUTO || algorithm > PPF_PRODUCT_TOOM3)
        return root_error(rop, ctx);
    if (rop == a) {
        if (ppf_init2_ctx(&temporary, rop->precision, ctx) != 0) {
            /* The in-place form is transactional: failure to create its
               private destination must not destroy the source value. */
            return PPF_TERNARY_ERROR;
        }
        rc = ppf_rec_sqrt_algorithm_threads(
            &temporary, a, rnd, workers, algorithm, ctx);
        if (rc != PPF_TERNARY_ERROR) ppf_swap(rop, &temporary);
        ppf_clear(&temporary);
        return rc;
    }
    if (!ppf_regular_p(a) || a->sign < 0 ||
        rop->precision < PPF_RECSQRT_NEWTON_MIN_BITS)
        return rootn_dispatch(rop, a, 2UL, 1, rnd, ctx, 0);
    if (ppf_product_plan_internal(
            &plan, a, NULL, PPF_PRODUCT_OP_REC_SQRT,
            workers ? workers : 1u, algorithm) != 0)
        return root_error(rop, ctx);
    rc = recsqrt_newton_positive(rop, a, rnd,
                                 plan.effective_workers,
                                 plan.algorithm, ctx);
    if (rc != PPF_TERNARY_ERROR) return rc;
    /* Exact powers or rare interval ambiguities use the older independent
       root-plus-quotient certificate.  This is a native fallback, never an
       MPFR arithmetic call. */
    return rootn_dispatch(rop, a, 2UL, 1, rnd, ctx, 0);
}

int ppf_rec_sqrt_threads(ppf_t *rop, const ppf_t *a,
                         ppf_rnd_t rnd, unsigned long workers,
                         ppf_context_t *ctx) {
    return ppf_rec_sqrt_algorithm_threads(
        rop, a, rnd, workers, PPF_PRODUCT_AUTO, ctx);
}

int ppf_rec_sqrt(ppf_t *rop, const ppf_t *a,
                 ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_rec_sqrt_threads(rop, a, rnd, 1u, ctx);
}

int ppf_cbrt(ppf_t *rop, const ppf_t *a,
             ppf_rnd_t rnd, ppf_context_t *ctx) {
    return ppf_rootn_ui(rop, a, 3UL, rnd, ctx);
}

static size_t hypot_trim(mp_srcptr value, size_t used) {
    while (used && value[used - 1] == 0) --used;
    return used;
}

static size_t hypot_bit_length(mp_srcptr value, size_t used) {
    mp_limb_t top;
    unsigned leading;
    used = hypot_trim(value, used);
    if (!used) return 0;
    top = value[used - 1];
#if ULONG_MAX == UINT64_MAX
    leading = (unsigned)__builtin_clzl((unsigned long)top);
    return (used - 1) * PPF_LIMB_BITS
         + (sizeof(unsigned long) * CHAR_BIT - leading);
#else
    {
        size_t bits = 0;
        while (top) { ++bits; top >>= 1; }
        return (used - 1) * PPF_LIMB_BITS + bits;
    }
#endif
}

static void hypot_copy_lshift(mp_ptr out, size_t out_size,
                              mp_srcptr value, size_t used, size_t shift) {
    size_t limb_offset = shift / PPF_LIMB_BITS;
    unsigned bit_offset = (unsigned)(shift % PPF_LIMB_BITS);
    mp_limb_t carry = 0;
    memset(out, 0, out_size * sizeof(*out));
    if (!used || limb_offset >= out_size) return;
    if (used > out_size - limb_offset) used = out_size - limb_offset;
    if (bit_offset) {
        carry = mpn_lshift(out + limb_offset, value,
                           (mp_size_t)used, bit_offset);
        if (carry && limb_offset + used < out_size)
            out[limb_offset + used] = carry;
    } else {
        memcpy(out + limb_offset, value, used * sizeof(*out));
    }
}

static int ppf_hypot_direct(ppf_t *rop, const ppf_t *a, const ppf_t *b,
                            ppf_rnd_t rnd, ppf_context_t *ctx) {
    ppf_exp_t ea, eb, square_a_base, square_b_base, common, object_exponent;
    uint64_t shift_a64, shift_b64, maximum_shift;
    size_t square_a_size, square_b_size, square_a_used, square_b_used;
    size_t result_bits, result_limbs, result_used, needed, exact_bits;
    ppf_arena_mark_t mark;
    mp_ptr memory, square_a, square_b, aligned_b, result;
    mp_limb_t carry;
    ppf_t exact_sum;
    int rc;

    if (!ppf_regular_p(a) || !ppf_regular_p(b) ||
        ppf_exp_sub_prec(a->exponent, a->precision, &ea) != 0 ||
        ppf_exp_sub_prec(b->exponent, b->precision, &eb) != 0 ||
        ppf_exp_add(ea, ea, &square_a_base) != 0 ||
        ppf_exp_add(eb, eb, &square_b_base) != 0)
        return PPF_TERNARY_ERROR;
    common = square_a_base < square_b_base ? square_a_base : square_b_base;
    shift_a64 = (uint64_t)square_a_base - (uint64_t)common;
    shift_b64 = (uint64_t)square_b_base - (uint64_t)common;
    maximum_shift = shift_a64 > shift_b64 ? shift_a64 : shift_b64;
    if (maximum_shift > (uint64_t)SIZE_MAX ||
        maximum_shift > (uint64_t)(2u * (a->precision + b->precision) + 4096u))
        return PPF_TERNARY_ERROR;
    square_a_size = 2u * a->used_limbs;
    square_b_size = 2u * b->used_limbs;
    if ((size_t)(2u * a->precision) > SIZE_MAX - (size_t)shift_a64 ||
        (size_t)(2u * b->precision) > SIZE_MAX - (size_t)shift_b64)
        return PPF_TERNARY_ERROR;
    result_bits = (size_t)(2u * a->precision) + (size_t)shift_a64;
    {
        size_t other = (size_t)(2u * b->precision) + (size_t)shift_b64;
        if (other > result_bits) result_bits = other;
    }
    if (result_bits == SIZE_MAX) return PPF_TERNARY_ERROR;
    ++result_bits;
    result_limbs = (result_bits + PPF_LIMB_BITS - 1u) / PPF_LIMB_BITS;
    if (!result_limbs || square_a_size > SIZE_MAX - square_b_size ||
        square_a_size + square_b_size > SIZE_MAX - 2u * result_limbs)
        return PPF_TERNARY_ERROR;
    needed = square_a_size + square_b_size + 2u * result_limbs;
    mark = ppf_arena_mark(ctx);
    memory = ppf_arena_alloc(ctx, needed * sizeof(*memory),
                             _Alignof(mp_limb_t));
    if (!memory) return PPF_TERNARY_ERROR;
    square_a = memory;
    square_b = square_a + square_a_size;
    aligned_b = square_b + square_b_size;
    result = aligned_b + result_limbs;
    mpn_sqr(square_a, a->limbs, (mp_size_t)a->used_limbs);
    mpn_sqr(square_b, b->limbs, (mp_size_t)b->used_limbs);
    square_a_used = hypot_trim(square_a, square_a_size);
    square_b_used = hypot_trim(square_b, square_b_size);
    hypot_copy_lshift(result, result_limbs, square_a, square_a_used,
                      (size_t)shift_a64);
    hypot_copy_lshift(aligned_b, result_limbs, square_b, square_b_used,
                      (size_t)shift_b64);
    carry = mpn_add_n(result, result, aligned_b, (mp_size_t)result_limbs);
    if (carry) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    result_used = hypot_trim(result, result_limbs);
    exact_bits = hypot_bit_length(result, result_used);
    if (!exact_bits || exact_bits > UINT64_MAX ||
        ppf_exp_add(common, (ppf_exp_t)exact_bits, &object_exponent) != 0 ||
        ppf_init2_ctx(&exact_sum, (ppf_prec_t)exact_bits, ctx) != 0) {
        ppf_arena_release(ctx, mark);
        return PPF_TERNARY_ERROR;
    }
    memcpy(exact_sum.limbs, result, result_used * sizeof(*result));
    exact_sum.used_limbs = ppf_limbs_for_precision(exact_sum.precision);
    if (exact_sum.used_limbs > result_used)
        memset(exact_sum.limbs + result_used, 0,
               (exact_sum.used_limbs - result_used) * sizeof(*exact_sum.limbs));
    exact_sum.exponent = object_exponent;
    exact_sum.sign = 1;
    exact_sum.kind = PPF_KIND_REGULAR;
    ppf_zero_unused_bits(&exact_sum);
    rc = ppf_sqrt(rop, &exact_sum, rnd, ctx);
    ppf_clear(&exact_sum);
    ppf_arena_release(ctx, mark);
    return rc;
}

static int ppf_hypot_mpz(ppf_t *rop, const ppf_t *a, const ppf_t *b,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpz_t za, zb, aa, bb, sum;
    ppf_exp_t ea, eb, common;
    uint64_t sa, sb;
    int rc;

    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !b || !ctx || !ppf_rounding_valid(rnd))
        return root_error(rop, ctx);
    if (ppf_inf_p(a) || ppf_inf_p(b)) {
        ppf_set_inf(rop, 1);
        return 0;
    }
    if (ppf_nan_p(a) || ppf_nan_p(b))
        return root_invalid(rop, ppf_nan_p(a) ? a->sign : b->sign, ctx);
    if (ppf_zero_p(a) && ppf_zero_p(b)) {
        ppf_set_zero(rop, 1);
        return 0;
    }
    if (ppf_zero_p(a)) return ppf_abs(rop, b, rnd, ctx);
    if (ppf_zero_p(b)) return ppf_abs(rop, a, rnd, ctx);

    mpz_inits(za, zb, aa, bb, sum, NULL);
    if (ppf_get_exact_dyadic(za, &ea, a) != 0 ||
        ppf_get_exact_dyadic(zb, &eb, b) != 0) {
        mpz_clears(za, zb, aa, bb, sum, NULL);
        return root_error(rop, ctx);
    }
    mpz_mul(aa, za, za);
    mpz_mul(bb, zb, zb);
    if (ppf_exp_add(ea, ea, &ea) != 0 ||
        ppf_exp_add(eb, eb, &eb) != 0) {
        mpz_clears(za, zb, aa, bb, sum, NULL);
        return root_error(rop, ctx);
    }
    common = ea < eb ? ea : eb;
    sa = (uint64_t)(ea - common);
    sb = (uint64_t)(eb - common);
    if (sa > (uint64_t)ULONG_MAX || sb > (uint64_t)ULONG_MAX) {
        mpz_clears(za, zb, aa, bb, sum, NULL);
        return root_error(rop, ctx);
    }
    if (sa) mpz_mul_2exp(aa, aa, (mp_bitcnt_t)sa);
    if (sb) mpz_mul_2exp(bb, bb, (mp_bitcnt_t)sb);
    mpz_add(sum, aa, bb);
    rc = root_interval_from_exact(rop, sum, common, 2UL, 1, rnd, ctx);
    mpz_clears(za, zb, aa, bb, sum, NULL);
    return rc;
}

int ppf_hypot(ppf_t *rop, const ppf_t *a, const ppf_t *b,
              ppf_rnd_t rnd, ppf_context_t *ctx) {
    int rc;
    ctx = ppf_resolve_context(ctx);
    if (ctx && rnd == PPF_RND_DEFAULT) rnd = ctx->default_rounding;
    if (!rop || !a || !b || !ctx || !ppf_rounding_valid(rnd))
        return root_error(rop, ctx);
    if (ppf_inf_p(a) || ppf_inf_p(b)) {
        ppf_set_inf(rop, 1);
        return 0;
    }
    if (ppf_nan_p(a) || ppf_nan_p(b))
        return root_invalid(rop, ppf_nan_p(a) ? a->sign : b->sign, ctx);
    if (ppf_zero_p(a) && ppf_zero_p(b)) {
        ppf_set_zero(rop, 1);
        return 0;
    }
    if (ppf_zero_p(a)) return ppf_abs(rop, b, rnd, ctx);
    if (ppf_zero_p(b)) return ppf_abs(rop, a, rnd, ctx);
    rc = ppf_hypot_direct(rop, a, b, rnd, ctx);
    if (rc != PPF_TERNARY_ERROR) return rc;
    return ppf_hypot_mpz(rop, a, b, rnd, ctx);
}

