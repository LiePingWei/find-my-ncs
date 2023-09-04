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

# Collect DTS overlay files
set(FIND_MY_APP_COMMON_TARGET_DTS_OVERLAY
  ${FIND_MY_COMMON_CONFIG_DIR}/boards/${BOARD}.overlay)

if(EXISTS ${FIND_MY_APP_COMMON_TARGET_DTS_OVERLAY})
  set(DTC_OVERLAY_FILE
    ${FIND_MY_APP_COMMON_TARGET_DTS_OVERLAY}
    ${DTC_OVERLAY_FILE})
endif()

# Set static partition configuration if it exists
set(PM_STATIC_YML_FILE
  ${FIND_MY_COMMON_CONFIG_DIR}/pm_static_${BOARD}.yml
  )

# Collect application configuration
set(CONF_FILE_temp ${CONF_FILE})

set(CONF_FILE
  ${FIND_MY_COMMON_CONFIG_DIR}/app_${CMAKE_BUILD_TYPE}.conf)

set(FIND_MY_APP_COMMON_TARGET_CONF
  "${FIND_MY_COMMON_CONFIG_DIR}/app_${CMAKE_BUILD_TYPE}_${BOARD}.conf")
if (EXISTS "${FIND_MY_APP_COMMON_TARGET_CONF}")
  set(CONF_FILE
    ${CONF_FILE}
    ${FIND_MY_APP_COMMON_TARGET_CONF})
endif()

set(CONF_FILE
  ${CONF_FILE}
  ${CONF_FILE_temp})

# Collect MCUboot configuration
set(mcuboot_CONF_FILE_temp ${mcuboot_CONF_FILE})

set(mcuboot_CONF_FILE
  ${FIND_MY_COMMON_CONFIG_DIR}/mcuboot_${CMAKE_BUILD_TYPE}.conf)

set(FIND_MY_MCUBOOT_COMMON_TARGET_CONF
  "${FIND_MY_COMMON_CONFIG_DIR}/mcuboot_${CMAKE_BUILD_TYPE}_${BOARD}.conf")
if (EXISTS "${FIND_MY_MCUBOOT_COMMON_TARGET_CONF}")
  set(mcuboot_CONF_FILE
    ${mcuboot_CONF_FILE}
    ${FIND_MY_MCUBOOT_COMMON_TARGET_CONF})
endif()

set(mcuboot_CONF_FILE
  ${mcuboot_CONF_FILE}
  ${mcuboot_CONF_FILE_temp})
