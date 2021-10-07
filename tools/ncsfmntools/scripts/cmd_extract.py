#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import base64
import re
import argparse
import six
import sys
from contextlib import contextmanager

from pynrfjprog import LowLevel as API
from . import device as DEVICE
from . import provisioned_metadata as PROVISIONED_METADATA
from . import settings_nvs_utils as nvs

NVS_UNPOPULATED_ATE = b'\xff\xff\xff\xff\xff\xff\xff\xff'

def extract_error_handle(msg, param_prefix = None):
    parser.print_usage()

    if param_prefix is None:
        param_prefix = ''
    else:
        param_prefix = 'argument %s: ' % param_prefix
    print('extract: error: ' + param_prefix + msg)

    sys.exit(1)

@contextmanager
def open_nrf(snr=None):
    # Read the serial numbers of conected devices
    with API.API(API.DeviceFamily.UNKNOWN) as api:
        serial_numbers = api.enum_emu_snr()

    # Determine which device shall be used
    if not snr:
        if not serial_numbers:
            extract_error_handle('no devices connected')
        elif len(serial_numbers) == 1:
            snr = serial_numbers[0]
        else:
            # User input required
            serial_numbers = {idx+1: serial_number for idx, serial_number in enumerate(serial_numbers)}
            print('Choose the device:')
            for idx, snr in serial_numbers.items():
                print(f"{idx}. {snr}")
            decision = input()

            try:
                decision = int(decision)
            except ValueError:
                choice_list = ', '.join(str(key) for key in  serial_numbers.keys())
                extract_error_handle('invalid choice (choose from: %s)' % choice_list)

            if decision in serial_numbers.keys():
                snr = serial_numbers[decision]
            elif decision in serial_numbers.values():
                # Option to provide serial number instead of index, for automation purpose.
                # Usage: "echo 123456789 | ncsfmntools extract -e NRF52840"
                snr = decision
            else:
                choice_list = ', '.join(str(key) for key in  serial_numbers.keys())
                extract_error_handle('invalid choice (choose from: %s)' % choice_list)
    elif snr not in serial_numbers:
        extract_error_handle(f'device with serial number: {snr} cannot be found')

    # Determine the DeviceFamily for the chosen device
    with API.API(API.DeviceFamily.UNKNOWN) as api:
        api.connect_to_emu_with_snr(snr)
        device_family = api.read_device_family()

    # Open the connection
    try:
        api = API.API(device_family)
        api.open()
        api.connect_to_emu_with_snr(snr)
        yield api
    finally:
        api.close()

def find_settings_value(bin_str, settings_key, value_len, offset = 0):
    offset = bin_str.find(settings_key, offset)
    if offset < 0:
        return None

    sector_size = DEVICE.SETTINGS_SECTOR_SIZE
    sector_num = int(offset / sector_size)
    sector_base_addr = sector_num * sector_size

    # The ATE at the end of sector marks closed sectors
    ate_offset = sector_base_addr + sector_size - nvs.NVS_ATE_LEN
    ate = bin_str[ate_offset : (ate_offset + nvs.NVS_ATE_LEN)]
    # closed_sector = ate != NVS_UNPOPULATED_ATE

    # This is the first ATE
    ate_offset -= nvs.NVS_ATE_LEN
    ate = bin_str[ate_offset : (ate_offset + nvs.NVS_ATE_LEN)]

    # Look for the ATE that describes Settings key payload.
    record_id = None
    while ate != NVS_UNPOPULATED_ATE:
        ate_no_crc = ate[:7]
        if nvs.crc8_ccitt(ate_no_crc, len(ate_no_crc)) == ate[7]:
            record_id = ate[:2]
            data_offset = ate[2:4]

            target_offset = (offset % sector_size)
            if data_offset == bytes(target_offset.to_bytes(2, byteorder='little')):
                break

        ate_offset -= nvs.NVS_ATE_LEN
        ate = bin_str[ate_offset : (ate_offset + nvs.NVS_ATE_LEN)]
        if int(ate_offset / sector_size) != sector_num:
            # ATE should be in the same sector as they payload it describes. Look elsewhere.
            return find_settings_value(bin_str, settings_key, value_len, offset + 1)
    if record_id is None:
        # ATE should have correct CRC. Look elsewhere.
        return find_settings_value(bin_str, settings_key, value_len, offset + 1)

    # Map record ID of Settings key to record of Settings value
    value_record_id = bytes([record_id[0], record_id[1] | 0x40])
    value_ate_offset = 0
    while value_ate_offset >= 0:
        value_ate_offset = int(bin_str.find(value_record_id, value_ate_offset))
        if value_ate_offset < 0:
            # Cannot find the value ATE corresponding to the key ATE. Look for new key ATE.
            return find_settings_value(bin_str, settings_key, value_len, offset + 1)

        value_ate = bin_str[value_ate_offset : (value_ate_offset + nvs.NVS_ATE_LEN)]
        value_ate_no_crc = value_ate[:7]
        value_data_len = int.from_bytes(value_ate[4 : 6], byteorder='little')
        if (nvs.crc8_ccitt(value_ate_no_crc, len(value_ate_no_crc)) == value_ate[7]) \
            and (value_data_len == value_len):
            break
        else:
            value_ate_offset += nvs.NVS_ATE_LEN

    sector_num = int(value_ate_offset / sector_size)
    sector_base_addr = sector_num * sector_size
    value_data_offset = sector_base_addr + int.from_bytes(value_ate[2 : 4], byteorder='little')
    value = bin_str[value_data_offset : (value_data_offset + value_data_len)]

    return value

