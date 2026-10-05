#!/usr/bin/env python3
"""Run one utility through the real shared adapter, not a replacement renderer."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--app',required=True,choices=['calculator','stopwatch','battery','alarms','countdown']);a=p.parse_args();s=a.system_apps.resolve();name=a.app
number={'calculator':1,'stopwatch':2,'battery':3,'alarms':4,'countdown':5}[name]
out=ROOT/'build/nova'/name;out.mkdir(parents=True,exist_ok=True);catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
owner='CALCULATOR_RETURN_APP' if name=='calculator' else 'ALARM_RETURN_APP' if name in ('alarms','countdown') else 'PORTABLE_RETURN_APP'
fixture=ROOT/'test/native_apps/calculator_nova_test.c' if name=='calculator' else s/'test/native_apps/nova_apps_test.c'
for san in (False,True):
 flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_APP_SLEEP_LOCAL','-D'+owner+'="springboard.elf"','-DNOVA_APP_ID='+str(number),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c')))]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 binary=out/f'test-{int(san)}';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(d) for d in (s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'lib/Alarm/include',s/'test/native_apps')],str(fixture),str(s/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(binary)],check=True,timeout=120)
 for case in range(7 if name=='calculator' else 2):
  frames=out/f'frames-{int(san)}-{case}';frames.mkdir(exist_ok=True);subprocess.run([str(binary),str(frames),str(case)],check=True,timeout=60)
