/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define main existing_test_main
#include "test_special_edge_audit.c"
#undef main
int main(void) {
 const unsigned ps[]={2,17,53,113};
 const char *x[]={"0.125","0.5","0.75","0.875","1.125","1.5","1.875","2.5","-0.125","-0.5","-0.875","-1.125","-1.5","-1.875","-3.5"};
 for(unsigned i=0;i<4;i++) for(unsigned j=0;j<15;j++) {
 unary_case("gamma",ppf_gamma,mpfr_gamma,x[j],ps[i],0);
 unary_case("digamma",ppf_digamma,mpfr_digamma,x[j],ps[i],0);
 unary_case("li2",ppf_li2,mpfr_li2,x[j],ps[i],0);
 unary_case("eint",ppf_eint,mpfr_eint,x[j],ps[i],0);
 }
 for(int s=-1;s<=1;s+=2) {
 high_input_precision_case("gamma",ppf_gamma,mpfr_gamma,2,s);
 high_input_precision_case("gamma",ppf_gamma,mpfr_gamma,-2,s);
 high_input_precision_case("digamma",ppf_digamma,mpfr_digamma,1,s);
 high_input_precision_case("li2",ppf_li2,mpfr_li2,-1,s);
 }
 ppf_free_constant_caches();mpfr_mp_memory_cleanup();
 printf("independent changed-path review sweep passed: %lu oracle comparisons\n",cases);return 0;
}
