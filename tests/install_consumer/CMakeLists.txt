# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.23)
project(PolyphaseFloatInstallConsumer LANGUAGES C)

include(CTest)
set(PPF_CONSUMER_COMPONENTS "" CACHE STRING
  "Optional installed-package components to request")
set(PPF_CONSUMER_MIN_VERSION "" CACHE STRING
  "Optional installed-package minimum version to request")
set(PPF_CONSUMER_OPTIONAL_COMPONENTS "" CACHE STRING
  "Optional installed-package components that may be unavailable")
option(PPF_CONSUMER_FORCE_STATIC "Require installed static targets" OFF)
set(_PPF_CONSUMER_OPTIONAL_ARGUMENTS)
if(PPF_CONSUMER_OPTIONAL_COMPONENTS)
  list(APPEND _PPF_CONSUMER_OPTIONAL_ARGUMENTS OPTIONAL_COMPONENTS
    ${PPF_CONSUMER_OPTIONAL_COMPONENTS})
endif()
if(PPF_CONSUMER_COMPONENTS)
  find_package(PolyphaseFloat ${PPF_CONSUMER_MIN_VERSION} CONFIG REQUIRED
    COMPONENTS ${PPF_CONSUMER_COMPONENTS} ${_PPF_CONSUMER_OPTIONAL_ARGUMENTS})
else()
  find_package(PolyphaseFloat ${PPF_CONSUMER_MIN_VERSION} CONFIG REQUIRED
    ${_PPF_CONSUMER_OPTIONAL_ARGUMENTS})
endif()

add_executable(polyphase_float_install_consumer main.c)
if(PPF_CONSUMER_FORCE_STATIC AND TARGET PolyphaseFloat::static)
  target_link_libraries(polyphase_float_install_consumer PRIVATE PolyphaseFloat::static)
elseif(PPF_CONSUMER_FORCE_STATIC)
  message(FATAL_ERROR "No installed static Polyphase Float core target was exported")
elseif(TARGET PolyphaseFloat::shared)
  target_link_libraries(polyphase_float_install_consumer PRIVATE PolyphaseFloat::shared)
elseif(TARGET PolyphaseFloat::static)
  target_link_libraries(polyphase_float_install_consumer PRIVATE PolyphaseFloat::static)
else()
  message(FATAL_ERROR "No installed Polyphase Float core target was exported")
endif()

add_test(NAME installed_consumer COMMAND polyphase_float_install_consumer)

if(TARGET PolyphaseFloat::mpfr_shared OR TARGET PolyphaseFloat::mpfr_static)
  add_executable(polyphase_float_mpfr_install_consumer mpfr_main.c)
  if(TARGET PolyphaseFloat::mpfr_shared)
    target_link_libraries(polyphase_float_mpfr_install_consumer PRIVATE
      PolyphaseFloat::mpfr_shared)
  else()
    target_link_libraries(polyphase_float_mpfr_install_consumer PRIVATE
      PolyphaseFloat::mpfr_static)
  endif()
  add_test(NAME installed_mpfr_consumer
    COMMAND polyphase_float_mpfr_install_consumer)
endif()
