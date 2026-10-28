#!/usr/bin/env python3
"""Explicit native-UTC visual service profiles, never a product install."""
import argparse,re,hashlib,json,os,subprocess
from pathlib import Path
from normalize_xtensa_relocations import normalize
import runtime_sdk_compat
ROOT=Path(__file__).resolve().parents[1]
RUNTIME='273bdbccf65647a803ab0586a418f8283732baec'
SYSTEM='6e4cda9072905c15b9a5ebc9b22913fec444a89d'
BASE='1eb8948ab4d7fcc20ae467638d8b4867e966316b'
DEFINES=['ALARM_NATIVE_UTC','ALARM_NATIVE_SCHEDULES_ONLY','ALARM_SERVICE_TAGGED_V2','ALARM_VISUAL_ONLY','ALARM_DND_CONTROL']
def run(command,**kw):return subprocess.check_output(list(map(str,command)),text=True,**kw)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def revision(value):
 if not re.fullmatch(r"[0-9a-f]{40}",value):raise argparse.ArgumentTypeError("Full lowercase 40-character source SHA required")
 return value
def exact(root,pin):
 revision(pin)
 if run(['git','-C',root,'rev-parse','HEAD']).strip()!=pin or run(['git','-C',root,'status','--porcelain']).strip():raise ValueError('Clean exact source required: '+str(root)+' @ '+pin)
def includes(runtime,system):return [ROOT/'lib/Alarm/include',runtime/'sdk/driver',runtime/'sdk/app',system/'lib/PortableApps/include']
def selection(clock_edit_authority,include_points=False):
 if include_points and not clock_edit_authority:raise ValueError('include-points requires clock-edit-authority')
 defines=[*DEFINES,*(['ALARM_SERVICE_CLOCK_EDIT_V2'] if clock_edit_authority else [])]
 if include_points:
  defines.remove('ALARM_NATIVE_SCHEDULES_ONLY')
  defines+=['POINTS_IN_TIME_SERVICE','POINTS_SERVICE_CONTEXT','ALARM_NATIVE_MIXED_POINTS']
 return defines

