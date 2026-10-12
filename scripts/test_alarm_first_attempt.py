#!/usr/bin/env python3
"""Qualify explicit Watch RTC edits and X4 no-I/O storage BUSY recovery."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path);a=p.parse_args()
out=a.output or ROOT/'build/alarm-first-attempt';out.mkdir(parents=True,exist_ok=True)
includes=[ROOT/'lib/Alarm/include',a.runtime/'sdk/driver',a.runtime/'sdk/app',a.system_apps/'lib/PortableApps/include']
cases=[('first-boot','points_catalog_first_boot_test.c',[],['PortableTimeZone.c','PortableTimeZoneCatalog.c'],[[]]),
       ('clock-change','alarm_clock_change_test.c',[],[],[[]]),
       ('startup-busy','alarm_startup_busy_test.c',[],['PortableTimeZone.c','PortableTimeZoneCatalog.c'],[[]]),
       ('catalog-profile','points_catalog_service_test.c',['-DALARM_NATIVE_UTC','-DALARM_STORAGE_BUSY_RETRY'],['PortableTimeZone.c','PortableTimeZoneCatalog.c'],[[str(i)] for i in range(8)]),
       ('watch-points-sleep','alarm_sleep_resume_test.c',['-DPOINTS_IN_TIME_SERVICE','-DPORTABLE_RTC_UTC8_DENVER','-DALARM_VOLUME_CONTROL','-DALARM_DND_CONTROL','-DALARM_CLOCK_CHANGE_CONTROL'],[],[[]]),
       ('watch-points','points_service_test.c',['-DALARM_VOLUME_CONTROL','-DALARM_CLOCK_CHANGE_CONTROL'],[],[[]])]
for sanitized in (False,True):
 for name,test,defines,extra,args in cases:
  target=out/(name+('-san' if sanitized else ''))
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*defines,*['-I'+str(i) for i in includes],str(ROOT/'test/native_apps'/test),*[str(a.system_apps/'lib/PortableApps/src'/f) for f in extra],'-o',str(target)]
  subprocess.run(command,check=True)
  for arguments in args:subprocess.run([str(target),*arguments],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=60)
print('First-attempt alarm: normal and ASan/UBSan clock-change, startup BUSY, full catalog fault, Points and sleep regressions PASS')
