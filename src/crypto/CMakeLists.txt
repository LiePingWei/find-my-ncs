#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

zephyr_library_include_directories($<TARGET_PROPERTY:mbedcrypto_oberon_imported,BASE_INCLUDE_PATH>)

zephyr_library_sources(crypto_helper.c)
zephyr_library_sources(fm_crypto_oberon.c)
