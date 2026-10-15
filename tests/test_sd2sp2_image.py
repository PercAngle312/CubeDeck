#!/usr/bin/env python3
"""Independently inspect GPT, FAT chains and VFAT name in generated fixtures."""
import binascii
import importlib.util
import struct
import tempfile
from pathlib import Path
spec=importlib.util.spec_from_file_location('sd_image',Path('tools/emulator/make_sd2sp2_image.py'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
def check(size):
    probe=bytearray(size)
    probe[0xa0:0xac]=b'CDTEST      '
    probe[0xb2]=0x96
    probe[0xbd]=(-(sum(probe[0xa0:0xbd])+0x19))&255
    for i in range(0xc0,size):probe[i]=(i*7)&255
    with tempfile.TemporaryDirectory() as temp:
        target=Path(temp)/'sd.img'
        module.make_image(target,probe)
        with target.open('rb') as f:
            def sector(lba):f.seek(lba*512);return f.read(512)
            mbr=sector(0);assert mbr[450]==0xee and mbr[510:]==b'\x55\xaa'
            header=bytearray(sector(1)); crc=struct.unpack_from('<I',header,16)[0]
            header[16:20]=bytes(4);assert binascii.crc32(header[:92])==crc
            table=b''.join(sector(i) for i in range(2,34))
            assert binascii.crc32(table)==struct.unpack_from('<I',header,88)[0]
            start,end=struct.unpack_from('<QQ',table,32);assert start==2048
            boot=sector(start);assert boot[82:90]==b'FAT32   '
            reserved=struct.unpack_from('<H',boot,14)[0];fats=boot[16]
            fat_size=struct.unpack_from('<I',boot,36)[0]
            fat_start=start+reserved;data_start=fat_start+fats*fat_size
            assert sector(fat_start)==sector(fat_start+fat_size)
            assert sector(start)==sector(start+6)
            assert end-start+1==struct.unpack_from('<I',boot,32)[0]
            root=sector(data_start);assert root[:11]==b'CUBEDECK   '
            directory=sector(data_start+1)
            decoded=[]
            for off in (96,64):
                e=directory[off:off+32];assert e[11]==0x0f
                decoded.extend(struct.unpack_from('<H',e,o)[0] for o in (1,3,5,7,9,14,16,18,20,22,24,28,30))
            assert ''.join(chr(x) for x in decoded[:decoded.index(0)])=='joyboot-probe.gba'
            e=directory[128:160];assert struct.unpack_from('<I',e,28)[0]==size
            cluster=struct.unpack_from('<H',e,26)[0];content=bytearray();seen=set()
            while cluster < 0x0ffffff8:
                assert cluster not in seen;seen.add(cluster)
                content.extend(sector(data_start+cluster-2))
                fat=sector(fat_start+(cluster*4)//512)
                cluster=struct.unpack_from('<I',fat,(cluster*4)%512)[0]&0x0fffffff
            assert content[:size]==probe
        try:module.make_image(target,probe)
        except FileExistsError:pass
        else:raise AssertionError('Existing image overwritten')
check(3200)
check(0x3ff40)
print('Synthetic GPT/FAT32 image: independent CRC, BPB, VFAT name, chain, full-size file and no-overwrite checks passed.')
