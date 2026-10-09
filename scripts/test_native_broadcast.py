#!/usr/bin/env python3
"""Actual paper Battery/controller/adapter/service and target qualification."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
from native_utility_build import flags,sources,TIME,IMPORTS,EXPORTS
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--drivers',type=Path,required=True);p.add_argument('--xtensa-cc',required=True);a=p.parse_args();system=a.system_apps.resolve();runtime=a.runtime.resolve();drivers=a.drivers.resolve()
out=ROOT/'build/native-broadcast';out.mkdir(parents=True,exist_ok=True);inc=out/'sdk/include';inc.mkdir(parents=True,exist_ok=True)
for file in (system/'lib/PortableApps/include').glob('*.h'):shutil.copyfile(file,inc/file.name)
shutil.copytree(system/'lib/PortableApps/time',inc.parent/'time',dirs_exist_ok=True)
for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:(inc/name).write_bytes(subprocess.check_output(['git','-C',runtime,'show','30dcec5ce6ce33223f2b203a2399283e1f758567:sdk/app/'+name]))
for name in ['AlarmServiceV1.h','AlarmServiceV2.h']:(inc/name).write_bytes(subprocess.check_output(['git','-C',ROOT,'show','637e13b0bce62ad49b756bec2468a6271d163fc7:lib/Alarm/include/'+name]))
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
defines=flags('battery',paper_transitions=True)+['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF']
includes=['-I'+str(d) for d in [inc,system/'lib/NativeApps/include',ROOT/'Apps',system/'Apps',drivers/'sdk/driver']]
src=sources('battery',system)
evidence={'hardware_verified':False,'runs':[],'defines':defines,'sources':{str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in [*src,ROOT/'Services/telemetry_broadcast/service.c']}}
for san in [False,True]:
 binary=out/('battery-san' if san else 'battery');extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,*defines,'-DNOVA_APP_ID=3','-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps/battery.c')),*includes,ROOT/'test/native_apps/battery_native_broadcast_test.c',*src[1:],ROOT/'Services/telemetry_broadcast/service.c',catalog,'-Wl,--wrap=free','-o',binary],check=True)
 for case in range(11):
  frames=out/f'frames-{san}-{case}';frames.mkdir(exist_ok=True)
  result=subprocess.run([binary,str(case),frames],capture_output=True,text=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'));(out/f'{san}-{case}.log').write_text(result.stdout+result.stderr)
  if result.returncode:raise RuntimeError(result.stdout+result.stderr)
  evidence['runs'].append({'sanitized':san,'case':case,'result':result.stdout.strip()});print(result.stdout.strip(),flush=True)
mapping=out/'exports.map';mapping.write_text('{ global: '+ '; '.join(sorted(EXPORTS))+'; local: *; };\n');elf=out/'battery.elf';cc=a.xtensa_cc
subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*defines,*includes,*src,catalog,'-lgcc','-o',elf],check=True)
syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',elf],text=True);imports={s.split()[-1] for s in syms.splitlines() if ' U ' in ' '+s};exports={s.split()[-1] for s in syms.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')};assert imports<=IMPORTS and exports==EXPORTS,(imports,exports)
validator=out/'validate';subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator],check=True);subprocess.run([validator,elf],check=True)
evidence['target']={'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'bytes':elf.stat().st_size,'imports':sorted(imports),'exports':sorted(exports)}
(out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
