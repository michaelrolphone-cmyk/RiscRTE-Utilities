#!/usr/bin/env python3
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 for baseline in (True,False):
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  if baseline:flags+=['-DTEST_BASELINE']
  target=a.output/f'color-{int(baseline)}-{int(san)}';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(x) for x in (r/'lib/Alarm/include',a.runtime/'sdk/driver',a.runtime/'sdk/app',a.system_apps/'lib/PortableApps/include')],str(r/'test/native_apps/alarm_color_startup_busy_test.c'),'-o',str(target)],check=True)
  subprocess.run([str(target)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 # The legacy fixture above selects ALARM_STARTUP_BUSY_RETRY. Qualify the
 # shipped 0.4.12 ALARM_RECONCILE_BUSY_RETRY profile before its first RTC anchor
 # separately; the existing anchored sleep fixture cannot establish this.
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 target=a.output/f'color-cold-reconcile-{int(san)}'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(x) for x in (r/'lib/Alarm/include',a.runtime/'sdk/driver',a.runtime/'sdk/app',a.system_apps/'lib/PortableApps/include')],str(r/'test/native_apps/alarm_color_cold_start_test.c'),'-o',str(target)],check=True)
 subprocess.run([str(target)],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Published color profile failure reproduced; startup fix normal/ASan/UBSan PASS')
print('Exact shipped color 0.4.12 cold-start reconciliation normal/ASan/UBSan PASS')
