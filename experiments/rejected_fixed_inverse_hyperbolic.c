/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

/* Positive atanh series at one fixed-point argument.  The argument is reduced
   until q=t^2 is small enough that twice the first omitted term bounds the
   complete positive tail. */
static int tfx_atanh_point_block4(ppf_trig_fx_t *out,
                                  mpz_srcptr argument,
                                  ppf_prec_t work,
                                  ppf_prec_t scale) {
    ppf_trig_fx_t t, q, powers[5], term, sum, block, quotient;
    ppf_trig_fx_t contribution, update, enclosed;
    mpz_t scratch, tail, one;
    unsigned long k = 0UL;
    int rc = -1;
    tfx_init(&t); tfx_init(&q);
    for (unsigned i = 0; i < 5; ++i) tfx_init(&powers[i]);
    tfx_init(&term); tfx_init(&sum); tfx_init(&block); tfx_init(&quotient);
    tfx_init(&contribution); tfx_init(&update); tfx_init(&enclosed);
    mpz_inits(scratch, tail, one, NULL);
    mpz_set(t.lo, argument); mpz_set(t.hi, argument);
    tfx_square_positive(&q, &t, scale, scratch);
    mpz_set_ui(one, 1UL); mpz_mul_2exp(one, one, (mp_bitcnt_t)scale);
    /* The two-times-first-omitted bound needs q <= 1/2. */
    mpz_mul_2exp(scratch, q.hi, 1UL);
    if (mpz_cmp(scratch, one) > 0) goto done;
    mpz_set(powers[0].lo, one); mpz_set(powers[0].hi, one);
    mpz_set(powers[1].lo, q.lo); mpz_set(powers[1].hi, q.hi);
    for (unsigned j = 2; j <= 4; ++j)
        tfx_mul_positive(&powers[j], &powers[j - 1], &q, scale, scratch);
    mpz_set(term.lo, one); mpz_set(term.hi, one);
    mpz_set_ui(sum.lo, 0UL); mpz_set_ui(sum.hi, 0UL);
    while (k < PPF_FAST_TRIG_MAX_TERMS) {
        mpz_set_ui(block.lo, 0UL); mpz_set_ui(block.hi, 0UL);
        for (unsigned j = 0; j < 4; ++j) {
            unsigned long den = 2UL * (k + j) + 1UL;
            tfx_div_ui_positive(&quotient, &powers[j], den);
            mpz_add(block.lo, block.lo, quotient.lo);
            mpz_add(block.hi, block.hi, quotient.hi);
        }
        tfx_mul_positive(&contribution, &term, &block, scale, scratch);
        mpz_add(sum.lo, sum.lo, contribution.lo);
        mpz_add(sum.hi, sum.hi, contribution.hi);
        tfx_mul_positive(&update, &term, &powers[4], scale, scratch);
        k += 4UL;
        tfx_div_ui_positive(&quotient, &update, 2UL * k + 1UL);
        if (tfx_tail_small(quotient.hi, scale, work)) {
            mpz_mul_2exp(tail, quotient.hi, 1UL);
            mpz_set(enclosed.lo, sum.lo);
            mpz_add(enclosed.hi, sum.hi, tail);
            tfx_mul_positive(out, &t, &enclosed, scale, scratch);
            rc = 0;
            goto done;
        }
        mpz_set(term.lo, update.lo); mpz_set(term.hi, update.hi);
    }
done:
    tfx_clear(&t); tfx_clear(&q);
    for (unsigned i = 0; i < 5; ++i) tfx_clear(&powers[i]);
    tfx_clear(&term); tfx_clear(&sum); tfx_clear(&block); tfx_clear(&quotient);
    tfx_clear(&contribution); tfx_clear(&update); tfx_clear(&enclosed);
    mpz_clears(scratch, tail, one, NULL);
    return rc;
}

