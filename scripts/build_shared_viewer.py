#!/usr/bin/env python3
"""Build the complete Viewer ELF once, repeat independently, preserve all sections."""
import argparse,hashlib,json,shlex,shutil,subprocess,sys,tempfile
from pathlib import Path
from build_canvas_apps import ROOT,stage
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(runtime,system,output):
 sys.path.insert(0,str(system/'scripts'))
 import scene_build
 from normalize_xtensa_relocations import normalize
 cc=scene_build.compiler_path()
 if sha(Path(cc))!='732bebd44b70a21687ffa3ac5da55edc9b555080ad21cfd74503b2288a143d3f':raise ValueError('Unsupported compiler')
 output.mkdir(parents=True,exist_ok=False)
 manifest=json.loads((ROOT/'Apps/model_viewer.shared.json').read_text())
 sources=[ROOT/'Apps/model_viewer.c',ROOT/'lib/ViewerCanvas/client.c',ROOT/'lib/CanvasApp/entry.c']
 scene_build.IMPORTS|={'calloc','strstr','strrchr'}
 records=[]
 with tempfile.TemporaryDirectory(prefix='shared-viewer-sdk-') as t:
  include=stage(runtime,system,Path(t))
  for name in ('RiscCanvasV1.h','RiscFileOpenSourceV1.h'):
   if not (include/name).is_file():raise ValueError('Missing canonical SDK '+name)
  for repeat in ('first','repeat'):
   out=output/repeat
   rec=scene_build.build(cc,include,out,manifest,sources,defines=['-DCANVAS_KIND=1'],normalize_relocations=True)
   closure={}
   for source in sources:
    dep=subprocess.check_output([cc,'-std=c11','-DCANVAS_KIND=1','-M','-I'+str(include),str(source)],text=True).replace('\\\n',' ')
    for token in shlex.split(dep.split(':',1)[1]):
     p=Path(token).resolve()
     if p.is_relative_to(include):name='sdk/'+p.name
     elif p.is_relative_to(ROOT):name='utilities/'+p.relative_to(ROOT).as_posix()
     elif p.is_relative_to(Path(cc).resolve().parents[1]):name='toolchain/'+p.relative_to(Path(cc).resolve().parents[1]).as_posix()
     else:raise ValueError('Unowned compiler dependency '+str(p))
     closure[name]=sha(p)
   rec['dependency_closure']=closure;rec['compiler_sha256']=sha(Path(cc))
   rec['source_commit']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
   rec['dependencies']={n:subprocess.check_output(['git','rev-parse','HEAD'],cwd=p,text=True).strip() for n,p in [('runtime',runtime),('system',system)]}
   (out/'build.json').write_text(json.dumps(rec,sort_keys=True,indent=2)+'\n');records.append(rec)
  for name in ('model_viewer.elf','model_viewer.json'):
   if (output/'first'/name).read_bytes()!=(output/'repeat'/name).read_bytes():raise ValueError('Repeat differs '+name)
  if records[0]['dependency_closure']!=records[1]['dependency_closure']:raise ValueError('Closure differs')
  validator=Path(t)/'validate'
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(runtime/'lib/elf_loader/include'),str(runtime/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
  subprocess.run([str(validator),str(output/'first/model_viewer.elf')],check=True)
 (output/'receipt.json').write_text(json.dumps({'schema':1,'complete_elf_identical':True,'builds':records,'hardware_tested':False},indent=2)+'\n')
 print(json.dumps({'sha256':records[0]['sha256'],'bytes':records[0]['size_bytes'],'complete_elf_identical':True}),flush=True)
if __name__=='__main__':
 p=argparse.ArgumentParser()
 for n in ('runtime','system','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();run(a.runtime.resolve(),a.system.resolve(),a.output.resolve())
