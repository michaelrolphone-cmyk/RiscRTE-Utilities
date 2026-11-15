#!/usr/bin/env python3
"""Build migrated full-source canvas apps against capability-only Runtime."""
import argparse,hashlib,json,subprocess,sys,tempfile,shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def stage(runtime,system,out):
 sys.path.insert(0,str(system/'scripts'));import scene_sdk,shutil
 include=scene_sdk.stage_sdk(runtime,system,out)
 for source in (ROOT/'lib/CanvasApp/include').glob('*.h'):
  if not source.name.startswith('Risc') or not (include/source.name).exists():shutil.copyfile(source,include/source.name)
 for name in ['RiscStorageVolumeV1.h','RiscAppDataV1.h','T5FileOpenApi.h']:shutil.copyfile(runtime/'sdk/app'/name,include/name)
 # Additive canonical helper; do not repin the staged Runtime ABI.
 from runtime_clock_sdk import stage_runtime_clock
 stage_runtime_clock(include)
 return include

def build(runtime,system,output,target):
 sys.path.insert(0,str(system/'scripts'));import scene_build
 cc=scene_build.compiler_path();output.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='canvas-sdk-') as temp:
  include=stage(runtime,system,Path(temp))
  scene_build.IMPORTS|={'malloc','calloc','free','strstr','strrchr'}
  for name,kind in [('model_viewer',1),('hollow_trail',2)]:
   if target=='watch' and kind==2:continue
   m=json.loads((ROOT/'Apps'/f'{name}.json').read_text())
   if target=='watch':m['requires']=[r for r in m['requires'] if r['capability'] not in ['input.navigation','storage.volume','file.open']]
   sources=[ROOT/'Apps'/f'{name}.c',ROOT/'lib/CanvasApp/canvas.c',ROOT/'lib/CanvasApp/entry.c']
   defines=['-DCANVAS_KIND='+str(kind),'-Wno-misleading-indentation','-Wno-unused-function']
   receipt=scene_build.build(cc,include,output/name,m,sources,defines=defines)
   dependencies={}
   for source in sources:
    dep=subprocess.check_output([cc,'-std=c11','-MM','-I'+str(include),*defines,str(source)],text=True).replace('\\\n',' ')
    for token in shlex.split(dep.split(':',1)[1]):
     f=Path(token).resolve()
     key='sdk/'+f.name if f.is_relative_to(include) else f.relative_to(ROOT).as_posix()
     dependencies[key]=hashlib.sha256(f.read_bytes()).hexdigest()
   receipt.update(target=target,dependencies=dependencies,repository_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),repository_tree=subprocess.check_output(['git','rev-parse','HEAD^{tree}'],cwd=ROOT,text=True).strip(),reader_origin=json.loads((ROOT/'lib/CanvasApp/migration-source.json').read_text()))
   (output/name/'build.json').write_text(json.dumps(receipt,indent=2)+'\n')
   validator=Path(temp)/'validate'
   subprocess.run(['cc','-std=c11','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
   subprocess.run([str(validator),str(output/name/f'{name}.elf')],check=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['runtime','system-apps','output']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--target',choices=['x4','watch'],required=True);a=p.parse_args();build(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve(),a.target)
