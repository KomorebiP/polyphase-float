/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "polyphase_float.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    ppf_context_t ctx;
    ppf_t x, y, z, pi;
    ppf_product_plan_t plan;
    unsigned long exact;
    double pi_d;

    if (ppf_context_init(&ctx) != 0)
        return 10;
    if (ppf_init2_ctx(&x, 128, &ctx) != 0 ||
        ppf_init2_ctx(&y, 128, &ctx) != 0 ||
        ppf_init2_ctx(&z, 192, &ctx) != 0 ||
        ppf_init2_ctx(&pi, 192, &ctx) != 0)
        return 11;
    if (ppf_set_ui(&x, 123456789UL, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&y, 987654321UL, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_mul(&z, &x, &y, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_add_ui(&z, &z, 111UL, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        return 12;
    exact = ppf_get_ui(&z, PPF_RNDZ, &ctx);
    if (exact != 121932631112635380UL)
        return 13;
    if (ppf_const_pi(&pi, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        return 14;
    pi_d = ppf_get_d(&pi, PPF_RNDN, &ctx);
    if (fabs(pi_d - 3.14159265358979323846) > 1e-15)
        return 15;
    if (ppf_plan_product(&plan, &x, &y,
                         PPF_PRODUCT_OP_MULTIPLY, 4UL) != 0)
        return 16;

    printf("ppf_t=%zu context=%zu exact=%lu pi=%.17g plan=%d/%lu/%lu\n",
           sizeof(ppf_t), sizeof(ppf_context_t), exact, pi_d,
           (int)plan.algorithm, plan.requested_workers,
           plan.effective_workers);

    ppf_clear(&pi);
    ppf_clear(&z);
    ppf_clear(&y);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    return 0;
}
