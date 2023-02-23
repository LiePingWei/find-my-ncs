#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(CONF_FILE prj.conf)
if (EXISTS "boards/${BOARD}.conf")
  set(CONF_FILE ${CONF_FILE} boards/${BOARD}.conf)
endif()

set(mcuboot_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/child_image/mcuboot.conf)

# MCUboot bootloader is always built for the secure domain.
# It is necessary to trim the "_ns" suffix from the main image target if it is an NS target.
string(REGEX REPLACE "(_?ns)$" "" board_without_ns ${BOARD})

set(app_mcuboot_BOARD_CONF
    ${CMAKE_CURRENT_SOURCE_DIR}/child_image/mcuboot_${board_without_ns}.conf)
if(EXISTS ${app_mcuboot_BOARD_CONF})
  set(mcuboot_CONF_FILE ${mcuboot_CONF_FILE} ${app_mcuboot_BOARD_CONF})
endif()

set(app_mcuboot_DTC
    ${CMAKE_CURRENT_SOURCE_DIR}/child_image/mcuboot_${board_without_ns}.overlay)
if(EXISTS ${app_mcuboot_DTC})
  set(mcuboot_DTC_OVERLAY_FILE ${app_mcuboot_DTC})
endif()

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
if(CONFIG_BOARD_THINGY53_NRF5340_CPUAPP OR CONFIG_BOARD_THINGY53_NRF5340_CPUAPP_NS)
  target_sources(app PRIVATE
                     src/platform/motion_poll.c
                     src/platform/speaker_none.c)
endif()
include(${FIND_MY_COMMON_CMAKE_DIR}/mcuboot_validation.cmake NO_POLICY_SCOPE)
