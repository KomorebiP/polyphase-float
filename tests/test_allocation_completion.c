/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "polyphase_float.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"allocation completion %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
typedef struct { size_t calls, fail_at, failed, live, bytes; } state;
typedef struct { size_t bytes; max_align_t alignment; unsigned char data[]; } record;
static record *rec(void *p) { return (record *)((unsigned char *)p - offsetof(record,data)); }
static int fail(state *s) { ++s->calls; if(s->fail_at && s->calls==s->fail_at) {++s->failed;return 1;}return 0; }
static void *allocate(void *v,size_t n) { state *s=v;record *r;if(fail(s))return NULL;r=malloc(offsetof(record,data)+n);CHECK(r);r->bytes=n;++s->live;s->bytes+=n;return r->data; }
static void *resize(void *v,void *p,size_t old,size_t n) {state*s=v;record*r=p?rec(p):NULL;if(p)CHECK(r->bytes==old);else CHECK(!old);if(fail(s))return NULL;r=realloc(r,offsetof(record,data)+n);CHECK(r);r->bytes=n;if(!p)++s->live;s->bytes+=n-old;return r->data;}
static void release(void *v,void*p,size_t n) {state*s=v;record*r;if(!p)return;r=rec(p);CHECK(r->bytes==n);CHECK(s->live&&s->bytes>=n);--s->live;s->bytes-=n;free(r);}
static void setup(ppf_context_t *c,state*s){ppf_allocator_t a={allocate,resize,release,s};CHECK(!ppf_context_init_custom(c,&a));}
static void arm(state*s,ppf_context_t*c,size_t at){s->calls=s->failed=0;s->fail_at=at;ppf_context_flags_clear(c,PPF_FLAGS_ALL);}
typedef struct {ppf_t object;mp_limb_t limbs[400];} snapshot;
static void save(snapshot*s,const ppf_t*x){CHECK(x->allocated_limbs<=400);s->object=*x;memcpy(s->limbs,x->limbs,x->allocated_limbs*sizeof(*x->limbs));}
static void same(const snapshot*s,const ppf_t*x){CHECK(!memcmp(&s->object,x,sizeof(*x)));CHECK(!memcmp(s->limbs,x->limbs,x->allocated_limbs*sizeof(*x->limbs)));}

/* These cases directly call every reviewed fixed-storage endpoint. Their
   source classification assumes valid initialized objects, existing contexts,
   and caller-provided capacity. GMP-managed allocations are a separate domain. */
