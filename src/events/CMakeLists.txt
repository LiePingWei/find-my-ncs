#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

zephyr_library_sources(fmna_event.c)
zephyr_library_sources(fmna_config_event.c)
zephyr_library_sources(fmna_non_owner_event.c)
zephyr_library_sources(fmna_owner_event.c)
zephyr_library_sources(fmna_pair_event.c)

zephyr_library_sources_ifdef(CONFIG_FMNA_QUALIFICATION fmna_debug_event.c)
