#!/usr/bin/env python3
"""Build the development Contexts app with its explicit shared lifecycle adapter."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from app_manifest import validate_manifest
ROOT=Path(__file__).resolve().parents[1]
IMPORTS={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy','strncmp','memchr'}
EXPORTS={'app_main','app_module_init','app_module_fini'}
def inventory():
    apps=json.loads((ROOT/'utilities-manifest.json').read_text())['contexts_apps']
    if len(apps)!=1 or apps[0]['id']!='contexts':raise ValueError('Invalid Contexts inventory')
    validate_manifest(ROOT/'Apps/contexts.c','contexts.elf')
    return apps
def build(system):
    required=['PortableContextsClient.h','PortableContextPreferences.h','PortableBackgroundServices.h']
    if not all((system/'lib/PortableApps/include'/name).is_file() for name in required):raise ValueError('Contexts lifecycle adapter required')
    pin={'system_apps':subprocess.check_output(['git','rev-parse','HEAD'],cwd=system,text=True).strip(),
         'system_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=system,text=True).strip())}
    cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out=ROOT/'dist/contexts-app';out.mkdir(parents=True,exist_ok=True)
    catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping=out/'exports.map';mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    validator=out/'validate-elf'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
    rows=[]
    for app in inventory():
        name=app['id'];elf=out/(name+'.elf')
        subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_CONTEXTS_CLIENT','-DPORTABLE_CONTEXTS_EDITOR','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_NOVA_UI',*['-I'+str(p) for p in (ROOT/'Apps',ROOT/'lib/Contexts/include',ROOT/'lib/Alarm/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include')],str(ROOT/app['source_path']),str(system/'lib/PortableApps/src/adapter.c'),str(catalog),*[str(system/'lib/PortableApps/src'/name) for name in ('quick_actions.c','quick_render.c','quick_session.c')],'-lgcc','-o',str(elf)],check=True)
        symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
        imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s}
        exports={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
        if not imports<=IMPORTS or exports!=EXPORTS:raise ValueError((name,imports-IMPORTS,exports))
        data=elf.read_bytes()
        if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Wrong target ABI')
        subprocess.run([str(validator),str(elf)],check=True)
        side=json.loads((ROOT/app['manifest_path']).read_text())
        manifest={'type':'application','id':name,'version':side['version'],'architecture':'xtensa-esp32s3','file_name':elf.name,'entry':'app_main','requires':[{'capability':r['capability'],'api':int(r['api'][2:])} for r in side['requires']]}
        elf.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
        rows.append({'id':name,'version':side['version'],'size_bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'imports':sorted(imports)})
    record={'schema':1,'purpose':'development-contexts-app-no-hardware-qualification','source_pins':pin,'repository_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'apps':rows}
    (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n')
    for name in ('LICENSE-FontAwesome.txt','LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json'):
        (out/name).write_bytes((system/'lib/PortableApps/fonts'/name).read_bytes())
    (out/'LICENSE-Utilities.txt').write_bytes((ROOT/'LICENSE').read_bytes())
    print('Validated Contexts app target ELF')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True)
    p.add_argument('--profile',choices=['watch','x4-paper'],default='watch')
    p.add_argument('--runtime',type=Path);p.add_argument('--x4-idle-source',type=Path);p.add_argument('--x4-idle-sdk',type=Path)
    p.add_argument('--output',type=Path,default=ROOT/'dist/contexts-paper');a=p.parse_args()
    if a.profile=='x4-paper':
        if not all((a.runtime,a.x4_idle_source,a.x4_idle_sdk)):p.error('x4-paper requires Runtime and typed X4 idle source/SDK')
        from build_contexts_paper import build as paper_build
        paper_build(a)
    else:
        if any((a.runtime,a.x4_idle_source,a.x4_idle_sdk)):p.error('Native inputs require --profile x4-paper')
        build(a.system_apps.resolve())
