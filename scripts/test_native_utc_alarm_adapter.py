#!/usr/bin/env python3
"""Local-only actual app/adapter composition. System sources are linked in place."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
from build_native_utc_alarm_apps import exact,SYSTEM,ADAPTER,RUNTIME
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--adapter',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--normal-only',action='store_true');a=p.parse_args();system=a.system_apps.resolve();adapter=a.adapter.resolve();runtime=a.runtime.resolve()
for repo,pin in [(system,SYSTEM),(adapter,ADAPTER),(runtime,RUNTIME)]:exact(repo,pin)
out=ROOT/'build/native-utc-adapter';out.mkdir(parents=True,exist_ok=True)
flags=['-DALARM_NATIVE_UTC','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_NOVA_UI','-DPORTABLE_PAPER_UTILITIES','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_APP_LAUNCH_GUARD','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DALARM_RETURN_APP="springboard.elf"','-DPORTABLE_QUICK_ACTIONS']
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
used=[adapter/'lib/PortableApps/src'/name for name in ['adapter.c','quick_actions.c','quick_render.c','quick_session.c']]+[system/'lib/PortableApps/src'/name for name in ['PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']]
receipt={'source_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'app_sources':{str(path.relative_to(ROOT)):digest(path) for path in (ROOT/'Apps').glob('*') if path.is_file() and path.name.startswith(('alarm','countdown','utility_paper'))},'system_time_ref':SYSTEM,'adapter_ref':subprocess.check_output(['git','rev-parse','HEAD'],cwd=adapter,text=True).strip(),'runtime_ref':subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip(),'profile':'native-utc-app-local-composition','publication':'blocked on separately owned System source clearance','hardware':False,'sources':{str(path):digest(path) for path in used},'runs':[]}
with tempfile.TemporaryDirectory(prefix='native-alarm-adapter-') as tmp:
 stage=Path(tmp);include=stage/'include';include.mkdir()
 for path in (adapter/'lib/PortableApps/include').iterdir():
  if path.is_file():(include/path.name).symlink_to(path)
 for path in (system/'lib/PortableApps/time').rglob('*'):
  destination=stage/'time'/path.relative_to(system/'lib/PortableApps/time')
  if path.is_dir():destination.mkdir(parents=True,exist_ok=True)
  else:destination.parent.mkdir(parents=True,exist_ok=True);destination.symlink_to(path)
 for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:
  path=include/name
  if path.exists():path.unlink()
  path.symlink_to(runtime/'sdk/app'/name)
 catalog=stage/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 includes=['-I'+str(x) for x in [include,ROOT/'lib/Alarm/include',system/'lib/NativeApps/include',system/'Apps']]
 for kind,name in [(1,'alarms'),(2,'countdown')]:
  for san in ([False] if a.normal_only else [False,True]):
   binary=out/(name+('-san' if san else ''));sanflags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sanflags,*flags,'-DNOVA_APP_ID='+str(4 if kind==1 else 5),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c'))),*includes,ROOT/'test/native_apps/native_utc_alarm_adapter_test.c',*used,catalog,'-Wl,--wrap=free','-o',binary],check=True)
   for paper in (0,1):
    for case in range(17):
     if case==15 and not paper:continue
     frames=out/f'{name}-{paper}-{case}-{int(san)}';frames.mkdir(exist_ok=True)
     subprocess.run([binary,frames,str(case),str(paper)],check=True,timeout=60,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
     receipt['runs'].append({'app':name,'paper':bool(paper),'scenario':case,'sanitized':san})
(out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Production native alarm adapter composition passed')
