#!/usr/bin/env python3
"""Selected actual utility controllers and real adapter from target receipts."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
out=ROOT/'build/resident-clients';out.mkdir(parents=True,exist_ok=True);runs=[]
for name,number in [('calculator',1),('stopwatch',2),('battery',3),('alarms',4),('countdown',5)]:
 r=json.loads((a.build/name/'x4-native-app.json').read_text());cmd=r['compile_command'];flags=[s for s in cmd if s.startswith(('-D','-I'))]
 src=[s for s in cmd if s.endswith('.c') and Path(s).name!=name+'.c']
 target=out/(name+('-san' if a.sanitize else ''))
 san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
 command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,*flags,'-DTEST_RESIDENT_CLIENT','-DNOVA_APP_ID='+str(number),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c'))),ROOT/'test/native_apps'/('native_utility_controller_test.c' if number<=3 else 'native_utc_alarm_adapter_test.c'),*src,'-Wl,--wrap=free','-o',target]
 subprocess.run(list(map(str,command)),check=True)
 for case in range(20 if number>=4 else 25 if number==2 else 8):
  frames=out/(name+'-frames-'+str(case));frames.mkdir(exist_ok=True)
  arguments=[str(target),*([str(frames)] if number>=4 else []),str(case),'1']
  subprocess.run(arguments,check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  runs.append({'app':name,'case':case,'sanitized':a.sanitize})
 if number>=4:
  hours=out/(name+'-hours'+('-san' if a.sanitize else ''))
  hours_command=[str(x).replace('native_utc_alarm_adapter_test.c','resident_alarm_hours_test.c') for x in command]
  hours_command[hours_command.index('-o')+1]=str(hours)
  subprocess.run(hours_command,check=True)
  subprocess.run([str(hours)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  runs.append({'app':name,'case':'unpadded-hours','sanitized':a.sanitize})
(out/('test-evidence-san.json' if a.sanitize else 'test-evidence.json')).write_text(json.dumps(runs,indent=2)+'\n')
