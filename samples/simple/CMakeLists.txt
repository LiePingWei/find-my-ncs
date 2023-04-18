#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/prj.conf)
set(FIND_MY_COMMON_CMAKE_DIR
    "$ENV{ZEPHYR_BASE}/../find-my/samples/common/cmake/"
    CACHE PATH "Common configuration directory")
include(${FIND_MY_COMMON_CMAKE_DIR}/boilerplate_setup.cmake NO_POLICY_SCOPE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("FMNA simple")
target_sources(app PRIVATE
  src/main.c
)
zephyr_library_include_directories(.)

include(${FIND_MY_COMMON_CMAKE_DIR}/mcuboot_validation.cmake NO_POLICY_SCOPE)
