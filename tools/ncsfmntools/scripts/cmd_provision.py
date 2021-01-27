import base64
import os
import re
import sys
from shutil import copyfile
from binascii import unhexlify, hexlify
from hashlib import md5

import click
import six

import ncsfmntools.utils.settings_nvs_utils as nvs

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

class MfiUuidType(click.ParamType):
    """docstring for MfiTokenType"""
    name = 'MFi UUID'

    def convert(self, value, param, ctx):
        pattern = re.compile('[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}')
        if not pattern.match(value):
            self.fail('--mfi-uuid specified with malformed Software Token UUID.')
        value = value.replace('-', '')
        if len(value) != FMN_MFI_AUTH_UUID_LEN * 2:
            self.fail('--mfi-uuid specified with invalid Software Token UUID length.')
        return value


class MfiTokenType(click.ParamType):
    """docstring for MfiTokenType"""
    name = 'MFi Token'

    def convert(self, value, param, ctx):
        try:
            if six.PY3:
                token = base64.decodebytes(value.encode('ascii'))
            else:
                token = base64.decodestring(value)
            padding = FMN_MFI_AUTH_TOKEN_LEN - len(token)
            if padding < 0:
                raise Exception
            token += bytes([0] * padding)
        except Exception as e:
            print(e)
            self.fail('--mfi-token specified with malformed Software Token.')
        return token


class SerialNumberType(click.ParamType):
    """docstring for SerialNumberType"""
    name = 'Serial number'
    CHARS_PER_BYTE = 2
    SERIAL_NUMBER_BLEN = 16

    def convert(self, value, param, ctx):
        expected_len = self.SERIAL_NUMBER_BLEN * self.CHARS_PER_BYTE
        pattern = re.compile('^[\\dA-Fa-f]*$')
        if not pattern.match(value) or len(value) != expected_len:
            self.fail('%s is a malformed serial number' % value)
        return value


class SettingsBaseAddressType(click.ParamType):
    """docstring for SettingsBaseAddressType"""
    name = 'Settings base address'

    def convert(self, value, param, ctx):
        if value[:2].lower() == '0x':
            value = value[2:]
        pattern = re.compile('^[\\dA-Fa-f]*$')
        if not pattern.match(value):
            self.fail('%s is a malformed Settings base address' % value)
        return value


SETTINGS_BASE = SettingsBaseAddressType()
MFI_TOKEN = MfiTokenType()
MFI_UUID = MfiUuidType()
SERIAL_NUMBER = SerialNumberType()


@click.command()
@click.option('-u', '--mfi-uuid', help='MFI uuid of accessory: aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee', type=MFI_UUID)
@click.option('-m', '--mfi-token', type=MFI_TOKEN,
              help='MFI token of accessory formatted as a base64 encoded string.')
@click.option('-s', '--serial-number', type=SERIAL_NUMBER,
              help='Serial number of accessory in a hex format.')
@click.option('-o', '--output-path', default='provisioned.hex',
              type=click.Path(resolve_path=True, dir_okay=True),
              help='Path to store the result of the provisioning. '
                   'Unique identifiers will be appended to any specified file name.')
@click.option('-e', '--device', help='Device of accessory to provision', type=click.Choice(settings_bases_without_bl.keys()),
              required=True)
@click.option('-f', '--settings-base', type=SETTINGS_BASE,
              help='Settings base address given in hex format. This only needs to be specified if the default values in the '
                   'NCS has been changed.')
@click.option('-x', '--input-hex-file', help='Hex file to be merged with provisioned Settings. If this option is set, the '
                                             'output hex file will be [input-hex-file + provisioned Settings].',
              type=click.Path(resolve_path=True, exists=True))
def cli(mfi_uuid, mfi_token, serial_number, output_path, device, settings_base, input_hex_file):
    """FMN Accessory Setup Provisioning Tool

    This tool generates and provisions accessory setup information for FMN.
    """
    provision(mfi_uuid, mfi_token, serial_number, output_path, device, settings_base, input_hex_file)


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


if __name__ == '__main__':
    cli()
