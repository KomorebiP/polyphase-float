# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.23)

project(PolyphaseFloat
  VERSION 0.20.0
  DESCRIPTION "Independent arbitrary-precision binary floating-point arithmetic"
  LANGUAGES C)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(PPF_BUILD_SHARED "Build the shared Polyphase Float library" ON)
option(PPF_BUILD_STATIC "Build the static Polyphase Float library" ON)
option(PPF_BUILD_EXAMPLES "Build API examples" ${PROJECT_IS_TOP_LEVEL})
option(PPF_BUILD_BENCHMARKS "Build benchmark smoke executables" OFF)
option(PPF_BUILD_FUZZERS "Build Clang/libFuzzer hardening targets" OFF)
option(PPF_ENABLE_NATIVE_OPTIMIZATIONS "Enable -march=native on supported compilers" OFF)
option(PPF_WARNINGS_AS_ERRORS "Treat Polyphase Float warnings as errors" OFF)
option(PPF_ENABLE_THREAD_SANITIZER
  "Instrument Polyphase Float and its tests with ThreadSanitizer" OFF)

set(PPF_MPFR_SUPPORT "AUTO" CACHE STRING
  "MPFR adapter and differential support: AUTO, ON, or OFF")
set(PPF_OPENMP "AUTO" CACHE STRING
  "OpenMP execution support: AUTO, ON, or OFF")
set(PPF_ELF_SYMBOL_VERSIONING "AUTO" CACHE STRING
  "ELF symbol versioning: AUTO, ON, or OFF")
set(PPF_PRODUCT_PROFILE_HEADER "" CACHE FILEPATH
  "Optional generated product-planner profile header")
