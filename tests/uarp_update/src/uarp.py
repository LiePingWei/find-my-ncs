#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import logging
import binascii
import threading
import time

from struct import unpack_from, unpack, pack, pack_into, calcsize

from .ble_uarp import BleUarp

logger = logging.getLogger(__name__)

# Message Set
kUARPMsgSync                             = 0x0000
kUARPMsgVersionDiscoveryRequest          = 0x0001
kUARPMsgVersionDiscoveryResponse         = 0x0002
kUARPMsgAccessoryInformationRequest      = 0x0003
kUARPMsgAccessoryInformationResponse     = 0x0004
kUARPMsgAssetAvailableNotification       = 0x0005
kUARPMsgAssetDataRequest                 = 0x0006
kUARPMsgAssetDataResponse                = 0x0007
kUARPMsgAssetDataTransferNotification    = 0x0008
kUARPMsgAssetProcessingNotification      = 0x0009
kUARPMsgApplyStagedAssetsRequest         = 0x000A
kUARPMsgApplyStagedAssetsResponse        = 0x000B
kUARPMsgAssetRescindedNotification       = 0x000C
kUARPMsgAssetAvailableNotificationAck    = 0x000D
kUARPMsgAssetDataTransferNotificationAck = 0x000E
kUARPMsgAssetProcessingNotificationAck   = 0x000F
kUARPMsgAssetRescindedNotificationAck    = 0x0010

# Accessory Information Options
kUARPTLVAccessoryInformationManufacturerName      = 0x00000001
kUARPTLVAccessoryInformationModelName             = 0x00000002
kUARPTLVAccessoryInformationSerialNumber          = 0x00000003
kUARPTLVAccessoryInformationHardwareVersion       = 0x00000004
kUARPTLVAccessoryInformationFirmwareVersion       = 0x00000005
kUARPTLVAccessoryInformationStagedFirmwareVersion = 0x00000006
kUARPTLVAccessoryInformationStatistics            = 0x00000007
kUARPTLVAccessoryInformationLastError             = 0x00000008

# Asset Type
kUARPAssetFlagsAssetTypeSuperBinary = 0x0001
kUARPAssetFlagsAssetTypeDynamic     = 0x0002

# Asset Transfer Flags
kUARPAssetDataTransferPause  = 0x0001
kUARPAssetDataTransferResume = 0x0002

# Asset Processing Flags
kUARPAssetProcessingFlagsUploadComplete  = 0x0001
kUARPAssetProcessingFlagsUploadDenied    = 0x0002
kUARPAssetProcessingFlagsUploadAbandoned = 0x0003
kUARPAssetProcessingFlagsAssetCorrupt    = 0x0004

# Staged Assets Flags
kUARPApplyStagedAssetsFlagsSuccess       = 0x0001
kUARPApplyStagedAssetsFlagsFailure       = 0x0002
kUARPApplyStagedAssetsFlagsNeedsRestart  = 0x0003
kUARPApplyStagedAssetsFlagsNothingStaged = 0x0004
kUARPApplyStagedAssetsFlagsMidUpload     = 0x0005
kUARPApplyStagedAssetsFlagsInUse         = 0x0006

# Message Status Codes
kUARPStatusSuccess                         = 0x0000
kUARPStatusUnknownMessageType              = 0x0001
kUARPStatusIncompatibleProtocolVersion     = 0x0002
kUARPStatusInvalidAssetTag                 = 0x0003
kUARPStatusInvalidDataRequestOffset        = 0x0004
kUARPStatusInvalidDataRequestLength        = 0x0005
kUARPStatusMismatchUUID                    = 0x0006
kUARPStatusAssetDataPaused                 = 0x0007
kUARPStatusInvalidMessageLength            = 0x0008
kUARPStatusInvalidMessage                  = 0x0009
kUARPStatusInvalidTLV                      = 0x000A
kUARPStatusNoResources                     = 0x000B
kUARPStatusUnknownAccessory                = 0x000C
kUARPStatusUnknownController               = 0x000D
kUARPStatusInvalidFunctionPointer          = 0x000E
kUARPStatusCorruptSuperBinary              = 0x000F
kUARPStatusAssetInFlight                   = 0x0010
kUARPStatusAssetIdUnknown                  = 0x0011
kUARPStatusInvalidDataResponseLength       = 0x0012
kUARPStatusAssetTransferComplete           = 0x0013
kUARPStatusUnkownPersonalizationOption     = 0x0014
kUARPStatusMismatchPersonalizationOption   = 0x0015
kUARPStatusInvalidAssetType                = 0x0016
kUARPStatusUnknownAsset                    = 0x0017
kUARPStatusNoVersionAgreement              = 0x0018
kUARPStatusUnknownDataTransferState        = 0x0019
kUARPStatusUnsupported                     = 0x001A
kUARPStatusInvalidObject                   = 0x001B
kUARPStatusUnknownInformationOption        = 0x001C
kUARPStatusInvalidDataResponse             = 0x001D
kUARPStatusInvalidArgument                 = 0x001E
kUARPStatusDataTransferPaused              = 0x001F
kUARPStatusUnknownPayload                  = 0x0020
kUARPStatusInvalidDataTransferNotification = 0x0021
kUARPStatusMetaDataTypeNotFound            = 0x0022
kUARPStatusMetaDataCorrupt                 = 0x0023
kUARPStatusOutOfOrderMessageID             = 0x0024
kUARPStatusDuplicateMessageID              = 0x0025
kUARPStatusMismatchDataOffset              = 0x0026
kUARPStatusInvalidLength                   = 0x0027
kUARPStatusNoMetaData                      = 0x0028
kUARPStatusDuplicateController             = 0x0029
kUARPStatusDuplicateAccessory              = 0x002A
kUARPStatusInvalidOffset                   = 0x002B
kUARPStatusInvalidPayload                  = 0x002C
kUARPStatusProcessingIncomplete            = 0x002D
kUARPStatusInvalidDataRequestType          = 0x002E
kUARPStatusInvalidSuperBinaryHeader        = 0x002F
kUARPStatusInvalidPayloadHeader            = 0x0030
kUARPStatusAssetNoBytesRemaining           = 0x0031
kUARPStatusNONE                            = 0xFFFF

