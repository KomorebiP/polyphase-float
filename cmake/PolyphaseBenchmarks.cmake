# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

function(ppf_add_mpfr_benchmark target source)
  if(NOT PPF_ADAPTER_TEST_TARGET)
    message(FATAL_ERROR "${target} requires PPF_MPFR_SUPPORT=ON or AUTO with MPFR available")
  endif()
  add_executable(${target} "${PROJECT_SOURCE_DIR}/benchmarks/${source}")
  target_compile_features(${target} PRIVATE c_std_11)
  set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF)
  target_include_directories(${target} PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src")
  if(NOT MPFR_HAS_HEADER)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/compat")
    target_compile_definitions(${target} PRIVATE PPF_USE_MINIMAL_MPFR_ABI)
  endif()
  target_link_libraries(${target} PRIVATE
    ${PPF_ADAPTER_TEST_TARGET}
    ${PPF_CORE_TEST_TARGET}
    MPFR::MPFR
    m)
  if(PPF_OPENMP_ENABLED)
    target_link_libraries(${target} PRIVATE OpenMP::OpenMP_C)
  endif()
  ppf_apply_project_warnings(${target})
endfunction()

function(ppf_add_core_benchmark target source)
  add_executable(${target} "${PROJECT_SOURCE_DIR}/benchmarks/${source}")
  target_compile_features(${target} PRIVATE c_std_11)
  set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF)
  target_include_directories(${target} PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src")
  target_link_libraries(${target} PRIVATE ${PPF_CORE_TEST_TARGET})
  ppf_apply_project_warnings(${target})
endfunction()

ppf_add_core_benchmark(inspect_product_plan inspect_product_plan.c)

ppf_add_mpfr_benchmark(benchmark_arithmetic_completion benchmark_arithmetic_completion.c)
ppf_add_mpfr_benchmark(benchmark_io_formatting benchmark_io_formatting.c)
ppf_add_mpfr_benchmark(benchmark_product_planner benchmark_product_planner.c)
ppf_add_mpfr_benchmark(benchmark_recsqrt_planner benchmark_recsqrt_planner.c)
ppf_add_mpfr_benchmark(benchmark_constants benchmark_constants.c)
ppf_add_mpfr_benchmark(benchmark_hotpath_method benchmark_hotpath_method.c)
ppf_add_mpfr_benchmark(benchmark_speedup_method benchmark_speedup_method.c)
ppf_add_mpfr_benchmark(benchmark_trig_hyperbolic benchmark_trig_hyperbolic.c)
ppf_add_mpfr_benchmark(benchmark_special_functions benchmark_special_functions.c)
ppf_add_mpfr_benchmark(benchmark_transcendental benchmark_transcendental.c)

if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME arithmetic_completion_smoke COMMAND benchmark_arithmetic_completion 1000 1)
  add_test(NAME io_formatting_smoke COMMAND benchmark_io_formatting 1000 1)
  add_test(NAME product_planner_smoke COMMAND benchmark_product_planner 10000 1 3 1)
  add_test(NAME product_plan_inspection_smoke COMMAND inspect_product_plan 100000 multiply 5)
  add_test(NAME recsqrt_planner_smoke COMMAND benchmark_recsqrt_planner 10000 1 3 1)
  add_test(NAME constants_smoke COMMAND benchmark_constants ppf_pi_cold 1000 2 3)
  add_test(NAME hotpath_smoke COMMAND benchmark_hotpath_method ppf_get_z 10000 1)
  add_test(NAME speedup_method_serial_smoke COMMAND benchmark_speedup_method ppf_mul_1t 10000 1 1 3)
  add_test(NAME speedup_method_parallel_smoke COMMAND benchmark_speedup_method ppf_mul_inner 10000 2 2 3)
  add_test(NAME trig_hyperbolic_smoke COMMAND benchmark_trig_hyperbolic 100 1)
  add_test(NAME special_functions_smoke COMMAND benchmark_special_functions 64 1)
  add_test(NAME transcendental_smoke COMMAND benchmark_transcendental 100 1)
  add_test(
    NAME benchmark_json_schema_smoke
    COMMAND "${Python3_EXECUTABLE}"
      "${PROJECT_SOURCE_DIR}/tests/test_benchmark_json_schema.py"
      --speedup "$<TARGET_FILE:benchmark_speedup_method>"
      --constants "$<TARGET_FILE:benchmark_constants>")
  set_tests_properties(
    arithmetic_completion_smoke io_formatting_smoke product_planner_smoke
    product_plan_inspection_smoke
    recsqrt_planner_smoke constants_smoke hotpath_smoke
    speedup_method_serial_smoke speedup_method_parallel_smoke
    trig_hyperbolic_smoke special_functions_smoke transcendental_smoke
    benchmark_json_schema_smoke
    PROPERTIES LABELS smoke TIMEOUT 600)
endif()
