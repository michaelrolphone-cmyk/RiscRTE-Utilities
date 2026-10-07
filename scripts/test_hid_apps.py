#!/usr/bin/env python3
"""Real HID app, pure gestures/persistence, sanitizers and actual Nova rasterizer."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);a=p.parse_args();system=a.system_apps.resolve()
out=ROOT/'build/hid-apps';out.mkdir(parents=True,exist_ok=True)
includes=[ROOT/'Apps',ROOT/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include',system/'lib/PortableApps/src']
base=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*['-I'+str(i) for i in includes]]
for san in (False,True):
 flags=['-fsanitize='+os.environ.get('HID_SANITIZERS','address,undefined'),'-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if san else []
 for name,source,extra in [('model','hid_model_test.c',[]),('touchpad','hid_app_test.c',['-DHID_TOUCHPAD=1']),('buttons','hid_app_test.c',['-DHID_TOUCHPAD=0'])]:
  exe=out/f'{name}-{int(san)}';subprocess.run([*base,*flags,*extra,str(ROOT/'tests'/source),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
for pad in (0,1):
 exe=out/f'render-{pad}';frames=out/('touchpad-frames' if pad else 'buttons-frames');frames.mkdir(exist_ok=True)
 subprocess.run([*base,'-Wno-unused-variable','-DHID_RENDER',f'-DHID_TOUCHPAD={pad}',str(ROOT/'tests/hid_app_test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,HID_FRAME_DIR=str(frames)))
print('HID app/model normal + '+os.environ.get('HID_SANITIZERS','address,undefined')+' and Nova frames passed')
