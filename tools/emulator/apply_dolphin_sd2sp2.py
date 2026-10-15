#!/usr/bin/env python3
"""Apply the opt-in read-only test adapter to our pinned Dolphin source."""
import argparse
import shutil
import subprocess
from pathlib import Path
BASE='b92e354389bb7c0bd114a8631b8af110d3cb3a14'
p=argparse.ArgumentParser();p.add_argument('dolphin_source',type=Path);args=p.parse_args()
root=args.dolphin_source.resolve();here=Path(__file__).resolve().parent
actual=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
if actual!=BASE:raise SystemExit(f'Expected Dolphin source {BASE}, found {actual}')
header=root/'Source/Core/Core/HW/EXI/SD2SP2SPI.h'
if header.exists():raise SystemExit('Model header already exists; refusing replacement')
subprocess.run(['git','-C',str(root),'apply','--check',str(here/'dolphin-sd2sp2.patch')],check=True)
subprocess.run(['git','-C',str(root),'apply',str(here/'dolphin-sd2sp2.patch')],check=True)
shutil.copyfile(here/'sd2sp2_spi.h',header)
print('Applied opt-in SD2SP2 test model. Rebuild Dolphin; no DOL files were changed.')
