#!/usr/bin/env python3
"""Run production BLE host/model fixtures without touching a radio."""
import os,subprocess,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path);a=p.parse_args()
out=ROOT/'build/ble-scanner';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 exe=out/f'core-{int(san)}'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,'-I'+str(ROOT/'Apps'),str(ROOT/'tests/ble_scan_test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

if a.system_apps:
 for san in (False,True):
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  exe=out/f'app-{int(san)}'
  includes=[ROOT/'Apps',a.system_apps/'Apps',ROOT/'lib/Bluetooth/include',a.system_apps/'lib/PortableApps/include',a.system_apps/'lib/NativeApps/include']
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(p) for p in includes],str(ROOT/'tests/ble_app_test.c'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))

 exe=out/'render';frames=out/'frames';frames.mkdir(exist_ok=True)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-variable','-DBLE_RENDER',*['-I'+str(p) for p in includes],'-I'+str(a.system_apps/'lib/PortableApps/src'),str(ROOT/'tests/ble_app_test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,BLE_FRAME_DIR=str(frames)))
