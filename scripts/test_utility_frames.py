#!/usr/bin/env python3
"""Production Utilities busy-frame tests, with I/O-only host doubles."""
import argparse, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();system=a.system_apps.resolve();a.output.mkdir(parents=True,exist_ok=True)
includes=[ROOT/'Apps',ROOT/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include',system/'lib/PortableApps/src',system/'Apps']
for san in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 for name,source,extra in [('buttons',ROOT/'tests/utility_hid_frame_test.c',['-DHID_TOUCHPAD=0']),('touchpad',ROOT/'tests/utility_hid_frame_test.c',['-DHID_TOUCHPAD=1']),('spectrum',ROOT/'test/native_apps/utility_spectrum_frame_test.c',[])]:
  exe=a.output/(name+('-san' if san else ''))
  cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*extra,*['-I'+str(i) for i in includes],str(source),'-lm','-o',str(exe)]
  subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
