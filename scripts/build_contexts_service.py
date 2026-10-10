#!/usr/bin/env python3
"""Build the ordinary, storage-free Contexts inference service."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from normalize_xtensa_relocations import normalize
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--drivers',required=True,type=Path);p.add_argument('--runtime',required=True,type=Path);p.add_argument('--profile',choices=('full','rf-only'),default='full');a=p.parse_args()
cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
if not cc:raise SystemExit('Set NATIVE_APP_CC to the pinned GCC8.4 toolchain')
compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
if '8.4.0' not in compiler:raise SystemExit('Pinned GCC8.4 required')
out=ROOT/('dist/contexts-service-rf-only' if a.profile=='rf-only' else 'dist/contexts-service');out.mkdir(parents=True,exist_ok=True)
mapping=out/'exports.map';mapping.write_text('{ global: t5_driver_get; local: *; };\n')
elf=out/'driver.elf'
subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wall','-Wextra','-Werror',*(['-DCONTEXTS_RF_ONLY=1'] if a.profile=='rf-only' else []),*['-I'+str(i) for i in [ROOT/'Apps',ROOT/'lib/Contexts/include',a.drivers/'sdk/driver',a.system_apps/'lib/PortableApps/include']],'-Wl,--version-script='+str(mapping),str(ROOT/'Services/contexts/service.c'),str(a.system_apps/'lib/NativeApps/src/SingleFloatDivisionCompat.c'),'-lgcc','-o',str(elf)],check=True)
normalized_relocations=normalize(elf)
symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s}
exports={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
assert imports<={'memcpy','memset','memcmp','memchr','strcmp','strlen'} and exports=={'t5_driver_get'},(imports,exports)
validator=out/'validate-elf'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
subprocess.run([str(validator),str(elf)],check=True)
raw=elf.read_bytes();assert raw[:7]==b'\x7fELF\x01\x01\x01' and raw[16:20]==b'\x03\x00\x5e\x00'
manifest=(ROOT/'Services/contexts'/('rf-only-manifest.json' if a.profile=='rf-only' else 'manifest.json')).read_bytes();(out/'manifest.json').write_bytes(manifest)
def source(repo):return {'commit':subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip(),'dirty':bool(subprocess.check_output(['git','-C',str(repo),'status','--porcelain','--untracked-files=no'],text=True).strip())}
evidence={'schema':1,'profile':a.profile,'source_mask':2 if a.profile=='rf-only' else 3,'requires':json.loads(manifest)['requires'],'manifest_sha256':hashlib.sha256(manifest).hexdigest(),'version':json.loads(manifest)['version'],'compiler':compiler,'sources':{'utilities':source(ROOT),'system-apps':source(a.system_apps),'drivers':source(a.drivers),'runtime':source(a.runtime)},'elf_sha256':hashlib.sha256(raw).hexdigest(),'size_bytes':len(raw),'imports':sorted(imports),'exports':sorted(exports),'hardware_verified':False}
sizes=subprocess.check_output([cc.removesuffix('gcc')+'size',str(elf)],text=True).splitlines()[1].split()
evidence['removed_noop_relocations']=normalized_relocations
evidence['sections_bytes']={name:int(value) for name,value in zip(('text','data','bss'),sizes[:3])}
evidence['models']={'signature_rooms_events':True,'temporal':True,'neural':True,'persistent_copy':False,'temporal_fingerprints':True,'adaptive_room_training':True,'checkpoint_owner':'app','heap':False,'audio':a.profile=='full','radio':True}
(out/'build-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print('Contexts-service '+evidence['version']+' target ELF, exact exports/imports and loader validation PASS')
