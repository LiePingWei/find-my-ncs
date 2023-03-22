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
import io
from enum import Enum

import intelhex

from pynrfjprog import LowLevel as API
from . import device as DEVICE
from . import provisioned_metadata as PROVISIONED_METADATA
from . import settings_nvs_utils as nvs

class SectorStatus(Enum):
    ERASED = 0
    OPEN = 1
    CLOSED = 2
    NA = 3

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

def is_close_ate(ate):
    if (not ate.is_valid()) or (ate.record_id != 0xffff) or (ate.data_len != 0):
        return False

    if (DEVICE.SETTINGS_SECTOR_SIZE - ate.data_offset) % nvs.NVS_ATE_LEN != 0:
        return False

    return True

def get_sector_cnt(bin_str):
    assert (len(bin_str) % DEVICE.SETTINGS_SECTOR_SIZE) == 0
    return int(len(bin_str) / DEVICE.SETTINGS_SECTOR_SIZE)

def get_sector_addr_range(sector_idx):
    start = DEVICE.SETTINGS_SECTOR_SIZE * sector_idx
    end = DEVICE.SETTINGS_SECTOR_SIZE * (sector_idx + 1)
    return start, end

def get_sector(bin_str, sector_idx):
    start, end = get_sector_addr_range(sector_idx)
    sector = bin_str[start:end]
    return sector

def is_erased_sector(sector):
    return sector == (DEVICE.SETTINGS_SECTOR_SIZE * DEVICE.FLASH_ERASE_VALUE)

def empty_records_sector_dict(status):
    return {"status": status, "records": None}

def parse_sector(sector):
    sector_metadata = {
        "status": None,
        "records": {}
    }
    data_ptr = 0

    if is_erased_sector(sector):
        # If erased, there is nothing else to check
        return empty_records_sector_dict(SectorStatus.ERASED)

    ate_ptr = len(sector) - nvs.NVS_ATE_LEN
    ate = nvs.ATE.from_bytes(sector[ate_ptr:(ate_ptr + nvs.NVS_ATE_LEN)])

    if is_close_ate(ate):
        sector_metadata["status"] = SectorStatus.CLOSED
    elif not ate.is_populated():
        sector_metadata["status"] = SectorStatus.OPEN
    else:
        # NVS sector shall have erase value
        # or close ATE at the end
        return empty_records_sector_dict(SectorStatus.NA)

    while ate_ptr >= 0:
        ate_ptr = ate_ptr - nvs.NVS_ATE_LEN

        if ate_ptr < data_ptr:
            # ATE would be already in the data segment of the sector,
            # ending analysis in this sector
            break

        ate = nvs.ATE.from_bytes(sector[ate_ptr:(ate_ptr + nvs.NVS_ATE_LEN)])

        if not ate.is_populated():
            # No more ATE, ending analysis in this sector
            break

        if not ate.is_valid():
            # Invalid ATE, move to the next one
            continue

        if ate.data_offset < data_ptr:
            # Invalid data offset, it cannot point to the offset in
            # the scope of the previous ATE
            return empty_records_sector_dict(SectorStatus.NA)

        if (ate.data_offset + ate.data_len) >= ate_ptr:
            # Invalid data len, it cannot point to the offset
            # where ATE records are already placed
            return empty_records_sector_dict(SectorStatus.NA)

        # We assume that the data is correct, dictionary is used to preserve the newest record
        sector_metadata["records"][ate.record_id] = sector[ate.data_offset:(ate.data_offset + ate.data_len)]
        data_ptr = ate.data_offset + ate.data_len

    # If classified as open but there was not even one ATE record or
    # it failed data offset or len check, it is not considered as the NVS sector
    if sector_metadata["status"] == SectorStatus.OPEN and len(sector_metadata["records"]) == 0:
        return empty_records_sector_dict(SectorStatus.NA)

    return sector_metadata

def parse_nvs_sectors(bin_str):
    metadata = []

    sector_cnt = get_sector_cnt(bin_str)
    for i in range(0, sector_cnt):
        sector = get_sector(bin_str, i)
        sector_metadata = parse_sector(sector)
        metadata.append(sector_metadata)

    return metadata

