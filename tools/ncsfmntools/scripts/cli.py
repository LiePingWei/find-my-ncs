import sys
import argparse

from .cmd_provision import cli as provision_cli
from .cmd_superbinary import cli as superbinary_cli
from .cmd_extract import cli as extract_cli


commands = [
    ('Provision', provision_cli, 'FMN Accessory Setup Provisioning Tool'),
    ('Extract', extract_cli, 'FMN Accessory MFi Token Extractor Tool'),
    ('SuperBinary', superbinary_cli, 'FMN SuperBinary Helper Tool')
]


def cli():
    argv = sys.argv.copy()

    if len(argv) > 1:
        cmd = argv[1].lower()
        argv = argv[2:]
        for command in commands:
            if cmd == command[0].lower():
                return (command[1])(command[0], argv)

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
