#!/usr/bin/env python3
"""Exercise exact color service and shared Watch sleep without physical I/O."""
from pathlib import Path
import argparse, os, subprocess
p=argparse.ArgumentParser()
for name in ('runtime','system-apps','watch','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
includes=[root/'lib/Alarm/include',a.watch,a.watch/'include',a.watch/'sdk/driver',a.watch/'sdk/app',a.runtime/'sdk/driver',a.runtime/'sdk/app',a.system_apps/'lib/PortableApps/include']
for sanitize in (False,True):
    for profile in ('wave-baseline','watch-baseline','fixed'):
        flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
        if profile!='fixed':flags+=['-DTEST_BASELINE']
        if profile=='watch-baseline':flags+=['-DTEST_WATCH_BASELINE']
        target=a.output/f'sleep-{profile}-{int(sanitize)}'
        command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,*['-I'+str(x.resolve()) for x in includes],str(root/'test/native_apps/alarm_color_sleep_busy_test.c'),str(root/'test/native_apps/alarm_color_sleep_policy.c'),'-o',str(target)]
        subprocess.run(command,check=True)
        subprocess.run([str(target)],check=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Exact sleep alarm -4 reproduction, all read phases, bounded wait/recovery and hard-failure fencing PASS')

# The changed profile also runs the existing RTC-edit, deadline/resume and Points
# output/persistence fixtures. They exercise the actual provider in each TU.
for sanitize in (False,True):
    for name,defs in [('alarm_clock_change_test',[]),('alarm_sleep_resume_test',['POINTS_IN_TIME_SERVICE','PORTABLE_RTC_UTC8_DENVER','ALARM_VOLUME_CONTROL','ALARM_DND_CONTROL','ALARM_CLOCK_CHANGE_CONTROL']),('points_service_test',['ALARM_VOLUME_CONTROL','ALARM_CLOCK_CHANGE_CONTROL'])]:
        target=a.output/(name+('-san' if sanitize else '-plain'))
        flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
        command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-DALARM_RECONCILE_BUSY_RETRY',*['-D'+x for x in defs],*['-I'+str(x.resolve()) for x in includes],str(root/'test/native_apps'/(name+'.c')),'-o',str(target)]
        subprocess.run(command,check=True)
        subprocess.run([str(target)],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Changed profile RTC edits, sleep resume/deadlines, Points outputs/persistence normal/ASan/UBSan PASS')
