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
#include <string.h>
#include <time.h>

static double now_s(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static int common_interval(mpz_t l,mpz_t h,ppf_exp_t *e,const ppf_t *lo,const ppf_t *hi){mpz_t a,b;ppf_exp_t ea,eb,c;mpz_inits(a,b,NULL);ea=ppf_get_z_2exp(a,lo);eb=ppf_get_z_2exp(b,hi);c=ea<eb?ea:eb;uint64_t sa=(uint64_t)ea-(uint64_t)c,sb=(uint64_t)eb-(uint64_t)c;if(sa>ULONG_MAX||sb>ULONG_MAX){mpz_clears(a,b,NULL);return -1;}if(sa)mpz_mul_2exp(a,a,(mp_bitcnt_t)sa);if(sb)mpz_mul_2exp(b,b,(mp_bitcnt_t)sb);mpz_set(l,a);mpz_set(h,b);*e=c;mpz_clears(a,b,NULL);return 0;}

static int halley_build_y(ppf_t *y,const ppf_t *x,ppf_prec_t qmax,ppf_context_t *ctx){
 ppf_prec_t levels[32];unsigned nl=0;ppf_prec_t v=qmax;levels[nl++]=v;while(v>96u&&nl<32u){v=(v+62u)/3u+16u;levels[nl++]=v;} double yd=atan(ppf_get_d(x,PPF_RNDN,ctx));
 if(ppf_init2_ctx(y,levels[nl-1]+32u,ctx)!=0||ppf_set_d(y,yd,PPF_RNDN,ctx)==PPF_TERNARY_ERROR)return -1;
 for(int li=(int)nl-2;li>=0;--li){ppf_prec_t w=levels[li]+40u;ppf_t xw,yw,t,xt,one,den,num,u,next;memset(&xw,0,sizeof xw);memset(&yw,0,sizeof yw);memset(&t,0,sizeof t);memset(&xt,0,sizeof xt);memset(&one,0,sizeof one);memset(&den,0,sizeof den);memset(&num,0,sizeof num);memset(&u,0,sizeof u);memset(&next,0,sizeof next);int ok=0;
#define I(Z) if(ppf_init2_ctx(&(Z),w,ctx)!=0)goto done
 I(xw);I(yw);I(t);I(xt);I(one);I(den);I(num);I(u);I(next);
 if(ppf_set(&xw,x,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_set(&yw,y,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_tan(&t,&yw,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_mul(&xt,&xw,&t,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_set_ui(&one,1,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_add(&den,&one,&xt,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_sub(&num,&xw,&t,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_div(&u,&num,&den,PPF_RNDN,ctx)==PPF_TERNARY_ERROR||ppf_add(&next,&yw,&u,PPF_RNDN,ctx)==PPF_TERNARY_ERROR)goto done;ppf_swap(y,&next);ok=1;
done: if(xw.limbs)ppf_clear(&xw);if(yw.limbs)ppf_clear(&yw);if(t.limbs)ppf_clear(&t);if(xt.limbs)ppf_clear(&xt);if(one.limbs)ppf_clear(&one);if(den.limbs)ppf_clear(&den);if(num.limbs)ppf_clear(&num);if(u.limbs)ppf_clear(&u);if(next.limbs)ppf_clear(&next);if(!ok){ppf_clear(y);return -1;}
#undef I
 }
 return 0;
}

static int atan_halley_cert(ppf_t *rop,const ppf_t *x,ppf_rnd_t rnd,ppf_context_t *ctx){
 if(ppf_sgn(x,ctx)<=0||ppf_cmp_ui(x,1,ctx)>0)return PPF_TERNARY_ERROR;ppf_context_t wide;if(ppf_context_clone(&wide,ctx)!=0)return PPF_TERNARY_ERROR;wide.emin=PPF_EMIN_MIN;wide.emax=PPF_EMAX_MAX;wide.flags=0;ppf_prec_t p=rop->precision,q=(p+250u)/3u+64u;ppf_t y;memset(&y,0,sizeof y);if(halley_build_y(&y,x,q,&wide)!=0){ppf_context_clear(&wide);return PPF_TERNARY_ERROR;}
 int result=PPF_TERNARY_ERROR;for(unsigned att=0;att<4;att++){ppf_prec_t w=p+160u+att*96u;ppf_t xw,yw,t,tlo,thi,one,nlo,nhi,dlo,dhi,ulo,uhi,u3,tmp,clo,chi,rlo,rhi;memset(&xw,0,sizeof xw);memset(&yw,0,sizeof yw);memset(&t,0,sizeof t);memset(&tlo,0,sizeof tlo);memset(&thi,0,sizeof thi);memset(&one,0,sizeof one);memset(&nlo,0,sizeof nlo);memset(&nhi,0,sizeof nhi);memset(&dlo,0,sizeof dlo);memset(&dhi,0,sizeof dhi);memset(&ulo,0,sizeof ulo);memset(&uhi,0,sizeof uhi);memset(&u3,0,sizeof u3);memset(&tmp,0,sizeof tmp);memset(&clo,0,sizeof clo);memset(&chi,0,sizeof chi);memset(&rlo,0,sizeof rlo);memset(&rhi,0,sizeof rhi);int ok=0;
#define I2(Z) if(ppf_init2_ctx(&(Z),w,&wide)!=0)goto cleanup
 I2(xw);I2(yw);I2(t);I2(tlo);I2(thi);I2(one);I2(nlo);I2(nhi);I2(dlo);I2(dhi);I2(ulo);I2(uhi);I2(u3);I2(tmp);I2(clo);I2(chi);I2(rlo);I2(rhi);
 if(ppf_set(&xw,x,PPF_RNDN,&wide)==PPF_TERNARY_ERROR||ppf_set(&yw,&y,PPF_RNDN,&wide)==PPF_TERNARY_ERROR||ppf_tan(&t,&yw,PPF_RNDN,&wide)==PPF_TERNARY_ERROR||ppf_set(&tlo,&t,PPF_RNDN,&wide)==PPF_TERNARY_ERROR||ppf_set(&thi,&t,PPF_RNDN,&wide)==PPF_TERNARY_ERROR)goto cleanup;ppf_nextbelow(&tlo,&wide);ppf_nextabove(&thi,&wide);
 if(ppf_sub(&nlo,&xw,&thi,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_sub(&nhi,&xw,&tlo,PPF_RNDU,&wide)==PPF_TERNARY_ERROR||ppf_mul(&dlo,&xw,&tlo,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_mul(&dhi,&xw,&thi,PPF_RNDU,&wide)==PPF_TERNARY_ERROR||ppf_set_ui(&one,1,PPF_RNDN,&wide)==PPF_TERNARY_ERROR||ppf_add(&dlo,&one,&dlo,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_add(&dhi,&one,&dhi,PPF_RNDU,&wide)==PPF_TERNARY_ERROR)goto cleanup;
 int sl=ppf_sgn(&nlo,&wide),sh=ppf_sgn(&nhi,&wide);if(sl>=0){if(ppf_div(&ulo,&nlo,&dhi,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_div(&uhi,&nhi,&dlo,PPF_RNDU,&wide)==PPF_TERNARY_ERROR)goto cleanup;}else if(sh<=0){if(ppf_div(&ulo,&nlo,&dlo,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_div(&uhi,&nhi,&dhi,PPF_RNDU,&wide)==PPF_TERNARY_ERROR)goto cleanup;}else{if(ppf_div(&ulo,&nlo,&dlo,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_div(&uhi,&nhi,&dlo,PPF_RNDU,&wide)==PPF_TERNARY_ERROR)goto cleanup;}
 /* atan correction bounds */
 if(ppf_set(&clo,&ulo,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_set(&chi,&uhi,PPF_RNDU,&wide)==PPF_TERNARY_ERROR)goto cleanup;
 if(sl>=0){if(ppf_sqr(&u3,&ulo,PPF_RNDU,&wide)==PPF_TERNARY_ERROR||ppf_mul(&u3,&u3,&ulo,PPF_RNDU,&wide)==PPF_TERNARY_ERROR||ppf_set_ui(&tmp,3,PPF_RNDN,&wide)==PPF_TERNARY_ERROR||ppf_div(&u3,&u3,&tmp,PPF_RNDU,&wide)==PPF_TERNARY_ERROR||ppf_sub(&clo,&ulo,&u3,PPF_RNDD,&wide)==PPF_TERNARY_ERROR)goto cleanup;}else if(sh<=0){if(ppf_sqr(&u3,&uhi,PPF_RNDU,&wide)==PPF_TERNARY_ERROR||ppf_mul(&u3,&u3,&uhi,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_set_ui(&tmp,3,PPF_RNDN,&wide)==PPF_TERNARY_ERROR||ppf_div(&u3,&u3,&tmp,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_sub(&chi,&uhi,&u3,PPF_RNDU,&wide)==PPF_TERNARY_ERROR)goto cleanup;}
 if(ppf_add(&rlo,&yw,&clo,PPF_RNDD,&wide)==PPF_TERNARY_ERROR||ppf_add(&rhi,&yw,&chi,PPF_RNDU,&wide)==PPF_TERNARY_ERROR)goto cleanup;
 {mpz_t zl,zh;ppf_exp_t e;int ter=0;mpz_inits(zl,zh,NULL);if(common_interval(zl,zh,&e,&rlo,&rhi)==0&&ppf_set_dyadic_interval(rop,zl,zh,e,rnd,&ter,ctx)==PPF_INTERVAL_DECIDED){result=ter;ok=1;}mpz_clears(zl,zh,NULL);}
cleanup: if(xw.limbs)ppf_clear(&xw);if(yw.limbs)ppf_clear(&yw);if(t.limbs)ppf_clear(&t);if(tlo.limbs)ppf_clear(&tlo);if(thi.limbs)ppf_clear(&thi);if(one.limbs)ppf_clear(&one);if(nlo.limbs)ppf_clear(&nlo);if(nhi.limbs)ppf_clear(&nhi);if(dlo.limbs)ppf_clear(&dlo);if(dhi.limbs)ppf_clear(&dhi);if(ulo.limbs)ppf_clear(&ulo);if(uhi.limbs)ppf_clear(&uhi);if(u3.limbs)ppf_clear(&u3);if(tmp.limbs)ppf_clear(&tmp);if(clo.limbs)ppf_clear(&clo);if(chi.limbs)ppf_clear(&chi);if(rlo.limbs)ppf_clear(&rlo);if(rhi.limbs)ppf_clear(&rhi);if(ok)break;
#undef I2
 }
 ppf_clear(&y);ppf_context_clear(&wide);return result;
}

static int match(const ppf_t*x,mpfr_srcptr y){mpfr_t z;mpfr_init2(z,mpfr_get_prec(y));ppf_get_mpfr(z,x,MPFR_RNDN);int r=mpfr_equal_p(z,y);mpfr_clear(z);return r;}
int main(int ac,char**av){unsigned long bits=ac>1?strtoul(av[1],0,10):5000;unsigned reps=ac>2?strtoul(av[2],0,10):3;ppf_context_t c;ppf_t x,a,b;mpfr_t mx,ma;ppf_context_init(&c);ppf_init2_ctx(&x,bits+32,&c);ppf_init2_ctx(&a,bits,&c);ppf_init2_ctx(&b,bits,&c);ppf_set_d(&x,0.3,PPF_RNDN,&c);mpfr_init2(mx,bits+32);mpfr_init2(ma,bits);mpfr_set_d(mx,0.3,MPFR_RNDN);mpfr_atan(ma,mx,MPFR_RNDN);int ra=atan_halley_cert(&a,&x,PPF_RNDN,&c),rb=ppf_atan(&b,&x,PPF_RNDN,&c);printf("rc=%d match=%d old=%d oldmatch=%d\n",ra,match(&a,ma),rb,match(&b,ma));for(int m=0;m<2;m++){double ar[16];for(unsigned i=0;i<reps;i++){double t=now_s();if(m==0)atan_halley_cert(&a,&x,PPF_RNDN,&c);else ppf_atan(&b,&x,PPF_RNDN,&c);ar[i]=now_s()-t;}for(unsigned i=0;i<reps;i++)for(unsigned j=i+1;j<reps;j++)if(ar[j]<ar[i]){double z=ar[i];ar[i]=ar[j];ar[j]=z;}printf("%s %.9f\n",m?"old":"cert",ar[reps/2]);}ppf_clear(&x);ppf_clear(&a);ppf_clear(&b);ppf_context_clear(&c);mpfr_clears(mx,ma,(mpfr_ptr)0);}
