#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

zephyr_include_directories(.)
zephyr_library()

zephyr_library_sources(
    CoreUARPAccessory.c
    CoreUARPPlatformAccessory.c
    CoreUARPPlatformZephyr.c
    CoreUARPUtils.c
    )
