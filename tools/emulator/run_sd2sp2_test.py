#!/usr/bin/env python3
"""Run an unchanged DOL in our patched Dolphin using an isolated profile.
Requires Linux, Xvfb, Pillow and a user-supplied GBA BIOS; bundles no BIOS.
"""
import argparse
import hashlib
import os
import random
import subprocess
import time
from pathlib import Path
from PIL import ImageGrab
p=argparse.ArgumentParser()
for name in ('dolphin','dol','image','bios','out'):p.add_argument('--'+name,required=True,type=Path)
a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=False)
profile=a.out/'profile';(profile/'Config').mkdir(parents=True);(profile/'Pipes').mkdir()
pipe=profile/'Pipes/cubedeck';os.mkfifo(pipe)
(profile/'Config/Dolphin.ini').write_text(f'''[Core]
SIDevice0 = 6
SIDevice1 = 13
SIDevice2 = 0
SIDevice3 = 0
SlotA = 255
SlotB = 255
SerialPort1 = 255
CPUThread = False
[Interface]
ConfirmStop = False
UsePanicHandlers = False
[Display]
RenderToMain = False
[Input]
BackgroundInput = True
[DSP]
Backend = NoAudio
[GBA]
BIOS = {a.bios.resolve()}
Rom2 =
Threads = False
SavesInRomPath = False
[Analytics]
Enabled = False
PermissionAsked = True
''')
(profile/'Config/GCPadNew.ini').write_text('[GCPad1]\nDevice = Pipe/0/cubedeck\nButtons/A = `Button A`\n')
(profile/'Config/Logger.ini').write_text('[Options]\nWriteToFile = True\nVerbosity = 4\n[Logs]\nBOOT = True\nCORE = True\nEXI = True\nSI = True\n')
env=os.environ.copy();display=random.randrange(100,1000)
env.update(DISPLAY=f'127.0.0.1:{display}',CUBEDECK_SD2SP2_IMAGE=str(a.image.resolve()),QT_X11_NO_MITSHM='1',LIBGL_ALWAYS_SOFTWARE='1')
def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()
image_before=digest(a.image);dol_before=digest(a.dol)
x=d=None
try:
    with (a.out/'xvfb.log').open('w') as xlog,(a.out/'dolphin.log').open('w') as log:
        x=subprocess.Popen(['Xvfb',f':{display}','-screen','0','1280x1024x24','-nolisten','unix','-listen','tcp','-ac'],env=env,stdout=xlog,stderr=xlog)
        time.sleep(1)
        if x.poll() is not None:raise RuntimeError('Xvfb failed')
        d=subprocess.Popen([str(a.dolphin.resolve()),'-u',str(profile.resolve()),'-e',str(a.dol.resolve()),'-b'],env=env,stdout=log,stderr=log)
        time.sleep(15)
        if d.poll() is not None:raise RuntimeError('Dolphin exited before input')
        fd=os.open(pipe,os.O_RDWR|os.O_NONBLOCK)
        try:
            os.write(fd,b'PRESS A\n');time.sleep(0.2);os.write(fd,b'RELEASE A\n')
        finally:os.close(fd)
        time.sleep(12)
        ImageGrab.grab(xdisplay=env['DISPLAY']).save(a.out/'result.png')
        if d.poll() is not None:raise RuntimeError('Dolphin exited during test')
        print('Process remained running; inspect result.png and EXI logs for actual PASS/failure.')
finally:
    for process in (d,x):
        if process:
            process.terminate()
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait()
assert digest(a.image)==image_before,'Synthetic image changed'
assert digest(a.dol)==dol_before,'DOL changed'
(a.out/'checksums.txt').write_text(f'{image_before}  SD image (unchanged)\n{dol_before}  DOL (unchanged)\n')
print('SD image and DOL checksums unchanged.')
