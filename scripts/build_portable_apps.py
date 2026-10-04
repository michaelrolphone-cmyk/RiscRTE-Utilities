#!/usr/bin/env python3
"""Build new shared daily tools against the pinned generic client adapter.

This is a distinct runtime profile from the preserved Reader migration cohort.
No source is copied into a Watch-specific application and no device is touched.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from app_manifest import validate_manifest
ROOT=Path(__file__).resolve().parents[1]
PIN='b28428505c9e067bb6ea8a84d12f13ec0bcc4992'
IMPORTS={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'}
EXPORTS={'app_main','app_module_init','app_module_fini'}
def inventory():
    apps=json.loads((ROOT/'utilities-manifest.json').read_text())['portable_apps']
    if [a.get('id') for a in apps]!=['calculator','stopwatch','waterfall']:
        raise ValueError('Unexpected portable daily tool inventory')
    for app in apps:
        name=app['id']
        if app.get('origin')!='original' or app.get('runtime_profile')!='portable-riscrte-v1' or app.get('source_path')!='Apps/'+name+'.c' or app.get('manifest_path')!='Apps/'+name+'.json' or app.get('file_name')!=name+'.elf':
            raise ValueError('Invalid portable app identity/provenance')
        validate_manifest(ROOT/app['source_path'],name+'.elf')
        manifest=json.loads((ROOT/app['manifest_path']).read_text())
        if manifest['version']!=app['version'] or manifest.get('runtime_profile')!='portable-riscrte-v1':
            raise ValueError('Portable manifest/version mismatch')
    return apps

def verify_system(system):
    if subprocess.check_output(['git','rev-parse','HEAD'],cwd=system,text=True).strip()!=PIN or subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=system,text=True).strip():
        raise ValueError('Clean exact shared client source required: '+PIN)
    pins=json.loads((system/'lib/PortableApps/SOURCES.json').read_text())
    for name,pin in pins.items():
        if hashlib.sha256((system/'lib/PortableApps/include'/name).read_bytes()).hexdigest()!=pin['sha256']:
            raise ValueError('Pinned SDK bytes changed: '+name)

def build(system):
    apps=inventory();verify_system(system)
    cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out=ROOT/'dist/portable-apps';out.mkdir(parents=True,exist_ok=True)
    catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping=out/'exports.map';mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    validator=out/'validate-elf'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
    rows=[]
    for app in apps:
        name=app['id'];elf=out/(name+'.elf')
        subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror','-DPORTABLE_FORCE_FULL_FRAMES',*['-I'+str(p) for p in (system/'lib/PortableApps/include',system/'lib/NativeApps/include')],str(ROOT/app['source_path']),str(system/'lib/PortableApps/src/adapter.c'),str(catalog),'-lgcc','-o',str(elf)],check=True)
        syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
        imports={s.split()[-1] for s in syms.splitlines() if ' U ' in ' '+s}
        exports={s.split()[-1] for s in syms.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
        if not imports<=IMPORTS or exports!=EXPORTS:raise ValueError((name,imports-IMPORTS,exports))
        data=elf.read_bytes()
        if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Wrong target ABI')
        subprocess.run([str(validator),str(elf)],check=True)
        source_manifest=json.loads((ROOT/app['manifest_path']).read_text())
        requires=[{'capability':row['capability'],'api':int(row['api'][2:])} for row in source_manifest['requires']]
        manifest={'type':'application','id':name,'version':app['version'],'architecture':'xtensa-esp32s3','file_name':name+'.elf','entry':'app_main','requires':requires}
        elf.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
        rows.append({'id':name,'version':app['version'],'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),'imports':sorted(imports)})
    record={'schema':1,'purpose':'portable-runtime-development-not-install-catalog','repository_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'shared_system_apps_sha':PIN,'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'apps':rows}
    (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n')
    for name in ('LICENSE-FontAwesome.txt','LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json'):
        (out/name).write_bytes((system/'lib/PortableApps/fonts'/name).read_bytes())
    print('Validated portable application ELFs; no install or release')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);a=p.parse_args();build(a.system_apps.resolve())
