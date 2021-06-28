#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import sys
import argparse
import importlib


commands = [
    ('Provision', '.cmd_provision', 'FMN Accessory Setup Provisioning Tool'),
    ('Extract', '.cmd_extract', 'FMN Accessory MFi Token Extractor Tool'),
    ('SuperBinary', '.cmd_superbinary', 'FMN SuperBinary Helper Tool')
]


def cli():
    argv = sys.argv.copy()

    if len(argv) > 1:
        cmd = argv[1].lower()
        argv = argv[2:]
        for command in commands:
            if cmd == command[0].lower():
                mod = importlib.import_module(command[1], 'scripts')
                return (mod.cli)(command[0], argv)

    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('command [--help]')
    parser.add_argument('parameter', nargs='*')
    parser.print_usage()
    print('')
    print('commands:')
    max_len = 0
    for command in commands:
        max_len = max(max_len, len(command[0]))
    for command in commands:
        print(f'  {(command[0] + (" " * max_len))[0:max_len]}   - {command[2]}')

    exit(1)

if __name__ == '__main__':
    cli()
