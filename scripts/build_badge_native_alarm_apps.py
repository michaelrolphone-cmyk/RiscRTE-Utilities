#!/usr/bin/env python3
"""Explicit native UTC/API2 visual clients, not sleeping-alarm integration."""
import argparse,re,hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
import runtime_sdk_compat
ROOT=Path(__file__).resolve().parents[1]
PROFILE=json.loads((ROOT/'Apps/badge-native-utc-alarms.json').read_text())
FLAGS=['-DALARM_NATIVE_UTC','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_APP_LAUNCH_GUARD','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_DISPLAY_ROTATION=0','-DPORTABLE_TOUCH_ROTATION=0','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_RETURN_APP="default.elf"','-DPORTABLE_SAFE_VIEWPORT','-DPORTABLE_FORCE_FULL_FRAMES']
NATIVE_PRESENTATION_FLAGS=['-D'+name for name in (
 'PORTABLE_ALARM_NATIVE_PRESENTATION','PORTABLE_NOVA_UI','PORTABLE_APP_OWNS_TOUCH_CHROME',
 'PORTABLE_RF_NATIVE_CANVAS','PORTABLE_WATCH_SAFE_INSETS','PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_WATCH_COLOR_ONLY')]
NATIVE_PROFILE_PATH=ROOT/'Apps/badge-native-presentation-alarms.json'
def presentation_profile(selected):
 return json.loads(NATIVE_PROFILE_PATH.read_text()) if selected else PROFILE
