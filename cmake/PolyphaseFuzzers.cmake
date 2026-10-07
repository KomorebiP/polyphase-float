# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
  message(FATAL_ERROR "PPF_BUILD_FUZZERS requires a Clang-compatible compiler")
endif()
if(NOT UNIX OR APPLE)
  message(FATAL_ERROR
    "The current PPFIF in-memory fuzzer requires the Linux fmemopen interface")
endif()

function(ppf_add_fuzzer target source)
  add_executable(${target} "${PROJECT_SOURCE_DIR}/fuzz/${source}")
  target_compile_features(${target} PRIVATE c_std_11)
  set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF)
  target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_options(${target} PRIVATE
    -fsanitize=fuzzer,address,undefined
    -fno-omit-frame-pointer)
  target_link_options(${target} PRIVATE
    -fsanitize=fuzzer,address,undefined
    -fno-omit-frame-pointer)
  target_link_libraries(${target} PRIVATE ${PPF_CORE_TEST_TARGET})
  ppf_apply_project_warnings(${target})
endfunction()

ppf_add_fuzzer(fuzz_text_parser fuzz_text_parser.c)
ppf_add_fuzzer(fuzz_fpif_import fuzz_fpif_import.c)
