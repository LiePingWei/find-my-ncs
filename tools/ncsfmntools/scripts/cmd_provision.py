import base64
import os
import re
import sys
import argparse
from shutil import copyfile
from binascii import unhexlify, hexlify
from hashlib import md5

import six

from . import settings_nvs_utils as nvs

DEVICE_NRF52832 = 'NRF52832'
DEVICE_NRF52840 = 'NRF52840'
DEVICE_NRF52833 = 'NRF52833'
NRF52840_FLASH_SIZE = 0x100000
NRF52832_FLASH_SIZE = 0x80000
NRF52833_FLASH_SIZE = 0x80000
SETTINGS_SIZE = 0x2000
SETTINGS_ADDR_840_WITHOUT_BL = NRF52840_FLASH_SIZE - SETTINGS_SIZE
SETTINGS_ADDR_832_WITHOUT_BL = NRF52832_FLASH_SIZE - SETTINGS_SIZE
SETTINGS_ADDR_833_WITHOUT_BL = NRF52833_FLASH_SIZE - SETTINGS_SIZE

settings_bases_without_bl = {DEVICE_NRF52840: SETTINGS_ADDR_840_WITHOUT_BL,
                        DEVICE_NRF52832: SETTINGS_ADDR_832_WITHOUT_BL,
                        DEVICE_NRF52833: SETTINGS_ADDR_833_WITHOUT_BL}

# Record IDs
FMN_PROVISIONING_SN = 997
FMN_PROVISIONING_MFI_TOKEN_UUID = 998
FMN_PROVISIONING_MFI_AUTH_TOKEN = 999

FMN_MAX_RECORD_ID = 999

# MFi Token lengths
FMN_MFI_AUTH_TOKEN_LEN = 1024
FMN_MFI_AUTH_UUID_LEN = 16

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
    SERIAL_NUMBER_BLEN = 16
    expected_len = SERIAL_NUMBER_BLEN * CHARS_PER_BYTE
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
              metavar='['+'|'.join(settings_bases_without_bl.keys())+']',
              choices=settings_bases_without_bl.keys(), required=True)
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
        settings_base = settings_bases_without_bl[device]

    print('Using %s as settings base.' % hex(settings_base))

    if input_hex_file:
        input_hex_file_md5 = md5(open(input_hex_file, 'rb').read()).hexdigest()

    nvs_dict = nvs.create_blank_nvs_dict(settings_base)

    if mfi_uuid and mfi_token:
        create_and_insert_record_dict(nvs_dict, unhexlify(mfi_uuid), FMN_PROVISIONING_MFI_TOKEN_UUID)
        create_and_insert_record_dict(nvs_dict, mfi_token, FMN_PROVISIONING_MFI_AUTH_TOKEN)
    else:
        raise click.BadArgumentUsage('Please provide the --mfi-token and --mfi-uuid options')

    if serial_number:
        create_and_insert_record_dict(nvs_dict, unhexlify(serial_number), FMN_PROVISIONING_SN)

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
