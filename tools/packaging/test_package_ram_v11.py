#!/usr/bin/env python3
"""Packaging checks reject truncated/corrupted console images."""
import struct
import unittest
from package_ram_v11 import validate_dol, validate_template


class PackageTests(unittest.TestCase):
    def test_dol(self):
        data = bytearray(272)
        struct.pack_into('>I', data, 0, 256)
        struct.pack_into('>I', data, 0x48, 0x80003100)
        struct.pack_into('>I', data, 0x90, 16)
        struct.pack_into('>I', data, 0xe0, 0x80003100)
        validate_dol(data)
        for offset, value in ((0, 255), (0x90, 17), (0x48, 0x81800000),
                              (0xe0, 0x80003200)):
            bad = bytearray(data)
            struct.pack_into('>I', bad, offset, value)
            with self.assertRaises(ValueError):
                validate_dol(bad)
        with self.assertRaises(ValueError):
            validate_dol(data[:100])
        bad = bytearray(data)
        struct.pack_into('>I', bad, 4, 256)
        struct.pack_into('>I', bad, 0x4c, 0x80004000)
        struct.pack_into('>I', bad, 0x94, 16)
        with self.assertRaises(ValueError):
            validate_dol(bad)

    def test_gba(self):
        data = bytearray(512)
        data[0xa0:0xa8] = b'CDHEAD11'
        data[0xac:0xb0] = b'CD11'
        data[0xb2] = 0x96
        data[0xbd] = (-sum(data[0xa0:0xbd]) - 0x19) & 255
        validate_template(data)
        for offset in (4, 0xa0, 0xac, 0xb2, 0xbd):
            bad = bytearray(data)
            bad[offset] ^= 1
            with self.assertRaises(ValueError):
                validate_template(bad)
        for size in (100, 511, 513, 16385):
            with self.assertRaises(ValueError):
                validate_template((data + bytes(16384))[:size])


if __name__ == '__main__':
    unittest.main()