def get_sectors_additional_metadata(metadata):
    additional_metadata = {
        "sectors_with_records_cnt": 0,
        "first_sector_with_records_idx": None,
    }

    for idx, sector in enumerate(metadata):
        if sector["records"] is not None:
            if additional_metadata["first_sector_with_records_idx"] is None:
                additional_metadata["first_sector_with_records_idx"] = idx
            additional_metadata["sectors_with_records_cnt"] += 1

    return additional_metadata

def find_newest_sector_id(metadata):
    newest_idx = None

    for idx, elem in enumerate(metadata):
        next_elem = metadata[(idx + 1) % len(metadata)]
        if elem["status"] == SectorStatus.CLOSED and \
            next_elem["status"] == SectorStatus.OPEN:
            if newest_idx is not None:
                print("Memory analysis: Found another transition from closed to open sector")
            newest_idx = (idx + 1) % len(metadata)

    if newest_idx is None:
        print("Memory analysis: There was no transition, will proceed from the end of memory")
        newest_idx = len(metadata) - 1

    return newest_idx

def get_settings_range(metadata):
    sector_cnt = len(metadata)
    settings_range = None

    additional_metadata = get_sectors_additional_metadata(metadata)

    if additional_metadata["sectors_with_records_cnt"] == 0:
        print("Memory analysis: No data records found in the memory")
        return None

    if additional_metadata["sectors_with_records_cnt"] == 1:
        print("Memory analysis: Found only one sector with data records")
        settings_range = (additional_metadata["first_sector_with_records_idx"], additional_metadata["first_sector_with_records_idx"] + 1)
    else:
        print("Memory analysis: Assuming range of settings partition by itself")
        settings_range = (additional_metadata["first_sector_with_records_idx"], sector_cnt)

    print(f"Memory analysis: Search in sector range: {settings_range}")
    return settings_range

def order_nvs_sectors(metadata):
    if len(metadata) == 0:
        print("Memory analysis: No sectors to be analysed")
        return None

    settings_range = get_settings_range(metadata)
    if settings_range is None:
        return None

    metadata = metadata[settings_range[0]:settings_range[1]]

    if len(metadata) > 1:
        # Calculate the oldest sector ID
        idx = (find_newest_sector_id(metadata) + 1) % len(metadata)
        # Reorder NVS sectors metadata from the oldest to the newest
        metadata = metadata[idx:] + metadata[:idx]

    return metadata

def parse_nvs_settings(metadata):
    if metadata is None:
        return None

    settings_records = {}

    # Consolidate all records to one dictionary
    consolidated_records = {}
    for sector in metadata:
        if sector["records"] is None:
            continue

        for record_id, value in sector["records"].items():
            consolidated_records[record_id] = value

    # Map settings key to settings value
    for record_id, value in consolidated_records.items():
        if (record_id <= nvs.NVS_NAMECNT_ID) or (record_id >= (nvs.NVS_NAMECNT_ID + nvs.NVS_NAME_ID_OFFSET)):
            # Record ID is not releated to settings key
            continue

        if (value is None) or (len(value) == 0):
            continue

        if value in settings_records:
            print(f"Memory analysis: Found duplicate record with key: {value}")

        settings_value = consolidated_records.get(record_id + nvs.NVS_NAME_ID_OFFSET)
        if (settings_value is None) or (len(settings_value) == 0):
            continue

        settings_records[value] = settings_value

    return settings_records

def remove_zeros(items):
    while items and len(items) > 0 and items[-1] == 0:
        items.pop()

