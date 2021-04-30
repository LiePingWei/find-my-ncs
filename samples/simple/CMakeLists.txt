#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#
cmake_minimum_required(VERSION 3.13.1)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ZDebug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/configuration/app_${CMAKE_BUILD_TYPE}.conf")
  message(FATAL_ERROR
          "Configuration file for build type ${CMAKE_BUILD_TYPE} is missing.\n"
          "Please add file ${CMAKE_CURRENT_SOURCE_DIR}/configuration/app_${CMAKE_BUILD_TYPE}.conf")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "ZRelease")
  if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/configuration/mcuboot_private.pem")
    message(FATAL_ERROR
      "Mcuboot private key file is missing.\n"
      "Please add file ${CMAKE_CURRENT_SOURCE_DIR}/configuration/mcuboot_private.pem")
  endif()
endif()

# Define configuration files.
set(CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/configuration/app_${CMAKE_BUILD_TYPE}.conf)

set(mcuboot_CONF_FILE
  ${CMAKE_CURRENT_SOURCE_DIR}/configuration/mcuboot_${CMAKE_BUILD_TYPE}.conf
  )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("FMNA simple")

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.c
)
# NORDIC SDK APP END
zephyr_library_include_directories(.)

if(CONFIG_BOOTLOADER_MCUBOOT)
  assert_exists(mcuboot_CONF_FILE)
endif()
