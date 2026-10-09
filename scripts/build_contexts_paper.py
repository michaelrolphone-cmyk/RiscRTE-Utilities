#!/usr/bin/env python3
"""Explicit X4 paper Contexts target; Watch/service inventories stay untouched."""
import argparse,hashlib,json,os,shutil,subprocess,importlib.util
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
RUNTIME='c77e717571a80bc18009b9c3a98532fee32e6fd2'
ALARM='637e13b0bce62ad49b756bec2468a6271d163fc7'
FLAGS=['PORTABLE_CONTEXTS_PAPER','PORTABLE_CONTEXTS_CLIENT','PORTABLE_CONTEXTS_EDITOR','PORTABLE_NOVA_UI','PORTABLE_ALARM_CLIENT','ALARM_SERVICE_TAGGED_V2','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_APP_LAUNCH_GUARD','PORTABLE_PAPER_PREFERENCES','PORTABLE_PAPER_TRANSITIONS','PORTABLE_APP_TOUCH_SCROLL','PORTABLE_TOUCH_SCROLL','PORTABLE_INPUT_NAVIGATION','PORTABLE_X4_IDLE_POLICY','PORTABLE_LOW_BATTERY','PORTABLE_APP_SLEEP_LOCAL']
SOURCES=['adapter.c','quick_actions.c','quick_render.c','quick_session.c','quick_radios.c','PortableNativeTimeSource.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
def git(repo,*a):return subprocess.check_output(['git','-C',str(repo),*a],text=True).strip()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def prepare(system,runtime,sdk,out):
 inc=out/'sdk/include';inc.mkdir(parents=True,exist_ok=True)
 for path in (system/'lib/PortableApps/include').glob('*.h'):shutil.copyfile(path,inc/path.name)
 shutil.copytree(system/'lib/PortableApps/time',inc.parent/'time',dirs_exist_ok=True)
 for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):(inc/name).write_bytes(subprocess.check_output(['git','-C',runtime,'show',RUNTIME+':sdk/app/'+name]))
 for name in ('AlarmServiceV1.h','AlarmServiceV2.h'):(inc/name).write_bytes(subprocess.check_output(['git','-C',ROOT,'show',ALARM+':lib/Alarm/include/'+name]))
 for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):shutil.copyfile(sdk/name,inc/name)
 for name in ('RiscLightSleepV1.h','RiscTimedSleepV1.h','RiscDeepSleepV1.h'):shutil.copyfile(runtime/'sdk/driver'/name,inc/name)
 return inc

def build(a):
 system=a.system_apps.resolve();runtime=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 cc=os.environ['NATIVE_APP_CC'];compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
 if '8.4.0' not in compiler or '2021r2-patch5' not in compiler:raise ValueError('Pinned GCC8.4 2021r2-patch5 required')
 inc=prepare(system,runtime,a.x4_idle_sdk,out)
 flags=['-D'+f for f in FLAGS]+['-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_HOME_APP="default.elf"']
 includes=['-I'+str(p) for p in (inc,ROOT/'Apps',ROOT/'lib/Contexts/include',ROOT/'lib/Alarm/include',system/'Apps',system/'lib/NativeApps/include')]
 sources=[ROOT/'Apps/contexts.c',*[system/'lib/PortableApps/src'/n for n in SOURCES],a.x4_idle_source]
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 mapping=out/'exports.map';mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
 elf=out/'contexts.elf'
 subprocess.run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*flags,*includes,*map(str,sources),str(catalog),'-lgcc','-o',str(elf)],check=True)
 symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',elf],text=True)
 imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s};exports={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
 allowed={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','calloc','free','strcpy','strncmp','memchr'}
 if not imports<=allowed or exports!={'app_main','app_module_init','app_module_fini'}:raise ValueError((imports-allowed,exports))
 validator=out/'validate-elf';subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator],check=True);subprocess.run([validator,elf],check=True)
 profile=json.loads((ROOT/'Apps/contexts-paper.json').read_text());grants=profile['required_grants']
 manifest={'type':'application','id':'contexts','version':profile['version'],'architecture':'xtensa-esp32s3','file_name':'contexts.elf','entry':'app_main','requires':[{'capability':g['capability'],'api':g['api']} for g in grants]}
 (out/'contexts.json').write_text(json.dumps(manifest,indent=2)+'\n');(out/'contexts.boot-policy.json').write_text(json.dumps({'manifest':'contexts.json','grants':grants},indent=2)+'\n')
 deps={}
 for source in sources:
  dep=subprocess.check_output([cc,'-std=c11','-M',*flags,*includes,source],text=True).replace('\\\n',' ')
  for token in dep.split()[1:]:
   path=Path(token).resolve()
   for label,base in [('CompiledSDK',inc),('Utilities',ROOT),('System',system)]:
    if path.is_relative_to(base):deps[label+'/'+str(path.relative_to(base))]=sha(path);break
   else:
    if path.is_file():deps['External/'+str(path)]=sha(path)
 receipt={'schema':1,'profile':profile['profile'],'version':profile['version'],'source_revision':git(ROOT,'rev-parse','HEAD'),'source_dirty':bool(git(ROOT,'status','--porcelain')),'system_revision':git(system,'rev-parse','HEAD'),'system_dirty':bool(git(system,'status','--porcelain')),'runtime_sdk_revision':RUNTIME,'alarm_sdk_revision':ALARM,'compiler':compiler,'elf_sha256':sha(elf),'elf_bytes':elf.stat().st_size,'imports':sorted(imports),'exports':sorted(exports),'build_defines':flags,'required_grants':grants,'compiled_dependencies_sha256':deps,'idle_helper_sha256':sha(a.x4_idle_source),'automatic_idle':'Light only, preserved drafts, no capture restart','hardware_verified':False,'publication':'none'}
 (out/'build-evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
 for folder in ('fonts','paper_fonts','quick_fonts'):
  target=out/'licenses'/folder;target.mkdir(parents=True,exist_ok=True)
  for p in (system/'lib/PortableApps'/folder).iterdir():
   if p.is_file() and ('LICENSE' in p.name or 'OFL' in p.name or p.name=='SOURCES.json'):shutil.copyfile(p,target/p.name)
 for repo,label in ((ROOT,'Utilities'),(system,'System'),(runtime,'Runtime')):shutil.copyfile(repo/'LICENSE',out/'licenses'/(label+'.txt'))
 print('Contexts paper '+profile['version']+' GCC8.4 target/imports/exports/loader PASS')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--x4-idle-source',type=Path,required=True);p.add_argument('--x4-idle-sdk',type=Path,required=True);p.add_argument('--output',type=Path,default=ROOT/'dist/contexts-paper');build(p.parse_args())