/* t -> t/(1+sqrt(1-t^2)), the atanh half-angle map. */
static int tfx_atanh_half_angle(ppf_trig_fx_t *out,
                                const ppf_trig_fx_t *t,
                                ppf_prec_t scale,
                                ppf_trig_fx_t *q,
                                ppf_trig_fx_t *radicand,
                                ppf_trig_fx_t *root,
                                mpz_ptr one, mpz_ptr scratch) {
    if (mpz_sgn(t->lo) < 0 || scale > (ppf_prec_t)ULONG_MAX) return -1;
    tfx_square_positive(q, t, scale, scratch);
    mpz_sub(radicand->lo, one, q->hi);
    mpz_sub(radicand->hi, one, q->lo);
    if (mpz_sgn(radicand->lo) <= 0) return -1;
    if (tfx_sqrt_positive(root, radicand, scale, scratch) != 0) return -1;
    mpz_add(q->lo, one, root->lo); /* reuse q as denominator interval */
    mpz_add(q->hi, one, root->hi);
    if (mpz_sgn(q->lo) <= 0) return -1;
    mpz_mul_2exp(scratch, t->lo, (mp_bitcnt_t)scale);
    mpz_fdiv_q(out->lo, scratch, q->hi);
    mpz_mul_2exp(scratch, t->hi, (mp_bitcnt_t)scale);
    mpz_cdiv_q(out->hi, scratch, q->lo);
    return mpz_cmp(out->lo, out->hi) <= 0 ? 0 : -1;
}

static int tfx_atanh_positive_value(ppf_trig_fx_t *out,
                                    const ppf_trig_fx_t *input,
                                    ppf_prec_t work,
                                    ppf_prec_t scale) {
    ppf_trig_fx_t t, temp, q, radicand, root, value;
    mpz_t one, scratch, width, limit;
    unsigned reductions = tfx_atan_full_reductions(work);
    int rc = -1;
    tfx_init(&t); tfx_init(&temp); tfx_init(&q);
    tfx_init(&radicand); tfx_init(&root); tfx_init(&value);
    mpz_inits(one, scratch, width, limit, NULL);
    mpz_set(t.lo, input->lo); mpz_set(t.hi, input->hi);
    mpz_set_ui(one, 1UL); mpz_mul_2exp(one, one, (mp_bitcnt_t)scale);
    if (mpz_sgn(t.lo) < 0 || mpz_cmp(t.hi, one) >= 0) goto done;
    for (unsigned i = 0; i < reductions; ++i) {
        if (tfx_atanh_half_angle(&temp, &t, scale, &q, &radicand,
                                 &root, one, scratch) != 0)
            goto done;
        mpz_set(t.lo, temp.lo); mpz_set(t.hi, temp.hi);
    }
    /* Ensure the derivative bound 1/(1-t^2) < 2 used below. */
    mpz_mul(limit, t.hi, t.hi);
    mpz_fdiv_q_2exp(limit, limit, (mp_bitcnt_t)scale);
    mpz_mul_2exp(limit, limit, 1UL);
    if (mpz_cmp(limit, one) >= 0) goto done;
    mpz_sub(width, t.hi, t.lo);
    if (tfx_atanh_point_block4(&value, t.lo, work, scale) != 0) goto done;
    /* atanh is increasing and has derivative < 2 on the reduced interval. */
    mpz_mul_2exp(width, width, 1UL);
    mpz_add(value.hi, value.hi, width);
    if (reductions > (unsigned)LONG_MAX) goto done;
    tfx_scale2_interval(out, &value, (long)reductions);
    rc = 0;
done:
    tfx_clear(&t); tfx_clear(&temp); tfx_clear(&q);
    tfx_clear(&radicand); tfx_clear(&root); tfx_clear(&value);
    mpz_clears(one, scratch, width, limit, NULL);
    return rc;
}

