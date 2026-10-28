#!/usr/bin/env python3
"""Owning native controllers and selected generic adapter; modeled providers."""
import argparse,json,os,shutil,subprocess,sys
from pathlib import Path
from build_badge_native_alarm_apps import ROOT,PROFILE,FLAGS,TIME,stage,sha,git,revision,NATIVE_PRESENTATION_FLAGS,NATIVE_PROFILE_PATH,presentation_profile
p=argparse.ArgumentParser(description=__doc__)
for name in ['runtime','system-apps','output']:p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--clock-edit-authority',action='store_true')
p.add_argument('--native-alarm-presentation',action='store_true',help='Add native geometry harness; all historical generic lanes remain unchanged')
p.add_argument('--foreground-alarm-sleep',action='store_true',help='Add modeled returning-Light adapter lanes; legacy 68 lanes always run')
p.add_argument('--system-revision',type=revision)
a=p.parse_args();a.system_revision=a.system_revision or presentation_profile(a.native_alarm_presentation)['system_sha'];runtime=a.runtime.resolve();system=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
FLAGS=[*FLAGS,*(['-DALARM_SERVICE_CLOCK_EDIT_V2'] if a.clock_edit_authority else [])]
inc,sdk=stage(system,runtime,out,a.system_revision,a.clock_edit_authority);sources=[system/'lib/PortableApps/src'/n for n in TIME]
common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
includes=['-I'+str(x) for x in [inc,ROOT/'lib/Alarm/include',runtime/'sdk/driver',system/'lib/NativeApps/include',system/'Apps']]
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
runs=[];dependencies={}
for kind,name in [(1,'alarms'),(2,'countdown')]:
 for san in [False,True]:
  sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
  binary=out/f'{name}-controller-{int(san)}'
  subprocess.run([*common,*sf,*(['-DALARM_SERVICE_CLOCK_EDIT_V2'] if a.clock_edit_authority else []),'-DDAILY_ALARM_KIND='+str(kind),*includes,ROOT/'test/native_apps/native_utc_alarm_app_test.c',*sources,'-o',binary],check=True)
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
native_geometry_evidence=None
if a.native_alarm_presentation:
 native_geometry_evidence=[]
 for runner,folder in [('tests_native_alarm_geometry.py','native-geometry'),('test_native_alarm_adapter.py','native-adapter')]:
  native_out=out/folder
  native_command=[sys.executable,str(ROOT/'scripts'/runner),
   '--runtime',str(runtime),'--system-apps',str(system),'--system-revision',a.system_revision,
   '--output',str(native_out),*(['--clock-edit-authority'] if a.clock_edit_authority else [])]
  subprocess.run(native_command,check=True)
  item={'command':native_command,'output_directory':str(native_out),
   'receipts_sha256':{str(f.relative_to(native_out)):sha(f) for f in native_out.rglob('*.json')},
   'runner_sha256':sha(ROOT/'scripts'/runner)}
  if not item['receipts_sha256']:raise ValueError('Native geometry runner produced no evidence receipt: '+runner)
  native_geometry_evidence.append(item)
receipt={'schema':1,'profile':presentation_profile(a.native_alarm_presentation)['profile'],'native_alarm_geometry':a.native_alarm_presentation,'native_geometry_evidence':native_geometry_evidence,'selected_presentation_profile_sha256':sha(NATIVE_PROFILE_PATH) if a.native_alarm_presentation else None,'native_presentation_flags':[*FLAGS,*NATIVE_PRESENTATION_FLAGS] if a.native_alarm_presentation else [],'source_sha':git(ROOT,'rev-parse','HEAD'),'source_dirty':bool(git(ROOT,'status','--porcelain')),'system_sha':a.system_revision,'runtime_sha':PROFILE['runtime_sha'],'sdk':sdk,'flags':FLAGS,'foreground_alarm_lane':a.foreground_alarm_sleep,'foreground_flags':foreground_flags if a.foreground_alarm_sleep else [],'runs':runs,'excluded_scenarios':{'15':'historical paper QuickActions swipe/panel test; this generic color profile selects neither paper nor QuickActions'},'compiled_dependencies_sha256':dependencies,'source_sha256':{str(f):sha(f) for f in [Path(__file__),ROOT/'scripts/build_badge_native_alarm_apps.py',ROOT/'test/native_apps/native_utc_alarm_app_test.c',ROOT/'test/native_apps/native_utc_alarm_adapter_test.c',*sources]},'physical_tested':False,'sleeping_alarm_integration':False,'modeled':'runtime/provider tables, native time and storage medium; actual app controllers and shared adapter'}
(out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Badge native alarm controllers and exact selected adapter: normal+ASan/UBSan PASS; no physical/sleeping alarm claim')
