#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fmna_crypto_test)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR}/../../src/crypto)
target_link_libraries(app PRIVATE mbedcrypto_oberon_imported)