static int ppf_fast_inverse_hyperbolic_common(ppf_t *rop, const ppf_t *x,
                                               int fn, ppf_rnd_t rnd,
                                               ppf_context_t *ctx,
                                               int *ternary_out) {
    ppf_trig_fx_t ax, q, radicand, root, num, den, t, value, temp;
    mpz_t one, scratch;
    ppf_prec_t work;
    int neg, ok = 0, ternary = 0;
    if (!rop || !x || !ctx || !ppf_regular_p(x) || ppf_zero_p(x)) return 0;
    tfx_init(&ax); tfx_init(&q); tfx_init(&radicand); tfx_init(&root);
    tfx_init(&num); tfx_init(&den); tfx_init(&t); tfx_init(&value);
    tfx_init(&temp);
    mpz_inits(one, scratch, NULL);
    neg = x->sign < 0;
    work = rop->precision + 80u;
    if (work < rop->precision) goto done;
    for (unsigned attempt = 0; attempt < 5u; ++attempt) {
        ppf_prec_t scale;
        unsigned reductions = tfx_atan_full_reductions(work);
        if (work > UINT64_MAX - reductions - 160u) break;
        scale = work + reductions + 160u;
        if (scale > (ppf_prec_t)ULONG_MAX || scale > (ppf_prec_t)INT64_MAX)
            break;
        if (tfx_from_ppf(ax.lo, x, scale, neg ? 1 : 0) != 0) break;
        if (neg) mpz_neg(ax.lo, ax.lo);
        if (tfx_from_ppf(ax.hi, x, scale, neg ? 0 : 1) != 0) break;
        if (neg) mpz_neg(ax.hi, ax.hi);
        if (mpz_cmp(ax.lo, ax.hi) > 0) mpz_swap(ax.lo, ax.hi);
        mpz_set_ui(one, 1UL); mpz_mul_2exp(one, one, (mp_bitcnt_t)scale);
        if (fn == 0) { /* asinh(x)=2 atanh(x/(1+sqrt(1+x^2))) */
            tfx_square_positive(&q, &ax, scale, scratch);
            mpz_add(radicand.lo, one, q.lo);
            mpz_add(radicand.hi, one, q.hi);
            if (tfx_sqrt_positive(&root, &radicand, scale, scratch) != 0)
                goto next_attempt;
            mpz_add(den.lo, one, root.lo);
            mpz_add(den.hi, one, root.hi);
            mpz_mul_2exp(scratch, ax.lo, (mp_bitcnt_t)scale);
            mpz_fdiv_q(t.lo, scratch, den.hi);
            mpz_mul_2exp(scratch, ax.hi, (mp_bitcnt_t)scale);
            mpz_cdiv_q(t.hi, scratch, den.lo);
        } else if (fn == 1) { /* acosh(x)=2 atanh(sqrt((x-1)/(x+1))) */
            if (mpz_cmp(ax.lo, one) <= 0) goto next_attempt;
            mpz_sub(num.lo, ax.lo, one); mpz_sub(num.hi, ax.hi, one);
            mpz_add(den.lo, ax.lo, one); mpz_add(den.hi, ax.hi, one);
            mpz_mul_2exp(scratch, num.lo, (mp_bitcnt_t)scale);
            mpz_fdiv_q(q.lo, scratch, den.hi);
            mpz_mul_2exp(scratch, num.hi, (mp_bitcnt_t)scale);
            mpz_cdiv_q(q.hi, scratch, den.lo);
            if (tfx_sqrt_positive(&t, &q, scale, scratch) != 0)
                goto next_attempt;
        } else { /* atanh */
            mpz_set(t.lo, ax.lo); mpz_set(t.hi, ax.hi);
        }
        if (mpz_sgn(t.lo) < 0 || mpz_cmp(t.hi, one) >= 0)
            goto next_attempt;
        if (tfx_atanh_positive_value(&value, &t, work, scale) != 0)
            goto next_attempt;
        if (fn != 2) {
            tfx_scale2_interval(&temp, &value, 1L);
            mpz_set(value.lo, temp.lo); mpz_set(value.hi, temp.hi);
        }
        if (neg) {
            tfx_neg_interval(&temp, &value, scratch);
            mpz_set(value.lo, temp.lo); mpz_set(value.hi, temp.hi);
        }
        {
            ppf_interval_status_t status = ppf_set_dyadic_interval(
                rop, value.lo, value.hi, -(ppf_exp_t)scale,
                rnd, &ternary, ctx);
            if (status == PPF_INTERVAL_DECIDED) {
                if (ternary_out) *ternary_out = ternary;
                ok = 1;
                goto done;
            }
        }
next_attempt:
        {
            ppf_prec_t next = work + work / 8u + 64u;
            if (next <= work) break;
            work = next;
        }
    }
done:
    tfx_clear(&ax); tfx_clear(&q); tfx_clear(&radicand); tfx_clear(&root);
    tfx_clear(&num); tfx_clear(&den); tfx_clear(&t); tfx_clear(&value);
    tfx_clear(&temp);
    mpz_clears(one, scratch, NULL);
    return ok;
}

int ppf_fast_asinh_internal(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                            ppf_context_t *ctx, int *ternary_out) {
    return ppf_fast_inverse_hyperbolic_common(rop, x, 0, rnd, ctx,
                                              ternary_out);
}

int ppf_fast_acosh_internal(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                            ppf_context_t *ctx, int *ternary_out) {
    return ppf_fast_inverse_hyperbolic_common(rop, x, 1, rnd, ctx,
                                              ternary_out);
}

int ppf_fast_atanh_internal(ppf_t *rop, const ppf_t *x, ppf_rnd_t rnd,
                            ppf_context_t *ctx, int *ternary_out) {
    return ppf_fast_inverse_hyperbolic_common(rop, x, 2, rnd, ctx,
                                              ternary_out);
}
