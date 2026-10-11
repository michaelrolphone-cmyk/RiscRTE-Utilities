#!/usr/bin/env python3
import argparse,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
out=root/'build/stopwatch-hours';out.mkdir(parents=True,exist_ok=True)
for selected in (False,True):
 for sanitized in (False,True):
  flags=['-DPORTABLE_UNPADDED_HOURS'] if selected else []
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  for source in ('tests/stopwatch_core_test.c','test/native_apps/stopwatch_test.c','test/native_apps/stopwatch_hour_test.c','test/native_apps/stopwatch_retained_test.c'):
   binary=out/(Path(source).stem+'-'+str(int(selected))+'-'+str(int(sanitized)))
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/NativeApps/include'),'-I'+str(a.system_apps/'lib/PortableApps/include'),str(root/source),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Stopwatch legacy/selected core, real draw strings, storage/controller and retained cleanup pass normal+ASan/UBSan')
