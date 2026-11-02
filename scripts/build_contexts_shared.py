#!/usr/bin/env python3
"""Build one Contexts ELF; device-specific input is deliberately absent."""
import argparse,json,tempfile,hashlib,shutil,os,subprocess
from pathlib import Path
from build_contexts_scene import stage,load,ROOT
def validate_declarations(manifest):
 # Actual Runtime AppDeclarations/appPolicies use integer, exact API matching.
 # A range or duplicate non-KV capability cannot express alternative APIs.
 seen=set()
 for group in ('requires','optional'):
  rows=manifest.get(group,[])
  if not isinstance(rows,list):raise ValueError('Capability declarations must be arrays')
  for row in rows:
   if set(row)!={'capability','api'} or not isinstance(row['capability'],str) or not row['capability']:
    raise ValueError('Invalid capability declaration')
   if type(row['api']) is not int or not 1<=row['api']<=0xffffffff:
    raise ValueError('Capability API must be a positive uint32 integer')
   if row['capability'] in seen:raise ValueError('Duplicate capability declaration')
   seen.add(row['capability'])
 return manifest

def run(runtime,system,output):
 if output.exists():raise FileExistsError(output)
 output.mkdir(parents=True)
 builder=load('scene_build',system/'scripts/scene_build.py')
 with tempfile.TemporaryDirectory(prefix='contexts-common-') as temp:
  include=stage(runtime,system,Path(temp))
  manifest=validate_declarations(json.loads((ROOT/'Apps/contexts-shared.json').read_text()))
  receipt=builder.build(builder.compiler_path(),include,output/'contexts',manifest,
   [ROOT/'Apps/contexts.c'],defines=['-DCONTEXTS_SHARED_APPLICATION','-DPORTABLE_CONTEXTS_SHARED_TEXT_INPUT'],
   exports={'app_main','risc_resident_app_descriptor_v1'})
  validator=Path(temp)/'validate'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
   '-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),
   str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),
   '-o',str(validator)],check=True)
  subprocess.run([str(validator),str(output/'contexts/contexts.elf')],check=True)
  # Deployment references, not three recompilations: the immutable object is one.
  references={device:{'elf':'contexts/contexts.elf','sha256':receipt['sha256']}
   for device in ('X4','T-Watch','Waveshare')}
  pins={name:subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
   for name,path in (('utilities',ROOT),('system',system),('runtime',runtime))}
  builder.json_write(output/'source-lock.json',{'schema':1,'commits':pins})
  builder.json_write(output/'delivery.json',{'schema':1,'application':receipt,
   'deployments':references,'physical_testing':'not performed',
   'presentation':'provider-owned ui.scene and ui.text-input',
   'data':'existing rule/model/preference formats unchanged'})
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve())
