#!/usr/bin/env python3
"""One profile-free Frequency ELF, repeat build and identical selections."""
import argparse,json,sys,subprocess,os
from pathlib import Path
from frequency_scene_support import stage,ROOT
from lifecycle_dependencies import Closure
from build_shared_scene_apps import distribute
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','system','power-key-header','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();a.runtime=a.runtime.resolve();a.system=a.system.resolve();a.output=a.output.resolve()
a.output.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(a.system/'scripts'))
import scene_build
inc=stage(a.runtime,a.system,a.power_key_header.resolve(),a.output/'sdk')
manifest=json.loads((ROOT/'Apps/frequency_scene.json').read_text());cc=scene_build.compiler_path()
closure=Closure([('Runtime',a.runtime),('System',a.system),('Utilities',ROOT),('PowerKeyContract',a.power_key_header.resolve().parent),('generated-sdk',a.output/'sdk')],a.output/'dependencies.json')
original=subprocess.run
def run(command,*args,**kwargs):
 if isinstance(command,list) and command and command[0]==cc and '-o' in command:closure.record(command)
 return original(command,*args,**kwargs)
subprocess.run=run
try:
 results=[scene_build.build(cc,inc,a.output/name,manifest,[ROOT/'Apps/frequency_scene.c'],exports={'app_main','risc_resident_app_descriptor_v1'}) for name in ('package','repeat')]
finally:subprocess.run=original
if results[0]['sha256']!=results[1]['sha256']:raise ValueError('Non-reproducible Frequency ELF')
results[0]['selections']=distribute(a.output/'package',manifest,a.output)
validator=a.output/'validate'
cmd=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(a.runtime/'lib/elf_loader/include'),str(a.runtime/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)]
closure.record(cmd);original(cmd,check=True);original([str(validator),str(a.output/'package'/manifest['file_name'])],check=True)
(a.output/'receipt.json').write_text(json.dumps({'build':results[0],'repeat':results[1],'physical_testing':'not performed','deployment':'not performed'},indent=2))
