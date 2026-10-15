#!/usr/bin/env python3
"""Verify and package the separate logo-free v11 hardware experiment."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile


def validate_dol(data):
    if len(data) < 256:
        raise ValueError('Incomplete DOL header')
    offsets = struct.unpack_from('>18I', data, 0)
    addresses = struct.unpack_from('>18I', data, 0x48)
    sizes = struct.unpack_from('>18I', data, 0x90)
    entry, = struct.unpack_from('>I', data, 0xe0)
    ranges = []
    executable = False
    for i, (offset, address, size) in enumerate(zip(offsets, addresses, sizes)):
        if not size:
            continue
        if offset < 256 or offset + size > len(data):
            raise ValueError('DOL section outside file')
        if address < 0x80000000 or address + size > 0x81800000:
            raise ValueError('DOL section outside GameCube RAM')
        if any(offset < end and start < offset + size for start, end in ranges):
            raise ValueError('Overlapping DOL file sections')
        ranges.append((offset, offset + size))
        executable |= i < 7 and address <= entry < address + size
    if not executable:
        raise ValueError('DOL entry point outside text sections')


def validate_template(data):
    if not (512 <= len(data) <= 16384 and len(data) % 16 == 0):
        raise ValueError('Invalid GBA RAM template size')
    if data[4:0xa0] != bytes(156):
        raise ValueError('GBA template must have logo region cleared')
    if data[0xa0:0xac] != b'CDHEAD11\0\0\0\0' or data[0xac:0xb0] != b'CD11':
        raise ValueError('Wrong GBA template identity')
    if data[0xb2] != 0x96 or (sum(data[0xa0:0xbe]) + 0x19) & 255:
        raise ValueError('Invalid GBA template header')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--run', required=True)
    args = parser.parse_args()
    dol = args.input / 'cubedeck-ram-header-v11.dol'
    preflight = args.input / 'cubedeck-ram-preflight-v11.dol'
    gba = args.input / 'cubedeck/ram-header-v11.gba'
    validate_dol(dol.read_bytes())
    validate_dol(preflight.read_bytes())
    validate_template(gba.read_bytes())
    readme = Path('experiments/cartridge-probe/RAM_HEADER_V11.md').read_bytes()
    payloads = {dol.name: dol.read_bytes(), preflight.name: preflight.read_bytes(), 'cubedeck/' + gba.name: gba.read_bytes(),
                'README.md': readme,
                'BUILD.json': (json.dumps({'commit': args.commit, 'ci_run': args.run,
                    'hardware_validated': False, 'requires_ram_preflight_cold_start_passes': 3},
                    indent=2) + '\n').encode()}
    payloads['SHA256SUMS'] = ''.join(
        f'{hashlib.sha256(data).hexdigest()}  {name}\n'
        for name, data in payloads.items()).encode()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in payloads.items():
            archive.writestr(name, data)
    with zipfile.ZipFile(args.output) as archive:
        assert archive.testzip() is None
        assert set(archive.namelist()) == set(payloads)
    print(f'Verified v11 package: {args.output}')


if __name__ == '__main__':
    main()
