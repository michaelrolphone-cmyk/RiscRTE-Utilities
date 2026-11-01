#!/usr/bin/env python3
"""Build/repeat one provider-neutral LoRa ELF and validate all copied selections."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
from scanner_support import sdk
from lifecycle_dependencies import Closure
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(runtime,system,out):
 if out.exists():raise FileExistsError(out)
 out.mkdir(parents=True)
 include=sdk(runtime,system,out/'sdk')
 sys.path.insert(0,str(system/'scripts'))
 import scene_build
 compiler=scene_build.compiler_path()
 manifest=json.loads((ROOT/'Apps/lora_scene.json').read_text())
 sources=[ROOT/'Apps/lora_scene.c',system/'lib/NativeApps/src/UnsignedDivisionCompat.c']
 closure=Closure([('Runtime',runtime),('System',system),('Utilities',ROOT),('generated-sdk',out/'sdk')],out/'dependencies.json')
 original=subprocess.run
 def recorded(command,*args,**kwargs):
  if isinstance(command,(list,tuple)) and command and command[0]==compiler and '-o' in command:closure.record(command)
  return original(command,*args,**kwargs)
 subprocess.run=recorded
 try:
  a=scene_build.build(compiler,include,out/'package',manifest,sources,exports={'app_main','risc_resident_app_descriptor_v1'})
  b=scene_build.build(compiler,include,out/'repeat',manifest,sources,exports={'app_main','risc_resident_app_descriptor_v1'})
 finally:subprocess.run=original
 if a['sha256']!=b['sha256']:raise ValueError('Nonidentical repeat')
 validator=out/'validate-elf'
 cmd=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(runtime/'lib/elf_loader/include'),str(runtime/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)]
 closure.record(cmd);original(cmd,check=True)
 elf=out/'package'/manifest['file_name'];original([str(validator),str(elf)],check=True)
 selections={}
 for device in ('x4','twatch','waveshare'):
  dest=out/'selections'/device;dest.mkdir(parents=True);shutil.copyfile(elf,dest/elf.name)
  if (dest/elf.name).read_bytes()!=elf.read_bytes():raise ValueError('Selection mismatch')
  selections[device]=sha(dest/elf.name)
 (out/'receipt.json').write_text(json.dumps({'build':a,'repeat':b,'selections':selections,'physical':'not tested','deployment':'not performed','builder_sha256':sha(Path(__file__))},indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=Path);p.add_argument('--system',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();run(a.runtime.resolve(),a.system.resolve(),a.output.resolve())
