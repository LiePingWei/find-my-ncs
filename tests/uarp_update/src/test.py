#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import os
import logging
import hashlib
import time
from _thread import start_new_thread
from . import uarp
from .super_binary import SuperBinary

#---------------------------------
SERIAL_PORT = '/dev/ttyACM1'
JLINK_SNR = '682716472'
CHECK_CONNECTIVITY_FW = True
DEVICE_NAME = 'Nordic_FMN'
SUPER_BINARY_VER = (1, 0, 20, 0)
FIRMWARE_IMAGE_FILE = os.path.dirname(__file__) + '/../../../samples/simple/build/zephyr/app_update.bin'
APPLY_FLAGS_METADATA = None # None or bytes, e.g.: b'\x01'
SUPER_BINARY_FILE = None # If provided it is used directly. SUPER_BINARY_VER, FIRMWARE_IMAGE_FILE and APPLY_FLAGS_METADATA are ignored.
#---------------------------------

logging.basicConfig(level=logging.INFO)

TLV_TYPE_SHA2          = 0xF4CE36FE
TLV_TYPE_APPLY_FLAGS   = 0xF4CE36FC
APPLY_FLAGS_FAST_RESET = 0xFF


def file_io(file, mode, data=None):
    f = open(file, mode)
    if data is None:
        data = f.read()
    else:
        f.write(data)
    f.close()
    return data


class ProgressBar:
    def __init__(self):
        self.done_map = [' '] * 75
        self.t = time.monotonic()
        self.window = 0
        self.speed = 0

    def progress(self, offset, length, total_length):
        t = time.monotonic()
        delta = t - self.t
        if length is not None:
            self.window += length
        if delta > 2 or (self.window > 16384 and delta > 0.1):
            self.speed = self.window / delta / 1024
            self.t = t
            self.window = 0
        if offset is not None:
            p = min(round(offset / total_length * 75), 74)
            self.done_map[p] = '+'
            print(f'[{"".join(self.done_map)}] {self.speed:.2f}KB/s')
            self.done_map[p] = '-'
        else:
            print(f'[{"".join(self.done_map)}] {self.speed:.2f}KB/s')


def create_super_binary():
    s = SuperBinary()
    s.set_version(*SUPER_BINARY_VER)
    p = s.add_payload()
    p.set_tag('FWUP')
    p.set_version(*SUPER_BINARY_VER)
    p.content = file_io(FIRMWARE_IMAGE_FILE, 'rb')
    p.add_metadata(TLV_TYPE_SHA2, hashlib.sha256(p.content).digest())
    if APPLY_FLAGS_METADATA is not None:
        p.add_metadata(TLV_TYPE_APPLY_FLAGS, APPLY_FLAGS_METADATA)
    return s.generate()


def main():

    def keyboard_worker():
        print('Press ENTER to stop!')
        input()
        print('Stopping...')
        acc.interrupt()

    acc = uarp.Uarp()
    acc.connect(SERIAL_PORT, DEVICE_NAME, JLINK_SNR if CHECK_CONNECTIVITY_FW else None)
    start_new_thread(keyboard_worker, ())

    if SUPER_BINARY_FILE is not None:
        super_binary = file_io(SUPER_BINARY_FILE, 'rb')
    else:
        super_binary = create_super_binary()

    try:
        acc.msgSync()
        acc.msgVersionDiscoveryRequest()
        acc.msgAccessoryInformationRequest(uarp.kUARPTLVAccessoryInformationManufacturerName)
        acc.msgAccessoryInformationRequest(uarp.kUARPTLVAccessoryInformationModelName)
        acc.msgAccessoryInformationRequest(uarp.kUARPTLVAccessoryInformationSerialNumber)
        acc.msgAccessoryInformationRequest(uarp.kUARPTLVAccessoryInformationHardwareVersion)
        acc.msgAccessoryInformationRequest(uarp.kUARPTLVAccessoryInformationFirmwareVersion)
        acc.msgAccessoryInformationRequest(uarp.kUARPTLVAccessoryInformationStagedFirmwareVersion)
        acc.msgAccessoryInformationRequest(uarp.kUARPTLVAccessoryInformationStatistics)
        acc.msgAccessoryInformationRequest(uarp.kUARPTLVAccessoryInformationLastError)
        acc.msgAssetAvailableNotification(super_binary)
        flags = acc.asset_processing_loop(ProgressBar().progress)
        if flags == uarp.kUARPAssetProcessingFlagsUploadComplete:
            acc.MsgApplyStagedAssetsRequest()
        time.sleep(1)

    except uarp.UarpInterruptedException:
        print('Interrupted by user')

    finally:
        try:
            acc.disconnect()
        except:
            pass
