#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

zephyr_library_sources(crypto_helper.c)
zephyr_library_sources(fm_crypto_oberon.c)

if(CONFIG_NORDIC_SECURITY_BACKEND)
  zephyr_library_link_libraries(mbedcrypto_oberon_imported)
else()
  zephyr_library_link_libraries(nrfxlib_crypto)
endif()