TIME=['PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
IMPORTS={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'}
EXPORTS={'app_main','app_module_init','app_module_fini'}
def git(repo,*args):return subprocess.check_output(['git','-C',str(repo),*args],text=True).strip()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def revision(value):
 if not re.fullmatch(r"[0-9a-f]{40}",value):raise argparse.ArgumentTypeError("Full lowercase 40-character source SHA required")
 return value
def exact(repo,pin):
 revision(pin)
 if git(repo,'rev-parse','HEAD')!=pin or git(repo,'status','--porcelain'):raise ValueError('Clean exact source required: '+str(repo)+' @ '+pin)
def run(args,**kw):return subprocess.run(list(map(str,args)),check=True,**kw)
def stage(system,runtime,out,system_revision=PROFILE['system_sha'],clock_edit_authority=False):
 exact(system,system_revision);runtime_sdk_compat.verify(system,runtime,PROFILE['runtime_sha'])
 include=out/'sdk/include';shutil.copytree(system/'lib/PortableApps/include',include,dirs_exist_ok=True)
 shutil.copytree(system/'lib/PortableApps/time',out/'sdk/time',dirs_exist_ok=True)
 for path in (runtime/'sdk/app').glob('*.h'):(include/path.name).write_bytes(runtime_sdk_compat.header(runtime,PROFILE['runtime_sha'],path.name))
 spec=importlib.util.spec_from_file_location('badge_alarm_sdk',system/'scripts/portable_alarm_build.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
 if (helper.CLOCK_EDIT_COMMIT if clock_edit_authority else helper.UTILITIES_COMMIT)!=PROFILE['clock_edit_sdk_sha' if clock_edit_authority else 'alarm_sdk_sha']:raise ValueError('Unreviewed alarm SDK lineage')
 receipt=helper.stage(argparse.Namespace(tagged_alarm_utilities=ROOT,alarm_client=True,clock_edit_authority=clock_edit_authority),argparse.ArgumentParser(),out,include)
 for name in (helper.CLOCK_EDIT_HEADERS if clock_edit_authority else helper.HEADERS):
  expected=subprocess.check_output(['git','-C',str(ROOT),'show',PROFILE['clock_edit_sdk_sha' if clock_edit_authority else 'alarm_sdk_sha']+':lib/Alarm/include/'+name])
  if sha(include/name)!=hashlib.sha256(expected).hexdigest():raise ValueError('Working alarm ABI differs from pinned descriptor: '+name)
 return include,receipt
class PresentationArgumentParser(argparse.ArgumentParser):
 def parse_args(self,args=None,namespace=None):
  parsed=super().parse_args(args,namespace)
  if parsed.system_revision is None:
   parsed.system_revision=presentation_profile(parsed.native_alarm_presentation)['system_sha']
  return parsed

def argument_parser():
 p=PresentationArgumentParser(description=__doc__)
 for name in ['runtime','system-apps','output']:p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--system-revision',type=revision)
 p.add_argument('--clock-edit-authority',action='store_true')
 p.add_argument('--native-alarm-presentation',action='store_true',help='Opt in to content-local native Alarms/Countdown geometry')
 p.add_argument('--foreground-alarm-sleep',action='store_true')
 p.add_argument('--alarm-ready-drain',action='store_true',help='Budget immediately ready tagged alarm steps through the selected shared System helper')
 for option in ('foreground-idle-helper-dir','foreground-idle-runtime-dir','foreground-idle-drivers-sdk-dir'):
  p.add_argument('--'+option,type=Path)
 p.add_argument('--foreground-idle-fallback-seconds',type=int)
 p.add_argument('--cc',required=True)
 return p
def main():
 p=argument_parser();a=p.parse_args();profile=presentation_profile(a.native_alarm_presentation);a.system_revision=a.system_revision or profile['system_sha'];runtime=a.runtime.resolve();system=a.system_apps.resolve();out=a.output.resolve()
 if a.alarm_ready_drain and not a.foreground_alarm_sleep:p.error('Alarm ready drain requires tagged foreground alarm sleep')
 if out.exists() and any(out.iterdir()):p.error('Fresh output required')
 for key in ['CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','GCC_EXEC_PREFIX','COMPILER_PATH','LD_PRELOAD']:
  if os.environ.get(key):p.error('Ambient compiler input forbidden: '+key)
 compiler=subprocess.check_output([a.cc,'--version'],text=True).splitlines()[0]
 if 'esp-15.2.0_20251204' not in compiler or subprocess.check_output([a.cc,'-dumpfullversion'],text=True).strip()!='15.2.0':p.error('Pinned Xtensa GCC15.2 required')
 out.mkdir(parents=True,exist_ok=True);include,sdk=stage(system,runtime,out,a.system_revision,a.clock_edit_authority)
 idle=None;idle_flags=[];idle_sources=[]
 idle_selected=any(getattr(a,key) is not None for key in ('foreground_idle_helper_dir','foreground_idle_runtime_dir','foreground_idle_drivers_sdk_dir','foreground_idle_fallback_seconds'))
 if idle_selected or a.foreground_alarm_sleep or a.alarm_ready_drain:
  if not a.foreground_alarm_sleep:p.error('Dedicated alarm returning Light requires --foreground-alarm-sleep')
  from build_portable_apps import foreground_idle_module
  a.alarm_client=True;a.tagged_alarm_utilities=ROOT;a.navigation=True
  idle=foreground_idle_module(system,a,p)
  previous=sys.modules.pop('portable_alarm_build',None);sys.path.insert(0,str(system/'scripts'))
  try:include,idle_flags,idle_sources=idle.configure(a,p,system,out,include)
  finally:
   sys.path.pop(0);sys.modules.pop('portable_alarm_build',None)
   if previous is not None:sys.modules['portable_alarm_build']=previous
 selected_flags=[*FLAGS,*(NATIVE_PRESENTATION_FLAGS if a.native_alarm_presentation else []),*idle_flags,*(['-DALARM_SERVICE_CLOCK_EDIT_V2'] if a.clock_edit_authority else [])]
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 mapping=out/'exports.map';mapping.write_text('{ global: '+'; '.join(sorted(EXPORTS))+'; local: *; };\n')
 common=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror']
 includes=[include,ROOT/'lib/Alarm/include',system/'lib/NativeApps/include',system/'Apps']
 validator=out/'validate-elf';run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(runtime/'lib/elf_loader/include'),runtime/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator])
 rows=[]
 for name,version in profile['versions'].items():
  elf=out/(name+'.elf');sources=[ROOT/'Apps'/(name+'.c'),system/'lib/PortableApps/src/adapter.c',catalog,*[system/'lib/PortableApps/src'/n for n in TIME],*idle_sources]
  command=[a.cc,*common,*selected_flags,*['-I'+str(x) for x in includes],*sources,'-lgcc','-o',elf];run(command);run([validator,elf])
  symbols=subprocess.check_output([a.cc.removesuffix('gcc')+'nm','-D',elf],text=True)
  imports={l.split()[-1] for l in symbols.splitlines() if ' U ' in ' '+l};exports={l.split()[-1] for l in symbols.splitlines() if len(l.split())>=3 and l.split()[-2] in ('T','D','B','R')}
  if not imports<=IMPORTS or exports!=EXPORTS:raise ValueError((name,imports-IMPORTS,exports))
  grants=PROFILE['common_grants']+PROFILE['app_grants'][name]
  manifest={'type':'application','id':name,'version':version,'architecture':'xtensa-esp32s3','file_name':name+'.elf','entry':'app_main','requires':[dict(capability=c,api=v) for c,v in dict.fromkeys((g['capability'],g['api']) for g in grants)]}
  if idle:idle.requirements(a,manifest['requires'])
  (out/(name+'.json')).write_text(json.dumps(manifest,indent=2)+'\n')
  (out/(name+'.boot-policy.json')).write_text(json.dumps({'manifest':name+'.json','grants':[{k:g[k] for k in ['capability','api','instance_id']} for g in grants]},indent=2)+'\n')
  dependencies={}
  for source in sources:
   dep=subprocess.check_output([a.cc,'-std=c11','-M',*selected_flags,*['-I'+str(x) for x in includes],source],text=True).replace('\\\n',' ')
   for word in dep.split()[1:]:
    path=Path(word).resolve()
    if path.is_file():dependencies[str(path)]=sha(path)
  row={'native_alarm_geometry':a.native_alarm_presentation,'app':name,'version':version,'elf_sha256':sha(elf),'elf_bytes':elf.stat().st_size,'imports':sorted(imports),'exports':sorted(exports),'grants':grants,'compile_command':list(map(str,command)),'compiled_dependencies_sha256':dependencies}
  if idle:
   row['required_grants']=[{k:g[k] for k in ('capability','api','instance_id') if k in g} for g in grants]
   row['required_grants'] += [{'capability':c,'api':1} for c in idle.CAPABILITIES]
   idle.record(a,row,selected_flags,private_kv=(3,1),compile_command=list(map(str,command)))
   row.update(repository_commit=git(ROOT,'rev-parse','HEAD'),working_tree_dirty=bool(git(ROOT,'status','--porcelain')),system_revision=a.system_revision)
   (out/(name+'-build-record.json')).write_text(json.dumps(row,indent=2)+'\n')
  rows.append(row)
 loader=out/'target-loader'
 loader_command=[os.environ.get('CC','cc'),'-std=gnu11','-O1','-g','-I'+str(ROOT/'test/alarm-loader-stubs'),'-I'+str(runtime/'lib/elf_loader/include'),ROOT/'test/alarm_target_loader.c',runtime/'lib/elf_loader/src/esp_elf.c',runtime/'lib/elf_loader/src/arch/esp_elf_xtensa.c',runtime/'lib/elf_loader/src/esp_elf_validate.c','-o',loader]
 run(loader_command)
 loader_result=subprocess.check_output([str(loader),*[str(out/(name+'.elf')) for name in PROFILE['versions']]],text=True);print(loader_result,end='')
 licenses=out/'licenses';licenses.mkdir(exist_ok=True)
 for repo,name in [(ROOT,'Utilities-MIT.txt'),(system,'System-MIT.txt'),(runtime,'Runtime-MIT.txt')]:shutil.copy2(repo/'LICENSE',licenses/name)
 shutil.copy2(system/'lib/PortableApps/time/TIMEZONE_PROVENANCE.json',licenses/'TIMEZONE_PROVENANCE.json')
 for folder in ['fonts','paper_fonts','quick_fonts']:
  dest=licenses/folder;dest.mkdir(exist_ok=True)
  for path in (system/'lib/PortableApps'/folder).iterdir():
   if path.is_file() and ('LICENSE' in path.name or 'OFL' in path.name or path.name=='SOURCES.json'):shutil.copy2(path,dest/path.name)
 receipt={'schema':1,'profile':profile['profile'],'native_alarm_geometry':a.native_alarm_presentation,'utilities_sha':git(ROOT,'rev-parse','HEAD'),'working_tree_dirty':bool(git(ROOT,'status','--porcelain')),'system_sha':a.system_revision,'runtime_sha':PROFILE['runtime_sha'],'runtime_sdk_identity':runtime_sdk_compat.verify(system,runtime,PROFILE['runtime_sha']),'alarm_sdk':sdk,'compiler':compiler,'compiler_sha256':sha(Path(a.cc).resolve()),'build_defines':selected_flags,'apps':rows,'loader_command':list(map(str,loader_command)),'loader_result':loader_result,'loader_sources_sha256':{str(f):sha(f) for f in [ROOT/'test/alarm_target_loader.c',runtime/'lib/elf_loader/src/esp_elf.c',runtime/'lib/elf_loader/src/arch/esp_elf_xtensa.c',runtime/'lib/elf_loader/src/esp_elf_validate.c']},'profile_sha256':sha(ROOT/'Apps/badge-native-utc-alarms.json'),'builder_sha256':sha(__file__),'licenses_sha256':{str(f.relative_to(licenses)):sha(f) for f in licenses.rglob('*') if f.is_file()},'hardware_verified':False,'sleeping_alarm_integration':False,'presentation':profile['presentation'],'selected_presentation_profile_sha256':sha(NATIVE_PROFILE_PATH) if a.native_alarm_presentation else None}
 (out/'build-evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print('Badge native UTC/API2 Alarms and Countdown: target ELF/import/export validation passed; sleeping alarms and hardware unqualified')
if __name__=='__main__':main()
