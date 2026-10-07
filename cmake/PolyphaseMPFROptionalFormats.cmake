# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

# Detect optional MPFR conversion APIs using the same feature opt-ins required
# by the official header. Header visibility alone is insufficient because the
# corresponding symbols may be disabled when MPFR itself is configured.

set(PPF_MPFR_HAVE_FLOAT128_API 0)
set(PPF_MPFR_HAVE_DECIMAL_API 0)

if(NOT PPF_MPFR_ENABLED)
  return()
endif()

if(MPFR_HAS_HEADER)
  include(CheckCSourceCompiles)

  set(_ppf_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
  set(_ppf_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
  set(_ppf_saved_required_quiet "${CMAKE_REQUIRED_QUIET}")

  set(CMAKE_REQUIRED_INCLUDES
    "${PROJECT_SOURCE_DIR}/include"
    "${MPFR_INCLUDE_DIR}"
    "${GMP_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES "${MPFR_LIBRARY}" "${GMP_LIBRARY}" m)
  set(CMAKE_REQUIRED_QUIET TRUE)

  check_c_source_compiles([=[
#include "polyphase_float.h"
#if !PPF_HAVE_FLOAT128
# error "compiler has no binary128 type"
#endif
#define MPFR_USE_EXTENSION 1
#define MPFR_WANT_FLOAT128 1
#define mpfr_float128 ppf_float128_t
#include <mpfr.h>
int main(void) {
  mpfr_t x;
  ppf_float128_t value = (ppf_float128_t) 0;
  mpfr_init2(x, 53);
  (void) mpfr_set_float128(x, value, MPFR_RNDN);
  value = (ppf_float128_t) mpfr_get_float128(x, MPFR_RNDN);
  mpfr_clear(x);
  return value != (ppf_float128_t) 0;
}
]=] PPF_MPFR_FLOAT128_LINKS)

  check_c_source_compiles([=[
#include "polyphase_float.h"
#if !PPF_HAVE_DECIMAL_FLOAT
# error "compiler has no decimal64/decimal128 types"
#endif
#define MPFR_USE_EXTENSION 1
#define MPFR_WANT_DECIMAL_FLOATS 1
#include <mpfr.h>
int main(void) {
  mpfr_t x;
  ppf_decimal64_t value64 = (ppf_decimal64_t) 0;
  ppf_decimal128_t value128 = (ppf_decimal128_t) 0;
  mpfr_init2(x, 53);
  (void) mpfr_set_decimal64(x, value64, MPFR_RNDN);
  value64 = (ppf_decimal64_t) mpfr_get_decimal64(x, MPFR_RNDN);
  (void) mpfr_set_decimal128(x, value128, MPFR_RNDN);
  value128 = (ppf_decimal128_t) mpfr_get_decimal128(x, MPFR_RNDN);
  mpfr_clear(x);
  return value64 != (ppf_decimal64_t) 0 ||
         value128 != (ppf_decimal128_t) 0;
}
]=] PPF_MPFR_DECIMAL_LINKS)

  set(CMAKE_REQUIRED_INCLUDES "${_ppf_saved_required_includes}")
  set(CMAKE_REQUIRED_LIBRARIES "${_ppf_saved_required_libraries}")
  set(CMAKE_REQUIRED_QUIET "${_ppf_saved_required_quiet}")
else()
  include(CheckLibraryExists)
  check_library_exists("${MPFR_LIBRARY}" mpfr_set_float128 ""
    PPF_MPFR_HAVE_SET_FLOAT128_SYMBOL)
  check_library_exists("${MPFR_LIBRARY}" mpfr_get_float128 ""
    PPF_MPFR_HAVE_GET_FLOAT128_SYMBOL)
  check_library_exists("${MPFR_LIBRARY}" mpfr_set_decimal64 ""
    PPF_MPFR_HAVE_SET_DECIMAL64_SYMBOL)
  check_library_exists("${MPFR_LIBRARY}" mpfr_get_decimal64 ""
    PPF_MPFR_HAVE_GET_DECIMAL64_SYMBOL)
  check_library_exists("${MPFR_LIBRARY}" mpfr_set_decimal128 ""
    PPF_MPFR_HAVE_SET_DECIMAL128_SYMBOL)
  check_library_exists("${MPFR_LIBRARY}" mpfr_get_decimal128 ""
    PPF_MPFR_HAVE_GET_DECIMAL128_SYMBOL)

  if(PPF_MPFR_HAVE_SET_FLOAT128_SYMBOL AND
     PPF_MPFR_HAVE_GET_FLOAT128_SYMBOL)
    set(PPF_MPFR_FLOAT128_LINKS TRUE)
  endif()
  if(PPF_MPFR_HAVE_SET_DECIMAL64_SYMBOL AND
     PPF_MPFR_HAVE_GET_DECIMAL64_SYMBOL AND
     PPF_MPFR_HAVE_SET_DECIMAL128_SYMBOL AND
     PPF_MPFR_HAVE_GET_DECIMAL128_SYMBOL)
    set(PPF_MPFR_DECIMAL_LINKS TRUE)
  endif()
endif()

if(PPF_MPFR_FLOAT128_LINKS)
  set(PPF_MPFR_HAVE_FLOAT128_API 1)
endif()
if(PPF_MPFR_DECIMAL_LINKS)
  set(PPF_MPFR_HAVE_DECIMAL_API 1)
endif()

message(STATUS "MPFR binary128 conversion API: ${PPF_MPFR_HAVE_FLOAT128_API}")
message(STATUS "MPFR decimal conversion API: ${PPF_MPFR_HAVE_DECIMAL_API}")
