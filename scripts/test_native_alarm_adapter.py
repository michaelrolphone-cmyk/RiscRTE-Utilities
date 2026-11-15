#!/usr/bin/env python3
"""Actual native Alarm/Countdown adapter with modeled hardware providers."""
import argparse,json,os,subprocess
from pathlib import Path
from build_badge_native_alarm_apps import ROOT,PROFILE,FLAGS,NATIVE_PRESENTATION_FLAGS,TIME,stage,sha,git,revision,presentation_profile
p=argparse.ArgumentParser(description=__doc__)
for n in ('runtime','system-apps','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--system-revision',type=revision,default=presentation_profile(True)['system_sha']);p.add_argument('--clock-edit-authority',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);system=a.system_apps.resolve();runtime=a.runtime.resolve()
inc,sdk=stage(system,runtime,out,a.system_revision,a.clock_edit_authority)
flags=[*FLAGS,*NATIVE_PRESENTATION_FLAGS,*(['-DALARM_SERVICE_CLOCK_EDIT_V2'] if a.clock_edit_authority else []),'-DTEST_ORDERED_TOUCH']
sources=[ROOT/'test/native_apps/native_alarm_geometry_adapter_test.c',system/'lib/PortableApps/src/adapter.c',*[system/'lib/PortableApps/src'/x for x in TIME]]
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
includes=['-I'+str(x) for x in (inc,ROOT/'lib/Alarm/include',runtime/'sdk/driver',system/'lib/NativeApps/include',system/'Apps')]
rows=[];dependencies={}
for kind,name in ((1,'alarms'),(2,'countdown')):
 for san in (False,True):
  extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  defs=[*flags,'-DDAILY_TEST_KIND='+str(kind),'-DNOVA_APP_ID='+str(4 if kind==1 else 5),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c')))]
  binary=out/f'{name}-{int(san)}';command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,*defs,*includes,*sources,catalog,'-Wl,--wrap=free','-o',binary]
  subprocess.run(command,check=True)
  for source in sources:
   dep=subprocess.check_output([command[0],'-std=c11','-M',*defs,*includes,source],text=True).replace('\\\n',' ')
   for word in dep.split()[1:]:
    f=Path(word).resolve()
    if f.is_file():dependencies[str(f)]=sha(f)
  for scenario in range(7):
   frames=out/f'{name}-{int(san)}-{scenario}';frames.mkdir(exist_ok=True)
   result=subprocess.run([binary,frames,str(scenario)],check=False,text=True,capture_output=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
   (frames/'run.log').write_text(result.stdout+result.stderr);result.check_returncode()
   print(result.stdout,end='');rows.append({'app':name,'sanitized':san,'scenario':scenario,'stdout':result.stdout,'command':list(map(str,command))})
receipt={'passed':True,'native_alarm_geometry':True,'source_revision':git(ROOT,'rev-parse','HEAD'),'source_dirty':bool(git(ROOT,'status','--porcelain')),'system_revision':a.system_revision,'runtime_revision':PROFILE['runtime_sha'],'sdk':sdk,'flags':flags,'results':rows,'compiled_dependencies_sha256':dependencies,'runner_sha256':sha(__file__),'physical_tested':False,'modeled':'466x466 hardware display with69px safe insets, input, realtime, storage and alarm service tables; actual app and shared adapter'}
(out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
