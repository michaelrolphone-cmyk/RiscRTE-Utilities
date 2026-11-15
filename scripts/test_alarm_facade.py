#!/usr/bin/env python3
"""Production facade forwarding, unchanged owner coexistence, normal/sanitized."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
inc=[ROOT/'lib/Alarm/include',a.runtime/'sdk/driver',a.runtime/'sdk/app',a.system_apps/'lib/PortableApps/include']
rows=[]
for sanitized in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 for profile,define in [('legacy-clock-change','ALARM_FACADE_LEGACY_CLOCK_CHANGE'),('tagged-owner','ALARM_FACADE_TAGGED_OWNER')]:
  prefix=a.output/(profile+('-san' if sanitized else ''))
  common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-D'+define,*['-I'+str(i) for i in inc]]
  obj=prefix.with_suffix('.o')
  subprocess.run([*common,'-Dt5_driver_get=alarm_facade_driver_get','-c',str(ROOT/'Services/alarm_facade/facade.c'),'-o',str(obj)],check=True)
  cases=['alarm_facade_test.c']+(['alarm_facade_legacy_owner_test.c'] if profile=='legacy-clock-change' else [])
  for test in cases:
   target=Path(str(prefix)+'-'+test.removesuffix('.c'))
   command=[*common,str(ROOT/'test/native_apps'/test),str(obj),'-o',str(target)]
   subprocess.run(command,check=True)
   result=subprocess.run([str(target)],check=True,capture_output=True,text=True,timeout=60,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1'))
   print(result.stdout,end='');rows.append({'profile':profile,'sanitized':sanitized,'test':test,'command':command,'output':result.stdout})
sources=[ROOT/'Services/alarm_facade/facade.c',ROOT/'scripts/test_alarm_facade.py',
 ROOT/'test/native_apps/alarm_facade_test.c',ROOT/'test/native_apps/alarm_facade_legacy_owner_test.c',
 ROOT/'test/native_apps/alarm_service_test.c',ROOT/'Services/alarm_service/service.c',*sorted((ROOT/'lib/Alarm/include').glob('*.h'))]
receipt={'schema':1,'tests':rows,'sources':{str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in sources},'owner_implementation_changed':False,'hardware_tested':False}
(a.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