def cli(cmd, argv):
    global parser

    parser = argparse.ArgumentParser(description='FMN Accessory MFi Token Extractor Tool', prog=cmd, add_help=False)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('-e', '--device', default=None, help='Device of accessory to use',
              metavar='['+'|'.join(DEVICE.FLASH_SIZE.keys())+']',
              choices=DEVICE.FLASH_SIZE.keys())
    group.add_argument('-i', '--input-file', default=None, help='File in *.hex or *.bin format with Settings partition memory dump')
    parser.add_argument('-f', '--settings-base', default=None, metavar='ADDRESS',
              help='Settings base address given in hex format. This only needs to be specified if the default values in the '
                   'NCS has been changed.')
    parser.add_argument('--help', action='help',
                        help='Show this help message and exit')

    args = parser.parse_args(argv)

    if args.device is None and args.settings_base:
        extract_error_handle("argument -f/--settings-base: cannot be used with other arguments than -e/--device")

    if args.device:
        bin_str = load_from_device(args.device, args.settings_base)
    elif args.input_file:
        bin_str = load_from_file(args.input_file)

    extract(bin_str)

def settings_base_input_handle(settings_base, device):
    param_prefix = '-f/--settings-base'

    flash_size = DEVICE.FLASH_SIZE[device]
    partition_size = DEVICE.SETTINGS_PARTITION_SIZE_DEFAULT[device]

    if settings_base:
        if settings_base[:2].lower() == '0x':
            settings_base = settings_base[2:]
        pattern = re.compile(r'^[\da-f]+$', re.I)
        if not pattern.match(settings_base):
            extract_error_handle('malformed memory address: %s' % settings_base, param_prefix)
        settings_base = int(settings_base, 16)
    else:
        settings_base = flash_size - partition_size

    if (flash_size - settings_base) <= 0:
        extract_error_handle('address is bigger than the target device memory: %s >= %s'
            % (hex(settings_base), hex(flash_size)), param_prefix)

    if settings_base % DEVICE.SETTINGS_SECTOR_SIZE != 0:
        aligned_page = hex(settings_base & ~(DEVICE.SETTINGS_SECTOR_SIZE - 1))
        extract_error_handle('address should be page aligned: %s -> %s'
              % (hex(settings_base), aligned_page), param_prefix)

    return settings_base

def load_from_device(device, settings_base):
    settings_base = settings_base_input_handle(settings_base, device)
    settings_size = DEVICE.FLASH_SIZE[device] - settings_base
    print('Looking for the provisioned data in the following memory range: %s - %s'
        % (hex(settings_base), hex(settings_base + settings_size)))

    # Open connection to the device and read the NVS data
    with open_nrf(None) as api:
        bin_data = api.read(settings_base, settings_size)

    return bytes(bin_data)

def load_from_file(filename):
    print("Searching for the provisioned data in provided settings partition memory dump file")

    # Read content from assumed settings partition memory dump
    if filename.endswith(".hex"):
        out = io.BytesIO(b"")
        intelhex.hex2bin(filename, out)
        bin_str = out.getvalue()
    elif filename.endswith(".bin"):
        with open(filename, "rb") as f:
            bin_str = f.read()
    else:
        extract_error_handle("Not supported file type, use .hex or .bin!")

    # Align input to settings sector size
    bin_str = bin_str.ljust(DEVICE.SETTINGS_SECTOR_SIZE, DEVICE.FLASH_ERASE_VALUE)

    return bin_str

def extract(bin_str):
    metadata = parse_nvs_sectors(bin_str)
    metadata = order_nvs_sectors(metadata)
    settings = parse_nvs_settings(metadata)

    if (settings is None) or (len(settings) == 0):
        extract_error_handle("Provided settings partition does not contain any records")

    # Get the UUID Value
    auth_uuid_key = nvs.get_kvs_name(PROVISIONED_METADATA.MFI_TOKEN_UUID.ID)
    auth_uuid = settings.get(auth_uuid_key)

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
    auth_token = settings.get(auth_token_key)

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
    serial_number = settings.get(serial_number_key)

    if serial_number:
        print("Serial Number: %s" % serial_number.hex().upper())
    else:
        print("Serial Number: not found in the provisioned data")

    # Extracting operation was not successful, exit with error
    if (auth_uuid is None) or (auth_token is None):
        extract_error_handle("Provisioned data does not contain valid MFi token")

if __name__ == '__main__':
    cli()
