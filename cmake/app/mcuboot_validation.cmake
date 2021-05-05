#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

if(CONFIG_BOOTLOADER_MCUBOOT)
  if (CMAKE_BUILD_TYPE STREQUAL "ZRelease")
    if (NOT EXISTS "${FIND_MY_COMMON_CONFIG_DIR}/mcuboot_private.pem")
      message(FATAL_ERROR
        "Mcuboot private key file is missing.\n"
        "Please add file ${FIND_MY_COMMON_CONFIG_DIR}/mcuboot_private.pem")
    endif()
  endif()

  assert_exists(mcuboot_CONF_FILE)
endif()
