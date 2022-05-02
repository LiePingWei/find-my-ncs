#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(FIND_MY_COMMON_CONFIG_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../configuration/"
    CACHE PATH "Common configuration directory")

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ZDebug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (NOT EXISTS "${FIND_MY_COMMON_CONFIG_DIR}/app_${CMAKE_BUILD_TYPE}.conf")
  message(FATAL_ERROR
          "Configuration file for build type ${CMAKE_BUILD_TYPE} is missing.\n"
          "Please add file ${FIND_MY_COMMON_CONFIG_DIR}/app_${CMAKE_BUILD_TYPE}.conf")
endif()

# Define configuration files.
set(CONF_FILE ${FIND_MY_COMMON_CONFIG_DIR}/app_${CMAKE_BUILD_TYPE}.conf ${CONF_FILE})

set(mcuboot_CONF_FILE
  ${FIND_MY_COMMON_CONFIG_DIR}/mcuboot_${CMAKE_BUILD_TYPE}.conf
  ${mcuboot_CONF_FILE})
