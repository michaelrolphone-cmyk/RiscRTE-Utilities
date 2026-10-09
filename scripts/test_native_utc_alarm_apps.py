#!/usr/bin/env python3
"""Native controller tests against pinned pure System helpers; no target claim."""
import argparse,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
from build_native_utc_alarm_apps import exact,SYSTEM,RUNTIME
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args();s=a.system_apps.resolve();r=a.runtime.resolve()
exact(s,SYSTEM);exact(r,RUNTIME)
out=ROOT/'build/native-utc-app-tests';out.mkdir(parents=True,exist_ok=True)
shutil.copytree(s/'lib/PortableApps/include',out/'include',dirs_exist_ok=True);shutil.copytree(s/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):shutil.copy2(r/'sdk/app'/name,out/'include'/name)
for name in ('PortableNativeCustody.h','PortableNativeTimeToolbar.h'):shutil.copy2(ROOT/'test/native_apps/native_utc_stubs'/name,out/'include'/name)
sources=[s/'lib/PortableApps/src'/name for name in ('PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')]
for kind in (1,2):
 for san in (False,True):
  binary=out/f'controller-{kind}-{int(san)}';flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-DDAILY_ALARM_KIND='+str(kind),*['-I'+str(x) for x in (out/'include',ROOT/'lib/Alarm/include',r/'sdk/driver',s/'lib/NativeApps/include')],ROOT/'test/native_apps/native_utc_alarm_app_test.c',*sources,'-o',binary],check=True)
  env=os.environ.copy()
  if san:env['ASAN_OPTIONS']='detect_leaks=0' # Container ptrace blocks LeakSanitizer; ASan/UBSan remain enabled.
  subprocess.run([binary],check=True,env=env)
print('Native Alarms/Countdown actual controllers passed normal and ASan/UBSan; adapter retention is a test double here')
