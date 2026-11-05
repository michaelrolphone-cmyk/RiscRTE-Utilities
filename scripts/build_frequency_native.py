#!/usr/bin/env python3
"""Build an explicitly selected native Frequency development ELF, no recipes."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from frequency_native_profile import load
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--snapshot',action='store_true');a=p.parse_args();s=a.system_apps.resolve()
pin,profile=load()
if subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip()!=pin['system_apps'] or subprocess.check_output(['git','-C',str(s),'status','--porcelain'],text=True).strip():
 raise ValueError('Exact clean native Frequency System source required')
cc=os.environ.get('NATIVE_APP_CC') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
out=ROOT/'dist/frequency-native'/('snapshot' if a.snapshot else 'direct');out.mkdir(parents=True,exist_ok=True)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
exports={'app_main','app_module_init','app_module_fini'};mapping=out/'exports.map';mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
flags=['-D'+f for f in pin['defines']]
if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_RETAINED_RGB565_HANDOFF']
includes=[s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'Apps',ROOT/'lib/Alarm/include']
sources=[ROOT/'Apps/frequency_generator.c',*[s/'lib/PortableApps/src'/n for n in ('adapter.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')],catalog]
(out/'frequency-native-qualification.json').write_text(json.dumps(profile,indent=2)+'\n')
elf=out/'frequency_generator.elf'
subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in includes],*map(str,sources),'-lgcc','-o',str(elf)],check=True)
syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
imports={l.split()[-1] for l in syms.splitlines() if ' U ' in ' '+l};actual={l.split()[-1] for l in syms.splitlines() if len(l.split())>=3 and l.split()[-2] in ('T','D','B','R')}
assert imports<={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'} and actual==exports,(imports,actual)
validator=out/'validate-elf'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(s/'test/native_apps/stubs'),'-I'+str(s/'lib/elf_loader/include'),str(s/'lib/elf_loader/src/esp_elf_validate.c'),str(s/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
subprocess.run([str(validator),str(elf)],check=True)
def source_hashes(root):
 paths=subprocess.check_output(['git','-C',str(root),'ls-files','--cached','--others','--exclude-standard'],text=True).splitlines()
 return {n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in paths if (root/n).is_file() and n.startswith(('Apps/','lib/PortableApps/','lib/NativeApps/','lib/Alarm/'))}
receipt={'qualification_profile':profile,'source_pin':pin,'purpose':'development native Frequency structural qualification','snapshot':a.snapshot,'hardware_tested':False,'source':{str(root):{'commit':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'files':source_hashes(root)} for root in (ROOT,s)},'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'flags':flags,'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'bytes':elf.stat().st_size,'imports':sorted(imports),'exports':sorted(actual)}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(f'PASS: Frequency native target ELF {elf.stat().st_size} bytes, snapshot={a.snapshot}')
