#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import sys

required_version = (3, 6, 0)

if sys.version_info < required_version:
    print('Current Python version is %s.%s.%s.' % (sys.version_info.major,
                                                   sys.version_info.minor,
                                                   sys.version_info.micro))
    print('Minimum required version is %s.%s.%s.' % (required_version[0],
                                                     required_version[1],
                                                     required_version[2]))
    exit(2)

import scripts.cli

scripts.cli.cli()
