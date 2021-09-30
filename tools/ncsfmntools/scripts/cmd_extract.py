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
from . import settings_nvs_utils as nvs

DEVICE_NRF52832 = 'NRF52832'
DEVICE_NRF52840 = 'NRF52840'
DEVICE_NRF52833 = 'NRF52833'
SETTINGS_SIZE = 0x2000

NVS_SECTOR_SIZE = 0x1000
NVS_UNPOPULATED_ATE = b'\xff\xff\xff\xff\xff\xff\xff\xff'

device_flash_size = {
    DEVICE_NRF52840: 0x100000,
    DEVICE_NRF52832: 0x80000,
    DEVICE_NRF52833: 0x80000,
}

settings_bases_without_bl = {
    DEVICE_NRF52840: (device_flash_size[DEVICE_NRF52840] - SETTINGS_SIZE),
    DEVICE_NRF52832: (device_flash_size[DEVICE_NRF52832] - SETTINGS_SIZE),
    DEVICE_NRF52833: (device_flash_size[DEVICE_NRF52833] - SETTINGS_SIZE)
}

# Record IDs
FMN_PROVISIONING_SN = 997
FMN_PROVISIONING_MFI_TOKEN_UUID = 998
FMN_PROVISIONING_MFI_AUTH_TOKEN = 999

FMN_MAX_RECORD_ID = 999

# MFi Token lengths
FMN_MFI_AUTH_TOKEN_LEN = 1024
FMN_MFI_AUTH_UUID_LEN = 16


def SETTINGS_BASE(value):
    if value[:2].lower() == '0x':
        value = value[2:]
    pattern = re.compile('^[\\dA-Fa-f]*$')
    if not pattern.match(value):
        raise ValueError('%s is a malformed FDS base address' % value)
    return value

@contextmanager
def open_nrf(snr=None):
    # Read the serial numbers of conected devices
    with API.API(API.DeviceFamily.UNKNOWN) as api:
        serial_numbers = api.enum_emu_snr()

    # Determine which device shall be used
    if not snr:
        if not serial_numbers:
            print("Extract error: No devices connected!")
            sys.exit(1)
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
                print(f"Please choose the option: {list(serial_numbers.keys())}")
                sys.exit(1)

            if decision in serial_numbers.keys():
                snr = serial_numbers[decision]
            elif decision in serial_numbers.values():
                # Option to provide serial number instead of index, for automation purpose.
                # Usage: "echo 123456789 | ncsfmntools extract -e NRF52840"
                snr = decision
            else:
                print(f"Please choose the option: {list(serial_numbers.keys())}")
                sys.exit(1)
    elif snr not in serial_numbers:
        print(f"Extract error: Device with serial number {snr} not found!")
        sys.exit(1)

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

def find_settings_value(bin_str, settings_key):
    offset = bin_str.find(settings_key)

    sector_num = int(offset / NVS_SECTOR_SIZE)
    sector_base_addr = sector_num * NVS_SECTOR_SIZE

    # The ATE at the end of sector marks closed sectors
    ate_offset = sector_base_addr + NVS_SECTOR_SIZE - nvs.NVS_ATE_LEN
    ate = bin_str[ate_offset : (ate_offset + nvs.NVS_ATE_LEN)]
    # closed_sector = ate != NVS_UNPOPULATED_ATE

    # This is the first data ATE
    ate_offset -= nvs.NVS_ATE_LEN
    ate = bin_str[ate_offset : (ate_offset + nvs.NVS_ATE_LEN)]

    # Look for match with data value
    while ate != NVS_UNPOPULATED_ATE:
        record_id = ate[:2]
        data_offset = ate[2:4]
        target_offset = (offset % NVS_SECTOR_SIZE)
        if data_offset == bytes(target_offset.to_bytes(2, byteorder='little')):
            break
        ate_offset -= nvs.NVS_ATE_LEN
        ate = bin_str[ate_offset : (ate_offset + nvs.NVS_ATE_LEN)]

    # Map record ID of Settings key to record of Settings value
    value_record_id = bytes([record_id[0], record_id[1] | 0x40])
    value_ate_offset = int(bin_str.find(value_record_id))

    sector_num = int(value_ate_offset / NVS_SECTOR_SIZE)
    sector_base_addr = sector_num * NVS_SECTOR_SIZE
    value_data_offset = sector_base_addr + int.from_bytes(bin_str[(value_ate_offset + 2) : (value_ate_offset + 4)], byteorder='little')
    value_data_len = int.from_bytes(bin_str[(value_ate_offset + 4) : (value_ate_offset + 6)], byteorder='little')

    value = bin_str[value_data_offset : (value_data_offset + value_data_len)]

    return value

def remove_zeros(items):
    while items[-1] == 0:
        items.pop()

def cli(cmd, argv):
    parser = argparse.ArgumentParser(description='FMN Accessory MFi Token Extractor Tool', prog=cmd, add_help=False)
    parser.add_argument('-e', '--device', help='Device of accessory to use',
              metavar='['+'|'.join(settings_bases_without_bl.keys())+']',
              choices=settings_bases_without_bl.keys(), required=True)
    parser.add_argument('-f', '--settings-base', type=SETTINGS_BASE, metavar='ADDRESS',
              help='Settings base address given in hex format. This only needs to be specified if the default values in the '
                   'NCS has been changed.')
    parser.add_argument('--help', action='help',
                        help='Show this help message and exit')
    args = parser.parse_args(argv)

    extract(args.device, args.settings_base)

def extract(device, settings_base):
    if settings_base:
        settings_base = int(settings_base, 16)
    else:
        settings_base = settings_bases_without_bl[device]

    print('Using %s as settings base.' % hex(settings_base))

    # Open connection to the device and read the NVS data.
    with open_nrf(None) as api:
        bin_data = api.read(settings_base, device_flash_size[device] - settings_base)

    if six.PY3:
        bin_str = bytes(bin_data)
    else:
        bin_str = ''.join([chr(byte) for byte in bin_data])

    # Get the UUID Value
    auth_uuid_key = nvs.get_kvs_name(FMN_PROVISIONING_MFI_TOKEN_UUID)
    auth_uuid = find_settings_value(bin_str, auth_uuid_key)

    auth_uuid = auth_uuid.hex()
    print("SW Authentication UUID: %s-%s-%s-%s-%s" % (
        auth_uuid[:8],
        auth_uuid[8:12],
        auth_uuid[12:16],
        auth_uuid[16:20],
        auth_uuid[20:]))

    # Get the Authentication Token Value
    auth_token_key = nvs.get_kvs_name(FMN_PROVISIONING_MFI_AUTH_TOKEN)
    auth_token = find_settings_value(bin_str, auth_token_key)

    # Trim zeroes at the end and covert to base64 format
    auth_token = bytearray(auth_token)
    remove_zeros(auth_token)
    auth_token_base64 = base64.encodebytes(auth_token).replace(six.binary_type(b'\n'), six.binary_type(b'')).decode()

    print("SW Authentication Token: %s" % auth_token_base64)

    # Get the Serial Number Value
    try:
        # The serial number is optional, so reading it may raise an exception.
        serial_number_key = nvs.get_kvs_name(FMN_PROVISIONING_SN)
        serial_number = find_settings_value(bin_str, serial_number_key)
        print("Serial Number: %s" % serial_number.hex().upper())
    except:
        print("Serial Number: not found in the provisioned data")

if __name__ == '__main__':
    cli()
