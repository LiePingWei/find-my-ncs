#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#
cmake_minimum_required(VERSION 3.13.1)

set(CONF_FILE prj.conf)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/app/boilerplate_setup.cmake NO_POLICY_SCOPE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("FMNA qualification")
target_sources(app PRIVATE
  ../simple/src/main.c
)
zephyr_library_include_directories(.)

include(${FIND_MY_DIR}/cmake/app/mcuboot_validation.cmake NO_POLICY_SCOPE)