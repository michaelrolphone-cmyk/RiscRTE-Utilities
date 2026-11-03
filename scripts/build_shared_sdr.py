#!/usr/bin/env python3
"""Build the complete installed RF controller with a generic capability canvas.
One target artifact; no product, device, geometry or hardware selector.
"""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','canvas-system','runtime','watch','compiler','output'):
 p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args()
for n,v in vars(a).items():setattr(a,n,v.resolve())
def run(cmd):subprocess.run(list(map(str,cmd)),check=True)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def head(root):
 h=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
 subprocess.run(['git','-C',str(root),'diff','--exit-code','HEAD','--'],check=True,capture_output=True)
 return h
expected={'system':'96742a20af660808cae6cd3a36eadd12a83a460f','canvas_system':'710b200021b983ea55eb86a6fbddbbf5ce10a984','runtime':'712ecc3f894dba99d9c9638941778a82f5856b97','watch':'6e82fdcaec0b73ed700d2104cfb4d94f011c19d4'}
pins={name:head(getattr(a,name)) for name in expected}
if pins!=expected:raise ValueError('Exact installed/controller capability sources required')
a.output.mkdir(parents=True,exist_ok=False);out=a.output
sdk=out/'sdk';sdk.mkdir();source=out/'adapter'
# Materialize each header identity once, selected Runtime last.
for base in (a.system/'lib/NativeApps/include',a.system/'lib/PortableApps/include',a.watch/'sdk/driver',a.watch/'sdk/app',ROOT/'lib/Alarm/include',ROOT/'lib/Contexts/include',a.runtime/'sdk/app'):
 for path in base.glob('*.h'):shutil.copyfile(path,sdk/path.name)
shutil.copytree(a.system/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
sys.path.insert(0,str(a.canvas_system/'scripts'))
from shared_home_sdk import stage_home_headers
closure=stage_home_headers(a.canvas_system,sdk,roots=('PortableHomeCanvas.h','PortableHomeCanvasTouch.h','RiscCanvasInputV1.h'))
if (sdk/'RiscRuntimeV1.h').read_bytes()!=(a.runtime/'sdk/app/RiscRuntimeV1.h').read_bytes():raise ValueError('Runtime header identity replaced')
run([sys.executable,ROOT/'Apps/shared_sdr/materialize_adapter.py','--system',a.system,'--output',source])
run([sys.executable,ROOT/'Apps/shared_sdr/materialize_lifecycle.py','--system',a.system,'--output',source])
run([sys.executable,ROOT/'Apps/shared_sdr/materialize_sleep.py','--watch',a.watch,'--output',source])
(source/'catalog.c').write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
defines=[
'PORTABLE_RF_SHARED_CANVAS','PORTABLE_RF_NATIVE_CANVAS','PORTABLE_RF_SHARED_TEXT_INPUT','PORTABLE_TEXT_INPUT_CLIENT',
'PORTABLE_APP_HOME_GUARD','PORTABLE_APP_LAUNCH_GUARD','PORTABLE_CHECKED_STORAGE_RUNTIME','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_ALARM_TERMINAL_RETENTION',
'PORTABLE_FORCE_FULL_FRAMES','PORTABLE_INPUT_NAVIGATION','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_ALARM_CLIENT','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS',
'PORTABLE_LOW_BATTERY','PORTABLE_BLE_BROADCAST','PORTABLE_MOTION_WAKE','WATCH_ALARM_SLEEP_RESUME','PORTABLE_NOVA_UI','PORTABLE_BLE_FOREGROUND','PORTABLE_RADIO_SESSION',
'PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RADIO_CONTINUOUS_CAPTURE','RF_STORAGE_INSTANCE=0','RF_APP_DATA_INSTANCE=3','RF_RETURN_APP="springboard.elf"',
'PORTABLE_CONTEXTS_CLIENT','PORTABLE_NAVIGATION_RETENTION','PORTABLE_ASYNC_RGB565','PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_RUNTIME_APP_CATALOG',
'PORTABLE_RESIDENT_SHELL_CLIENT','PORTABLE_RESIDENT_POLICY','ALARM_SERVICE_TAGGED_V2',
'PORTABLE_QUICK_ATOMIC_PREFERENCES','PORTABLE_HOME_AUTOMATIC_PREFERENCES','PORTABLE_HOME_APP="springboard.elf"']
exports=['app_main','app_module_init','app_module_fini','risc_resident_app_descriptor_v1']
mapping=out/'exports.map';mapping.write_text('{ global: '+'; '.join(exports)+'; local: *; };\n')
sources=[ROOT/'Apps/waterfall.c',source/'adapter.c',source/'catalog.c',source/'portable_sleep.c',*[a.system/'lib/PortableApps/src'/name for name in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')],a.system/'lib/NativeApps/src/SingleFloatDivisionCompat.c']
includes=[source,ROOT/'Apps/shared_sdr',ROOT/'Apps',sdk,a.system/'lib/PortableApps/src',a.system/'lib/NativeApps/include',a.system/'Apps',a.watch/'include',a.watch/'apps/clock']
flags=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*['-D'+d for d in defines],*['-I'+str(i) for i in includes],'-include',str(a.watch/'apps/clock/faces/ContextsFaces.h')]
objects=[];commands=[];dependencies={}
for i,path in enumerate(sources):
 obj=out/(str(i)+'.o');dep=out/(str(i)+'.d')
 cmd=[a.compiler,*flags,'-MMD','-MF',dep,'-c',path,'-o',obj];commands.append(list(map(str,cmd)));run(cmd);objects.append(obj)
 import shlex
 for f in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):dependencies[f]=sha(f)
elf=out/'waterfall.elf'
cmd=[a.compiler,'-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--no-keep-memory','-Wl,--version-script='+str(mapping),*objects,'-lgcc','-o',elf];commands.append(list(map(str,cmd)));run(cmd)
sys.path.insert(0,str(ROOT/'scripts'))
from normalize_xtensa_relocations import normalize
normalize(elf)
symbols=subprocess.check_output([str(a.compiler).removesuffix('gcc')+'nm','-D',str(elf)],text=True)
actual={r.split()[-1] for r in symbols.splitlines() if len(r.split())>=3 and r.split()[-2] in ('T','D','B','R')}
imports={r.split()[-1] for r in symbols.splitlines() if ' U ' in ' '+r}
allowed={'risc_runtime_get_api','memcpy','memset','memcmp','memmove','strcmp','strncmp','strlen','snprintf','malloc','calloc','free','strcpy','memchr','strrchr'}
if actual!=set(exports) or not imports<=allowed:raise ValueError((actual,imports-allowed))
manifest=ROOT/'Apps/shared_sdr/waterfall.json';shutil.copyfile(manifest,out/'waterfall.json')
record={'schema':1,'source':head(ROOT),'source_pins':pins,'commands':commands,'dependencies':dependencies,'header_closure':closure,'sha256':sha(elf),'bytes':elf.stat().st_size,'manifest_sha256':sha(manifest),'exports':sorted(actual),'imports':sorted(imports),'compiler':subprocess.check_output([str(a.compiler),'--version'],text=True),'compiler_sha256':sha(a.compiler),'physical_testing':False}
(out/'build.json').write_text(json.dumps(record,indent=2)+'\n')
print('SHARED SDR ELF '+json.dumps({k:record[k] for k in ('sha256','bytes','exports','imports')}),flush=True)
