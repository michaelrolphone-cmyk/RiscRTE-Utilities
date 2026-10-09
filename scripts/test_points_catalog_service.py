#!/usr/bin/env python3
"""Production selected Points provider, both domains, normal and ASan/UBSan."""
import argparse,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
out=root/'build/points-catalog-service';out.mkdir(parents=True,exist_ok=True)
for native in (False,True):
 for sanitized in (False,True):
  flags=['-DALARM_NATIVE_UTC'] if native else ['-DPORTABLE_RTC_UTC8_DENVER']
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
  sources=[root/'test/native_apps/points_catalog_service_test.c']
  if native:sources += [a.system_apps/'lib/PortableApps/src/PortableTimeZone.c',a.system_apps/'lib/PortableApps/src/PortableTimeZoneCatalog.c']
  target=out/f'test-{int(native)}-{int(sanitized)}'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
    *['-I'+str(i) for i in [root/'lib/Alarm/include',a.runtime/'sdk/app',a.runtime/'sdk/driver',a.system_apps/'lib/PortableApps/include']],
    *map(str,sources),'-o',str(target)],check=True)
  for retained in range(5):subprocess.run([str(target),str(retained)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('Selected Points catalog Watch/X4 production service normal + ASan/UBSan PASS')
