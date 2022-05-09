#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import base64
import os
import re
import sys
import argparse
from binascii import unhexlify
from intelhex import IntelHex

import six

from . import device as DEVICE
from . import provisioned_metadata as PROVISIONED_METADATA
from . import settings_nvs_utils as nvs

MFI_UUID_EXAMPLE = '12345678-1234-1234-1234-123456789abc'
SERIAL_NUMBER_EXAMPLE = '30313233343536373839414243444546' # '0123456789ABCDEF' in ASCII

def provision_error_handle(msg, param_prefix = None):
    parser.print_usage()

    if param_prefix is None:
        param_prefix = ''
    else:
        param_prefix = 'argument %s: ' % param_prefix
    print('provision: error: ' + param_prefix + msg)

    sys.exit(1)

def cli(cmd, argv):
    global parser

    parser = argparse.ArgumentParser(description='FMN Accessory Setup Provisioning Tool', prog=cmd, add_help=False)
    parser.add_argument('-u', '--mfi-uuid', required=True, metavar='UUID',
              help='MFi UUID of the accessory. Example UUID: ' + MFI_UUID_EXAMPLE)
    parser.add_argument('-m', '--mfi-token', metavar='TOKEN', required=True,
              help='MFi Token of the accessory formatted as a base64 encoded string.')
    parser.add_argument('-s', '--serial-number', metavar='SN',
              help='Serial number of the accessory in a hex format. Example SN: ' + SERIAL_NUMBER_EXAMPLE)
    parser.add_argument('-o', '--output-path', default='provisioned.hex', metavar='PATH',
              help='Path to store the result of the provisioning.')
    parser.add_argument('-e', '--device', help='Device of accessory to provision',
              metavar='['+'|'.join(DEVICE.FLASH_SIZE.keys())+']',
              choices=DEVICE.FLASH_SIZE.keys(), required=True)
    parser.add_argument('-f', '--settings-base', metavar='ADDRESS',
              help='Settings base address given in hex format. This only needs to be specified if the default values in the '
                   'NCS has been changed.')
    parser.add_argument('-x', '--input-hex-file',
              help='Hex file to be merged with provisioned Settings. If this option is set, the '
                   'output hex file will be [input-hex-file + provisioned Settings].')
    parser.add_argument('--help', action='help',
                        help='Show this help message and exit')
    args = parser.parse_args(argv)

    provision(args.mfi_uuid, args.mfi_token, args.serial_number, args.output_path, args.device, args.settings_base, args.input_hex_file)

def settings_base_input_handle(settings_base, device):
    param_prefix = '-f/--settings-base'

    flash_size = DEVICE.FLASH_SIZE[device]
    partition_size = DEVICE.SETTINGS_PARTITION_SIZE_DEFAULT[device]

    if settings_base:
        if settings_base[:2].lower() == '0x':
            settings_base = settings_base[2:]
        pattern = re.compile(r'^[\da-f]+$', re.I)
        if not pattern.match(settings_base):
            provision_error_handle('maflormed memory address: %s' % settings_base, param_prefix)
        settings_base = int(settings_base, 16)
    else:
        settings_base = flash_size - partition_size

    if (flash_size - settings_base) <= 0:
        provision_error_handle('address is bigger than the target device memory: %s >= %s'
            % (hex(settings_base), hex(flash_size)), param_prefix)

    if settings_base % DEVICE.SETTINGS_SECTOR_SIZE != 0:
        aligned_page = hex(settings_base & ~(DEVICE.SETTINGS_SECTOR_SIZE - 1))
        provision_error_handle('address should be page aligned: %s -> %s'
              % (hex(settings_base), aligned_page), param_prefix)

    return settings_base

def mfi_uuid_input_handle(mfi_uuid):
    param_prefix = '-u/--mfi-uuid'

    pattern = re.compile('[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}')
    if not pattern.match(mfi_uuid) or len(mfi_uuid) != len(MFI_UUID_EXAMPLE):
        msg = 'malformed formatting\n'
        msg += 'Please use the correct format as in the following example: %s' % MFI_UUID_EXAMPLE
        provision_error_handle(msg, param_prefix)

    mfi_uuid = unhexlify(mfi_uuid.replace('-', ''))
    if len(mfi_uuid) != PROVISIONED_METADATA.MFI_TOKEN_UUID.LEN:
        provision_error_handle('incorrect length', param_prefix)

    return mfi_uuid

def mfi_token_input_handle(mfi_token):
    param_prefix = '-m/--mfi-token'

    try:
        mfi_token = base64.decodebytes(mfi_token.encode('ascii'))
    except Exception as e:
        msg = 'malformed formatting\n' + str(e)
        provision_error_handle(msg, param_prefix)

    mfi_token_len = len(mfi_token)
    if mfi_token_len <= PROVISIONED_METADATA.MFI_AUTH_TOKEN.LEN:
        mfi_token += (bytes(PROVISIONED_METADATA.MFI_AUTH_TOKEN.LEN - mfi_token_len))
    else:
        msg = 'exceeded maximum length: %d' % PROVISIONED_METADATA.MFI_AUTH_TOKEN.LEN 
        provision_error_handle(msg, param_prefix)

    return mfi_token

