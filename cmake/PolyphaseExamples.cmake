# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

function(ppf_add_example name source)
  add_executable(example_${name} "${PROJECT_SOURCE_DIR}/examples/${source}")
  target_compile_features(example_${name} PRIVATE c_std_11)
  set_target_properties(example_${name} PROPERTIES C_EXTENSIONS OFF)
  target_include_directories(example_${name} PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries(example_${name} PRIVATE ${PPF_CORE_TEST_TARGET})
  ppf_apply_project_warnings(example_${name})
endfunction()

ppf_add_example(basic basic.c)
ppf_add_example(arithmetic arithmetic.c)
ppf_add_example(arithmetic_completion arithmetic_completion.c)
ppf_add_example(assignment_conversion assignment_conversion.c)
ppf_add_example(utilities_remainder utilities_remainder.c)
ppf_add_example(constants constants.c)
ppf_add_example(range_roots_division range_roots_division.c)
ppf_add_example(transcendentals transcendentals.c)
ppf_add_example(trig_hyperbolic trig_hyperbolic.c)
ppf_add_example(special_functions special_functions.c)
ppf_add_example(io_formatting io_formatting.c)
ppf_add_example(runtime_completion runtime_completion.c)

if(PPF_ADAPTER_TEST_TARGET)
  add_executable(example_mpfr_bridge "${PROJECT_SOURCE_DIR}/examples/mpfr_bridge.c")
  target_compile_features(example_mpfr_bridge PRIVATE c_std_11)
  set_target_properties(example_mpfr_bridge PROPERTIES C_EXTENSIONS OFF)
  target_include_directories(example_mpfr_bridge PRIVATE "${PROJECT_SOURCE_DIR}/include")
  if(NOT MPFR_HAS_HEADER)
    target_include_directories(example_mpfr_bridge PRIVATE "${PROJECT_SOURCE_DIR}/compat")
    target_compile_definitions(example_mpfr_bridge PRIVATE PPF_USE_MINIMAL_MPFR_ABI)
  endif()
  target_link_libraries(example_mpfr_bridge PRIVATE
    ${PPF_ADAPTER_TEST_TARGET}
    ${PPF_CORE_TEST_TARGET}
    MPFR::MPFR)
  ppf_apply_project_warnings(example_mpfr_bridge)
endif()
