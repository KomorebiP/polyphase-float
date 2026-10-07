# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_MPFR QUIET mpfr)
endif()

find_path(MPFR_INCLUDE_DIR
  NAMES mpfr.h
  HINTS ${PC_MPFR_INCLUDE_DIRS}
  PATH_SUFFIXES x86_64-linux-gnu aarch64-linux-gnu)

find_library(MPFR_LIBRARY
  NAMES mpfr libmpfr.so.6 mpfr.so.6
  HINTS ${PC_MPFR_LIBRARY_DIRS})

set(MPFR_VERSION "${PC_MPFR_VERSION}")
set(MPFR_HAS_HEADER FALSE)
if(MPFR_INCLUDE_DIR)
  set(MPFR_HAS_HEADER TRUE)
endif()

# The library is the required component. The official header is preferred, but
# the project can use compat/mpfr_minimal.h for test-only local builds.
find_package_handle_standard_args(MPFR
  REQUIRED_VARS MPFR_LIBRARY
  VERSION_VAR MPFR_VERSION)

if(MPFR_FOUND AND NOT TARGET MPFR::MPFR)
  add_library(MPFR::MPFR UNKNOWN IMPORTED)
  set_target_properties(MPFR::MPFR PROPERTIES
    IMPORTED_LOCATION "${MPFR_LIBRARY}")
  if(MPFR_INCLUDE_DIR)
    set_target_properties(MPFR::MPFR PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${MPFR_INCLUDE_DIR}")
  endif()
  if(TARGET GMP::GMP)
    set_property(TARGET MPFR::MPFR APPEND PROPERTY
      INTERFACE_LINK_LIBRARIES GMP::GMP)
  endif()
endif()

mark_as_advanced(MPFR_INCLUDE_DIR MPFR_LIBRARY)
