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
from shutil import copyfile
from binascii import unhexlify, hexlify
from hashlib import md5

import six

from . import device as DEVICE
from . import provisioned_metadata as PROVISIONED_METADATA
from . import settings_nvs_utils as nvs

def MFI_UUID(value):
    pattern = re.compile('[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}')
    if not pattern.match(value):
        raise ValueError('--mfi-token specified with malformed Software Token UUID.')
    return value.replace('-', '')


def MFI_TOKEN(value):
    try:
        if six.PY3:
            token = base64.decodebytes(value.encode('ascii'))
        else:
            token = base64.decodestring(value)
    except Exception as e:
        print(e)
        raise ValueError('--mfi-token specified with malformed Software Token.')
    return token


def SERIAL_NUMBER(value):
    CHARS_PER_BYTE = 2
    expected_len = PROVISIONED_METADATA.SERIAL_NUMBER.LEN * CHARS_PER_BYTE
    pattern = re.compile('^[\\dA-Fa-f]*$')
    if not pattern.match(value) or len(value) != expected_len:
        raise ValueError('%s is a malformed serial number' % value)
    return value


def SETTINGS_BASE(value):
    if value[:2].lower() == '0x':
        value = value[2:]
    pattern = re.compile('^[\\dA-Fa-f]*$')
    if not pattern.match(value):
        raise ValueError('%s is a malformed FDS base address' % value)
    return value


def EXISTING_FILE_PATH(value):
    try:
        value = os.path.realpath(value)
    except:
        raise ValueError('%s is invalid' % value)
    if not os.path.exists(value):
        raise ValueError('%s does not exists' % value)
    if not os.path.isfile(value):
        raise ValueError('%s is not a file' % value)
    return value


def cli(cmd, argv):
    parser = argparse.ArgumentParser(description='FMN Accessory Setup Provisioning Tool', prog=cmd, add_help=False)
    parser.add_argument('-u', '--mfi-uuid', required=True, type=MFI_UUID, metavar='UUID',
              help='MFI uuid of accessory: aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee')
    parser.add_argument('-m', '--mfi-token', type=MFI_TOKEN, metavar='TOKEN', required=True,
              help='MFI token of accessory formatted as a base64 encoded string.')
    parser.add_argument('-s', '--serial-number', type=SERIAL_NUMBER, metavar='SN',
              help='Serial number of accessory in a hex format.')
    parser.add_argument('-o', '--output-path', default='provisioned.hex', metavar='PATH',
              help='Path to store the result of the provisioning. '
                   'Unique identifiers will be appended to any specified file name.')
    parser.add_argument('-e', '--device', help='Device of accessory to provision',
              metavar='['+'|'.join(DEVICE.FLASH_SIZE.keys())+']',
              choices=DEVICE.FLASH_SIZE.keys(), required=True)
    parser.add_argument('-f', '--settings-base', type=SETTINGS_BASE, metavar='ADDRESS',
              help='Settings base address given in hex format. This only needs to be specified if the default values in the '
                   'NCS has been changed.')
    parser.add_argument('-x', '--input-hex-file',
              help='Hex file to be merged with provisioned Settings. If this option is set, the '
                   'output hex file will be [input-hex-file + provisioned Settings].',
              type=EXISTING_FILE_PATH)
    parser.add_argument('--help', action='help',
                        help='Show this help message and exit')
    args = parser.parse_args(argv)

    provision(args.mfi_uuid, args.mfi_token, args.serial_number, args.output_path, args.device, args.settings_base, args.input_hex_file)


def provision(mfi_uuid, mfi_token, serial_number, output_path, device, settings_base, input_hex_file):
    if settings_base:
        settings_base = int(settings_base, 16)
    else:
        settings_base = DEVICE.FLASH_SIZE[device] - DEVICE.SETTINGS_PARTITION_SIZE_DEFAULT

    print('Using %s as settings base.' % hex(settings_base))

    if input_hex_file:
        input_hex_file_md5 = md5(open(input_hex_file, 'rb').read()).hexdigest()

    nvs_dict = nvs.create_blank_nvs_dict(settings_base)

    if mfi_uuid and mfi_token:
        mfi_token_len = len(mfi_token)
        if mfi_token_len <= PROVISIONED_METADATA.MFI_AUTH_TOKEN.LEN:
            mfi_token += (bytes(PROVISIONED_METADATA.MFI_AUTH_TOKEN.LEN - mfi_token_len))
        else:
            raise ValueError('--mfi-token specified with data longer than {} bytes.'.format(PROVISIONED_METADATA.MFI_AUTH_TOKEN.LEN))

        create_and_insert_record_dict(nvs_dict, unhexlify(mfi_uuid), PROVISIONED_METADATA.MFI_TOKEN_UUID.ID)
        create_and_insert_record_dict(nvs_dict, mfi_token, PROVISIONED_METADATA.MFI_AUTH_TOKEN.ID)
    else:
        raise click.BadArgumentUsage('Please provide the --mfi-token and --mfi-uuid options')

    if serial_number:
        create_and_insert_record_dict(nvs_dict, unhexlify(serial_number), PROVISIONED_METADATA.SERIAL_NUMBER.ID)

    provision_data_hex_file = output_path
    nvs.write_nvs_dict_to_hex_file(nvs_dict, provision_data_hex_file)

    if input_hex_file:
        merge_hex_files(merge_file_a=input_hex_file, merge_file_b=provision_data_hex_file,
                        output_file=provision_data_hex_file)


def merge_hex_files(merge_file_a, merge_file_b, output_file):
    from intelhex import IntelHex
    h = IntelHex(merge_file_a)
    h.merge(IntelHex(merge_file_b))
    h.write_hex_file(output_file)


def generate_padded_record_data(data):
    """
    Returns word aligned record data with padding after the last byte to word
    align the data.
    """
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
