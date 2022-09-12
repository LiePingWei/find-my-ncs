#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(CONF_FILE prj.conf)
set(mcuboot_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/child_image/mcuboot.conf)
set(FIND_MY_COMMON_CMAKE_DIR
    "$ENV{ZEPHYR_BASE}/../find-my/samples/common/cmake/"
    CACHE PATH "Common configuration directory")
include(${FIND_MY_COMMON_CMAKE_DIR}/boilerplate_setup.cmake NO_POLICY_SCOPE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("Find My Thingy")

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
target_sources_ifdef(CONFIG_BOARD_THINGY52_NRF52832 app PRIVATE
                                                        src/platform/motion_int.c
                                                        src/platform/speaker_pin_ctrl.c)
target_sources_ifdef(CONFIG_BOARD_THINGY53_NRF5340_CPUAPP app PRIVATE
                                                        src/platform/motion_poll.c
                                                        src/platform/speaker_none.c)
include(${FIND_MY_COMMON_CMAKE_DIR}/mcuboot_validation.cmake NO_POLICY_SCOPE)
