#!/usr/bin/env python3
"""Explicit temporal Contexts successor build; historical pinned lanes stay intact.

Rebuild the full Watch app cohort with the shared temporal adapter. Preserve the
accepted HID foreground pair (which deliberately suspends Contexts while using
Bluetooth). Hardware drivers other than IQ are retained during packaging.
"""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
from normalize_xtensa_relocations import normalize
U=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('watch','system','productivity','runtime','drivers','baseline','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();W=a.watch.resolve();S=a.system.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(W/'scripts'))
from build_current_apps import application_inputs
from build_contexts_cohort import definitions,write_catalog
from contexts_profile import catalog,APPS
from current_apps_overlay import SYSTEM_APPS,UTILITY_APPS,CLOCK_APPS
from compact_current_elf import compact
cc=os.environ['NATIVE_APP_CC'];compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
assert '8.4.0' in compiler and '2021r2-patch5' in compiler
repos={'system-apps':S,'utilities':U,'productivity':a.productivity.resolve(),'runtime':a.runtime.resolve()}
rows=catalog(W);write_catalog(out,rows)
# Every eligible foreground can run a launch workflow, so each receives the real catalogue.
(out/'empty_catalog.c').write_bytes((out/'catalog.c').read_bytes())
validator=out/'validate-elf';subprocess.run(['cc','-std=c11','-I'+str(U/'test/native_apps/stubs'),'-I'+str(U/'lib/elf_loader/include'),U/'lib/elf_loader/src/esp_elf_validate.c',U/'test/native_apps/validate_test.c','-o',validator],check=True)
records={};(out/'debug').mkdir(exist_ok=True)
def build(name,sources,flags,includes,exports):
 flags=[x for x in flags if x!='-flto'] if name in ('audio_spectrum','waterfall') else list(flags)
 if '-flto' in flags and any(x.name=='SingleFloatDivisionCompat.c' for x in sources):flags+=['-Wl,--undefined=__divsf3']
 target=out/(name+'.elf');mapping=out/(name+'.map');mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
 cmd=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,*['-I'+str(x) for x in includes],*map(str,sources),'-lgcc','-o',str(target)]
 if name in ('audio_spectrum','waterfall'):cmd=[x for x in cmd if x not in ('-ffunction-sections','-fdata-sections','-Wl,--gc-sections')]
 subprocess.run(cmd,check=True);normalize(target);subprocess.run([validator,target],check=True)
 proof=compact(target,cc,debug_path=out/'debug'/(name+'.elf'));subprocess.run([validator,target],check=True)
 symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',target],text=True)
 imported={x.split()[-1] for x in symbols.splitlines() if ' U ' in ' '+x};exported={x.split()[-1] for x in symbols.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','B','R')}
 allowed={'risc_runtime_get_api','memcpy','memset','memcmp','memmove','strcmp','strncmp','strlen','snprintf','malloc','calloc','free','strcpy','memchr','strrchr'}
 assert imported<=allowed and exported==exports,(name,imported-allowed,exported)
 deps={}
 for source in sources:
  if source.suffix=='.o':continue
  dep=subprocess.check_output([cc,'-std=c11','-MM',*flags,*['-I'+str(x) for x in includes],str(source)],text=True).replace('\\\n',' ')
  for tok in dep.split()[1:]:
   f=Path(tok).resolve()
   if f.is_file():deps[str(f)]=hashlib.sha256(f.read_bytes()).hexdigest()
 records[name]={'compiler':compiler,'command':cmd,'dependencies':deps,'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'bytes':target.stat().st_size,'imports':sorted(imported),'exports':sorted(exported),'compaction':proof,'hardware_verified':False}
 (out/'build.json').write_text(json.dumps(records,indent=2)+'\n');print(name+': target, ABI and loader PASS',flush=True)
for name in APPS:
 if name in (*CLOCK_APPS,'ble_touchpad','ble_buttons'):continue
 old=json.loads((a.baseline/(name+'.json')).read_text());v=list(map(int,old['version'].split('.')));v[-1]+=1;version='.'.join(map(str,v))
 if name=='contexts':version='0.3.1'
 if name=='waterfall':version='0.3.1'
 if name=='audio_spectrum':version='0.5.1'
 owner='system-apps' if name in SYSTEM_APPS else 'utilities' if name in (*UTILITY_APPS,'contexts') else 'productivity'
 source=repos[owner]/'Apps'/('timecard_portable.c' if name=='timecard' else name+'.c')
 sources,includes,_=application_inputs(name,source,repos,W,out,'runtime-features')
 helper=S/'lib/NativeApps/src/SingleFloatDivisionCompat.c'
 if name in ('audio_spectrum','contexts') and helper not in sources:sources.append(helper)
 includes+=[U/'lib/Contexts/include']
 build(name,sources,definitions(name,version,W),includes,{'app_main','app_module_init','app_module_fini'})
 old['version']=version;(out/(name+'.json')).write_text(json.dumps(old,indent=2)+'\n')
effect=out/'boot-effect.o'
subprocess.run([cc.removesuffix('gcc')+'g++','-std=c++11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-fno-exceptions','-fno-rtti','-fno-threadsafe-statics','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(W/'sdk/driver'),'-c',str(W/'apps/clock/effects/boot.cpp'),'-o',effect],check=True)
for name in CLOCK_APPS:
 if name=='clock':
  build(name,[W/'apps/clock/return_to_default.c'],[],[],{'app_main'})
  records[name]['role']='default-forwarder'
  (out/'build.json').write_text(json.dumps(records,indent=2)+'\n')
  old=json.loads((a.baseline/(name+'.json')).read_text());old.update(version='0.11.1',requires=[])
  (out/(name+'.json')).write_text(json.dumps(old,indent=2)+'\n')
  continue
 sources=[W/'apps/clock/crown.c',W/'apps/clock/nova/nova.c',*[S/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')],W/'apps/clock/points_projection.c',W/'apps/clock/effects/divdi3.c',effect]
 includes=[S/'lib/PortableApps/include',U/'lib/Alarm/include',U/'lib/Contexts/include',W/'sdk/app',W/'sdk/driver',W/'include']
 build(name,sources,definitions(name,'0.11.1',W),includes,{'app_main'})
 old=json.loads((a.baseline/(name+'.json')).read_text());old['version']='0.11.1';(out/(name+'.json')).write_text(json.dumps(old,indent=2)+'\n')
