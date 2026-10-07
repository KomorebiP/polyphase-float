# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.23)
project(PolyphaseFloatInstallConsumer LANGUAGES C)

include(CTest)
find_package(PolyphaseFloat CONFIG REQUIRED)

add_executable(polyphase_float_install_consumer main.c)
if(TARGET PolyphaseFloat::shared)
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
