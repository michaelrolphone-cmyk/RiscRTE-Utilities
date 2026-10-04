#!/usr/bin/env python3
"""Exercise actual app/provider source. No hardware qualification claim."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
out=ROOT/'build/alarm';out.mkdir(parents=True,exist_ok=True)
includes=[ROOT/'lib/Alarm/include',a.runtime/'sdk/driver',a.system_apps/'lib/PortableApps/include',a.system_apps/'lib/NativeApps/include']
fixtures=[('tests/alarm_records_test.c',[]),('test/native_apps/alarm_service_test.c',[]),('test/native_apps/alarm_service_regressions.c',[]),('test/native_apps/alarm_app_test.c',['-DDAILY_ALARM_KIND=1']),('test/native_apps/alarm_app_test.c',['-DDAILY_ALARM_KIND=2']),('test/native_apps/alarm_app_test.c',['-DDAILY_ALARM_KIND=1','-DPORTABLE_RTC_UTC8_DENVER'])]
for index,(fixture,defs) in enumerate(fixtures):
 for sanitized in (False,True):
  target=out/f'fixture-{index}-{int(sanitized)}'
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitized else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*defs,*['-I'+str(x) for x in includes],str(ROOT/fixture),'-o',str(target)],check=True,timeout=120)
  subprocess.run([str(target)],check=True,timeout=120)
print('Alarm/Countdown production-source normal and ASan/UBSan fixtures passed')