def serial_number_input_handle(serial_number):
    param_prefix = '-s/--serial-number'

    if not serial_number:
        return serial_number

    pattern = re.compile(r'^([\da-f][\da-f])+$', re.I)
    if not pattern.match(serial_number):
        msg = 'malformed formatting\n'
        msg += 'Please use the correct format as in the following example: %s' % SERIAL_NUMBER_EXAMPLE
        provision_error_handle(msg, param_prefix)

    serial_number = unhexlify(serial_number)
    if len(serial_number) != PROVISIONED_METADATA.SERIAL_NUMBER.LEN:
        provision_error_handle('incorrect length: %d' % len(serial_number), param_prefix)

    return serial_number

def input_hex_file_input_handle(input_hex_file):
    param_prefix = '-x/--input-hex-file'

    if not input_hex_file:
        return input_hex_file

    try:
        input_hex_file = os.path.realpath(input_hex_file)
    except:
        provision_error_handle('malformed path format', param_prefix)

    if not os.path.exists(input_hex_file):
        provision_error_handle('target file does not exist', param_prefix)

    if not os.path.isfile(input_hex_file):
        provision_error_handle('target path is not a file', param_prefix)

    try:
        IntelHex(input_hex_file)
    except Exception as e:
        msg = 'target file with malformed content format\n' + str(e)
        provision_error_handle(msg, param_prefix)

    return input_hex_file

def output_path_input_handle(output_path):
    param_prefix = '-o/--output-path'

    if not output_path:
        return output_path

    try:
        output_path = os.path.realpath(output_path)
    except:
        provision_error_handle('malformed path format', param_prefix)

    if os.path.isdir(output_path):
        provision_error_handle('target is an existing directory', param_prefix)

    if not os.path.exists(os.path.split(output_path)[0]):
        provision_error_handle('target directory does not exist', param_prefix)

    if os.path.exists(output_path):
        provision_error_handle('target file already exists', param_prefix)

    return output_path

def generate_padded_record_data(data):
    num_bytes = len(data)
    num_bytes_to_be_padded = (num_bytes % 4)
    padding = 0
    if num_bytes_to_be_padded != 0:
        padding = 4 - num_bytes_to_be_padded

    record_data = data
    record_data += six.binary_type(b'\xff') * int(padding)

    return record_data

def create_and_insert_record_dict(nvs_dict, record_data, settings_key):
    record = generate_padded_record_data(record_data)
    nvs_dict['records'][nvs_dict['record_id']] = nvs.create_nvs_dict(settings_key,
                                                               (len(record)) // 4,
                                                               nvs_dict['record_id'],
                                                               record)
    nvs_dict['record_id'] += 1

def merge_hex_files(input_hex_file, provisioned_data_hex_file, output_file):
    try:
        h = IntelHex(input_hex_file)
        h.merge(IntelHex(provisioned_data_hex_file))
        h.write_hex_file(output_file)
    except Exception as e:
        os.remove(provisioned_data_hex_file)
        msg = '--input-hex-file target cannot be merged with provisioning data\n' + str(e)
        provision_error_handle(msg)

def provision(mfi_uuid, mfi_token, serial_number, output_path, device, settings_base, input_hex_file):
    settings_base = settings_base_input_handle(settings_base, device)
    mfi_uuid = mfi_uuid_input_handle(mfi_uuid)
    mfi_token = mfi_token_input_handle(mfi_token)
    serial_number = serial_number_input_handle(serial_number)
    input_hex_file = input_hex_file_input_handle(input_hex_file)
    output_path = output_path_input_handle(output_path)

    print('Using %s as settings base.' % hex(settings_base))

    nvs_dict = nvs.create_blank_nvs_dict(settings_base)
    create_and_insert_record_dict(nvs_dict, mfi_uuid, PROVISIONED_METADATA.MFI_TOKEN_UUID.ID)
    create_and_insert_record_dict(nvs_dict, mfi_token, PROVISIONED_METADATA.MFI_AUTH_TOKEN.ID)
    if serial_number:
        create_and_insert_record_dict(nvs_dict, serial_number, PROVISIONED_METADATA.SERIAL_NUMBER.ID)
    nvs.write_nvs_dict_to_hex_file(nvs_dict, output_path)

    if input_hex_file:
        provisioned_data_hex_file = output_path
        merge_hex_files(input_hex_file, provisioned_data_hex_file, output_path)

if __name__ == '__main__':
    cli()