static void fixed_storage(void) {
 state s={0};ppf_context_t c;ppf_t x,y,o,f,e,custom;mp_limb_t external[160]={0},moved[160]={0};mpz_t z;mpq_t q;mpf_t g;long quo;ppf_exp_t exp;
 setup(&c,&s);mpz_init_set_ui(z,57);mpq_init(q);mpq_set_ui(q,57,8);mpf_init2(g,128);mpf_set_d(g,7.125);
 for(ppf_prec_t p=3;p<=4097;p=p==3?53:p==53?257:p==257?4097:4097+1) {
  CHECK(!ppf_init2_ctx(&x,p,&c));CHECK(!ppf_init2_ctx(&y,p,&c));CHECK(!ppf_init2_ctx(&o,p,&c));CHECK(!ppf_init2_ctx(&f,p,&c));CHECK(!ppf_init2_ctx(&e,p+1,&c));
  for(int regime=0;regime<3;++regime)for(int mode=PPF_RNDN;mode<=PPF_RNDF;++mode) {
   ppf_rnd_t r=(ppf_rnd_t)mode;
   c.emin=PPF_EMIN_DEFAULT;c.emax=PPF_EMAX_DEFAULT;
   CHECK(ppf_set_d(&x,7.125,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);CHECK(ppf_set_d(&y,-2.5,PPF_RNDN,&c)!=PPF_TERNARY_ERROR);
   if(regime==1){c.emin=-2;c.emax=2;}else if(regime==2){c.emin=4;c.emax=8;}
   arm(&s,&c,1);
#define OK(call) do{CHECK((call)!=PPF_TERNARY_ERROR);CHECK(s.calls==0);}while(0)
   OK(ppf_set(&o,&x,r,&c));OK(ppf_set_ui(&o,57,r,&c));OK(ppf_set_si(&o,-57,r,&c));OK(ppf_set_uj(&o,57,r,&c));OK(ppf_set_sj(&o,-57,r,&c));
   OK(ppf_set_flt(&o,7.125f,r,&c));OK(ppf_set_d(&o,7.125,r,&c));OK(ppf_set_ld(&o,7.125L,r,&c));
#if PPF_HAVE_FLOAT128
   OK(ppf_set_float128(&o,(ppf_float128_t)7.125L,r,&c));
#endif
#if PPF_HAVE_DECIMAL_FLOAT
   OK(ppf_set_decimal64(&o,(ppf_decimal64_t)7.125L,r,&c));OK(ppf_set_decimal128(&o,(ppf_decimal128_t)7.125L,r,&c));
#endif
   OK(ppf_set_z(&o,z,r,&c));OK(ppf_set_q(&o,q,r,&c));OK(ppf_set_f(&o,g,r,&c));
   OK(ppf_set_ui_2exp(&o,57,-3,r,&c));OK(ppf_set_si_2exp(&o,-57,-3,r,&c));OK(ppf_set_uj_2exp(&o,57,-3,r,&c));OK(ppf_set_sj_2exp(&o,-57,-3,r,&c));OK(ppf_set_z_2exp(&o,z,-3,r,&c));
   OK(ppf_neg(&o,&x,r,&c));OK(ppf_abs(&o,&y,r,&c));OK(ppf_setsign(&o,&x,1,r,&c));OK(ppf_copysign(&o,&x,&y,r,&c));
   OK(ppf_frexp(&exp,&o,&x,r,&c));OK(ppf_rint(&o,&x,r,&c));OK(ppf_ceil(&o,&x,&c));OK(ppf_floor(&o,&x,&c));OK(ppf_round(&o,&x,&c));OK(ppf_roundeven(&o,&x,&c));OK(ppf_trunc(&o,&x,&c));
   OK(ppf_rint_ceil(&o,&x,r,&c));OK(ppf_rint_floor(&o,&x,r,&c));OK(ppf_rint_round(&o,&x,r,&c));OK(ppf_rint_roundeven(&o,&x,r,&c));OK(ppf_rint_trunc(&o,&x,r,&c));
   OK(ppf_frac(&o,&x,r,&c));OK(ppf_modf(&o,&f,&x,r,&c));OK(ppf_fmod(&o,&x,&y,r,&c));OK(ppf_fmodquo(&o,&quo,&x,&y,r,&c));OK(ppf_remainder(&o,&x,&y,r,&c));OK(ppf_remquo(&o,&quo,&x,&y,r,&c));
   OK(ppf_min(&o,&x,&y,r,&c));OK(ppf_max(&o,&x,&y,r,&c));OK(ppf_check_range(&o,0,r,&c));OK(ppf_subnormalize(&o,0,r,&c));
   OK(ppf_set(&e,&x,PPF_RNDN,&c));OK(ppf_round_nearest_away_finish(&o,&e,0,&c));
   ppf_nextabove(&o,&c);ppf_nextbelow(&o,&c);ppf_nexttoward(&o,&y,&c);CHECK(s.calls==0);
   if(ppf_regular_p(&o)){CHECK(!ppf_set_exp(&o,c.emin<=1&&c.emax>=1?1:c.emin,&c));}
   ppf_set_zero(&o,-1);ppf_set_inf(&o,-1);ppf_set_nan(&o,-1);CHECK(s.calls==0);
   OK(ppf_set(&o,&x,r,&c));OK(ppf_set(&o,&o,r,&c));OK(ppf_neg(&o,&o,r,&c));OK(ppf_abs(&o,&o,r,&c));OK(ppf_frac(&o,&o,r,&c));
   CHECK(s.calls==0);s.fail_at=0;
  }
  /* Reuse capacity while changing raw precision, expose/commit limbs, and
     move external storage without replacing ownership. */
  arm(&s,&c,1);ppf_set_prec_raw(&o,p>3?p-1:p);CHECK(s.calls==0);
  {mp_limb_t *l=ppf_limbs_write(&o);size_t top=(o.precision-1)/GMP_NUMB_BITS;CHECK(l);l[top]=(mp_limb_t)1<<((o.precision-1)%GMP_NUMB_BITS);CHECK(!ppf_commit_regular(&o,-1,c.emin<=1&&c.emax>=1?1:c.emin,&c));}
  CHECK(!ppf_custom_init_set(&custom,PPF_ZERO_KIND,0,p,external));
  memcpy(moved,external,sizeof(external));ppf_custom_move(&custom,moved);CHECK(custom.limbs==moved);ppf_clear(&custom);
  CHECK(s.calls==0);s.fail_at=0;
  ppf_clear(&x);ppf_clear(&y);ppf_clear(&o);ppf_clear(&f);ppf_clear(&e);c.emin=PPF_EMIN_DEFAULT;c.emax=PPF_EMAX_DEFAULT;
 }
 mpf_clear(g);mpq_clear(q);mpz_clear(z);ppf_context_clear(&c);CHECK(s.live==0&&s.bytes==0);
 (void)moved;
 puts("fixed-storage allocation and ownership cases passed");
}

static int initialized_call(int k,ppf_t*o,const ppf_t*x,mpz_srcptr z,mpq_srcptr q,mpf_srcptr g,ppf_rnd_t r,ppf_context_t*c){
 switch(k){
 case 0:return ppf_init_set(o,x,r,c);case 1:return ppf_init_set_ui(o,57,r,c);case 2:return ppf_init_set_si(o,-57,r,c);case 3:return ppf_init_set_uj(o,57,r,c);case 4:return ppf_init_set_sj(o,-57,r,c);case 5:return ppf_init_set_flt(o,7.125f,r,c);case 6:return ppf_init_set_d(o,7.125,r,c);case 7:return ppf_init_set_ld(o,7.125L,r,c);case 8:return ppf_init_set_z(o,z,r,c);case 9:return ppf_init_set_q(o,q,r,c);case 10:return ppf_init_set_f(o,g,r,c);case 11:return ppf_init_set_str(o,"7.125",10,r,c);
 }return PPF_TERNARY_ERROR;
}
static void initializer_sweeps(void){
 size_t failures=0;
 for(int k=0;k<12;++k)for(int mode=PPF_RNDN;mode<=PPF_RNDF;++mode){size_t total=0;
  for(size_t at=0;at==0||at<=total;++at){state s={0};ppf_context_t c;ppf_t o={0},x;mpz_t z;mpq_t q;mpf_t g;int t;
   setup(&c,&s);CHECK(!ppf_init2_ctx(&x,128,&c));CHECK(ppf_set_d(&x,7.125,PPF_RNDN,&c)==0);mpz_init_set_ui(z,57);mpq_init(q);mpq_set_ui(q,57,8);mpf_init2(g,128);mpf_set_d(g,7.125);
   arm(&s,&c,at);t=initialized_call(k,&o,&x,z,q,g,(ppf_rnd_t)mode,&c);
   if(!at){CHECK(t!=PPF_TERNARY_ERROR&&(k!=11||t==0));total=s.calls;CHECK(total>0);CHECK(ppf_validate(&o));}
   else {++failures;CHECK(s.failed==1);CHECK(t==PPF_TERNARY_ERROR||(k==11&&t==-1));CHECK(ppf_context_flags(&c)&PPF_FLAG_ALLOC);CHECK(!o.limbs||ppf_validate(&o));}
   s.fail_at=0;ppf_clear(&o);ppf_clear(&x);mpz_clear(z);mpq_clear(q);mpf_clear(g);ppf_context_clear(&c);CHECK(!s.live&&!s.bytes);
  }
 }
 printf("12 initialize-and-set endpoints: %zu injected failure cases passed\n",failures);
}
static int operation(int k,ppf_t*o,const ppf_t*x,const ppf_t*y,ppf_rnd_t r,ppf_context_t*c){char *end=NULL;switch(k){case 0:return ppf_strtofr(o,"7.125tail",&end,10,r,c);case 1:return ppf_dim(o,x,y,r,c);case 2:return ppf_mul_2exp(o,x,3,r,c);case 3:return ppf_div_2exp(o,x,3,r,c);}return PPF_TERNARY_ERROR;}
static void transactional_sweeps(void){
 size_t failures=0,fallbacks=0,unchanged_failures=0,by_endpoint[4]={0};
 for(int k=0;k<4;++k)for(int regime=0;regime<2;++regime)for(int mode=PPF_RNDN;mode<=PPF_RNDF;++mode)for(int overlap=0;overlap<2;++overlap)for(int ext=0;ext<2;++ext){size_t total=0;
  for(size_t at=0;at==0||at<=total;++at){state s={0};ppf_context_t c;ppf_t x,y,d,want;ppf_t *o;mp_limb_t storage[400]={0};snapshot before;int t,expected;
   setup(&c,&s);CHECK(!ppf_init2_ctx(&x,16385,&c));CHECK(!ppf_init2_ctx(&y,16385,&c));if(ext)CHECK(!ppf_custom_init_set(&d,PPF_ZERO_KIND,0,16385,storage));else CHECK(!ppf_init2_ctx(&d,16385,&c));
   CHECK(!ppf_set_d(&x,7.125,PPF_RNDN,&c));CHECK(!ppf_set_d(&y,-2.5,PPF_RNDN,&c));CHECK(!ppf_set_d(&d,-1.25,PPF_RNDN,&c));o=overlap?&x:&d;save(&before,o);CHECK(!ppf_init2_ctx(&want,16385,&c));if(regime){c.emin=-2;c.emax=2;}expected=operation(k,&want,&x,&y,(ppf_rnd_t)mode,&c);CHECK(expected!=PPF_TERNARY_ERROR);ppf_context_clear(&c);setup(&c,&s);if(regime){c.emin=-2;c.emax=2;}
   arm(&s,&c,at);t=operation(k,o,&x,&y,(ppf_rnd_t)mode,&c);
   if(!at){CHECK(t!=PPF_TERNARY_ERROR);total=s.calls;CHECK(ppf_validate(o));}
   else {++failures;++by_endpoint[k];CHECK(s.failed==1);CHECK(ppf_context_flags(&c)&PPF_FLAG_ALLOC);if(t==PPF_TERNARY_ERROR){++unchanged_failures;same(&before,o);}else {++fallbacks;CHECK(t==expected);CHECK(ppf_equal_p(o,&want));CHECK(o->sign==want.sign);}}
   s.fail_at=0;ppf_clear(&x);ppf_clear(&y);ppf_clear(&d);ppf_clear(&want);ppf_context_clear(&c);CHECK(!s.live&&!s.bytes);
  }
 }
 printf("4 allocation-completion wrappers: %zu injected failures, %zu unchanged errors, %zu exact successful fallbacks\n",failures,unchanged_failures,fallbacks);
 CHECK(by_endpoint[0]>0&&by_endpoint[1]>0&&by_endpoint[2]==0&&by_endpoint[3]==0);
 printf("wrapper failures strtofr/dim/mul_2exp/div_2exp: %zu/%zu/%zu/%zu\n",by_endpoint[0],by_endpoint[1],by_endpoint[2],by_endpoint[3]);
}
static void lifecycle(void){
 state s={0};ppf_context_t *c=ppf_default_context();ppf_t x[3],untouched;ppf_allocator_t a={allocate,resize,release,&s},original;
 CHECK(c);original=c->allocator;c->allocator=a;
 for(int which=0;which<2;++which)for(size_t at=1;at<=3;++at){
  memset(x,0xa5,sizeof(x));untouched=x[0];arm(&s,c,at);
  {int rc=which?ppf_inits(&x[0],&x[1],&x[2],(ppf_t*)NULL):ppf_inits2(257,&x[0],&x[1],&x[2],(ppf_t*)NULL);if(rc!=-1)fprintf(stderr,"lifecycle which=%d at=%zu rc=%d calls=%zu failed=%zu\n",which,at,rc,s.calls,s.failed);CHECK(rc==-1);}
  CHECK(s.failed==1);for(size_t i=0;i<at;++i){ppf_t zero={0};CHECK(!memcmp(&x[i],&zero,sizeof(zero)));}for(size_t i=at;i<3;++i)CHECK(!memcmp(&x[i],&untouched,sizeof(untouched)));CHECK(!s.live&&!s.bytes);
 }
 arm(&s,c,0);CHECK(!ppf_inits2(257,&x[0],&x[1],&x[2],(ppf_t*)NULL));
 arm(&s,c,1);ppf_swap(&x[0],&x[1]);ppf_clears(&x[0],&x[1],&x[2],(ppf_t*)NULL);CHECK(!s.calls&&!s.live&&!s.bytes);c->allocator=original;
 puts("variadic prefix cleanup, unvisited preservation, swap and clear passed");
}

static void invalid_special_rounding(void){
 state s={0};ppf_context_t c;ppf_t x;snapshot before;double values[]={0.0,-0.0,INFINITY,-INFINITY,NAN,1.25};setup(&c,&s);CHECK(!ppf_init2_ctx(&x,53,&c));
 for(size_t i=0;i<sizeof(values)/sizeof(*values);++i){
  CHECK(!ppf_set_d(&x,-3.5,PPF_RNDN,&c));save(&before,&x);arm(&s,&c,1);
  CHECK(ppf_set_flt(&x,(float)values[i],(ppf_rnd_t)999,&c)==PPF_TERNARY_ERROR);same(&before,&x);
  CHECK(ppf_set_d(&x,values[i],(ppf_rnd_t)999,&c)==PPF_TERNARY_ERROR);same(&before,&x);
  CHECK(ppf_set_ld(&x,(long double)values[i],(ppf_rnd_t)999,&c)==PPF_TERNARY_ERROR);same(&before,&x);
#if PPF_HAVE_FLOAT128
  CHECK(ppf_set_float128(&x,(ppf_float128_t)values[i],(ppf_rnd_t)999,&c)==PPF_TERNARY_ERROR);same(&before,&x);
#endif
#if PPF_HAVE_DECIMAL_FLOAT
  CHECK(ppf_set_decimal64(&x,(ppf_decimal64_t)values[i],(ppf_rnd_t)999,&c)==PPF_TERNARY_ERROR);same(&before,&x);
  CHECK(ppf_set_decimal128(&x,(ppf_decimal128_t)values[i],(ppf_rnd_t)999,&c)==PPF_TERNARY_ERROR);same(&before,&x);
#endif
  CHECK(c.flags==PPF_FLAG_ERANGE&&s.calls==0);s.fail_at=0;
 }
 ppf_clear(&x);ppf_context_clear(&c);CHECK(!s.live&&!s.bytes);puts("special floating setters reject invalid rounding before mutation");
}

int main(void){invalid_special_rounding();fixed_storage();initializer_sweeps();transactional_sweeps();lifecycle();ppf_default_context_cleanup();puts("allocation completion passed");return 0;}
