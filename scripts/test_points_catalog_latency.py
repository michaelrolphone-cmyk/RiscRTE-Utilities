#!/usr/bin/env python3
"""Actual Points service slow-scan, due crossing and fresh-clock failures."""
import argparse,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='points-latency-') as directory:
 for native in (False,True):
  for sanitize in (False,True):
   flags=['-DALARM_NATIVE_UTC'] if native else ['-DPORTABLE_RTC_UTC8_DENVER']
   if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   sources=[root/'test/native_apps/points_catalog_sleep_latency_test.c']
   if native:sources += [a.system_apps/'lib/PortableApps/src/PortableTimeZone.c',a.system_apps/'lib/PortableApps/src/PortableTimeZoneCatalog.c']
   binary=Path(directory)/f'latency-{native}-{sanitize}'
   subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
       *['-I'+str(i) for i in [root/'lib/Alarm/include',a.runtime/'sdk/app',a.runtime/'sdk/driver',a.system_apps/'lib/PortableApps/include']],
       *map(str,sources),'-o',str(binary)],check=True)
   for scene in range(6):subprocess.run([str(binary),str(scene)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('24 slow-scan/sleep boundary cases PASS')
