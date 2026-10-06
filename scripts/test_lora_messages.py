#!/usr/bin/env python3
"""Production model/controller fixtures, normal and sanitizers; no hardware."""
import argparse, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);a=p.parse_args()
system=a.system_apps.resolve();out=ROOT/'build/lora-tests';out.mkdir(parents=True,exist_ok=True)
for fixture in ('tests/lora_messages_model_test.c','test/native_apps/lora_messages_test.c'):
 for sanitized in (False,True):
  target=out/(Path(fixture).stem+('-san' if sanitized else ''))
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(system/i) for i in ('lib/PortableApps/include','lib/NativeApps/include')],str(ROOT/fixture),'-o',str(target)],check=True)
  subprocess.run([str(target)],check=True,timeout=60)
print('LoRa model and controller passed normal + ASan/UBSan')
