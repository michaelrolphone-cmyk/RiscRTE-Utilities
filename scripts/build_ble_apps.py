#!/usr/bin/env python3
"""Exact-pinned BLE Scanner ELFs. No installation or hardware access."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
from app_manifest import validate_manifest
ROOT=Path(__file__).resolve().parents[1]
IMPORTS={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'}
EXPORTS={'app_main','app_module_init','app_module_fini'}
def inventory():
    apps=json.loads((ROOT/'utilities-manifest.json').read_text())['ble_apps']
    if not apps or len(apps)>1 or len({a['id'] for a in apps})!=len(apps):raise ValueError('Invalid BLE inventory')
    for app in apps:
        name=app['id']
        if name not in ('ble_scanner',) or app['source_path']!=f'Apps/{name}.c' or app['manifest_path']!=f'Apps/{name}.json' or app['file_name']!=name+'.elf':raise ValueError('Invalid BLE source identity')
        validate_manifest(ROOT/app['source_path'],name+'.elf')
        side=json.loads((ROOT/app['manifest_path']).read_text())
        if app['version']!=side['version'] or side['runtime_profile']!='portable-riscrte-v1' or side['min_firmware_version']!='0.1.27':raise ValueError('BLE version/profile mismatch')
        if any(not(ROOT/p).is_file() for p in app['additional_sources']):raise ValueError('Missing BLE source')
    return apps
def build(system, *, paper=False, rotation=0, navigation=False, partial=False, return_app="springboard.elf", output=None,home_app=None,quick_actions=False):
    if not return_app.endswith(".elf") or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-." for c in return_app):raise ValueError("Invalid return app")
    if home_app and not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.elf',home_app):raise ValueError('Invalid Home target')
    pin=json.loads((ROOT/'sdk/ble-sources.json').read_text())
    if subprocess.check_output(['git','rev-parse','HEAD'],cwd=system,text=True).strip()!=pin['system_apps'] or subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=system,text=True).strip():raise ValueError('Exact clean radio lifecycle adapter required')
    if not (system/'lib/PortableApps/include/PortableRadioSession.h').is_file():raise ValueError('Radio lifecycle hooks missing')
    cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out=output or ROOT/'dist/ble-apps';out.mkdir(parents=True,exist_ok=True)
    catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping=out/'exports.map';mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    validator=out/'validate-elf'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
    defines=['-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RADIO_SESSION','-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RETURN_APP="'+return_app+'"','-DPORTABLE_DISPLAY_ROTATION='+str(rotation)]
    if not partial:defines+=['-DPORTABLE_FORCE_FULL_FRAMES']
    if navigation:defines+=['-DPORTABLE_INPUT_NAVIGATION']
    quick=['quick_actions.c','quick_render.c','quick_session.c'] if quick_actions or not paper else []
    if quick:defines+=['-DPORTABLE_QUICK_ACTIONS']
    if not paper:quick+=['quick_radios.c'];defines+=['-DPORTABLE_QUICK_RADIOS']
    if home_app:defines+=['-DPORTABLE_HOME_APP="'+home_app+'"']
    rows=[]
    for app in inventory():
        name=app['id'];elf=out/(name+'.elf')
        subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*defines,*['-I'+str(p) for p in (ROOT/'Apps',system/'Apps',ROOT/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include')],str(ROOT/app['source_path']),str(system/'lib/PortableApps/src/adapter.c'),str(catalog),*[str(system/'lib/PortableApps/src'/f) for f in quick],'-lgcc','-o',str(elf)],check=True)
        symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
        imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s}
        exports={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
        if not imports<=IMPORTS or exports!=EXPORTS:raise ValueError((name,imports-IMPORTS,exports))
        data=elf.read_bytes()
        if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Wrong target ABI')
        subprocess.run([str(validator),str(elf)],check=True)
        side=json.loads((ROOT/app['manifest_path']).read_text())
        manifest={'type':'application','id':name,'version':side['version'],'architecture':'xtensa-esp32s3','file_name':elf.name,'entry':'app_main','requires':[{'capability':r['capability'],'api':int(r['api'][2:])} for r in side['requires']+side.get('optional',[])]}
        if paper:manifest['requires']=[r for r in manifest['requires'] if r['capability']!='net.wifi']
        if navigation:manifest['requires'].append({'capability':'input.navigation','api':1})
        elf.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
        rows.append({'id':name,'version':side['version'],'size_bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'imports':sorted(imports)})
    record={'schema':1,'purpose':'development-ble-scanner-no-hardware-qualification','source_pins':pin,'profile':{'paper':paper,'display_rotation':rotation,'navigation':navigation,'partial_damage':partial,'return_app':return_app,'home_app':home_app,'quick_actions':bool(quick_actions or not paper)},'repository_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'apps':rows}
    (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n')
    for name in ('LICENSE-FontAwesome.txt','LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json'):
        (out/name).write_bytes((system/'lib/PortableApps/fonts'/name).read_bytes())
    (out/'LICENSE-Utilities.txt').write_bytes((ROOT/'LICENSE').read_bytes())
    print('Validated BLE Scanner target ELFs')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True)
    p.add_argument('--paper-profile',action='store_true',help='Omit compact quick controls and Wi-Fi dependency; layout still follows display capabilities')
    p.add_argument('--display-rotation',type=int,choices=[0,90],default=0)
    p.add_argument('--navigation',action='store_true');p.add_argument('--partial-damage',action='store_true')
    p.add_argument('--return-app',default='springboard.elf');p.add_argument('--output-dir',type=Path)
    p.add_argument('--home-app');p.add_argument('--quick-actions',action='store_true')
    a=p.parse_args();build(a.system_apps.resolve(),paper=a.paper_profile,rotation=a.display_rotation,navigation=a.navigation,partial=a.partial_damage,return_app=a.return_app,output=a.output_dir,home_app=a.home_app,quick_actions=a.quick_actions)
