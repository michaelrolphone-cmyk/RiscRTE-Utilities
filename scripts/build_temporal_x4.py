#!/usr/bin/env python3
"""Build temporal Contexts/RF resident clients against explicitly supplied X4 sources.

The caller supplies the current resident System, native SDK, canonical display
SDK and verified tagged-alarm SDK. Records actual compiled dependencies. Does
not change historical builders or claim physical validation.
"""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
from normalize_xtensa_relocations import normalize
U=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('system','runtime','display-sdk','alarm-sdk','baseline','output','watch'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();S=a.system.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);inc=out/'sdk/include';inc.mkdir(parents=True,exist_ok=True)
for d in (S/'lib/PortableApps/include',a.runtime/'sdk/app',a.display_sdk,a.alarm_sdk,U/'lib/Contexts/include'):
 for f in d.glob('*.h'):shutil.copyfile(f,inc/f.name)
for name in ('PaperPresentation.h','PaperFrame.h'):
 f=inc/name
 if f.exists():f.unlink()
shutil.copytree(S/'lib/PortableApps/time',inc.parent/'time',dirs_exist_ok=True)
sys.path.insert(0,str(S/'scripts'));import portable_quick_build as quick
opts=argparse.Namespace(resident_shell_client=True,resident_shell_host=False,resident_policy=True,resident_runtime_sdk=a.runtime/'sdk/app',alarm_client=True,quick_actions=False,quick_radios=False,quick_usb_transfer=False,paper_transitions=False,home_app=None)
common,extra=quick.configure(opts,p,S,out,inc);assert not extra
sys.path.insert(0,str(a.watch.resolve()/'scripts'));from compact_current_elf import compact
rows=json.loads((S/'examples/x4-resident-cohort-catalog.json').read_text())['apps']
cat=out/'catalog.c';cat.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={'+','.join('{'+','.join('.'+k+'='+json.dumps(v) for k,v in sorted(row.items()))+',.compatible=true}' for row in rows)+'};\nconst unsigned portable_catalog_count='+str(len(rows))+';\n')
cc=os.environ['NATIVE_APP_CC'];compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0];assert '8.4.0' in compiler and '2021r2-patch5' in compiler
validator=out/'validate-elf';subprocess.run(['cc','-std=c11','-I'+str(U/'test/native_apps/stubs'),'-I'+str(U/'lib/elf_loader/include'),U/'lib/elf_loader/src/esp_elf_validate.c',U/'test/native_apps/validate_test.c','-o',validator],check=True)
base=['PORTABLE_NOVA_UI','PORTABLE_ALARM_CLIENT','ALARM_SERVICE_TAGGED_V2','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_INPUT_NAVIGATION','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_APP_LAUNCH_GUARD','PORTABLE_PAPER_PREFERENCES','PORTABLE_CONTEXTS_CLIENT','PORTABLE_UNPADDED_HOURS','PORTABLE_BLE_BROADCAST','PORTABLE_BLE_BROADCAST_DEFAULT_OFF']
records={};(out/'debug').mkdir(exist_ok=True)
for name in ('contexts','waterfall'):
 flags=['-D'+s for s in base]+common+['-DPORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO']+['-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_HOME_APP="default.elf"']
 if name=='contexts':flags+=['-DPORTABLE_CONTEXTS_PAPER','-DPORTABLE_CONTEXTS_EDITOR','-DPORTABLE_APP_TOUCH_SCROLL','-DPORTABLE_TOUCH_SCROLL']
 else:flags+=['-DPORTABLE_RADIO_SESSION','-DPORTABLE_RADIO_CONTINUOUS_CAPTURE','-DPORTABLE_BLE_FOREGROUND','-DPORTABLE_FORCE_FULL_FRAMES','-DRF_STORAGE_INSTANCE=8','-DRF_APP_DATA_INSTANCE=3','-DRF_RETURN_APP="springboard.elf"']
 sources=[U/'Apps'/(name+'.c'),S/'lib/PortableApps/src/adapter.c',*[S/'lib/PortableApps/src'/n for n in ('PortableNativeTimeSource.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')],S/'lib/NativeApps/src/SingleFloatDivisionCompat.c',cat]
 includes=[inc,S/'lib/PortableApps/include',S/'lib/NativeApps/include',S/'Apps',U/'Apps',U/'lib/Contexts/include',U/'lib/Bluetooth/include']
 target=out/(name+'.elf');mapping=out/(name+'.map');exports={'app_main','app_module_init','app_module_fini','risc_resident_app_descriptor_v1'};mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
 cmd=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,*['-I'+str(x) for x in includes],*map(str,sources),'-lgcc','-o',str(target)]
 if name=='waterfall':cmd=[x for x in cmd if x not in ('-ffunction-sections','-fdata-sections','-Wl,--gc-sections')]
 subprocess.run(cmd,check=True);normalize(target);subprocess.run([validator,target],check=True);proof=compact(target,cc,debug_path=out/'debug'/(name+'.elf'));subprocess.run([validator,target],check=True)
 syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',target],text=True);imports={x.split()[-1] for x in syms.splitlines() if ' U ' in ' '+x};actual={x.split()[-1] for x in syms.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','B','R')}
 allowed={'risc_runtime_get_api','memcpy','memset','memcmp','memmove','strcmp','strncmp','strlen','snprintf','malloc','calloc','free','strcpy','memchr','strrchr'};assert imports<=allowed and actual==exports,(name,imports-allowed,actual)
 allsyms=subprocess.check_output([cc.removesuffix('gcc')+'nm',out/'debug'/(name+'.elf')],text=True);assert not any(x.split()[-1].startswith(('pqa_render','pqa_sheet','pqa_font')) for x in allsyms.splitlines() if x.split())
 deps={}
 for source in sources:
  text=subprocess.check_output([cc,'-std=c11','-MM',*flags,*['-I'+str(x) for x in includes],source],text=True).replace('\\\n',' ')
  for tok in text.split()[1:]:
   f=Path(tok).resolve()
   if f.is_file():deps[str(f)]=hashlib.sha256(f.read_bytes()).hexdigest()
 records[name]={'compiler':compiler,'command':cmd,'dependencies':deps,'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'bytes':target.stat().st_size,'imports':sorted(imports),'exports':sorted(actual),'compaction':proof,'hardware_verified':False}
 old=json.loads((a.baseline/(name+'.json')).read_text());old['version']='0.3.0';(out/(name+'.json')).write_text(json.dumps(old,indent=2)+'\n');(out/'build.json').write_text(json.dumps(records,indent=2)+'\n');print(name+': resident target, descriptor, zero shared UI, ABI and loader PASS',flush=True)
