#!/usr/bin/env python3
"""Build the shipping Contexts scene client (no legacy display/touch adapter)."""
import argparse, importlib.util, json, os, shutil, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def load(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def stage(runtime,system,output):
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
 return include

def run(runtime,system,output,catalog=None,target="watch",face_header=None,shared_input=False):
 if output.exists():raise FileExistsError('Choose a new output directory')
 output.mkdir(parents=True)
 builder=load('scene_build',system/'scripts/scene_build.py')
 with tempfile.TemporaryDirectory(prefix='contexts-scene-') as tmp:
  tmp=Path(tmp);include=stage(runtime,system,tmp)
  rows=json.loads(catalog.read_text()) if catalog else []
  if isinstance(rows,dict):rows=rows['apps']
  allowed=('file_name','display_name','icon','category','min_firmware_version','version')
  cat=output/'catalog.c';cat.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={'+(','.join('{'+','.join('.'+k+'='+json.dumps(v) for k,v in row.items() if k in allowed and isinstance(v,str))+',.compatible=true}' for row in rows) if rows else '{.compatible=false}')+'};\nconst unsigned portable_catalog_count='+str(len(rows))+';\n')
  manifest=json.loads((ROOT/'Apps/contexts-scene.json').read_text())
  flags=[]
  if shared_input:
   flags+=['-DPORTABLE_CONTEXTS_SHARED_TEXT_INPUT']
   manifest['version']='0.4.3'
   manifest['requires'].append({'capability':'ui.text-input','api':1})
  if target=='x4':
   flags+=['-DCONTEXTS_ALARM_API=2','-DCONTEXTS_SETTING_MASK=0x274u','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO']
   next(r for r in manifest['requires'] if r['capability']=='alarm.service')['api']=2
   clock=next(r for r in manifest['requires'] if r['capability']=='rtc.clock')
   clock.update(capability='runtime.realtime',api=1)
  if face_header:flags+=['-include',str(face_header.resolve())]
  receipt=builder.build(builder.compiler_path(),include,output/'contexts',manifest,[ROOT/'Apps/contexts.c',cat],defines=flags,exports={'app_main','risc_resident_app_descriptor_v1'})
  validator=tmp/'validate'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
  subprocess.run([str(validator),str(output/'contexts/contexts.elf')],check=True)
  builder.json_write(output/'packages.json',{'schema':1,'packages':[receipt],'physical_testing':'not performed','renderer':'ui.scene components; no portable adapter'})
 print('Contexts shipping scene-client Xtensa ELF validated.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--shared-text-input',action='store_true')
 p.add_argument('--catalog',type=Path);p.add_argument('--target',choices=('watch','x4'),default='watch');p.add_argument('--face-header',type=Path)
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve(),a.catalog,a.target,a.face_header,a.shared_text_input)
