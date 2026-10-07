# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_GMP QUIET gmp)
endif()

find_path(GMP_INCLUDE_DIR
  NAMES gmp.h
  HINTS ${PC_GMP_INCLUDE_DIRS}
  PATH_SUFFIXES x86_64-linux-gnu aarch64-linux-gnu)

find_library(GMP_LIBRARY
  NAMES gmp
  HINTS ${PC_GMP_LIBRARY_DIRS})

set(GMP_VERSION "${PC_GMP_VERSION}")
find_package_handle_standard_args(GMP
  REQUIRED_VARS GMP_LIBRARY GMP_INCLUDE_DIR
  VERSION_VAR GMP_VERSION)

if(GMP_FOUND AND NOT TARGET GMP::GMP)
  add_library(GMP::GMP UNKNOWN IMPORTED)
  set_target_properties(GMP::GMP PROPERTIES
    IMPORTED_LOCATION "${GMP_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}")
endif()

mark_as_advanced(GMP_INCLUDE_DIR GMP_LIBRARY)
