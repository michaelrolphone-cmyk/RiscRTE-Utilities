#!/usr/bin/env python3
"""Production apps and pinned adapter: Watch and retaining mono paper profiles."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--no-sanitizers',action='store_true');p.add_argument('--sanitizer',choices=['address,undefined','undefined'],default='address,undefined');a=p.parse_args();s=a.system_apps.resolve()
assert subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()=='2aa0cf63346e507525af884bbfbf6b69313442c4'
out=ROOT/'build/paper-utilities';out.mkdir(parents=True,exist_ok=True)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
for name,number in [('calculator',1),('stopwatch',2),('countdown',5),('alarms',4),('battery',3)]:
 for san in ([False] if a.no_sanitizers else [False,True]):
  flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_PAPER_UTILITIES','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_QUICK_ACTIONS','-DNOVA_APP_ID='+str(number),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c')))]
  if name=='battery':flags+=['-DPORTABLE_POWER_STATUS']
  if name!='stopwatch':flags+=['-D'+('CALCULATOR_RETURN_APP' if name=='calculator' else 'BATTERY_RETURN_APP' if name=='battery' else 'ALARM_RETURN_APP')+'="springboard.elf"']
  if san:flags+=['-fsanitize='+a.sanitizer,'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
  binary=out/(name+('-san' if san else ''))
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(d) for d in [s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'lib/Alarm/include',s/'Apps']],str(ROOT/'test/native_apps/paper_utility_test.c'),str(s/'lib/PortableApps/src/adapter.c'),str(catalog),*[str(s/'lib/PortableApps/src'/f) for f in ['quick_actions.c','quick_render.c','quick_session.c']],'-o',str(binary)],check=True)
  for profile in (0,1):
   for case in range(11 if name=='calculator' else 17 if name=='battery' else 17 if name=='alarms' else 12):
    if case in (8,10) and not profile:continue
    frames=out/f'{name}-{profile}-{case}-{int(san)}';frames.mkdir(exist_ok=True)
    subprocess.run([str(binary),str(frames),str(case),str(profile)],check=True,timeout=60)
print('All paper and Watch production adapter scenarios passed')
