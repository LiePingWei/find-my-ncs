#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fmna_crypto_test)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR}/../../src/crypto)
if(CONFIG_NORDIC_SECURITY_BACKEND)
  zephyr_library_link_libraries(mbedcrypto_oberon_imported)
else()
  zephyr_library_link_libraries(nrfxlib_crypto)
endif()
