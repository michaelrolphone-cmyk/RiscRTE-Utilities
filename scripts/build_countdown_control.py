#!/usr/bin/env python3
"""Build clock-policy adapters; these are providers, never Countdown app variants."""
import argparse,hashlib,importlib.util,json,os,subprocess,sys,tempfile
from pathlib import Path
from scene_support import ROOT,sdk
from lifecycle_dependencies import Closure

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(runtime,system,output):
 output.mkdir(parents=True,exist_ok=False)
 sys.path.insert(0,str(system/'scripts'))
 try:
  spec=importlib.util.spec_from_file_location('selected_scene_build',system/'scripts/scene_build.py');builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
 finally:sys.path.pop(0)
 compiler=builder.compiler_path();receipts=[]
 with tempfile.TemporaryDirectory(prefix='countdown-control-sdk-') as tmp:
  tmp=Path(tmp);include=sdk(runtime,system,tmp/'sdk')
  closure=Closure([('Runtime',runtime),('System',system),('Utilities',ROOT),('generated-sdk',tmp/'sdk')],output/'dependencies.json')
  original_run=subprocess.run
  def recorded(command,*args,**kwargs):
   if isinstance(command,(list,tuple)) and command and str(command[0])==compiler and '-o' in command:closure.record(command)
   return original_run(command,*args,**kwargs)
  subprocess.run=recorded
  try:
   for policy,defines in [('raw',[]),('utc',['-DALARM_NATIVE_UTC','-DALARM_SERVICE_CLOCK_EDIT_V2'])]:
    manifest_path=ROOT/f'Services/countdown_control/{policy}.json';manifest=json.loads(manifest_path.read_text())
    flags=[*defines,'-DCOUNTDOWN_CONTROL_ID="'+manifest['id']+'"'];sources=[ROOT/'Services/countdown_control/control.c']
    package=output/manifest['id'];one=builder.build(compiler,include,package,manifest,sources,defines=flags)
    repeat=builder.build(compiler,include,tmp/('repeat-'+policy),manifest,sources,defines=flags)
    if one['sha256']!=repeat['sha256'] or one['size_bytes']!=repeat['size_bytes']:raise ValueError('Nonreproducible provider')
    elf=package/manifest['file_name']
    if digest(elf)!=one['sha256'] or elf.stat().st_size!=one['size_bytes']:raise ValueError('Builder receipt differs from actual provider')
    if json.loads((package/'manifest.json').read_text())!=manifest:raise ValueError('Provider manifest changed')
    one['reproducibility']={'sha256':repeat['sha256'],'size_bytes':repeat['size_bytes'],'matches':True}
    one['manifest_sha256']=digest(manifest_path);one['policy']=policy;one['package_path']=str(package.relative_to(output));receipts.append(one)
  finally:subprocess.run=original_run
  validator=tmp/'validate-elf';validator_source=runtime/'lib/elf_loader/src/esp_elf_validate.c';validator_fixture=ROOT/'test/native_apps/validate_test.c'
  command=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(runtime/'lib/elf_loader/include'),str(validator_source),str(validator_fixture),'-o',str(validator)]
  closure.record(command);original_run(command,check=True)
  for one in receipts:
   elf=output/one['package_path']/one['file'];original_run([str(validator),str(elf)],check=True)
   one['selected_runtime_structural_validation']='passed'
 (output/'packages.json').write_text(json.dumps({'providers':receipts,'application_variants':False,'deployment':'not performed','hardware':'not tested',
  'validator':{'source':str(validator_source),'sha256':digest(validator_source),'fixture_sha256':digest(validator_fixture)},
  'selected_builder_sha256':{name:digest(system/'scripts'/name) for name in ('scene_build.py','scene_sdk.py','native_toolchain.py')},
  'source_helper_sha256':{name:digest(ROOT/'scripts'/name) for name in ('build_countdown_control.py','scene_support.py','lifecycle_dependencies.py')},
  'dependency_receipt':'dependencies.json'},indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve())
