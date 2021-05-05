#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

from struct import pack

SUPER_BINARY_FORMAT_VERSION = 2
SUPER_BINARY_HEADER_LENGTH = 44
PAYLOAD_HEADER_LENGTH = 40


class MetadataContainer:

    def __init__(self):
        self.output = bytearray()
        self.tlv = {}
        self.version = b'\0' * 16
        self.header_offset = None
        self.metadata_start = 0
        self.metadata_end = 0

    def add_metadata(self, tlvType, tlvValue):
        if tlvType in self.tlv:
            raise Exception(f'Metadata already has type 0x{tlvType:08X} ({tlvType})')
        self.tlv[tlvType] = tlvValue

    def set_version(self, a, b=0, c=0, d=0):
        self.version = pack('>LLLL', a, b, c, d)

    def _set_output(self, output):
        self.output = output

    def _generate_metadata(self):
        self.metadata_start = len(self.output)
        for tlvType, tlvValue in self.tlv.items():
            self.output += pack('>LL', tlvType, len(tlvValue))
            self.output += tlvValue
        self.metadata_end = len(self.output)

    def _generate_header(self, before, after):
        if self.header_offset is None:
            self.header_offset = len(self.output)
            self.output += b'\0' * (len(before) + 24 + len(after))
        output = bytearray(before)
        output += self.version
        output += pack('>LL',
            self.metadata_start,
            self.metadata_end - self.metadata_start)
        output += after
        self.output[self.header_offset:self.header_offset + len(output)] = output


class SuperBinary(MetadataContainer):

    def __init__(self):
        super().__init__()
        self.payloads = []
        self.payload_headers_start = 0
        self.payload_headers_end = 0

    def add_payload(self, payload=None):
        if payload is None:
            payload = Payload()
        payload._set_output(self.output)
        self.payloads.append(payload)
        return payload

    def generate(self):
        self.output.clear()
        # SuperBinary header placeholder
        self._generate_header()
        # Payload header placeholders
        self.payload_headers_start = len(self.output)
        for p in self.payloads:
            p._generate_header()
        self.payload_headers_end = len(self.output)
        # SuperBinary metadata
        self._generate_metadata()
        # Payloads
        for p in self.payloads:
            # Payload metadata
            p._generate_metadata()
            # Payload content
            p._generate_content()
            # Rewrite payload header
            p._generate_header()
        # Rewrite SuperBinary header
        self._generate_header()
        return self.output

    def _generate_header(self):
        super()._generate_header(
            pack('>LLL',
                SUPER_BINARY_FORMAT_VERSION,
                SUPER_BINARY_HEADER_LENGTH,
                len(self.output)),
            pack('>LL',
                self.payload_headers_start,
                self.payload_headers_end - self.payload_headers_start))


class Payload(MetadataContainer):

    def __init__(self):
        super().__init__()
        self.content = bytearray()
        self.tag = b'\0\0\0\0'
        self.content_offset = 0

    def set_tag(self, tag):
        self.tag = tag.encode('utf-8')
        if len(self.tag) != 4:
            raise Exception('Tag must be 4-chars long')

    def _generate_header(self):
        super()._generate_header(
            pack('>L', PAYLOAD_HEADER_LENGTH) + self.tag,
            pack('>LL', self.content_offset, len(self.content))
        )

    def _generate_content(self):
        self.content_offset = len(self.output)
        self.output += self.content

