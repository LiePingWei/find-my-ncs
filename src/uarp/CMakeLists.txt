#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

zephyr_library_include_directories(.)
zephyr_library_sources(
    fmna_uarp_service.c
    fmna_uarp.c
    )

add_subdirectory(UARPDK)