set_property(CACHE PPF_MPFR_SUPPORT PROPERTY STRINGS AUTO ON OFF)
set_property(CACHE PPF_OPENMP PROPERTY STRINGS AUTO ON OFF)
set_property(CACHE PPF_ELF_SYMBOL_VERSIONING PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${PPF_MPFR_SUPPORT}" PPF_MPFR_SUPPORT)
string(TOUPPER "${PPF_OPENMP}" PPF_OPENMP)
string(TOUPPER "${PPF_ELF_SYMBOL_VERSIONING}" PPF_ELF_SYMBOL_VERSIONING)
if(NOT PPF_MPFR_SUPPORT MATCHES "^(AUTO|ON|OFF)$")
  message(FATAL_ERROR "PPF_MPFR_SUPPORT must be AUTO, ON, or OFF")
endif()
if(NOT PPF_OPENMP MATCHES "^(AUTO|ON|OFF)$")
  message(FATAL_ERROR "PPF_OPENMP must be AUTO, ON, or OFF")
endif()
if(NOT PPF_ELF_SYMBOL_VERSIONING MATCHES "^(AUTO|ON|OFF)$")
  message(FATAL_ERROR
    "PPF_ELF_SYMBOL_VERSIONING must be AUTO, ON, or OFF")
endif()

set(PPF_ELF_SYMBOL_VERSIONING_ENABLED FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(NOT PPF_ELF_SYMBOL_VERSIONING STREQUAL "OFF")
    set(PPF_ELF_SYMBOL_VERSIONING_ENABLED TRUE)
  endif()
elseif(PPF_ELF_SYMBOL_VERSIONING STREQUAL "ON")
  message(FATAL_ERROR
    "PPF_ELF_SYMBOL_VERSIONING=ON is currently supported only on Linux")
endif()

if(NOT PPF_BUILD_SHARED AND NOT PPF_BUILD_STATIC)
  message(FATAL_ERROR "At least one of PPF_BUILD_SHARED or PPF_BUILD_STATIC must be ON")
endif()

if(PPF_BUILD_FUZZERS)
  if(PPF_ENABLE_THREAD_SANITIZER)
    message(FATAL_ERROR
      "PPF_BUILD_FUZZERS and PPF_ENABLE_THREAD_SANITIZER are mutually exclusive")
  endif()
  if(BUILD_TESTING OR PPF_BUILD_EXAMPLES OR PPF_BUILD_BENCHMARKS OR
      PPF_BUILD_SHARED OR NOT PPF_BUILD_STATIC)
    message(FATAL_ERROR
      "PPF_BUILD_FUZZERS=ON requires an isolated static-core build: "
      "set BUILD_TESTING=OFF, PPF_BUILD_EXAMPLES=OFF, "
      "PPF_BUILD_BENCHMARKS=OFF, PPF_BUILD_SHARED=OFF, and "
      "PPF_BUILD_STATIC=ON")
  endif()
endif()

find_package(GMP REQUIRED)
find_package(Threads REQUIRED)

set(PPF_OPENMP_ENABLED FALSE)
if(NOT PPF_OPENMP STREQUAL "OFF")
  find_package(OpenMP QUIET COMPONENTS C)
  if(OpenMP_C_FOUND)
    set(PPF_OPENMP_ENABLED TRUE)
  elseif(PPF_OPENMP STREQUAL "ON")
    message(FATAL_ERROR
      "PPF_OPENMP=ON, but the active C toolchain has no usable OpenMP support")
  else()
    message(STATUS
      "OpenMP not found: the serial compatibility path will be built")
  endif()
endif()
set(PPF_INSTALLED_WITH_OPENMP ${PPF_OPENMP_ENABLED})

if(PPF_ENABLE_THREAD_SANITIZER)
  if(PPF_OPENMP_ENABLED)
    message(FATAL_ERROR
      "ThreadSanitizer validation requires PPF_OPENMP=OFF; instrumented "
      "OpenMP runtimes are not part of the supported race-detection path")
  endif()
  if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    message(FATAL_ERROR
      "PPF_ENABLE_THREAD_SANITIZER requires GCC or a Clang-compatible compiler")
  endif()
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
endif()

set(PPF_MPFR_ENABLED FALSE)
if(NOT PPF_MPFR_SUPPORT STREQUAL "OFF")
  find_package(MPFR QUIET)
  if(MPFR_FOUND)
    set(PPF_MPFR_ENABLED TRUE)
  elseif(PPF_MPFR_SUPPORT STREQUAL "ON")
    message(FATAL_ERROR "PPF_MPFR_SUPPORT=ON, but the MPFR library was not found")
  else()
    message(STATUS "MPFR not found: adapter and differential tests are disabled")
  endif()
endif()

include(cmake/PolyphaseMPFROptionalFormats.cmake)

set(PPF_PRODUCT_PROFILE_GENERATED_DIR
  "${PROJECT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${PPF_PRODUCT_PROFILE_GENERATED_DIR}")
if(PPF_PRODUCT_PROFILE_HEADER)
  get_filename_component(PPF_PRODUCT_PROFILE_SOURCE
    "${PPF_PRODUCT_PROFILE_HEADER}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
  if(NOT EXISTS "${PPF_PRODUCT_PROFILE_SOURCE}")
    message(FATAL_ERROR
      "PPF_PRODUCT_PROFILE_HEADER does not exist: ${PPF_PRODUCT_PROFILE_SOURCE}")
  endif()
else()
  set(PPF_PRODUCT_PROFILE_SOURCE
    "${PROJECT_SOURCE_DIR}/src/ppf_product_profile_build.h")
endif()
configure_file(
  "${PPF_PRODUCT_PROFILE_SOURCE}"
  "${PPF_PRODUCT_PROFILE_GENERATED_DIR}/ppf_product_profile_build.h"
  COPYONLY)

set(PPF_CORE_SOURCES
  src/abi.c
  src/context.c
  src/result_transaction.c
  src/arithmetic_root_transaction.c
  src/transcendental_special_transaction.c
  src/object.c
  src/round.c
  src/limb_round.c
  src/toom3.c
  src/window_product.c
  src/product_planner.c
  src/arithmetic.c
  src/arithmetic_variants.c
  src/assignment.c
  src/io.c
  src/format.c
  src/utilities.c
  src/remainder.c
  src/range_flags.c
  src/roots.c
  src/divisor_plan.c
  src/constants.c
  src/constant_pi.c
  src/constant_log2.c
  src/transcendental_fast.c
  src/transcendental.c
  src/trig_fast.c
  src/inverse_angle_fast.c
  src/trig_hyperbolic.c
  src/special_common.c
  src/special_gamma.c
  src/special_misc.c
  src/special_bessel.c
  src/runtime_support.c
  src/random.c
  src/conditional_formats.c)

function(ppf_apply_project_warnings target)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(PPF_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(PPF_ENABLE_NATIVE_OPTIMIZATIONS)
      target_compile_options(${target} PRIVATE -march=native)
    endif()
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(PPF_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  endif()
endfunction()

function(ppf_configure_core_target target)
  target_sources(${target} PRIVATE ${PPF_CORE_SOURCES})
  target_compile_features(${target} PUBLIC c_std_11)
  set_target_properties(${target} PROPERTIES
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON)
  target_include_directories(${target}
    PUBLIC
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
      "${PPF_PRODUCT_PROFILE_GENERATED_DIR}"
      "${PROJECT_SOURCE_DIR}/src")
  target_link_libraries(${target}
    PUBLIC GMP::GMP
    PRIVATE Threads::Threads)
  if(PPF_OPENMP_ENABLED)
    target_link_libraries(${target} PRIVATE OpenMP::OpenMP_C)
  endif()
  if(NOT WIN32)
    target_link_libraries(${target} PRIVATE m)
  endif()
  if(PPF_BUILD_FUZZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
      message(FATAL_ERROR
        "PPF_BUILD_FUZZERS requires a Clang-compatible compiler")
    endif()
    target_compile_options(${target} PRIVATE
      -fsanitize=fuzzer-no-link,address,undefined
      -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE
      -fsanitize=address,undefined
      -fno-omit-frame-pointer)
  endif()
  ppf_apply_project_warnings(${target})
endfunction()

set(PPF_INSTALL_TARGETS)
set(PPF_CORE_TEST_TARGET "")

if(PPF_BUILD_SHARED)
  add_library(polyphase_float SHARED)
  add_library(PolyphaseFloat::shared ALIAS polyphase_float)
  ppf_configure_core_target(polyphase_float)
  target_compile_definitions(polyphase_float PRIVATE
    PPF_BUILD_SHARED
    PPF_BUILDING_LIBRARY)
  set_target_properties(polyphase_float PROPERTIES
    OUTPUT_NAME polyphase_float
    EXPORT_NAME shared
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
  if(PPF_ELF_SYMBOL_VERSIONING_ENABLED)
    target_link_options(polyphase_float PRIVATE
      "LINKER:--version-script=${PROJECT_SOURCE_DIR}/abi/polyphase_float.map")
    set_property(TARGET polyphase_float APPEND PROPERTY LINK_DEPENDS
      "${PROJECT_SOURCE_DIR}/abi/polyphase_float.map")
  endif()
  list(APPEND PPF_INSTALL_TARGETS polyphase_float)
  set(PPF_CORE_TEST_TARGET polyphase_float)
endif()

if(PPF_BUILD_STATIC)
  add_library(polyphase_float_static STATIC)
  add_library(PolyphaseFloat::static ALIAS polyphase_float_static)
  ppf_configure_core_target(polyphase_float_static)
  set_target_properties(polyphase_float_static PROPERTIES
    OUTPUT_NAME polyphase_float
    EXPORT_NAME static)
  list(APPEND PPF_INSTALL_TARGETS polyphase_float_static)
  if(NOT PPF_CORE_TEST_TARGET)
    set(PPF_CORE_TEST_TARGET polyphase_float_static)
  endif()
endif()

set(PPF_ADAPTER_TEST_TARGET "")
set(PPF_ADAPTER_INSTALL_TARGETS)
set(PPF_INSTALLED_WITH_MPFR FALSE)

function(ppf_configure_mpfr_adapter target core_target)
  target_sources(${target} PRIVATE adapter/mpfr_adapter.c)
  target_compile_features(${target} PUBLIC c_std_11)
  target_include_directories(${target}
    PUBLIC
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
  target_link_libraries(${target} PUBLIC ${core_target} MPFR::MPFR)
  if(NOT MPFR_HAS_HEADER)
    target_include_directories(${target} PUBLIC
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/compat>")
    target_compile_definitions(${target} PUBLIC PPF_USE_MINIMAL_MPFR_ABI)
  endif()
  ppf_apply_project_warnings(${target})
endfunction()

if(PPF_MPFR_ENABLED)
  if(NOT MPFR_HAS_HEADER)
    message(WARNING
      "MPFR library found without mpfr.h. The compatibility declaration file "
      "will be used for local validation. Install-tree adapter targets will not "
      "be exported from this configuration.")
  endif()

  if(PPF_BUILD_SHARED)
    add_library(polyphase_float_mpfr SHARED)
    add_library(PolyphaseFloat::mpfr_shared ALIAS polyphase_float_mpfr)
    ppf_configure_mpfr_adapter(polyphase_float_mpfr polyphase_float)
    target_compile_definitions(polyphase_float_mpfr PRIVATE
      PPF_BUILD_SHARED
      PPF_BUILDING_LIBRARY)
    set_target_properties(polyphase_float_mpfr PROPERTIES
      OUTPUT_NAME polyphase_float_mpfr
      EXPORT_NAME mpfr_shared
      VERSION "${PROJECT_VERSION}"
      SOVERSION "${PROJECT_VERSION_MAJOR}"
      C_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES)
    if(PPF_ELF_SYMBOL_VERSIONING_ENABLED)
      target_link_options(polyphase_float_mpfr PRIVATE
        "LINKER:--version-script=${PROJECT_SOURCE_DIR}/abi/polyphase_float_mpfr.map")
      set_property(TARGET polyphase_float_mpfr APPEND PROPERTY LINK_DEPENDS
        "${PROJECT_SOURCE_DIR}/abi/polyphase_float_mpfr.map")
    endif()
    set(PPF_ADAPTER_TEST_TARGET polyphase_float_mpfr)
    if(MPFR_HAS_HEADER)
      list(APPEND PPF_ADAPTER_INSTALL_TARGETS polyphase_float_mpfr)
    endif()
  endif()

  if(PPF_BUILD_STATIC)
    add_library(polyphase_float_mpfr_static STATIC)
    add_library(PolyphaseFloat::mpfr_static ALIAS polyphase_float_mpfr_static)
    ppf_configure_mpfr_adapter(polyphase_float_mpfr_static polyphase_float_static)
    set_target_properties(polyphase_float_mpfr_static PROPERTIES
      OUTPUT_NAME polyphase_float_mpfr
      EXPORT_NAME mpfr_static)
    if(NOT PPF_ADAPTER_TEST_TARGET)
      set(PPF_ADAPTER_TEST_TARGET polyphase_float_mpfr_static)
    endif()
    if(MPFR_HAS_HEADER)
      list(APPEND PPF_ADAPTER_INSTALL_TARGETS polyphase_float_mpfr_static)
    endif()
  endif()

  if(MPFR_HAS_HEADER)
    set(PPF_INSTALLED_WITH_MPFR TRUE)
  endif()
endif()

if(BUILD_TESTING)
  include(cmake/PolyphaseTests.cmake)
endif()

if(PPF_BUILD_EXAMPLES)
  include(cmake/PolyphaseExamples.cmake)
endif()

if(PPF_BUILD_BENCHMARKS)
  include(cmake/PolyphaseBenchmarks.cmake)
endif()

if(PPF_BUILD_FUZZERS)
  include(cmake/PolyphaseFuzzers.cmake)
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
  set(PPF_INSTALL_PACKAGING_COMMAND
    "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_install_packaging.py"
    --root "${PROJECT_SOURCE_DIR}" --cmake "${CMAKE_COMMAND}"
    --ctest "${CMAKE_CTEST_COMMAND}" --cc "${CMAKE_C_COMPILER}"
    --gmp-include "${GMP_INCLUDE_DIR}" --gmp-library "${GMP_LIBRARY}")
  if(PKG_CONFIG_EXECUTABLE)
    list(APPEND PPF_INSTALL_PACKAGING_COMMAND
      --pkg-config "${PKG_CONFIG_EXECUTABLE}")
  endif()
  if(PPF_INSTALLED_WITH_MPFR)
    list(APPEND PPF_INSTALL_PACKAGING_COMMAND --with-mpfr
      --mpfr-include "${MPFR_INCLUDE_DIR}" --mpfr-library "${MPFR_LIBRARY}")
  endif()
  add_test(NAME install_packaging COMMAND ${PPF_INSTALL_PACKAGING_COMMAND})
  set_tests_properties(install_packaging PROPERTIES
    LABELS "fast;packaging" TIMEOUT 900)

  add_test(NAME release_api_audit
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/tests/test_release_api_audit.py" --cc "${CMAKE_C_COMPILER}")
  set_tests_properties(release_api_audit PROPERTIES LABELS "fast;hardening;semantic" TIMEOUT 180)

  add_test(NAME qualification_runner_policy
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/tests/test_qualification_runner.py")
  set_tests_properties(qualification_runner_policy PROPERTIES LABELS "fast;hardening" TIMEOUT 180)

  add_test(NAME product_profile_generator
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/test_generate_product_profile.py")
  set_tests_properties(product_profile_generator PROPERTIES
    LABELS "fast"
    TIMEOUT 180)

  add_test(NAME semantic_contract_current
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/semantic_contract.py" verify
            --root "${PROJECT_SOURCE_DIR}"
            --contract "${PROJECT_SOURCE_DIR}/semantics/contract-v1.json")
  set_tests_properties(semantic_contract_current PROPERTIES
    LABELS "fast;hardening;semantic" TIMEOUT 180)

  add_test(NAME semantic_contract_policy
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/test_semantic_contract.py"
            --root "${PROJECT_SOURCE_DIR}")
  set_tests_properties(semantic_contract_policy PROPERTIES
    LABELS "fast;hardening;semantic" TIMEOUT 180)

  add_test(NAME aliasing_contract_current
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/aliasing_contract.py" verify
            --root "${PROJECT_SOURCE_DIR}"
            --contract "${PROJECT_SOURCE_DIR}/semantics/aliasing-contract-v1.json")
  set_tests_properties(aliasing_contract_current PROPERTIES
    LABELS "fast;hardening;semantic;aliasing" TIMEOUT 180)

  add_test(NAME aliasing_contract_policy
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/test_aliasing_contract.py"
            --root "${PROJECT_SOURCE_DIR}")
  set_tests_properties(aliasing_contract_policy PROPERTIES
    LABELS "fast;hardening;semantic;aliasing" TIMEOUT 180)

  add_test(NAME abi_contract_current
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/abi_contract.py" verify
            --root "${PROJECT_SOURCE_DIR}"
            --contract "${PROJECT_SOURCE_DIR}/abi/contract-v1.json")
  set_tests_properties(abi_contract_current PROPERTIES
    LABELS "fast;hardening" TIMEOUT 180)

  add_test(NAME abi_contract_compatibility
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/abi_contract.py" compare
            --baseline "${PROJECT_SOURCE_DIR}/abi/baselines/0.11.0-contract.json"
            --current "${PROJECT_SOURCE_DIR}/abi/contract-v1.json")
  set_tests_properties(abi_contract_compatibility PROPERTIES
    LABELS "fast;hardening" TIMEOUT 180)

  add_test(NAME abi_contract_previous_release
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/abi_contract.py" compare
            --baseline "${PROJECT_SOURCE_DIR}/abi/baselines/0.19.0-contract.json"
            --current "${PROJECT_SOURCE_DIR}/abi/contract-v1.json")
  set_tests_properties(abi_contract_previous_release PROPERTIES
    LABELS "fast;hardening" TIMEOUT 180)

  add_test(NAME abi_layout_compatibility
    COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/abi_layout_compare.py"
            --probe "$<TARGET_FILE:abi_layout_probe>"
            --baseline "${PROJECT_SOURCE_DIR}/abi/baselines/0.11.0-linux-x86_64-lp64-gmp64-layout.json"
            --skip-model-mismatch)
  set_tests_properties(abi_layout_compatibility PROPERTIES
    LABELS "fast;hardening" TIMEOUT 180 SKIP_RETURN_CODE 77)

  if(PPF_BUILD_SHARED)
    set(PPF_ABI_EXPORT_COMMAND
      "${Python3_EXECUTABLE}"
      "${PROJECT_SOURCE_DIR}/tools/abi_exports.py"
      --root "${PROJECT_SOURCE_DIR}"
      --core-library "$<TARGET_FILE:polyphase_float>")
    if(TARGET polyphase_float_mpfr)
      list(APPEND PPF_ABI_EXPORT_COMMAND
        --adapter-library "$<TARGET_FILE:polyphase_float_mpfr>")
    endif()
    if(PPF_ELF_SYMBOL_VERSIONING_ENABLED)
      list(APPEND PPF_ABI_EXPORT_COMMAND --require-versions)
    endif()
    add_test(NAME abi_exports COMMAND ${PPF_ABI_EXPORT_COMMAND})
    set_tests_properties(abi_exports PROPERTIES
      LABELS "fast;hardening" TIMEOUT 180)
  endif()
endif()
if(Python3_Interpreter_FOUND)
  add_custom_target(semantic-contract-audit
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/semantic_contract.py" verify
            --root "${PROJECT_SOURCE_DIR}"
            --contract "${PROJECT_SOURCE_DIR}/semantics/contract-v1.json"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_semantic_contract.py"
            --root "${PROJECT_SOURCE_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/aliasing_contract.py" verify
            --root "${PROJECT_SOURCE_DIR}"
            --contract "${PROJECT_SOURCE_DIR}/semantics/aliasing-contract-v1.json"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_aliasing_contract.py"
            --root "${PROJECT_SOURCE_DIR}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM)

  add_custom_target(abi-contract-audit
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/abi_contract.py" verify
            --root "${PROJECT_SOURCE_DIR}"
            --contract "${PROJECT_SOURCE_DIR}/abi/contract-v1.json"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/abi_contract.py" compare
            --baseline "${PROJECT_SOURCE_DIR}/abi/baselines/0.11.0-contract.json"
            --current "${PROJECT_SOURCE_DIR}/abi/contract-v1.json"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/abi_contract.py" compare
            --baseline "${PROJECT_SOURCE_DIR}/abi/baselines/0.19.0-contract.json"
            --current "${PROJECT_SOURCE_DIR}/abi/contract-v1.json"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM)
  if(PPF_BUILD_SHARED)
    add_custom_target(release-audit
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/release_audit.py"
              --library "$<TARGET_FILE:polyphase_float>"
      DEPENDS polyphase_float
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      VERBATIM)
  else()
    add_custom_target(release-audit
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/release_audit.py"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      VERBATIM)
  endif()
endif()

# pkg-config metadata lives below the configured libdir, which may contain
# several components (for example lib/x86_64-linux-gnu). Derive the prefix
# instead of assuming that it is exactly two parent directories away.
file(RELATIVE_PATH PPF_PC_PREFIX_RELATIVE
  "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig" "${CMAKE_INSTALL_PREFIX}")
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
  set(PPF_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
  set(PPF_PC_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
  set(PPF_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
  set(PPF_PC_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

if(PPF_INSTALLED_WITH_MPFR)
  configure_file(
    "${PROJECT_SOURCE_DIR}/polyphase-float-mpfr.pc.in"
    "${PROJECT_BINARY_DIR}/polyphase-float-mpfr.pc"
    @ONLY)
endif()

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/PolyphaseFloatConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/PolyphaseFloatConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PolyphaseFloat")

write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/PolyphaseFloatConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion)
# ABI epoch 1 preserves the public 0.11.0 interface across compatible 0.x
# releases. Older interfaces are outside the governed compatibility evidence.
file(APPEND "${PROJECT_BINARY_DIR}/PolyphaseFloatConfigVersion.cmake" [=[

if(DEFINED PACKAGE_FIND_VERSION AND
    PACKAGE_FIND_VERSION VERSION_LESS "0.11.0")
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
  set(PACKAGE_VERSION_EXACT FALSE)
endif()
]=])

install(TARGETS ${PPF_INSTALL_TARGETS}
  EXPORT PolyphaseFloatTargets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(PPF_ADAPTER_INSTALL_TARGETS)
  install(TARGETS ${PPF_ADAPTER_INSTALL_TARGETS}
    EXPORT PolyphaseFloatMPFRTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  install(EXPORT PolyphaseFloatMPFRTargets
    FILE PolyphaseFloatMPFRTargets.cmake
    NAMESPACE PolyphaseFloat::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PolyphaseFloat")
endif()

install(FILES include/polyphase_float.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
if(PPF_INSTALLED_WITH_MPFR)
  install(FILES include/polyphase_float_mpfr.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif()

install(FILES "${PROJECT_BINARY_DIR}/polyphase-float.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
if(PPF_INSTALLED_WITH_MPFR)
  install(FILES "${PROJECT_BINARY_DIR}/polyphase-float-mpfr.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

install(EXPORT PolyphaseFloatTargets
  FILE PolyphaseFloatTargets.cmake
  NAMESPACE PolyphaseFloat::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PolyphaseFloat")

install(FILES
  "${PROJECT_BINARY_DIR}/PolyphaseFloatConfig.cmake"
  "${PROJECT_BINARY_DIR}/PolyphaseFloatConfigVersion.cmake"
  cmake/FindGMP.cmake
  cmake/FindMPFR.cmake
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PolyphaseFloat")

set(PPF_PC_MATH_LIB "")
if(NOT WIN32)
  set(PPF_PC_MATH_LIB "-lm")
endif()
set(PPF_PC_OPENMP_LIB "")
if(PPF_OPENMP_ENABLED)
  set(PPF_PC_OPENMP_LIB "${OpenMP_C_FLAGS}")
  string(REPLACE ";" " " PPF_PC_OPENMP_LIB "${PPF_PC_OPENMP_LIB}")
endif()
# Reconfigure after PPF_PC_MATH_LIB is known.
configure_file(
  "${PROJECT_SOURCE_DIR}/polyphase-float.pc.in"
  "${PROJECT_BINARY_DIR}/polyphase-float.pc"
  @ONLY)

# Preserve source-relative documentation links in installed copies. Installing
# docs/*.md as a flat list breaks README's docs/ links and sibling references.
install(FILES
  README.md README_FIRST.txt LICENSE.md LICENSE COPYING AUTHORS.md
  THIRD_PARTY_NOTICES.md ROADMAP.md STATUS.md RELEASE_NOTES.md
  SUPPORT.md SECURITY.md CONTRIBUTING.md CODE_OF_CONDUCT.md GOVERNANCE.md
  CITATION.cff
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/PolyphaseFloat")
install(DIRECTORY docs examples
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/PolyphaseFloat"
  FILES_MATCHING
    PATTERN "*.md" PATTERN "*.txt" PATTERN "*.json" PATTERN "*.csv"
    PATTERN "*.c" PATTERN "*.h")
install(DIRECTORY abi/
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/PolyphaseFloat/abi"
  FILES_MATCHING PATTERN "*.json" PATTERN "*.txt" PATTERN "*.md")
install(FILES
  semantics/contract-v1.json
  semantics/function-matrix-v1.csv
  semantics/aliasing-contract-v1.json
  semantics/aliasing-matrix-v1.csv
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/PolyphaseFloat/semantics")

message(STATUS "Polyphase Float ${PROJECT_VERSION}")
message(STATUS "  shared library: ${PPF_BUILD_SHARED}")
message(STATUS "  static library: ${PPF_BUILD_STATIC}")
message(STATUS "  MPFR support:   ${PPF_MPFR_ENABLED} (${PPF_MPFR_SUPPORT})")
message(STATUS "  MPFR header:    ${MPFR_HAS_HEADER}")
message(STATUS "  OpenMP support: ${PPF_OPENMP_ENABLED} (${PPF_OPENMP})")
message(STATUS "  ELF symbol versions: ${PPF_ELF_SYMBOL_VERSIONING_ENABLED} (${PPF_ELF_SYMBOL_VERSIONING})")
message(STATUS "  examples:       ${PPF_BUILD_EXAMPLES}")
message(STATUS "  benchmarks:     ${PPF_BUILD_BENCHMARKS}")
message(STATUS "  fuzzers:        ${PPF_BUILD_FUZZERS}")
message(STATUS "  ThreadSanitizer:${PPF_ENABLE_THREAD_SANITIZER}")
message(STATUS "  native tuning:  ${PPF_ENABLE_NATIVE_OPTIMIZATIONS}")
message(STATUS "  product profile:${PPF_PRODUCT_PROFILE_SOURCE}")