# Set containing messages with status codes
msg_has_status = {
    kUARPMsgVersionDiscoveryResponse,
    kUARPMsgAccessoryInformationResponse,
    kUARPMsgAssetDataResponse,
    kUARPMsgApplyStagedAssetsResponse
}

# Get global contant name based on its value and prefix
def get_const(value, prefix):
    for n, v in globals().items():
        if value == v and n.startswith(prefix):
            return n[0:len(prefix)] + ':' + n[len(prefix):]
    return f'{prefix}:0x{value:04X}'


class UarpInterruptedException(Exception):
    pass


class Uarp:

    def __init__(self):
        self.running = True
        self.tx_msg_id = 1
        self.rx_msg_id = 1
        self.response_sem = threading.BoundedSemaphore(1)
        self.response_sem.acquire()
        self.response = None
        self.con = None

    def interrupt(self):
        self.running = False

    def is_running(self):
        return self.running

    def connect(self, serial_port, device_name, jlink_snr=None):
        if jlink_snr is not None:
            BleUarp.flash_connectivity(serial_port, jlink_snr)
        self.con = BleUarp(serial_port, device_name)
        self.con.set_message_handler(self._packet_received)
        self.con.connect()
        time.sleep(0.3)

    def disconnect(self):
        try:
            self.con.disconnect()
        except:
            pass
        time.sleep(0.3)
        try:
            self.con.close()
        except:
            pass
        time.sleep(0.3)

    def send(self, msg_type, payload=b'', id=None):
        if id is not None:
            self.tx_msg_id = id
        self.con.send_message(pack('>HHH', msg_type, len(payload), self.tx_msg_id) + payload)
        self.tx_msg_id += 1

    def wait_for_response(self, *args, **kwargs):
        t = time.monotonic()
        while not self.response_sem.acquire(timeout=0.3):
            if not self.running:
                raise UarpInterruptedException()
            if 'timeout' in kwargs and time.monotonic() - t >= kwargs['timeout']:
                return (None, None, None)
        r = self.response
        self.response = None
        if r[0] not in set(args):
            raise Exception(f'Unexpected response {get_const(r[0], "kUARPMsg")}')
        return r

    def _packet_received(self, data):
        msg_type, payload_len, id = unpack_from('>HHH', data)
        if msg_type == kUARPMsgSync:
            self.rx_msg_id = id
        if id != self.rx_msg_id:
            raise Exception(f'Unexpected msgID {id}, expecting {self.rx_msg_id}')
        self.rx_msg_id += 1
        data = data[calcsize('>HHH'):]
        if len(data) != payload_len:
            raise Exception(f'Unexpected msgPayloadLength {payload_len}, expecting {len(data)}')
        if msg_type in msg_has_status:
            status = unpack_from('>H', data)[0]
            data = data[2:]
        else:
            status = kUARPStatusNONE
        logger.debug(f'RECV: {get_const(msg_type, "kUARPMsg")} [{id}, {payload_len}] {get_const(status, "kUARPStatus")} {binascii.hexlify(data).decode("utf-8")}')
        if msg_type == kUARPMsgSync:
            pass
        else:
            while self.running:
                try:
                    self.response_sem.release()
                    self.response = (msg_type, data, status)
                    break
                except:
                    time.sleep(0.01)

    def msgSync(self):
        self.send(kUARPMsgSync, id=1)

    def msgVersionDiscoveryRequest(self):
        self.send(kUARPMsgVersionDiscoveryRequest, pack('>H', 1))
        r = self.wait_for_response(kUARPMsgVersionDiscoveryResponse)
        print(f'Discovered version {unpack(">H", r[1])[0]}')

    def msgAccessoryInformationRequest(self, n):
        self.send(kUARPMsgAccessoryInformationRequest, pack('>L', n))
        r = self.wait_for_response(kUARPMsgAccessoryInformationResponse)
        if n <= 4:
            print(f'{get_const(n, "kUARPTLVAccessoryInformation")}: {r[1][8:].decode("utf-8")}')
        elif n <= 6:
            v = unpack('>LLLL', r[1][8:])
            print(f'{get_const(n, "kUARPTLVAccessoryInformation")}: {v[0]}.{v[1]}.{v[2]}.{v[3]}')
        elif n == kUARPTLVAccessoryInformationStatistics:
            v = unpack('>LLLL', r[1][8:])
            print('kUARPTLVAccessoryInformation:Statistics:')
            print(f'    packetsNoVersionAgreement: {v[0]}')
            print(f'    packetsMissed:             {v[1]}')
            print(f'    packetsDuplicate:          {v[2]}')
            print(f'    packetsOutOfOrder:         {v[3]}')
        elif n == kUARPTLVAccessoryInformationLastError:
            v = unpack('>LL', r[1][8:])
            print('kUARPTLVAccessoryInformation:LastError:')
            print(f'    lastAction: 0x{v[0]:08X}')
            print(f'    lastError:  0x{v[1]:08X}')

    def msgAssetAvailableNotification(self, super_binary):
        num_payloads = unpack_from('>L', super_binary, 40)[0]
        self.send(kUARPMsgAssetAvailableNotification, pack('>LHH16sLH',
            0,                    # uint32_t assetTag;
            1,                    # uint16_t assetFlags;
            1,                    # uint16_t assetID;
            super_binary[12:28],  # struct UARPVersion assetVersion;
            len(super_binary),    # uint32_t assetLength;
            num_payloads,         # uint16_t assetNumPayloads;
        ))
        self.wait_for_response(kUARPMsgAssetAvailableNotificationAck)
        self.super_binary = super_binary

    def MsgApplyStagedAssetsRequest(self):
        self.send(kUARPMsgApplyStagedAssetsRequest)
        r = self.wait_for_response(kUARPMsgApplyStagedAssetsResponse)
        flags = unpack(">H", r[1])[0]
        print(get_const(flags, 'kUARPApplyStagedAssetsFlags'))

    def MsgAssetRescindedNotification(self):
        self.send(kUARPMsgAssetRescindedNotification, pack('>H', 1))
        self.wait_for_response(kUARPMsgAssetRescindedNotificationAck)

    def asset_processing_loop(self, progress_callback=None):
        while self.running:

            msg_type, payload, _ = self.wait_for_response(kUARPMsgAssetDataRequest, kUARPMsgAssetProcessingNotification, timeout=1)

            if msg_type is None:
                if progress_callback is not None:
                    ret = progress_callback(None, None, len(self.super_binary))
                    if ret != None:
                        print(f'Asset Processing Interrupted: {str(ret)}')
                        return ret

            if msg_type == kUARPMsgAssetDataRequest:
                asset_id, data_offset, num_bytes = unpack('>HLH', payload)
                if data_offset <= len(self.super_binary):
                    response_bytes = min(num_bytes, len(self.super_binary) - data_offset)
                    self.send(kUARPMsgAssetDataResponse, pack('>HHLHH',
                        kUARPStatusSuccess,
                        asset_id,
                        data_offset,
                        num_bytes,
                        response_bytes
                    ) + self.super_binary[data_offset:data_offset + response_bytes])
                else:
                    self.send(kUARPMsgAssetDataResponse, pack('>HHLHH',
                        kUARPStatusInvalidDataRequestOffset,
                        asset_id,
                        data_offset,
                        num_bytes,
                        0
                    ))
                    num_bytes = None

                if progress_callback is not None:
                    ret = progress_callback(data_offset, num_bytes, len(self.super_binary))
                    if ret != None:
                        print(f'Asset Processing Interrupted: {str(ret)}')
                        return ret

            elif msg_type == kUARPMsgAssetProcessingNotification:
                _, flags = unpack('>HH', payload)
                print(f'Asset Processing Notification: {get_const(flags, "kUARPAssetProcessingFlags")}')
                return flags
        
        raise UarpInterruptedException()


