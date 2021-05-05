#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import logging
import time

from pc_ble_driver_py import config
config.__conn_ic_id__ = "nrf52"

from pc_ble_driver_py.exceptions import NordicSemiException
from pc_ble_driver_py.ble_driver import Flasher, BLEDriver, BLEDriverObserver, BLEConfig,\
     BLEConfigConnGatt, BLEUUIDBase, BLEUUID, BLEAdvData, BLEGapConnParams, ATT_MTU_DEFAULT, \
     BLEGattStatusCode, BLEGapSecStatus
from pc_ble_driver_py.ble_adapter import BLEAdapter, BLEAdapterObserver, EvtSync

FMN_SERVICE_UUID = 0xFD44

logger = logging.getLogger(__name__)


class BleUarpControlPoint(BLEDriverObserver, BLEAdapterObserver):

    LOCAL_ATT_MTU = 247
    CFG_TAG = 1
    BASE_UUID = BLEUUIDBase([0x94, 0x11, 0x00, 0x00, 0x6D, 0x9B, 0x42, 0x25, 0xA4, 0xF1, 0x6A, 0x4A,
                             0x7F, 0x01, 0xB0, 0xDE])
    UUID_FMN_UARP_DCP = BLEUUID(0x0001, BASE_UUID)

    def __init__(self, com_port, periph_name):
        driver = BLEDriver(serial_port=com_port, baud_rate=1000000)
        adapter = BLEAdapter(driver)
        self.evt_sync = EvtSync(['connected', 'disconnected'])
        self.target_device_name = periph_name
        self.target_device_addr = 0
        self.conn_handle = None
        self.adapter = adapter
        self.adapter.observer_register(self)
        self.adapter.driver.observer_register(self)
        self.adapter.driver.open()
        gatt_cfg = BLEConfigConnGatt()
        gatt_cfg.att_mtu = BleUarpControlPoint.LOCAL_ATT_MTU
        gatt_cfg.tag = BleUarpControlPoint.CFG_TAG
        self.adapter.driver.ble_cfg_set(BLEConfig.conn_gatt, gatt_cfg)
        self.adapter.driver.ble_enable()
        self.adapter.driver.ble_vs_uuid_add(BleUarpControlPoint.BASE_UUID)

    def connect(self):
        logger.debug('BLE: connect: target address: 0x{}'.format(self.target_device_addr))
        logger.info('BLE: Scanning...')
        self.adapter.driver.ble_gap_scan_start()
        self.conn_handle = self.evt_sync.wait('connected')
        if self.conn_handle is None:
            raise NordicSemiException('Timeout. Target device not found.')
        logger.info('BLE: Connected to target')
        logger.debug('BLE: Service Discovery')

        if BleUarpControlPoint.LOCAL_ATT_MTU > ATT_MTU_DEFAULT:
            logger.info('BLE: Enabling longer ATT MTUs')
            self.att_mtu = self.adapter.att_mtu_exchange(self.conn_handle, BleUarpControlPoint.LOCAL_ATT_MTU)
            logger.info('BLE: ATT MTU: {}'.format(self.att_mtu))
        else:
            self.att_mtu = ATT_MTU_DEFAULT
            logger.info('BLE: Using default ATT MTU')

        self.adapter.service_discovery(conn_handle=self.conn_handle)
        logger.debug('BLE: Enabling Indication')
        try:
            self.adapter.enable_indication(conn_handle=self.conn_handle, uuid=BleUarpControlPoint.UUID_FMN_UARP_DCP)
        except NordicSemiException as inst:
            if inst.error_code == BLEGattStatusCode.insuf_encryption:
                self.adapter.authenticate(conn_handle=self.conn_handle, _role=self.adapter.db_conns[self.conn_handle].role)
            else:
                raise inst

    def disconnect(self):
        self.adapter.disconnect(conn_handle=self.conn_handle)
    
    def close(self):
        self.adapter.close()

    def rx_handler(self, data):
        pass

    def write(self, data):
        self.adapter.write_req(self.conn_handle, BleUarpControlPoint.UUID_FMN_UARP_DCP, data)

    def on_gap_evt_connected(self, ble_driver, conn_handle, peer_addr, role, conn_params):
        self.evt_sync.notify(evt = 'connected', data = conn_handle)

    def on_gap_evt_disconnected(self, ble_driver, conn_handle, reason):
        self.evt_sync.notify(evt = 'disconnected', data = conn_handle)
        logger.info(f'BLE: Disconnected (reason {reason})...')

    def on_gap_evt_adv_report(self, ble_driver, conn_handle, peer_addr, rssi, adv_type, adv_data):
        uuid = 0

        if BLEAdvData.Types.service_data in adv_data.records:
            uuid = bytearray(adv_data.records[BLEAdvData.Types.service_data][:2])
            uuid = int.from_bytes(uuid, byteorder='little', signed=False)

        address_string  = "".join("{0:02X}".format(b) for b in peer_addr.addr)
        logger.debug('Received advertisement report, address: 0x{}'.format(address_string))

        if uuid == FMN_SERVICE_UUID:
            conn_params = BLEGapConnParams(min_conn_interval_ms = 15,
                                           max_conn_interval_ms = 30,
                                           conn_sup_timeout_ms  = 4000,
                                           slave_latency        = 0)
            logger.info('BLE: Found target advertiser, address: 0x{}'.format(address_string))
            logger.info('BLE: Connecting to 0x{}'.format(address_string))
            self.adapter.connect(address = peer_addr, conn_params = conn_params, tag=BleUarpControlPoint.CFG_TAG)
            # store the name and address for subsequent connections

    def on_gap_evt_auth_status(self, ble_driver, conn_handle, **kwargs):
        status = kwargs.get('auth_status')
        if status == BLEGapSecStatus.success:
            status = 'Success'
        else:
            status = 'Failed'

        logger.info('BLE Authentication status conn_handle: {} status: {} '.format(conn_handle, status))

    def on_notification(self, ble_adapter, conn_handle, uuid, data):
        logger.debug("Received notification {}".format(len(data)))

    def on_indication(self, ble_adapter, conn_handle, uuid, data):
        if self.conn_handle != conn_handle: return
        if BleUarpControlPoint.UUID_FMN_UARP_DCP.value != uuid.value:  return
        logger.debug("Received indication {}".format(len(data)))
        self.rx_handler(data)

    @staticmethod
    def flash_connectivity(comport, jlink_snr):
        flasher = Flasher(serial_port=comport, snr=jlink_snr)
        if flasher.fw_check():
            logger.info("Board already flashed with connectivity firmware.")
        else:
            logger.info("Flashing connectivity firmware...")
            flasher.fw_flash()
            logger.info("Connectivity firmware flashed.")
        flasher.reset()
        time.sleep(1)


class BleUarp(BleUarpControlPoint):

    BT_ATT_HEADER_LEN = 3

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.rx_packet = bytearray()
        self.message_handler = None

    def send_message(self, data):
        chunks = BleUarp._split_bytes(data, self.att_mtu - BleUarp.BT_ATT_HEADER_LEN - 1)
        for i in range(0, len(chunks)):
            prefix = b'\0' if i < len(chunks) - 1 else b'\1'
            self.write(prefix + bytes(chunks[i]))

    def rx_handler(self, data):
        if len(data) < 1: return
        self.rx_packet.extend(data[1:])
        if data[0] != 0:
            packet = self.rx_packet
            self.rx_packet = bytearray()
            if self.message_handler:
                self.message_handler(packet)

    def set_message_handler(self, h):
        self.message_handler = h

    @staticmethod
    def _split_bytes(data, chunk_size):
        return [data[i:i+chunk_size] for i in range(0, len(data), chunk_size)]
