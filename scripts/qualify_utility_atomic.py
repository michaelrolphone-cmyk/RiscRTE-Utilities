#!/usr/bin/env python3
"""Single-job experimental X4 ELFs and exact flag-off Utilities-base comparison."""
import argparse,hashlib,io,json,os,subprocess,tarfile
from pathlib import Path
from build_contexts_scene import stage
from native_utility_build import flags,sources,IMPORTS,EXPORTS
from test_utility_atomic import PROFILE,exact
ROOT=Path(__file__).resolve().parents[1]
def run(cmd):subprocess.run(list(map(str,cmd)),check=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('system-apps','runtime','output','xtensa-cc'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--target',choices=['x4'],required=True);a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 exact(a.system_apps,PROFILE['system_sha']);exact(a.runtime,PROFILE['runtime_sdk_sha'])
 inc=stage(a.runtime.resolve(),a.system_apps.resolve(),a.output/'sdk')
 baseline=a.output/'baseline';baseline.mkdir(exist_ok=True)
 with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['git','-C',ROOT,'archive',PROFILE['utilities_base'],'Apps','lib/Bluetooth/include']))) as archive:archive.extractall(baseline,filter='data')
 catalog=a.output/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 validator=a.output/'validate';run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator])
 rows=[]
 for name in ('battery','ble_scanner','contexts'):
  exports={'app_main','risc_resident_app_descriptor_v1'} if name=='contexts' else EXPORTS
  mapping=a.output/(name+'.map');mapping.write_text('{ global: '+'; '.join(sorted(exports))+'; local: *; };\n')
  common=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror']
  default=['-DCONTEXTS_ALARM_API=2','-DCONTEXTS_SETTING_MASK=0x274u','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO'] if name=='contexts' else flags(name,paper_transitions=True)
  if name=='ble_scanner':default+=['-DPORTABLE_TEXT_INPUT_CLIENT']
  if name=='battery':default+=['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF','-DPORTABLE_PAPER_PREFERENCES']
  for mode,repo in ([('atomic',ROOT),('default',ROOT),('baseline',baseline)]+([('atomic-shared',ROOT)] if name=='contexts' else [])):
   defines=default+(['-D'+d for d in PROFILE['defines']] if mode in ('atomic','atomic-shared') else [])
   if mode=='atomic-shared':defines+=['-DPORTABLE_CONTEXTS_SHARED_TEXT_INPUT']
   includes=['-I'+str(x) for x in [inc,a.system_apps/'lib/NativeApps/include',repo/'Apps',repo/'lib/Bluetooth/include',a.system_apps/'Apps']]
   src=[repo/'Apps/contexts.c'] if name=='contexts' else sources(name,a.system_apps,repo=repo)
   elf=a.output/(name+'-'+mode+'.elf');run([a.xtensa_cc,*common,*defines,*includes,*src,catalog,'-lgcc','-o',elf]);run([validator,elf])
   symbols=subprocess.check_output([str(a.xtensa_cc).removesuffix('gcc')+'nm','-D',elf],text=True)
   imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s};found={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
   allowed=IMPORTS|{'memchr','strncmp','strnlen','strchr'}
   assert imports<=allowed and found==exports,(name,mode,imports-allowed,found)
   rows.append({'app':name,'mode':mode,'sha256':sha(elf),'bytes':elf.stat().st_size,'imports':sorted(imports),'exports':sorted(found),'defines':defines})
  assert (a.output/(name+'-default.elf')).read_bytes()==(a.output/(name+'-baseline.elf')).read_bytes(),name+' flag-off bytes changed'
  print(name+': target atomic and default/base byte identity PASS',flush=True)
 # Selecting atomic without the explicit X4 target must fail at preprocessing.
 probe=a.output/'gate.c';probe.write_text('#include "utility_preference_atomic.h"\n')
 result=subprocess.run([a.xtensa_cc,'-E','-DPORTABLE_UTILITIES_ATOMIC_PREFERENCES','-I'+str(inc),'-I'+str(ROOT/'Apps'),probe],capture_output=True,text=True)
 assert result.returncode and 'explicit X4 experimental profile' in result.stderr
 record={'schema':1,'repository_sha':subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','-C',ROOT,'status','--porcelain','--untracked-files=no'],text=True).strip()),'hardware_tested':False,'published':False,'profile':PROFILE,'system_actual_sha':subprocess.check_output(['git','-C',a.system_apps,'rev-parse','HEAD'],text=True).strip(),'modules':rows,'flag_off_exact_base':True,'non_x4_rejected':True,'compiler':subprocess.check_output([a.xtensa_cc,'--version'],text=True).splitlines()[0],'sdk_sha256':{p.name:sha(p) for p in inc.glob('*.h')}}
 (a.output/'receipt.json').write_text(json.dumps(record,indent=2)+'\n')
if __name__=='__main__':main()