def remove_zeros(items):
    while items[-1] == 0:
        items.pop()

def cli(cmd, argv):
    global parser

    parser = argparse.ArgumentParser(description='FMN Accessory MFi Token Extractor Tool', prog=cmd, add_help=False)
    parser.add_argument('-e', '--device', help='Device of accessory to use',
              metavar='['+'|'.join(DEVICE.FLASH_SIZE.keys())+']',
              choices=DEVICE.FLASH_SIZE.keys(), required=True)
    parser.add_argument('-f', '--settings-base', metavar='ADDRESS',
              help='Settings base address given in hex format. This only needs to be specified if the default values in the '
                   'NCS has been changed.')
    parser.add_argument('--help', action='help',
                        help='Show this help message and exit')
    args = parser.parse_args(argv)

    extract(args.device, args.settings_base)

def settings_base_input_handle(settings_base, flash_size):
    param_prefix = '-f/--settings-base'

    if settings_base:
        if settings_base[:2].lower() == '0x':
            settings_base = settings_base[2:]
        pattern = re.compile(r'^[\da-f]+$', re.I)
        if not pattern.match(settings_base):
            extract_error_handle('maflormed memory address: %s' % settings_base, param_prefix)
        settings_base = int(settings_base, 16)
    else:
        settings_base = flash_size - DEVICE.SETTINGS_PARTITION_SIZE_DEFAULT

    if (flash_size - settings_base) <= 0:
        extract_error_handle('address is bigger than the target device memory: %s >= %s'
            % (hex(settings_base), hex(flash_size)), param_prefix)

    if settings_base % DEVICE.SETTINGS_SECTOR_SIZE != 0:
        aligned_page = hex(settings_base & ~(DEVICE.SETTINGS_SECTOR_SIZE - 1))
        extract_error_handle('address should be page aligned: %s -> %s'
              % (hex(settings_base), aligned_page), param_prefix)

    return settings_base

def extract(device, settings_base):
    settings_base = settings_base_input_handle(settings_base, DEVICE.FLASH_SIZE[device])
    settings_size = DEVICE.FLASH_SIZE[device] - settings_base

    # Open connection to the device and read the NVS data.
    with open_nrf(None) as api:
        bin_data = api.read(settings_base, settings_size)
    bin_str = bytes(bin_data)

    print('Looking for the provisioned data in the following memory range: %s - %s'
          % (hex(settings_base), hex(settings_base + settings_size)))

    # Get the UUID Value
    auth_uuid_key = nvs.get_kvs_name(PROVISIONED_METADATA.MFI_TOKEN_UUID.ID)
    auth_uuid = find_settings_value(
        bin_str, auth_uuid_key, PROVISIONED_METADATA.MFI_TOKEN_UUID.LEN)

    if auth_uuid is not None:
        auth_uuid = auth_uuid.hex()
        print("SW Authentication UUID: %s-%s-%s-%s-%s" % (
            auth_uuid[:8],
            auth_uuid[8:12],
            auth_uuid[12:16],
            auth_uuid[16:20],
            auth_uuid[20:]))
    else:
        print("SW Authentication UUID: not found in the provisioned data")

    # Get the Authentication Token Value
    auth_token_key = nvs.get_kvs_name(PROVISIONED_METADATA.MFI_AUTH_TOKEN.ID)
    auth_token = find_settings_value(
        bin_str, auth_token_key, PROVISIONED_METADATA.MFI_AUTH_TOKEN.LEN)
    if auth_token is not None:
        # Trim zeroes at the end and covert to base64 format
        auth_token = bytearray(auth_token)
        remove_zeros(auth_token)
        auth_token_base64 = base64.encodebytes(auth_token).replace(six.binary_type(b'\n'), six.binary_type(b'')).decode()

        print("SW Authentication Token: %s" % auth_token_base64)
    else:
        print("SW Authentication Token: not found in the provisioned data")

    # Get the Serial Number Value (optional)
    serial_number_key = nvs.get_kvs_name(PROVISIONED_METADATA.SERIAL_NUMBER.ID)
    serial_number = find_settings_value(
        bin_str, serial_number_key, PROVISIONED_METADATA.SERIAL_NUMBER.LEN)

    if serial_number:
        print("Serial Number: %s" % serial_number.hex().upper())
    else:
        print("Serial Number: not found in the provisioned data")

    # Extracting operation was not successful, exit with error
    if (auth_uuid is None) or (auth_token is None):
        extract_error_handle("Provisioned data does not contain valid MFi token")

if __name__ == '__main__':
    cli()
