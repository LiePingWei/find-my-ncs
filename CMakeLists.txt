#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

zephyr_include_directories(include)

add_subdirectory_ifdef(CONFIG_FMNA src)
