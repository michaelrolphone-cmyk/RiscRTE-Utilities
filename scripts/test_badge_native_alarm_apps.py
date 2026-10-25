#!/usr/bin/env python3
"""Owning native controllers and selected generic adapter; modeled providers."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
from build_badge_native_alarm_apps import ROOT,PROFILE,FLAGS,TIME,stage,sha,git,revision
p=argparse.ArgumentParser(description=__doc__)
for name in ['runtime','system-apps','output']:p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--foreground-alarm-sleep',action='store_true',help='Add modeled returning-Light adapter lanes; legacy 68 lanes always run')
p.add_argument('--system-revision',type=revision,default=PROFILE['system_sha'])
a=p.parse_args();runtime=a.runtime.resolve();system=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
inc,sdk=stage(system,runtime,out,a.system_revision);sources=[system/'lib/PortableApps/src'/n for n in TIME]
common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
includes=['-I'+str(x) for x in [inc,ROOT/'lib/Alarm/include',runtime/'sdk/driver',system/'lib/NativeApps/include',system/'Apps']]
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
runs=[];dependencies={}
for kind,name in [(1,'alarms'),(2,'countdown')]:
 for san in [False,True]:
  sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
  binary=out/f'{name}-controller-{int(san)}'
  subprocess.run([*common,*sf,'-DDAILY_ALARM_KIND='+str(kind),*includes,ROOT/'test/native_apps/native_utc_alarm_app_test.c',*sources,'-o',binary],check=True)
  subprocess.run([binary],check=True,env=env);runs.append({'app':name,'controller':True,'sanitized':san})
  binary=out/f'{name}-adapter-{int(san)}'
  subprocess.run([*common,*sf,*FLAGS,'-DTEST_ORDERED_TOUCH','-DNOVA_APP_ID='+str(4 if kind==1 else 5),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c'))),*includes,ROOT/'test/native_apps/native_utc_alarm_adapter_test.c',system/'lib/PortableApps/src/adapter.c',*sources,catalog,'-Wl,--wrap=free','-o',binary],check=True)
  for source in [ROOT/'test/native_apps/native_utc_alarm_app_test.c',ROOT/'test/native_apps/native_utc_alarm_adapter_test.c',system/'lib/PortableApps/src/adapter.c',*sources]:
   command=[common[0],'-std=c11','-M',*FLAGS,'-DTEST_ORDERED_TOUCH','-DDAILY_ALARM_KIND='+str(kind),'-DNOVA_APP_ID='+str(4 if kind==1 else 5),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c'))),*includes,source]
   for word in subprocess.check_output(command,text=True).replace('\\\n',' ').split()[1:]:
    path=Path(word).resolve()
    if path.is_file():dependencies[str(path)]=sha(path)
  for scenario in list(range(15))+[16]:
   frames=out/f'{name}-{scenario}-{int(san)}';frames.mkdir(exist_ok=True)
   subprocess.run([binary,frames,str(scenario),'0'],check=True,timeout=60,env=env)
   runs.append({'app':name,'adapter':True,'scenario':scenario,'sanitized':san})
# Additive opt-in foreground lane; all historical 68 runs above stay intact.
foreground_flags=[*FLAGS,'-DPORTABLE_FOREGROUND_IDLE','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_FOREGROUND_ALARM_SLEEP']
for kind,name in ([(1,'alarms'),(2,'countdown')] if a.foreground_alarm_sleep else []):
 for san in (False,True):
  sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  binary=out/f'{name}-foreground-{int(san)}'
  command=[*common,*sf,*foreground_flags,'-DTEST_ORDERED_TOUCH','-DNOVA_APP_ID='+str(4 if kind==1 else 5),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c'))),*includes,ROOT/'test/native_apps/native_alarm_foreground_test.c',system/'lib/PortableApps/src/adapter.c',*sources,catalog,'-Wl,--wrap=free','-o',binary]
  subprocess.run(command,check=True)
  dep_command=[common[0],'-std=c11','-M',*foreground_flags,'-DTEST_ORDERED_TOUCH','-DNOVA_APP_ID='+str(4 if kind==1 else 5),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c'))),*includes,ROOT/'test/native_apps/native_alarm_foreground_test.c']
  for dep_source in (ROOT/'test/native_apps/native_alarm_foreground_test.c',system/'lib/PortableApps/src/adapter.c'):
   for word in subprocess.check_output([*dep_command[:-1],dep_source],text=True).replace('\\\n',' ').split()[1:]:
    path=Path(word).resolve()
    if path.is_file():dependencies[str(path)]=sha(path)
  for scenario in (0,1):
   frames=out/f'{name}-foreground-{scenario}-{int(san)}';frames.mkdir(exist_ok=True)
   subprocess.run([binary,frames,str(scenario)],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
   runs.append({'app':name,'foreground_adapter':True,'scenario':scenario,'sanitized':san,'helper':'modeled returning Light'})
receipt={'schema':1,'profile':PROFILE['profile'],'source_sha':git(ROOT,'rev-parse','HEAD'),'source_dirty':bool(git(ROOT,'status','--porcelain')),'system_sha':a.system_revision,'runtime_sha':PROFILE['runtime_sha'],'sdk':sdk,'flags':FLAGS,'foreground_alarm_lane':a.foreground_alarm_sleep,'foreground_flags':foreground_flags if a.foreground_alarm_sleep else [],'runs':runs,'excluded_scenarios':{'15':'historical paper QuickActions swipe/panel test; this generic color profile selects neither paper nor QuickActions'},'compiled_dependencies_sha256':dependencies,'source_sha256':{str(f):sha(f) for f in [Path(__file__),ROOT/'scripts/build_badge_native_alarm_apps.py',ROOT/'test/native_apps/native_utc_alarm_app_test.c',ROOT/'test/native_apps/native_utc_alarm_adapter_test.c',*sources]},'physical_tested':False,'sleeping_alarm_integration':False,'modeled':'runtime/provider tables, native time and storage medium; actual app controllers and shared adapter'}
(out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Badge native alarm controllers and exact selected adapter: normal+ASan/UBSan PASS; no physical/sleeping alarm claim')
