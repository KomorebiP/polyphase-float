# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

include(CMakePackageConfigHelpers)

# Project versions and the C ABI epoch are independent.  Keep the standard
# CMake range, EXACT, and architecture checks, then apply our governed floor.
function(ppf_write_package_version_files output_dir version)
  if(version VERSION_LESS "0.11.0" OR NOT version VERSION_LESS "2.0.0")
    message(FATAL_ERROR
      "Polyphase Float ${version} needs an explicit package compatibility policy")
  endif()
  if(version VERSION_LESS "1.0.0")
    set(_ppf_compatibility SameMajorVersion)
  else()
    # Compatible 1.x releases also serve the governed 0.11+ consumer baseline.
    set(_ppf_compatibility AnyNewerVersion)
  endif()
  write_basic_package_version_file(
    "${output_dir}/PolyphaseFloatConfigVersionBase.cmake"
    VERSION "${version}"
    COMPATIBILITY "${_ppf_compatibility}")
  configure_file(
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/PolyphaseFloatConfigVersion.cmake.in"
    "${output_dir}/PolyphaseFloatConfigVersion.cmake" @ONLY)
endfunction()
