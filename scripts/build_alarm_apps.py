#!/usr/bin/env python3
"""Build staged Alarm/Countdown ELF slice; never package it as a working Watch BIN."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from build_portable_apps import ROOT,PIN,verify_system,IMPORTS,EXPORTS
from app_manifest import validate_manifest
RUNTIME_PIN='b2fc83280c54ca3ebd567184cc50e4785daa3951'
RUNTIME_TREE='b3399091995ec67d84a151e8e18be59fe4475ee9'
def run(cmd,**kwargs):return subprocess.run([str(x) for x in cmd],check=True,**kwargs)
def verify_runtime(runtime):
 if subprocess.check_output(['git','rev-parse','HEAD^{tree}'],cwd=runtime,text=True).strip()!=RUNTIME_TREE or subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=runtime,text=True).strip():
  raise ValueError('Clean reviewed Runtime 0.1.7 tree required')
def inventory():
 m=json.loads((ROOT/'utilities-manifest.json').read_text());apps=m['alarm_apps'];services=m['alarm_services']
 if [x['id'] for x in apps]!=['alarms','countdown'] or [x['id'] for x in services]!=['alarm-service']:raise ValueError('Wrong staged inventory')
 for x in apps:
  name=x['id'];validate_manifest(ROOT/x['source_path'],name+'.elf')
  side=json.loads((ROOT/x['manifest_path']).read_text())
  if side['version']!=x['version'] or x['runtime_profile']!='portable-riscrte-v1' or x['source_path']!=f'Apps/{name}.c' or x['manifest_path']!=f'Apps/{name}.json' or x['file_name']!=f'{name}.elf':raise ValueError('Wrong app identity')
  if len(side['requires'])!=5:raise ValueError('Unexpected app authority')
  if any(not(ROOT/p).is_file() for p in x['additional_sources']):raise ValueError('Missing shared source')
 service=json.loads((ROOT/services[0]['manifest_path']).read_text())
 if service['id']!='alarm-service' or service['version']!=services[0]['version'] or service['type']!='driver' or service['driver_abi']!=2 or service['provides']!=[{'capability':'alarm.service','api':1}]:raise ValueError('Wrong ordinary service identity')
 return apps,services[0]
def build(system,runtime):
 verify_system(system);verify_runtime(runtime);apps,service=inventory()
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
 out=ROOT/'dist/alarm-apps';out.mkdir(parents=True,exist_ok=True)
 flags=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wall','-Wextra','-Werror']
 inc=['-I'+str(x) for x in [ROOT/'lib/Alarm/include',runtime/'sdk/driver',system/'lib/PortableApps/include',system/'lib/NativeApps/include']]
 validator=out/'validate-elf';run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator])
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 rows=[]
 for name,source,manifest,is_service in [(x['id'],x['source_path'],x['manifest_path'],False) for x in apps]+[('alarm-service',service['source_path'],service['manifest_path'],True)]:
  folder=out/name;folder.mkdir(exist_ok=True);elf=folder/('driver.elf' if is_service else name+'.elf');mapping=folder/'exports.map'
  exports={'t5_driver_get'} if is_service else EXPORTS;mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
  sources=[ROOT/source] if is_service else [ROOT/source,system/'lib/PortableApps/src/adapter.c',catalog]
  run([*flags,*inc,'-DPORTABLE_FORCE_FULL_FRAMES','-Wl,--version-script='+str(mapping),*sources,'-lgcc','-o',elf])
  symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True);imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s};actual_exports={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
  permitted={'memcpy','memset','memcmp','strcmp','strlen'} if is_service else IMPORTS
  if not imports<=permitted or actual_exports!=exports:raise ValueError((name,imports-permitted,actual_exports))
  data=elf.read_bytes()
  if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Target ELF ABI mismatch')
  run([validator,elf]);side=json.loads((ROOT/manifest).read_text())
  if not is_service:side={'type':'application','id':name,'version':side['version'],'architecture':'xtensa-esp32s3','file_name':elf.name,'entry':'app_main','requires':[{'capability':x['capability'],'api':int(x['api'][2:])} for x in side['requires']]}
  elf.with_suffix('.json').write_text(json.dumps(side,indent=2)+'\n');rows.append({'id':name,'version':side['version'],'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),'imports':sorted(imports),'exports':sorted(exports)})
 evidence={'schema':1,'purpose':'staged-alarm-slice-not-working-watch-delivery','repository_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'system_apps_sha':PIN,'runtime_sha':RUNTIME_PIN,'runtime_tree':RUNTIME_TREE,'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'modules':rows,'integration_gates':['genuine output backend and safe cleanup','shared in-place foreground overlay','Clock timed Light/Deep and Settings mode','independent review and exact target CI']}
 (out/'build-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print('Validated staged Alarms, Countdown and ordinary alarm-service target ELFs')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args();build(a.system_apps.resolve(),a.runtime.resolve())
