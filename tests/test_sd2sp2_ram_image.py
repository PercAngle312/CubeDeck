#!/usr/bin/env python3
"""Inspect v9 FAT chains independently; all BIOS bytes here are synthetic."""
import hashlib
import struct
import sys
import tempfile
import warnings
import zipfile
from pathlib import Path
sys.path.insert(0, str(Path('tools/emulator').resolve()))
from make_sd2sp2_ram_image import make_ram_image, read_release, TEMPLATE, DOL

def template(size):
    b=bytearray(size);b[0xa0:0xa6]=b'CDRAM9';b[0xb2]=0x96
    b[0xbd]=(-(sum(b[0xa0:0xbd])+0x19))&255
    for i in range(0xc0,size):b[i]=(i*13+7)&255
    return bytes(b)

def inspect(path, expected):
    with path.open('rb') as f:
        def sector(lba):f.seek(lba*512);return f.read(512)
        boot=sector(2048); reserved=struct.unpack_from('<H',boot,14)[0]
        fat_size=struct.unpack_from('<I',boot,36)[0];fat=2048+reserved;data=fat+boot[16]*fat_size
        assert sector(data)[:11]==b'CUBEDECK   '
        directory=sector(data+1);long_parts={};used=set();found={}
        for off in range(64,512,32):
            e=directory[off:off+32]
            if not e[0]:break
            if e[11]==15:
                seq=e[0]&31
                if e[0]&64:long_parts={};count=seq;checksum=e[13]
                assert e[13]==checksum and seq not in long_parts
                long_parts[seq]=[struct.unpack_from('<H',e,pos)[0] for pos in (1,3,5,7,9,14,16,18,20,22,24,28,30)]
                continue
            alias_crc=0
            for b in e[:11]:alias_crc=(((alias_crc&1)<<7)+(alias_crc>>1)+b)&255
            assert alias_crc==checksum and set(long_parts)==set(range(1,count+1))
            chars=sum((long_parts[i] for i in range(1,count+1)),[])
            name=''.join(chr(c) for c in chars[:chars.index(0)])
            assert name not in found
            length=struct.unpack_from('<I',e,28)[0];cluster=struct.unpack_from('<H',e,26)[0]
            content=bytearray();chain=set()
            while cluster<0xffffff8:
                assert cluster>=4 and cluster not in used
                used.add(cluster);chain.add(cluster);content.extend(sector(data+cluster-2))
                a=sector(fat+cluster*4//512);b=sector(fat+fat_size+cluster*4//512)
                assert a==b
                cluster=struct.unpack_from('<I',a,cluster*4%512)[0]&0xfffffff
            assert len(chain)==(length+511)//512 and cluster==0xfffffff
            found[name]=bytes(content[:length]);long_parts={}
        assert found==expected

with tempfile.TemporaryDirectory() as temp:
    root=Path(temp)
    # Deliberately fake BIOS: no Nintendo bytes are required or stored by CI.
    bios=bytes((i*19+3)&255 for i in range(16384));before=hashlib.sha256(bios).hexdigest()
    for size in (512,2064,16384):
        p=root/f'{size}.img';t=template(size)
        r=make_ram_image(p,t,bios,allow_invalid_bios=True)
        assert r['private_image'] and not r['bios_revision_match']
        inspect(p,{'ram-status-v9.gba':t,'gba-bios.bin':bios})
        original=p.read_bytes()
        try:make_ram_image(p,t,bios,allow_invalid_bios=True)
        except FileExistsError:pass
        else:raise AssertionError('Existing target overwritten')
        assert p.read_bytes()==original
    p=root/'corrupt.img'
    make_ram_image(p,template(2064),bios,allow_invalid_bios=True,corrupt_bios=True)
    corrupted=bytes([bios[0]^1])+bios[1:]
    inspect(p,{'ram-status-v9.gba':template(2064),'gba-bios.bin':corrupted})
    assert hashlib.sha256(bios).hexdigest()==before
    # Validate failures occur before any image is created.
    bad_logo=bytearray(template(512));bad_logo[4]=1
    bad_header=bytearray(template(512));bad_header[0xbd]^=1
    bad_title=bytearray(template(512));bad_title[0xa0]^=1
    cases=[(template(512),bios,False),(template(512),bios[:-1],True),
           (template(512),bios+b'x',True),(bytes(bad_logo),bios,True),
           (bytes(bad_header),bios,True),(bytes(bad_title),bios,True),
           (template(512)[:-1],bios,True),(template(16400),bios,True)]
    for i,(t,b,allow) in enumerate(cases):
        path=root/f'bad-{i}.img'
        try:make_ram_image(path,t,b,allow_invalid_bios=allow)
        except ValueError:pass
        else:raise AssertionError('Invalid input accepted')
        assert not path.exists()
    # Archive checks read bounded known members without extracting paths.
    t=template(512);dol=b'FAKE TEST DOL'
    sums=''.join(hashlib.sha256(b).hexdigest()+'  '+name+'\n' for name,b in ((TEMPLATE,t),(DOL,dol)))
    for mode in ('good','tampered','duplicate','manifest-missing'):
        zpath=root/(mode+'.zip')
        with warnings.catch_warnings():
            warnings.simplefilter('ignore',UserWarning)
            with zipfile.ZipFile(zpath,'w') as z:
                z.writestr(TEMPLATE,t if mode!='tampered' else t[:-1]+b'x')
                z.writestr(DOL,dol)
                if mode=='duplicate':z.writestr(TEMPLATE,t)
                z.writestr('SHA256SUMS',sums if mode!='manifest-missing' else '')
                z.writestr('../../never-extract.txt','not extracted')
        if mode=='good':assert read_release(zpath)==t
        else:
            try:read_release(zpath)
            except ValueError:pass
            else:raise AssertionError('Invalid release accepted')
    assert not (root/'never-extract.txt').exists()
print('Private v9 image: two VFAT paths, disjoint chains, FAT copies, bounded templates, invalid inputs, manifest checks, no overwrite and source immutability passed.')
