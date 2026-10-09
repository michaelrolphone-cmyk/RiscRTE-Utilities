#!/usr/bin/env python3
"""Build the explicit native paper telemetry utility cohort. No product publication."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
import native_idle_build as idle
from native_utility_build import ROOT,PROFILE,RUNTIME,APPS,flags,sources,IMPORTS,EXPORTS,sha,git
TELEMETRY=json.loads((ROOT/'Apps/native-ble-broadcast.json').read_text())
VERSIONS=TELEMETRY['versions']
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--app',choices=APPS,action='append');p.add_argument('--output',type=Path,default=ROOT/'dist/native-ble-broadcast');idle.options(p);a=p.parse_args();system=a.system_apps.resolve();runtime=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
idle.validate(a,p,system,runtime)
if not idle.selected(a) and (git(system,'rev-parse','HEAD')!=TELEMETRY['system_sha'] or git(system,'status','--porcelain','--untracked-files=no')):raise ValueError('Clean exact telemetry System required: '+TELEMETRY['system_sha'])
cc=os.environ['NATIVE_APP_CC'];compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0];assert '8.4.0' in compiler and '2021r2-patch5' in compiler
inc=out/'sdk/include';inc.mkdir(parents=True,exist_ok=True)
for header in (system/'lib/PortableApps/include').glob('*.h'):shutil.copyfile(header,inc/header.name)
shutil.copytree(system/'lib/PortableApps/time',inc.parent/'time',dirs_exist_ok=True)
for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:(inc/name).write_bytes(subprocess.check_output(['git','-C',runtime,'show',(idle.RUNTIME if idle.selected(a) else RUNTIME)+':sdk/app/'+name]))
for name in ['AlarmServiceV1.h','AlarmServiceV2.h']:(inc/name).write_bytes(subprocess.check_output(['git','-C',ROOT,'show',PROFILE['alarm_sdk_sha']+':lib/Alarm/include/'+name]))
idle_flags,idle_sources,inc=idle.configure(a,p,system,out,inc)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
mapping=out/'exports.map';mapping.write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n')
validator=out/'validate';subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator],check=True)
for name in a.app or APPS:
 dest=out/name;dest.mkdir(exist_ok=True);elf=dest/(name+'.elf');defines=flags(name,paper_transitions=True)+['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF','-DPORTABLE_PAPER_PREFERENCES'];competing=name.startswith('ble_') or name=='waterfall'
 if competing:defines+=['-DPORTABLE_BLE_FOREGROUND']
 defines+=idle_flags
 if idle.selected(a) and name=='waterfall':defines+=['-DPORTABLE_RADIO_CONTINUOUS_CAPTURE']
 src=[*sources(name,system),*idle_sources]
 include=['-I'+str(d) for d in [inc,system/'lib/NativeApps/include',ROOT/'lib/Bluetooth/include',ROOT/'Apps',system/'Apps']]
 subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*defines,*include,*src,catalog,'-lgcc','-o',elf],check=True)
 syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',elf],text=True);imports={s.split()[-1] for s in syms.splitlines() if ' U ' in ' '+s};exports={s.split()[-1] for s in syms.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')};assert imports<=IMPORTS and exports==EXPORTS,(name,imports,exports);subprocess.run([validator,elf],check=True)
 grants=idle.grants(a,PROFILE['common_grants']+PROFILE['app_grants'][name]+[{'capability':'telemetry.broadcast','api':1,'instance_id':0}])
 pairs=list(dict.fromkeys((g['capability'],g['api']) for g in grants));manifest=dict(type='application',id=name,version=idle.version(a,name,VERSIONS[name]),architecture='xtensa-esp32s3',file_name=name+'.elf',entry='app_main',requires=[dict(capability=c,api=v) for c,v in pairs]);(dest/(name+'.json')).write_text(json.dumps(manifest,indent=2)+'\n');(dest/(name+'.boot-policy.json')).write_text(json.dumps({'manifest':name+'.json','grants':grants},indent=2)+'\n')
 receipt={'schema':1,'app':name,'version':manifest['version'],'source_repo':'michaelrolphone-cmyk/RiscRTE-Utilities','source_revision':git(ROOT,'rev-parse','HEAD'),'system_source_revision':git(system,'rev-parse','HEAD'),'runtime_source_revision':idle.RUNTIME if idle.selected(a) else RUNTIME,'alarm_source_revision':PROFILE['alarm_sdk_sha'],'alarm_api':2,'time_policy':'native-realtime-iana','elf_sha256':sha(elf),'elf_bytes':elf.stat().st_size,'requires':manifest['requires'],'sdk_sha256':{n:sha(inc/n) for n in ['RiscRuntimeV1.h','RiscRealtimeV1.h','AlarmServiceV1.h','AlarmServiceV2.h']},'build_defines':defines,'source_dirty':bool(git(ROOT,'status','--porcelain')),'system_dirty':bool(git(system,'status','--porcelain')),'compiler':compiler,'ble_broadcast':{'enabled':True,'default':'off','grant_lifetime':'transient','foreground_excluded':competing},'paper_motion':True,'hardware_verified':False}
 receipt.update(imports=sorted(imports),exports=sorted(exports),target_validation='passed',generated_sources_sha256={'catalog.c':sha(catalog),'exports.map':sha(mapping)})
 idle.record(a,receipt,defines,grants)
 receipt['compiled_dependencies_sha256']=idle.dependencies(cc,defines,include,src,[('CompiledSDK',inc),('Utilities',ROOT),('System',system)])
 licenses=dest/'licenses';licenses.mkdir(exist_ok=True)
 for repo,label in [(ROOT,'Utilities-MIT.txt'),(system,'System-MIT.txt'),(runtime,'Runtime-MIT.txt')]:shutil.copyfile(repo/'LICENSE',licenses/label)
 for folder in ['fonts','paper_fonts','quick_fonts']:
  (licenses/folder).mkdir(exist_ok=True)
  for notice in (system/'lib/PortableApps'/folder).iterdir():
   if notice.is_file() and ('LICENSE' in notice.name or 'OFL' in notice.name or notice.name=='SOURCES.json'):shutil.copyfile(notice,licenses/folder/notice.name)
 (dest/'x4-native-app.json').write_text(json.dumps(receipt,indent=2)+'\n');print(name+' '+manifest['version']+' target/imports/loader PASS',flush=True)
