/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef PPF_OPENMP_COMPAT_H
#define PPF_OPENMP_COMPAT_H

/* Keep OpenMP out of the numerical source contract.  In an OpenMP build these
   wrappers expand to the ordinary runtime and directives.  In a serial build
   they reduce to one-worker structured C, so the exact reconstruction and
   rounding paths remain unchanged. */
#ifdef _OPENMP
#include <omp.h>
#define PPF_OMP_STRINGIZE_IMPL(...) #__VA_ARGS__
#define PPF_OMP_STRINGIZE(...) PPF_OMP_STRINGIZE_IMPL(__VA_ARGS__)
#define PPF_OMP_PRAGMA(...) _Pragma(PPF_OMP_STRINGIZE(__VA_ARGS__))
#else
#define PPF_OMP_PRAGMA(...)
static inline int omp_in_parallel(void) { return 0; }
static inline int omp_get_thread_num(void) { return 0; }
static inline int omp_get_max_threads(void) { return 1; }
static inline void omp_set_dynamic(int dynamic_threads) {
    (void)dynamic_threads;
}
#endif

#endif
