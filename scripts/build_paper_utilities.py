#!/usr/bin/env python3
"""Build one explicitly selected Nova utility, never a mixed-version bundle."""
import argparse,hashlib,json,os,subprocess,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SYSTEM_PIN='2aa0cf63346e507525af884bbfbf6b69313442c4'
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--app',required=True,choices=['calculator','stopwatch','countdown','battery','alarms']);a=p.parse_args();system=a.system_apps.resolve();name=a.app
if subprocess.check_output(['git','-C',str(system),'rev-parse','HEAD'],text=True).strip()!=SYSTEM_PIN or subprocess.check_output(['git','-C',str(system),'status','--porcelain','--untracked-files=no'],text=True).strip():raise ValueError('Clean exact Nova System Apps source required')
cc=os.environ.get('NATIVE_APP_CC') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
out=ROOT/'dist/paper-utilities'/name;out.mkdir(parents=True,exist_ok=True)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
export={'app_main','app_module_init','app_module_fini'};mapping=out/'exports.map';mapping.write_text('{ global: '+'; '.join(sorted(export))+'; local: *; };\n')
owner='BATTERY_RETURN_APP' if name=='battery' else 'CALCULATOR_RETURN_APP' if name=='calculator' else 'ALARM_RETURN_APP' if name in ('alarms','countdown') else 'PORTABLE_RETURN_APP'
flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_PAPER_UTILITIES','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_QUICK_ACTIONS']
if name=='battery':flags+=['-DPORTABLE_POWER_STATUS']
if name!='stopwatch':flags+=['-D'+owner+'="springboard.elf"']
elf=out/(name+'.elf');includes=[system/'lib/PortableApps/include',system/'lib/NativeApps/include',ROOT/'lib/Alarm/include',system/'Apps'];sources=[ROOT/'Apps'/(name+'.c'),system/'lib/PortableApps/src/adapter.c',catalog]+[system/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_render.c','quick_session.c']]
subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in includes],*map(str,sources),'-lgcc','-o',str(elf)],check=True)
syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True);imports={l.split()[-1] for l in syms.splitlines() if ' U ' in ' '+l};exports={l.split()[-1] for l in syms.splitlines() if len(l.split())>=3 and l.split()[-2] in ('T','D','B','R')}
assert imports<={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'} and exports==export
validator=out/'validate-elf';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(system/'test/native_apps/stubs'),'-I'+str(system/'lib/elf_loader/include'),str(system/'lib/elf_loader/src/esp_elf_validate.c'),str(system/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True);subprocess.run([str(validator),str(elf)],check=True)
profile=json.loads((ROOT/'Apps/paper-utilities.json').read_text());m=json.loads((ROOT/'Apps'/(name+'.json')).read_text());m['version']=profile['versions'][name];record={'schema':1,'app':name,'version':m['version'],'repository_sha':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain'],text=True).strip()),'system_sha':SYSTEM_PIN,'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'defines':flags,'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'size':elf.stat().st_size,'imports':sorted(imports),'exports':sorted(exports),'qualification':'target structural/import/export only; no physical qualification'}
profile=json.loads((ROOT/'Apps/paper-utilities.json').read_text())
record['profile']=profile
record['grants']=profile['common_grants']+profile['app_grants'][name]
record['source_files']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Apps').iterdir() if p.is_file() and (p.name.startswith(('calculator','stopwatch','countdown','utility_paper','alarm_', 'alarms', 'battery')))}
manifest={'type':'application','id':name,'version':m['version'],'file_name':name+'.elf','entry':'app_main','architecture':'xtensa-esp32s3','requires':[dict(capability=cap,api=api) for cap,api in dict.fromkeys((g['capability'],g['api']) for g in record['grants'])]}
policy={'manifest':name+'.json','grants':[{k:g[k] for k in ('capability','api','instance_id')} for g in record['grants']]}
(out/(name+'.boot-policy.json')).write_text(json.dumps(policy,indent=2)+'\n')
(out/(name+'.json')).write_text(json.dumps(manifest,indent=2)+'\n')
for folder in ['fonts','paper_fonts','quick_fonts']:
 target=out/'licenses'/folder;target.mkdir(parents=True,exist_ok=True)
 for p in (system/'lib/PortableApps'/folder).iterdir():
  if 'LICENSE' in p.name or 'OFL' in p.name or p.name=='SOURCES.json':shutil.copyfile(p,target/p.name)
(out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n');print(name+': pinned target Nova ELF validated')
