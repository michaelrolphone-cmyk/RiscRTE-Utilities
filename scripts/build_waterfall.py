#!/usr/bin/env python3
"""Build the capability-only Waterfall app against the current shared adapter."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from app_manifest import validate_manifest
ROOT=Path(__file__).resolve().parents[1]
PIN='2d16d9dfa7abc50ce916eebc11d81423adef0000'
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args();system=a.system_apps.resolve()
if subprocess.check_output(['git','rev-parse','HEAD'],cwd=system,text=True).strip()!=PIN or subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=system,text=True).strip():raise ValueError('Clean exact System Apps source required: '+PIN)
validate_manifest(ROOT/'Apps/waterfall.c','waterfall.elf')
cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
out=ROOT/'dist/waterfall';out.mkdir(parents=True,exist_ok=True)
(out/'catalog.c').write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
(out/'exports.map').write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
elf=out/'waterfall.elf'
subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(out/'exports.map'),'-Wall','-Wextra','-Werror','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RADIO_SESSION','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RETURN_APP="springboard.elf"',*['-I'+str(x) for x in [system/'lib/PortableApps/include',system/'lib/NativeApps/include']],str(ROOT/'Apps/waterfall.c'),str(system/'lib/PortableApps/src/adapter.c'),str(out/'catalog.c'),'-lgcc','-o',str(elf)],check=True)
syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
imports={l.split()[-1] for l in syms.splitlines() if ' U ' in ' '+l}
exports={l.split()[-1] for l in syms.splitlines() if len(l.split())>=3 and l.split()[-2] in ['T','D','B','R']}
if imports-{'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','calloc','free','strcpy'} or exports!={'app_main','app_module_init','app_module_fini'}:raise ValueError('Waterfall ABI differs')
m=json.loads((ROOT/'Apps/waterfall.json').read_text());manifest=dict(type='application',id='waterfall',version=m['version'],architecture='xtensa-esp32s3',file_name='waterfall.elf',entry='app_main',requires=[dict(capability=q['capability'],api=int(q['api'][2:])) for q in m['requires']])
(out/'waterfall.json').write_text(json.dumps(manifest,indent=2)+'\n')
record=dict(schema=1,source_revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),system_source=PIN,sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),bytes=elf.stat().st_size,imports=sorted(imports),exports=sorted(exports))
(out/'build-record.json').write_text(json.dumps(record,indent=2)+'\n')
print('Built Waterfall',m['version'],record['bytes'],record['sha256'])

validator=out/'validate-elf'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(system/'test/native_apps/stubs'),'-I'+str(system/'lib/elf_loader/include'),str(system/'lib/elf_loader/src/esp_elf_validate.c'),str(system/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
subprocess.run([str(validator),str(elf)],check=True)
for name in ['LICENSE-FontAwesome.txt','LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json']:(out/name).write_bytes((system/'lib/PortableApps/fonts'/name).read_bytes())
(out/'LICENSE-Utilities.txt').write_bytes((ROOT/'LICENSE').read_bytes())
