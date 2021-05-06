#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

cmake_minimum_required(VERSION 3.13.1)

set(CONF_FILE prj.conf)
include(${CMAKE_CURRENT_LIST_DIR}/../common/cmake/boilerplate_setup.cmake NO_POLICY_SCOPE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("FMNA simple")
target_sources(app PRIVATE
  src/main.c
)
zephyr_library_include_directories(.)

include(${CMAKE_CURRENT_LIST_DIR}/../common/cmake/mcuboot_validation.cmake NO_POLICY_SCOPE)
