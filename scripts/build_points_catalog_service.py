#!/usr/bin/env python3
"""Build reserved catalog services and prove unchanged legacy target bytes."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PIN=json.loads((ROOT/'sdk/points-catalog-service-sources.json').read_text())
def run(argv):subprocess.run(list(map(str,argv)),check=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args();runtime=a.runtime.resolve();system=a.system_apps.resolve()
 if subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip()!=PIN['runtime_sha']:raise ValueError('Exact provider-bound Runtime checkpoint required')
 for checkout,key in [(runtime,'runtime_inputs'),(system,'system_inputs')]:
  for path,digest in PIN[key].items():
   if sha(checkout/path)!=digest:raise ValueError('Frozen input differs: '+path)
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
 if not cc:raise ValueError('Set NATIVE_APP_CC to pinned GCC8.4')
 compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
 if '8.4.0' not in compiler:raise ValueError('Pinned GCC8.4 required')
 out=ROOT/'dist/points-catalog-service';out.mkdir(parents=True,exist_ok=True)
 mapping=out/'exports.map';mapping.write_text('{ global: t5_driver_get; local: *; };\n')
 flags=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wall','-Wextra','-Werror','-Wl,--version-script='+str(mapping)]
 includes=[ROOT/'lib/Alarm/include',runtime/'sdk/driver',runtime/'sdk/app',system/'lib/PortableApps/include']
 baseline=out/'baseline';(baseline/'include').mkdir(parents=True,exist_ok=True)
 for name in subprocess.check_output(['git','ls-tree','-r','--name-only',PIN['base_sha'],'lib/Alarm/include/'],cwd=ROOT,text=True).splitlines():
  (baseline/'include'/Path(name).name).write_bytes(subprocess.check_output(['git','show',PIN['base_sha']+':'+name],cwd=ROOT))
 (baseline/'service.c').write_bytes(subprocess.check_output(['git','show',PIN['base_sha']+':Services/alarm_service/service.c'],cwd=ROOT))
 rows=[]
 for native in (False,True):
  name='native-utc' if native else 'watch'
  defines=['-DPOINTS_IN_TIME_SERVICE','-DALARM_DND_CONTROL']+(['-DALARM_NATIVE_UTC','-DALARM_VISUAL_ONLY','-DALARM_SERVICE_TAGGED_V2'] if native else ['-DALARM_VOLUME_CONTROL','-DPORTABLE_RTC_UTC8_DENVER'])
  extra=[system/p for p in PIN['system_inputs'] if p.endswith('.c')] if native else []
  legacy=out/(name+'-legacy.elf');before=out/(name+'-baseline.elf')
  run([cc,*flags,*defines,*['-I'+str(i) for i in includes],ROOT/'Services/alarm_service/service.c',*extra,'-lgcc','-o',legacy])
  run([cc,*flags,*defines,'-I'+str(baseline/'include'),*['-I'+str(i) for i in includes[1:]],baseline/'service.c',*extra,'-lgcc','-o',before])
  if legacy.read_bytes()!=before.read_bytes():raise ValueError('Unselected bytes changed: '+name)
  dest=out/name;dest.mkdir(exist_ok=True);elf=dest/'driver.elf'
  run([cc,*flags,*defines,'-DPOINTS_CATALOG_SERVICE',*['-I'+str(i) for i in includes],ROOT/'Services/alarm_service/service.c',*extra,'-lgcc','-o',elf])
  symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
  imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s};exports={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
  if not imports<={'memcpy','memset','memcmp','strcmp','strlen','malloc','free','memmove'} or exports!={'t5_driver_get'}:raise ValueError((name,imports,exports))
  manifest=ROOT/'Services/alarm_service'/('catalog-native-utc-manifest.json' if native else 'catalog-watch-manifest.json')
  data=json.loads(manifest.read_text());assert data['version']==PIN['versions'][name]
  (dest/'manifest.json').write_bytes(manifest.read_bytes())
  rows.append({'profile':name,'version':data['version'],'sha256':sha(elf),'size_bytes':elf.stat().st_size,'imports':sorted(imports),'exports':sorted(exports),'legacy_exact_base_bytes':True})
 validator=out/'validate-elf';run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator])
 for row in rows:run([validator,out/row['profile']/'driver.elf'])
 loader=out/'target-loader';run([os.environ.get('CC','cc'),'-std=gnu11','-O1','-g','-I'+str(ROOT/'test/alarm-loader-stubs'),'-I'+str(runtime/'lib/elf_loader/include'),ROOT/'test/alarm_target_loader.c',runtime/'lib/elf_loader/src/esp_elf.c',runtime/'lib/elf_loader/src/arch/esp_elf_xtensa.c',runtime/'lib/elf_loader/src/esp_elf_validate.c','-o',loader])
 run([loader,*[out/row['profile']/'driver.elf' for row in rows]])
 source_paths=['Services/alarm_service/service.c','Services/alarm_service/catalog.inc','sdk/points-catalog-service-sources.json',*sorted(str(p.relative_to(ROOT)) for p in (ROOT/'lib/Alarm/include').glob('*.h'))]
 (out/'build-evidence.json').write_text(json.dumps({'schema':1,'source_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'compiler':compiler,'target_loader':'passed eight alignments and all relocation sites','pins':PIN,'modules':rows,'sources':{p:sha(ROOT/p) for p in source_paths},'hardware_verified':False},indent=2)+'\n')
 print('Watch0.4.5 API1 / X4 native UTC0.4.6 API2 target ELF validation PASS; both unselected legacy ELFs exactly match baseline')
if __name__=='__main__':main()
