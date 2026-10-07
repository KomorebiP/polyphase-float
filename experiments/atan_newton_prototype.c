/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "polyphase_float.h"
#include "polyphase_float_mpfr.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_s(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}

static int atan_halley_uncert(ppf_t *rop,const ppf_t *x,ppf_context_t *ctx){
    ppf_prec_t target=rop->precision+96u;
    ppf_prec_t levels[32]; unsigned nlevels=0;
    ppf_prec_t v=target;
    levels[nlevels++]=v;
    while(v>128u && nlevels<32u){v=(v+62u)/3u + 16u; levels[nlevels++]=v;}
    ppf_prec_t q=levels[nlevels-1u];
    double xd=ppf_get_d(x,PPF_RNDN,ctx);
    double yd=atan(xd);
    ppf_t y;
    if(ppf_init2_ctx(&y,q+32u,ctx)!=0)return -1;
    if(ppf_set_d(&y,yd,PPF_RNDN,ctx)==PPF_TERNARY_ERROR){ppf_clear(&y);return -1;}
    for(int li=(int)nlevels-2; li>=0; --li){
        q=levels[li];
        ppf_prec_t w=q+32u;
        ppf_t xw,yw,t,xt,one,den,num,delta,next;
        int ok=0;
        if(ppf_init2_ctx(&xw,w,ctx)!=0||ppf_init2_ctx(&yw,w,ctx)!=0||
           ppf_init2_ctx(&t,w,ctx)!=0||ppf_init2_ctx(&xt,w,ctx)!=0||
           ppf_init2_ctx(&one,w,ctx)!=0||ppf_init2_ctx(&den,w,ctx)!=0||
           ppf_init2_ctx(&num,w,ctx)!=0||ppf_init2_ctx(&delta,w,ctx)!=0||
           ppf_init2_ctx(&next,w,ctx)!=0) return -1;
        if(ppf_set(&xw,x,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||
           ppf_set(&yw,&y,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||
           ppf_tan(&t,&yw,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||
           ppf_mul(&xt,&xw,&t,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||
           ppf_set_ui(&one,1,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||
           ppf_add(&den,&one,&xt,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||
           ppf_sub(&num,&xw,&t,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||
           ppf_div(&delta,&num,&den,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||
           ppf_add(&next,&yw,&delta,PPF_RNDN,ctx)==PPF_TERNARY_ERROR) goto done;
        ppf_swap(&y,&next); ok=1;
    done:
        ppf_clear(&xw);ppf_clear(&yw);ppf_clear(&t);ppf_clear(&xt);ppf_clear(&one);
        ppf_clear(&den);ppf_clear(&num);ppf_clear(&delta);ppf_clear(&next);
        if(!ok){ppf_clear(&y);return -1;}
    }
    int r=ppf_set(rop,&y,PPF_RNDN,ctx);ppf_clear(&y);return r;
}

int main(int argc,char**argv){unsigned long bits=argc>1?strtoul(argv[1],0,10):5000;unsigned reps=argc>2?atoi(argv[2]):5;
 ppf_context_t c;ppf_t x,a,b;mpfr_t mx,ma;ppf_context_init(&c);ppf_init2_ctx(&x,bits+32,&c);ppf_init2_ctx(&a,bits,&c);ppf_init2_ctx(&b,bits,&c);ppf_set_d(&x,0.3,PPF_RNDN,&c);mpfr_init2(mx,bits+32);mpfr_init2(ma,bits);mpfr_set_d(mx,0.3,MPFR_RNDN);mpfr_atan(ma,mx,MPFR_RNDN);
 atan_halley_uncert(&a,&x,&c);ppf_atan(&b,&x,PPF_RNDN,&c);mpfr_t ca;mpfr_init2(ca,bits);ppf_get_mpfr(ca,&a,MPFR_RNDN);printf("match=%d oldmatch=",mpfr_equal_p(ca,ma));ppf_get_mpfr(ca,&b,MPFR_RNDN);printf("%d\n",mpfr_equal_p(ca,ma));
 for(int mode=0;mode<2;mode++){double arr[32];for(unsigned i=0;i<reps;i++){double t=now_s();if(mode==0)atan_halley_uncert(&a,&x,&c);else ppf_atan(&b,&x,PPF_RNDN,&c);arr[i]=now_s()-t;}for(unsigned i=0;i<reps;i++)for(unsigned j=i+1;j<reps;j++)if(arr[j]<arr[i]){double z=arr[i];arr[i]=arr[j];arr[j]=z;}printf("%s %.9f\n",mode==0?"halley":"old",arr[reps/2]);}
 ppf_clear(&x);ppf_clear(&a);ppf_clear(&b);ppf_context_clear(&c);mpfr_clears(mx,ma,ca,(mpfr_ptr)0);return 0;}
