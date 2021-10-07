#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import collections

# ID is an provisioned data identifier in the NVS file system.
# LEN is a provisioned data length in bytes
ProvisionedMetadata = collections.namedtuple('ProvisionedMetadata', 'ID LEN')

SERIAL_NUMBER = ProvisionedMetadata(997, 16)
MFI_TOKEN_UUID = ProvisionedMetadata(998, 16)
MFI_AUTH_TOKEN = ProvisionedMetadata(999, 1024)
