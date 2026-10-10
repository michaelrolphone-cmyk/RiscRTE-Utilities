#!/usr/bin/env python3
"""Tagged profiles and byte-identical public Watch control; no product install."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PIN=json.loads((ROOT/'sdk/native-utc-alarm-sources.json').read_text())
def run(args,**kwargs):return subprocess.run([str(x) for x in args],check=True,**kwargs)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def verify(system,runtime):
 if subprocess.check_output(['git','rev-parse','HEAD'],cwd=system,text=True).strip()!=PIN['system_sha'] or subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=system,text=True).strip():raise ValueError('Clean exact System 1d589d90 required')
 for path,digest in PIN['system_sources'].items():
  if sha(system/path)!=digest:raise ValueError('Frozen System input differs: '+path)
 if subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip()!=PIN['runtime_sha'] or subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=runtime,text=True).strip():raise ValueError('Clean exact Runtime 0.1.52 required')
def inputs(system,runtime):
 return [ROOT/'lib/Alarm/include',runtime/'sdk/driver',runtime/'sdk/app',system/'lib/PortableApps/include']
def build(system,runtime,skip_loader=False):
 verify(system,runtime)
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
 if not cc:raise ValueError('Set NATIVE_APP_CC to the existing pinned GCC8.4 compiler')
 compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
 if '8.4.0' not in compiler:raise ValueError('Pinned GCC8.4 required: '+compiler)
 out=ROOT/'dist/native-utc-visual-service';out.mkdir(parents=True,exist_ok=True)
 mapping=out/'exports.map';mapping.write_text('{ global: t5_driver_get; local: *; };\n')
 flags=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wall','-Wextra','-Werror','-Wl,--version-script='+str(mapping)]
 baseline=out/'baseline';(baseline/'include').mkdir(parents=True,exist_ok=True)
 # Traverse the exact base tree, never a working-copy approximation.
 names=subprocess.check_output(['git','ls-tree','-r','--name-only',PIN['base_sha'],'lib/Alarm/include/'],cwd=ROOT,text=True).splitlines()
 for name in names:(baseline/'include'/Path(name).name).write_bytes(subprocess.check_output(['git','show',PIN['base_sha']+':'+name],cwd=ROOT))
 (baseline/'service.c').write_bytes(subprocess.check_output(['git','show',PIN['base_sha']+':Services/alarm_service/service.c'],cwd=ROOT))
 variants={'watch':['-DPOINTS_IN_TIME_SERVICE','-DALARM_DND_CONTROL','-DALARM_VOLUME_CONTROL','-DPORTABLE_RTC_UTC8_DENVER'],'tagged-watch':['-DALARM_SERVICE_TAGGED_V2','-DPOINTS_IN_TIME_SERVICE','-DALARM_DND_CONTROL','-DALARM_VOLUME_CONTROL','-DPORTABLE_RTC_UTC8_DENVER'],'raw-visual':['-DALARM_SERVICE_TAGGED_V2','-DPOINTS_IN_TIME_SERVICE','-DALARM_DND_CONTROL','-DALARM_VISUAL_ONLY'],'native-utc':['-DALARM_SERVICE_TAGGED_V2','-DPOINTS_IN_TIME_SERVICE','-DALARM_DND_CONTROL','-DALARM_VISUAL_ONLY','-DALARM_NATIVE_UTC']}
 rows=[]
 for name,defines in variants.items():
  elf=out/(name+'.elf');sources=[ROOT/'Services/alarm_service/service.c']
  if name=='native-utc':sources += [system/p for p in PIN['system_sources'] if p.endswith('.c')]
  inc=inputs(system,runtime)
  run([cc,*flags,*defines,*['-I'+str(p) for p in inc],*sources,'-lgcc','-o',elf])
  same=None
  if name=='watch':
   before=out/(name+'-baseline.elf');run([cc,*flags,*defines,'-I'+str(baseline/'include'),*['-I'+str(p) for p in inc[1:]],baseline/'service.c','-lgcc','-o',before]);same=elf.read_bytes()==before.read_bytes()
   if not same:raise ValueError('Flag-off target bytes changed: '+name)
  symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
  imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s};exports={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
  if not imports<={'memcpy','memset','memcmp','strcmp','strlen'} or exports!={'t5_driver_get'}:raise ValueError((name,imports,exports))
  rows.append({'profile':name,'sha256':sha(elf),'size_bytes':elf.stat().st_size,'defines':defines,'imports':sorted(imports),'exports':sorted(exports),'exact_base_bytes':same})
 validator=out/'validate-elf';run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator])
 for row in rows:run([validator,out/(row['profile']+'.elf')])
 if not skip_loader:
  loader=out/'target-loader';run([os.environ.get('CC','cc'),'-std=gnu11','-O1','-g','-I'+str(ROOT/'test/alarm-loader-stubs'),'-I'+str(runtime/'lib/elf_loader/include'),ROOT/'test/alarm_target_loader.c',runtime/'lib/elf_loader/src/esp_elf.c',runtime/'lib/elf_loader/src/arch/esp_elf_xtensa.c',runtime/'lib/elf_loader/src/esp_elf_validate.c','-o',loader])
  run([loader,*[out/(row['profile']+'.elf') for row in rows]])
 licenses=out/'licenses';licenses.mkdir(exist_ok=True);(licenses/'Utilities-MIT.txt').write_bytes((ROOT/'LICENSE').read_bytes());(licenses/'System-Reader-MIT.txt').write_bytes((system/'LICENSE').read_bytes());(licenses/'TIMEZONE_PROVENANCE.json').write_bytes((system/'lib/PortableApps/time/TIMEZONE_PROVENANCE.json').read_bytes())
 (out/'driver.elf').write_bytes((out/'native-utc.elf').read_bytes());(out/'manifest.json').write_bytes((ROOT/'Services/alarm_service/native-utc-visual-manifest.json').read_bytes())
 record={'target_loader':'skipped explicitly on this host' if skip_loader else 'passed eight alignments','schema':1,'profile':'native-utc-visual-only','service_version':'0.4.4','required_provider_bound_keys':9,'runtime_sha':PIN['runtime_sha'],'system_policy_sha':PIN['system_sha'],'system_checkout_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=system,text=True).strip(),'compiler':compiler,'source_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'modules':rows,'sources':{p:sha(ROOT/p) for p in ['Services/alarm_service/service.c','Services/alarm_service/native-utc-visual-manifest.json','Services/alarm_service/native-utc-visual-storage-policy.example.json',*sorted(str(p.relative_to(ROOT)) for p in (ROOT/'lib/Alarm/include').glob('*.h'))]},'system_sources':PIN['system_sources'],'hardware_verified':False}
 (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n');print('Native UTC target ELF passed; Watch ELF exactly matches public paper base bytes')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--skip-loader',action='store_true',help='Record target loader as unverified (e.g. incompatible host); CI must not use this');a=p.parse_args();build(a.system_apps.resolve(),a.runtime.resolve(),a.skip_loader)
