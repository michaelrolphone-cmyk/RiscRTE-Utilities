#!/usr/bin/env python3
"""Production-matched controllers, shared adapter and actual typed Light helper."""
import argparse,json,os,subprocess
from pathlib import Path
from native_utility_build import ROOT,sources
import native_idle_build as idle
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--broadcast-build',type=Path,required=True);p.add_argument('--alarm-build',type=Path,required=True);p.add_argument('--output',type=Path,default=ROOT/'build/idle-composition-tests');a=p.parse_args()
system=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
record={'system_revision':idle.git(system,'rev-parse','HEAD'),'utility_revision':idle.git(ROOT,'rev-parse','HEAD'),'runs':[],'hardware_verified':False}
for app,app_id in [('battery',3),('calculator',1),('stopwatch',2),('alarms',4),('countdown',5)]:
 alarm=app in ('alarms','countdown');build=(a.alarm_build/'native' if alarm else a.broadcast_build).resolve();target=json.loads((build/app/'x4-native-app.json').read_text());headers=Path(target['idle_policy']['compiled_include_directory']);helper=Path(target['idle_policy']['source'])
 assert target['system_source_revision']==record['system_revision'] and target['idle_policy']['helper_sha256']==idle.sha(helper)
 for name,digest in target['sdk_sha256'].items():assert idle.sha(headers/name)==digest
 defines=target['build_defines']+['-DNOVA_APP_ID='+str(app_id),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(app+'.c')))]
 src=([system/'lib/PortableApps/src'/n for n in ['adapter.c','quick_actions.c','quick_render.c','quick_session.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']] if alarm else sources(app,system)[1:])+[system/'lib/PortableApps/src/quick_radios.c',helper]
 includes=['-I'+str(x) for x in [headers,system/'lib/NativeApps/include',ROOT/'Apps',ROOT/'lib/Alarm/include',system/'Apps',helper.parent.parent/'drivers/x4pro_power']]
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 for san in (False,True):
  exe=out/(app+('-san' if san else ''));extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,*defines,*includes,ROOT/'test/native_apps/x4_idle_utility_test.c',*src,catalog,'-Wl,--wrap=free','-o',exe],check=True)
  for case in ([0,1,2,3,4,5] if alarm else [0,1,2,3,5]):
   run=subprocess.run([exe,str(case)],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
   (out/f'{app}-{san}-{case}.log').write_text(run.stdout+run.stderr)
   if run.returncode:raise RuntimeError(run.stdout+run.stderr)
   print(run.stdout.strip(),flush=True);record['runs'].append({'app':app,'sanitized':san,'scenario':case,'result':run.stdout.strip()})
 record.setdefault('target_elf_sha256',{})[app]=target['elf_sha256']
 record.setdefault('compiled_dependencies_sha256',{})[app]=idle.dependencies(os.environ.get('CC','cc'),defines,includes,[ROOT/'test/native_apps/x4_idle_utility_test.c',*src],[('CompiledSDK',headers),('Utilities',ROOT),('System',system)])
target=json.loads((a.broadcast_build/'waterfall/x4-native-app.json').read_text());headers=Path(target['idle_policy']['compiled_include_directory'])
for san in (False,True):
 exe=out/('waterfall-san' if san else 'waterfall');extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*extra,*target['build_defines'],*['-I'+str(x) for x in [headers,system/'lib/NativeApps/include',ROOT/'Apps',system/'Apps']],ROOT/'test/native_apps/x4_idle_waterfall_test.c','-Wl,--gc-sections','-lm','-o',exe],check=True)
 run=subprocess.run([exe],capture_output=True,text=True,check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
 print(run.stdout.strip(),flush=True);record['runs'].append({'app':'waterfall','sanitized':san,'result':run.stdout.strip()})
(out/'evidence.json').write_text(json.dumps(record,indent=2)+'\n')
