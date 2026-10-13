#!/usr/bin/env python3
"""Build the shipping Contexts scene client (no legacy display/touch adapter)."""
import argparse, hashlib, importlib.util, json, os, shutil, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def load(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

CATALOG_HEADERS=("PortableAppCatalog.h","RiscAppCatalogV1.h","PortableOptionalService.h")
def stage(runtime,system,output,catalog_system=None):
 sdk=load('scene_sdk',system/'scripts/scene_sdk.py');include=sdk.stage_sdk(runtime,system,output)
 for folder in (system/'lib/PortableApps/include',system/'lib/NativeApps/include',ROOT/'lib/Contexts/include',ROOT/'lib/Alarm/include'):
  for source in folder.glob('*.h'):
   if not (include/source.name).exists():shutil.copyfile(source,include/source.name)
 shutil.copyfile(system/'lib/PortableApps/include/RiscKeyValueV1.h',include/'RiscKeyValueV1.h')
 shutil.copytree(system/'lib/PortableApps/time',include.parent/'time',dirs_exist_ok=True)
 shutil.copyfile(ROOT/'Apps/alarm_writer.h',include/'alarm_writer.h')
 # Shared text entry's optional ABI belongs to the selected System host.
 text_api=system/'sdk/app/RiscTextEntryV1.h'
 if text_api.exists():shutil.copyfile(text_api,include/'RiscTextEntryV1.h')
 # This explicit, bounded union cannot replace scene/text/lifecycle clients.
 catalog_system=catalog_system or system
 provenance={}
 for name in CATALOG_HEADERS:
  source=catalog_system/'lib/PortableApps/include'/name
  if not source.is_file():raise FileNotFoundError('Catalog client header missing; select --catalog-system: '+str(source))
  raw=source.read_bytes()
  if name=='RiscAppCatalogV1.h' and (include/name).exists() and (include/name).read_bytes()!=raw:
   raise ValueError('Runtime and selected catalog ABI headers differ')
  shutil.copyfile(source,include/name)
  provenance[name]={'source':str(source.resolve()),'sha256':hashlib.sha256(raw).hexdigest()}
 (include/'.contexts-catalog-sources.json').write_text(json.dumps(provenance,indent=2)+'\n')
 return include

def run(runtime,system,output,catalog=None,target="watch",face_header=None,shared_input=False,catalog_system=None):
 if catalog is not None:raise ValueError("Contexts uses app.catalog; omit --catalog")
 if output.exists():raise FileExistsError('Choose a new output directory')
 output.mkdir(parents=True)
 builder=load('scene_build',system/'scripts/scene_build.py')
 with tempfile.TemporaryDirectory(prefix='contexts-scene-') as tmp:
  tmp=Path(tmp);include=stage(runtime,system,tmp,catalog_system)
  manifest=json.loads((ROOT/'Apps/contexts-scene.json').read_text())
  flags=[]
  if shared_input:
   flags+=['-DPORTABLE_CONTEXTS_SHARED_TEXT_INPUT']
   manifest['version']='0.4.5'
   manifest['requires'].append({'capability':'ui.text-input','api':1})
  if target=='x4':
   flags+=['-DCONTEXTS_ALARM_API=2','-DCONTEXTS_SETTING_MASK=0x274u','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO']
   next(r for r in manifest['requires'] if r['capability']=='alarm.service')['api']=2
   clock=next(r for r in manifest['requires'] if r['capability']=='rtc.clock')
   clock.update(capability='runtime.realtime',api=1)
  if face_header:flags+=['-include',str(face_header.resolve())]
  compiler=builder.compiler_path()
  receipt=builder.build(compiler,include,output/'contexts',manifest,[ROOT/'Apps/contexts.c'],defines=flags,exports={'app_main','risc_resident_app_descriptor_v1'})
  symbols=subprocess.check_output([compiler.removesuffix('gcc')+'nm',str(output/'contexts/contexts.elf')],text=True)
  if {'portable_catalog','portable_catalog_count'} & {line.split()[-1] for line in symbols.splitlines() if line.split()}:
   raise ValueError('Compiled catalog symbol remains in the complete Contexts ELF')
  receipt['catalog_client_sources']=json.loads((include/'.contexts-catalog-sources.json').read_text())
  receipt['catalog']='native read-only app.catalog; no compiled inventory'
  builder.json_write(output/'contexts/build.json',receipt)
  validator=tmp/'validate'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
  subprocess.run([str(validator),str(output/'contexts/contexts.elf')],check=True)
  builder.json_write(output/'packages.json',{'schema':1,'packages':[receipt],'physical_testing':'not performed','renderer':'ui.scene components; no portable adapter'})
 print('Contexts shipping scene-client Xtensa ELF validated.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--shared-text-input',action='store_true')
 p.add_argument('--catalog-system',type=Path);p.add_argument('--catalog',type=Path,help='Rejected legacy input; native app.catalog supplies the inventory');p.add_argument('--target',choices=('watch','x4'),default='watch');p.add_argument('--face-header',type=Path)
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve(),a.catalog,a.target,a.face_header,a.shared_text_input,a.catalog_system.resolve() if a.catalog_system else None)