def profile_files(clock_edit_authority,include_points=False):
 selection(clock_edit_authority,include_points)
 stem='native-utc-mixed-points-clock-edit' if include_points else 'native-utc-clock-edit' if clock_edit_authority else 'native-utc-schedules'
 return ROOT/'Services/alarm_service'/(stem+'-manifest.json'),ROOT/'Services/alarm_service'/(stem+'-storage-policy.example.json')

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['runtime','system-apps','output']:p.add_argument('--'+name,required=True,type=Path)
 p.add_argument('--system-revision',type=revision,default=SYSTEM)
 p.add_argument('--include-points',action='store_true');p.add_argument('--clock-edit-authority',action='store_true');p.add_argument('--cc',required=True);a=p.parse_args();runtime,system,out=[x.resolve() for x in [a.runtime,a.system_apps,a.output]]
 if a.include_points and not a.clock_edit_authority:p.error('include-points requires clock-edit-authority')
 exact(system,a.system_revision);runtime_identity=runtime_sdk_compat.verify(system,runtime,RUNTIME)
 for key in ['CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','GCC_EXEC_PREFIX','COMPILER_PATH','LD_PRELOAD']:
  if os.environ.get(key):raise ValueError('Ambient compiler input forbidden: '+key)
 cc=Path(a.cc).absolute();compiler=run([cc,'--version']).splitlines()[0]
 if 'esp-15.2.0_20251204' not in compiler or run([cc,'-dumpfullversion']).strip()!='15.2.0':raise ValueError('Pinned GCC15.2 required')
 if out.exists():raise ValueError('Fresh output required')
 out.mkdir(parents=True);mapping=out/'exports.map';mapping.write_text('{ global: t5_driver_get; local: *; };\n')
 defines=selection(a.clock_edit_authority,a.include_points)
 units=[ROOT/'Services/alarm_service/service.c']
 if a.include_points:units += [system/'lib/PortableApps/src/PortableTimeZone.c',system/'lib/PortableApps/src/PortableTimeZoneCatalog.c']
 inc=['-I'+str(x) for x in includes(runtime,system)]
 common=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wall','-Wextra','-Werror','-Wl,--version-script='+str(mapping)]
 elf=out/'driver.elf';command=[*common,*['-D'+x for x in defines],*inc,*units,'-lgcc','-o',elf];run(command);normalize(elf)
 symbols=run([str(cc).removesuffix('gcc')+'nm','-D',elf]);imports={x.split()[-1] for x in symbols.splitlines() if ' U ' in ' '+x};exports={x.split()[-1] for x in symbols.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','R','B')}
 if not imports<={'memcpy','memset','memcmp','strcmp','strlen'} or exports!={'t5_driver_get'}:raise ValueError((imports,exports))
 # Exact pre-change source with the same compiler/SDK: neither legacy profile
 # is replaced or reinterpreted by the new opt-in selection.
 baseline=out/'baseline';baseline.mkdir();old=baseline/'service.c';old.write_text(run(['git','-C',ROOT,'show',BASE+':Services/alarm_service/service.c']))
 parity={}
 for label,flags in [('watch',['POINTS_IN_TIME_SERVICE','ALARM_DND_CONTROL','ALARM_VOLUME_CONTROL','PORTABLE_RTC_UTC8_DENVER']),('native-points',['POINTS_IN_TIME_SERVICE','ALARM_DND_CONTROL','ALARM_VISUAL_ONLY','ALARM_SERVICE_TAGGED_V2','ALARM_NATIVE_UTC'])]:
  sources=[system/'lib/PortableApps/src/PortableTimeZone.c',system/'lib/PortableApps/src/PortableTimeZoneCatalog.c'] if label=='native-points' else []
  files=[]
  for name,source in [('before',old),('after',ROOT/'Services/alarm_service/service.c')]:
   target=baseline/(label+'-'+name+'.elf');run([*common,*['-D'+x for x in flags],*inc,source,*sources,'-lgcc','-o',target]);files.append(target)
  assert files[0].read_bytes()==files[1].read_bytes(),label+' legacy source parity changed';parity[label]=sha(files[0])
 loader=out/'target-loader';loader_command=[os.environ.get('CC','cc'),'-std=gnu11','-O1','-g','-I'+str(ROOT/'test/alarm-loader-stubs'),'-I'+str(runtime/'lib/elf_loader/include'),ROOT/'test/alarm_target_loader.c',runtime/'lib/elf_loader/src/esp_elf.c',runtime/'lib/elf_loader/src/arch/esp_elf_xtensa.c',runtime/'lib/elf_loader/src/esp_elf_validate.c','-o',loader]
 run(loader_command);result=run([loader,elf]);print(result,end='')
 manifest,policy=profile_files(a.clock_edit_authority,a.include_points);(out/'manifest.json').write_bytes(manifest.read_bytes());(out/'storage-policy.json').write_bytes(policy.read_bytes())
 deps=[token for token in run([cc,'-std=c11','-M',*['-D'+x for x in defines],*inc,*units]).replace('\\\n',' ').split() if not token.endswith(':')]
 sources={str(Path(x).resolve()):sha(x) for x in deps};sources.update({str(x.resolve()):sha(x) for x in [manifest,policy,Path(__file__),ROOT/'scripts/normalize_xtensa_relocations.py',ROOT/'scripts/runtime_sdk_compat.py']})
 licenses=out/'licenses';licenses.mkdir()
 for root,name in [(ROOT,'Utilities-MIT.txt'),(runtime,'Runtime-MIT.txt'),(system,'System-MIT.txt')]:(licenses/name).write_bytes((root/'LICENSE').read_bytes())
 receipt={'schema':1,'profile':'native-utc-mixed-points-clock-edit' if a.include_points else 'native-utc-schedules-clock-edit' if a.clock_edit_authority else 'native-utc-schedules-only','clock_edit_authority':a.clock_edit_authority,'version':json.loads(manifest.read_text())['version'],'source_revision':run(['git','-C',ROOT,'rev-parse','HEAD']).strip(),'working_tree_dirty':bool(run(['git','-C',ROOT,'status','--porcelain']).strip()),'runtime_revision':RUNTIME,'runtime_sdk_identity':runtime_identity,'runtime_checkout_revision':runtime_identity['checkout_commit'],'system_revision':a.system_revision,'compiler':compiler,'compiler_sha256':sha(cc.resolve()),'target_optimization':'-Os','optimization_note':'Elapsed durations narrowed to32 bits only after checked5-second orUINT32_MAX bounds; original-Os single-pass recipe, no64-bit division helper import' if a.clock_edit_authority else None,'defines':defines,'command':list(map(str,command)),'dependencies_sha256':sources,'elf_sha256':sha(elf),'elf_bytes':elf.stat().st_size,'imports':sorted(imports),'exports':sorted(exports),'bound_keys':json.loads(policy.read_text()),'legacy_baseline':BASE,'same_compiler_legacy_elf_parity':parity,'target_loader':result,'loader_command':list(map(str,loader_command)),'loader_sources_sha256':{str(f):sha(f) for f in [runtime/'lib/elf_loader/src/esp_elf.c',runtime/'lib/elf_loader/src/arch/esp_elf_xtensa.c',runtime/'lib/elf_loader/src/esp_elf_validate.c']},'licenses_sha256':{x.name:sha(x) for x in licenses.iterdir()},'points_selected':a.include_points,'points_context_selected':a.include_points,'hardware_verified':False,'sleeping_alarm_integration':False}
 (out/'build-evidence.json').write_text(json.dumps(receipt,indent=2)+'\n');print(('Mixed Points clock-editor target, ten-key boundKV2 and legacy-profile byte parity PASS' if a.include_points else 'Clock-editor target, seven-key boundKV2 and legacy-profile byte parity PASS' if a.clock_edit_authority else 'Schedules-only target and legacy-profile byte parity PASS')+'; full product/hardware qualification separate')
if __name__=='__main__':main()
