#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import binascii
import intelhex
import struct

from . import device as DEVICE

NVS_NAMECNT_ID = 0x8000
NVS_NAME_ID_OFFSET = 0x4000
NVS_ID_OFFSET = 0xff0
NVS_ATE_LEN = 8
NVS_UNPOPULATED_ATE = DEVICE.FLASH_ERASE_VALUE * NVS_ATE_LEN

crc8_ccitt_table = [0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a,
                    0x2d]

class ATE:
    def __init__(self, bytes, record_id, data_offset, data_len, crc):
        self.bytes = bytes
        self.record_id = record_id
        self.data_offset = data_offset
        self.data_len = data_len
        self.crc = crc

    @classmethod
    def from_bytes(cls, bytes):
        assert len(bytes) == NVS_ATE_LEN

        vals = struct.unpack('<HHHBB', bytes)
        record_id, data_offset, data_len, _, crc = vals
        return cls(bytes, record_id, data_offset, data_len, crc)

    def is_valid(self):
        ate_without_crc = self.bytes[:7]
        if (crc8_ccitt(ate_without_crc, len(ate_without_crc)) == self.crc) \
            and self.data_offset < (DEVICE.SETTINGS_SECTOR_SIZE - NVS_ATE_LEN):
            return True

        return False

    def is_populated(self):
        return self.bytes != NVS_UNPOPULATED_ATE

    def __str__(self):
        if not self.is_populated():
            return 'ATE: unpopulated'

        return (f'ATE: {"valid" if self.is_valid() else "invalid"}\n'
                f'  Record ID: {hex(self.record_id)}\n'
                f'  Data offset: {hex(self.data_offset)}\n'
                f'  Data length: {self.data_len}'
                f'  Bytes: {self.bytes.hex()}')

def create_blank_nvs_dict(base_addr):
    r = dict()
    r['records'] = dict()
    r['record_id'] = NVS_NAMECNT_ID+1
    r['start_addr'] = base_addr
    r['num_pages'] = 2
    r['page_size'] = 4096
    r['offset'] = 0
    r['ateoffset'] = base_addr + NVS_ID_OFFSET
    return r


def crc8_ccitt(buf, cnt):
    crc = 0xff
    for i in range(cnt):
        crc = (crc ^ buf[i]) & 0xff
        crc = ((crc << 4) ^ crc8_ccitt_table[(crc >> 4) & 0xff]) & 0xff
        crc = ((crc << 4) ^ crc8_ccitt_table[(crc >> 4) & 0xff]) & 0xff
    return crc


# Allocation Table Entry
def get_ate(offset, len, data_id):
    ate = [0, 0, 0, 0, 0, 0, 0, 0]
    ate[0:2] = [data_id & 0xff, data_id >> 8]
    ate[2:4] = [offset & 0xff, offset >> 8]
    ate[4:6] = [len & 0xff, len >> 8]
    ate[6] = 0xff
    ate[7] = crc8_ccitt(ate, 7)
    return ate


def get_kvs_name(kvs_key):
    name = "fmna/provisioning/%3s" % kvs_key
    return bytes(name, "utf8")


def _read_record_header(data_stream):
    r = dict()
    # TBD
    return r


def _get_num_bytes_in_record(record):
    return record['len'] * 4  # + fds_record_header_t_num_bytes


def _crc8_data(data):
    hex_representation = binascii.hexlify(data)
    crc = crc8_ccitt(hex_representation)
    return crc


def create_nvs_dict(kvs_key, len, record_id, data):
    r = dict()
    r['kvs_key'] = kvs_key
    r['len'] = len
    r['data'] = data
    r['record_id'] = record_id
    return r


def write_nvs_ate_data_pair(ih, nvs_dict, ate, data):
    ih.frombytes(ate, nvs_dict['ateoffset'])
    nvs_dict['ateoffset'] -= len(ate)
    ih.frombytes(data, nvs_dict['start_addr'] + nvs_dict['offset'])
    nvs_dict['offset'] += len(data)


def write_nvs_record_to_hex_file(ih, nvs_dict, record_id):
    record = nvs_dict['records'][record_id]

    ate = get_ate(nvs_dict['offset'], len(record['data']), record_id + NVS_NAME_ID_OFFSET)
    data = record['data']
    write_nvs_ate_data_pair(ih, nvs_dict, ate, data)

    data = get_kvs_name(record['kvs_key'])
    ate = get_ate(nvs_dict['offset'], len(data), record_id)
    write_nvs_ate_data_pair(ih, nvs_dict, ate, data)

    # Update largest name ID ATE:
    ate = get_ate(nvs_dict['offset'], 2, NVS_NAMECNT_ID)
    data = bytes([record_id & 0xff, record_id >> 8])
    write_nvs_ate_data_pair(ih, nvs_dict, ate, data)


def write_nvs_dict_to_hex_file(nvs_dict, dest):
    ih = intelhex.IntelHex()
    for record_id in range(NVS_NAMECNT_ID+1, nvs_dict['record_id']):
        write_nvs_record_to_hex_file(ih, nvs_dict, record_id)

    ih.write_hex_file(dest)