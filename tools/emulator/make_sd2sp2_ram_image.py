#!/usr/bin/env python3
"""Build a private disposable SD image for v9, never bundle or obtain BIOS assets."""
import argparse
import binascii
import hashlib
import json
import struct
import zipfile
from pathlib import Path
from make_sd2sp2_image import make_image

BIOS_SIZE = 16384
BIOS_CRC = 0x81977335
TEMPLATE = 'cubedeck/ram-status-v9.gba'
DOL = 'cubedeck-ram-upload-v9.dol'

def read_release(path):
    """Read only named members, verify their release manifest, extract no paths."""
    with zipfile.ZipFile(path) as z:
        for name in (TEMPLATE, DOL, 'SHA256SUMS'):
            info = z.getinfo(name)
            limit = 16384 if name == TEMPLATE else 4*1024*1024
            if info.file_size > limit or z.namelist().count(name) != 1:
                raise ValueError('Oversized or duplicate release member')
        sums = {}
        for line in z.read('SHA256SUMS').decode('ascii').splitlines():
            digest, name = line.split(None, 1)
            name = name.strip()
            if name in sums: raise ValueError('Duplicate checksum entry')
            sums[name] = digest
        for name in (TEMPLATE, DOL):
            if sums.get(name) != hashlib.sha256(z.read(name)).hexdigest():
                raise ValueError('Release checksum mismatch: '+name)
        return z.read(TEMPLATE)

def make_ram_image(target, template, bios, *, allow_invalid_bios=False,
                   corrupt_bios=False):
    # Validate all inputs before creating the output. No target overwrite.
    if not 512 <= len(template) <= 16384 or len(template) % 16:
        raise ValueError('Invalid v9 template size')
    if template[0xa0:0xa6] != b'CDRAM9' or template[4:0xa0] != bytes(156):
        raise ValueError('Expected logo-free CDRAM9 template')
    if template[0xb2] != 0x96 or template[0xbd] != (-(sum(template[0xa0:0xbd])+0x19)) & 255:
        raise ValueError('Invalid template header')
    if len(bios) != BIOS_SIZE: raise ValueError('BIOS must be exactly 16 KiB')
    matched = binascii.crc32(bios) == BIOS_CRC
    if not matched and not allow_invalid_bios:
        raise ValueError('BIOS revision mismatch; use --allow-invalid-bios only for fault fixtures')
    if corrupt_bios:
        bios = bytes([bios[0] ^ 1]) + bios[1:]
    report = make_image(target, template, ram_status=True)
    # Existing fixture reserves root cluster 2, CUBEDECK cluster 3 and the
    # template's chain from cluster 4. Append a nonoverlapping BIOS chain.
    first = 4 + (len(template)+511)//512
    with target.open('r+b') as f:
        f.seek(2048*512); boot = f.read(512)
        reserved = struct.unpack_from('<H', boot, 14)[0]
        fats, fat_size = boot[16], struct.unpack_from('<I', boot, 36)[0]
        fat_start = 2048 + reserved
        data = fat_start + fats*fat_size
        f.seek((data+1)*512); directory = bytearray(f.read(512))
        alias = b'GBABIO~1BIN'
        checksum = 0
        for b in alias: checksum = (((checksum&1)<<7)+(checksum>>1)+b)&255
        lfn = bytearray(32); lfn[0] = 0x41; lfn[11] = 0x0f; lfn[13] = checksum
        chars = [ord(c) for c in 'gba-bios.bin'] + [0]
        for off, value in zip((1,3,5,7,9,14,16,18,20,22,24,28,30), chars):
            struct.pack_into('<H', lfn, off, value)
        entry = bytearray(32); entry[:11] = alias; entry[11] = 0x20
        struct.pack_into('<HI', entry, 26, first, len(bios))
        directory[160:192] = lfn; directory[192:224] = entry
        for i in range(BIOS_SIZE//512):
            cluster = first+i
            next_cluster = 0x0fffffff if i == BIOS_SIZE//512-1 else cluster+1
            for fat in range(fats):
                f.seek((fat_start+fat*fat_size)*512+cluster*4)
                f.write(struct.pack('<I', next_cluster))
            f.seek((data+cluster-2)*512); f.write(bios[i*512:(i+1)*512])
        f.seek((data+1)*512); f.write(directory)
    report.update(template_path=TEMPLATE, bios_path='cubedeck/gba-bios.bin',
                  bios_revision_match=matched and not corrupt_bios,
                  private_image=True)
    return report

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--v9-zip', required=True, type=Path)
    p.add_argument('--bios', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--allow-invalid-bios', action='store_true', help='Deliberately build a failing revision-gate fixture')
    p.add_argument('--corrupt-bios', action='store_true', help='Flip one byte in the image copy; source BIOS stays unchanged')
    a = p.parse_args()
    # Read a bounded input; don't load an arbitrary-sized asset into RAM.
    with a.bios.open('rb') as f: bios = f.read(BIOS_SIZE+1)
    print(json.dumps(make_ram_image(a.output, read_release(a.v9_zip), bios,
          allow_invalid_bios=a.allow_invalid_bios, corrupt_bios=a.corrupt_bios), indent=2))
    print('PRIVATE IMAGE: contains your BIOS copy. Do not upload, commit or distribute it.')
