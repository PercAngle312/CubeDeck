#!/usr/bin/env python3
"""Create a disposable 128 MiB GPT/FAT32 fixture containing homebrew only."""
import argparse
import binascii
import hashlib
import struct
import uuid
import zipfile
from pathlib import Path

def make_image(target, probe):
    if not 512 <= len(probe) <= 0x3ff40 or len(probe) % 16:
        raise ValueError('Invalid homebrew fixture size')
    if probe[0xb2] != 0x96 or probe[0xbd] != (-(sum(probe[0xa0:0xbd])+0x19)) & 0xff:
        raise ValueError('Invalid probe header checksum/fixed byte')
    if any(probe[4:0xa0]):
        raise ValueError('Use the logo-free storage fixture from v7')
    sectors, start, volume = 262144, 2048, 258048
    reserved, fats, fat_size = 32, 2, 1
    while True:
        clusters = volume - reserved - fats * fat_size
        needed = (clusters + 2 + 127) // 128
        if needed == fat_size: break
        fat_size = needed
    data_start = start + reserved + fats * fat_size
    with target.open('xb') as f:
        f.truncate(sectors * 512)
        def sector(lba, data):
            assert len(data) == 512
            f.seek(lba * 512)
            f.write(data)
        mbr = bytearray(512)
        mbr[446+4] = 0xee
        struct.pack_into('<II', mbr, 446+8, 1, sectors-1)
        mbr[510:] = b'\x55\xaa'
        sector(0, mbr)
        entries = bytearray(128*128)
        entries[:16] = uuid.UUID('ebd0a0a2-b9e5-4433-87c0-68b6b72699c7').bytes_le
        entries[16:32] = uuid.UUID('cdd00001-1234-5678-9abc-000000000001').bytes_le
        struct.pack_into('<QQQ', entries, 32, start, start+volume-1, 0)
        entries[56:56+16] = 'CUBEDECK'.encode('utf-16le')
        def header(my, backup, array_lba):
            h = bytearray(512)
            h[:8] = b'EFI PART'
            struct.pack_into('<IIIIQQQQ', h, 8, 0x10000, 92, 0, 0, my, backup, 34, sectors-34)
            h[56:72] = uuid.UUID('cdd00000-1234-5678-9abc-000000000001').bytes_le
            struct.pack_into('<QIII', h, 72, array_lba, 128, 128, binascii.crc32(entries))
            struct.pack_into('<I', h, 16, binascii.crc32(h[:92]))
            return h
        for i in range(32):
            sector(2+i, entries[i*512:(i+1)*512])
            sector(sectors-33+i, entries[i*512:(i+1)*512])
        sector(1, header(1, sectors-1, 2))
        sector(sectors-1, header(sectors-1, 1, sectors-33))
        boot = bytearray(512)
        boot[:3] = b'\xeb\x58\x90'; boot[3:11] = b'CUBEDECK'
        struct.pack_into('<HBHBHHBHHHII', boot, 11, 512, 1, reserved, fats, 0, 0, 0xf8, 0, 63, 255, start, volume)
        struct.pack_into('<IHHIHH', boot, 36, fat_size, 0, 0, 2, 1, 6)
        boot[64] = 0x80; boot[66] = 0x29
        struct.pack_into('<I', boot, 67, 0xcdd02026)
        boot[71:82] = b'CUBEDECK   '; boot[82:90] = b'FAT32   '
        boot[510:] = b'\x55\xaa'
        sector(start, boot); sector(start+6, boot)
        fsinfo = bytearray(512)
        struct.pack_into('<I', fsinfo, 0, 0x41615252)
        struct.pack_into('<III', fsinfo, 484, 0x61417272, 0xffffffff, 0xffffffff)
        struct.pack_into('<I', fsinfo, 508, 0xaa550000)
        sector(start+1, fsinfo); sector(start+7, fsinfo)
        blocks = (len(probe)+511)//512
        fat = bytearray(max(512, ((blocks+4)*4+511)//512*512))
        struct.pack_into('<IIII', fat, 0, 0x0ffffff8, 0x0fffffff, 0x0fffffff, 0x0fffffff)
        for i in range(blocks):
            struct.pack_into('<I', fat, (4+i)*4, 0x0fffffff if i==blocks-1 else 5+i)
        for j in range(len(fat)//512):
            sector(start+reserved+j, fat[j*512:(j+1)*512])
            sector(start+reserved+fat_size+j, fat[j*512:(j+1)*512])
        def entry(name, attr, cluster, size=0):
            e = bytearray(32); e[:11] = name; e[11] = attr
            struct.pack_into('<H', e, 20, cluster>>16)
            struct.pack_into('<HI', e, 26, cluster&0xffff, size)
            return e
        root = bytearray(512); root[:32] = entry(b'CUBEDECK   ', 0x10, 3)
        sector(data_start, root)
        directory = bytearray(512)
        directory[:32] = entry(b'.          ', 0x10, 3)
        directory[32:64] = entry(b'..         ', 0x10, 0)
        alias = b'JOYBOO~1GBA'; checksum = 0
        for b in alias: checksum = (((checksum&1)<<7)+(checksum>>1)+b)&255
        chars = list(struct.unpack('<17H', 'joyboot-probe.gba'.encode('utf-16le')))+[0]
        chars += [0xffff]*(26-len(chars))
        for pos, seq in enumerate((2,1)):
            e = bytearray(32); e[0] = seq | (0x40 if seq==2 else 0)
            e[11] = 0x0f; e[13] = checksum
            chunk = chars[(seq-1)*13:seq*13]
            for off, value in zip((1,3,5,7,9,14,16,18,20,22,24,28,30), chunk):
                struct.pack_into('<H', e, off, value)
            directory[64+pos*32:96+pos*32] = e
        directory[128:160] = entry(alias, 0x20, 4, len(probe))
        sector(data_start+1, directory)
        for i in range(blocks): sector(data_start+2+i, probe[i*512:(i+1)*512].ljust(512,b'\0'))
    return {'image':str(target),'bytes':sectors*512,'partition_lba':start,'probe_bytes':len(probe),'probe_sha256':hashlib.sha256(probe).hexdigest()}

if __name__ == '__main__':
    import json
    p = argparse.ArgumentParser()
    p.add_argument('--v7-zip', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    with zipfile.ZipFile(args.v7_zip) as z: probe = z.read('cubedeck/joyboot-probe.gba')
    print(json.dumps(make_image(args.output, probe),indent=2))
