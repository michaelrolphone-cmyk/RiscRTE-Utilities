#!/usr/bin/env python3
"""Exercise actual Calculator/scene/Runtime ELFs; only physical devices are doubled."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess
from pathlib import Path
from lifecycle_dependencies import Closure
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,system,output,sanitize=False):
 output.mkdir(parents=True,exist_ok=False)
 spec=importlib.util.spec_from_file_location('selected_sdk',system/'scripts/scene_sdk.py');sdk=importlib.util.module_from_spec(spec);spec.loader.exec_module(sdk)
 include=sdk.stage_sdk(runtime,system,output/'sdk')
 cc,cxx=os.environ.get('CC','cc'),os.environ.get('CXX','c++')
 san=['-g1']+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitize else [])
 cf=[*san,'-std=c11','-O1','-Wall','-Wextra','-Werror','-I'+str(include)]
 closure=Closure([('Runtime',runtime),('System',system),('Utilities',ROOT),('generated-sdk',output/'sdk')],output/'dependencies.json')
 def call(command):closure.record(command);subprocess.run(command,check=True)
 def build(path,sources,flags=(),shared=True):call([cc,*cf,*(['-fPIC','-fvisibility=hidden','-shared'] if shared else ['-c']),*flags,*map(str,sources),'-o',str(path)])
 common=output/'common';common.mkdir();fixture=ROOT/'test/shared_calculator'
 build(common/'calculator.elf',[fixture/'hooks.c']);build(common/'scene-host.elf',[system/'Services/scene_host/host.c'])
 build(common/'launcher.elf',[fixture/'launcher.c'])
 for i,(name,cap) in enumerate((('display','display.output'),('touch','input.touch.raw'),('navigation','input.navigation'))):
  build(common/(name+'.elf'),[ROOT/'test/scene/runtime_provider.c'],[f'-DPROVIDER_INDEX={i}',f'-DPROVIDER_ID="{name}"',f'-DPROVIDER_CAP="{cap}"','-DPROVIDER_API=1'])
 build(output/'hardware.o',[fixture/'hardware.c'],shared=False)
 sources=('bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp','runtime/storage/AppDataFiles.cpp')
 executable=output/'runtime-test'
 call([cxx,*san,'-std=c++17','-O0' if sanitize else '-O1','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-rdynamic','-I'+str(include),*['-I'+str(runtime/p) for p in ('src','lib/ArduinoJson/src','test/drivers/stubs','test')],*[str(runtime/'src'/p) for p in sources],str(fixture/'runtime_test.cpp'),str(output/'hardware.o'),'-ldl','-o',str(executable)])
 env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0');results=[]
 app_hash=hashlib.sha256((common/'calculator.elf').read_bytes()).hexdigest()
 for name,width,height,fmt in (('paper',800,480,1),('watch',240,240,5),('amoled',410,502,5)):
  stage=output/name;shutil.copytree(common,stage)
  build(stage/'scene-presentation-profile.elf',[system/'Services/scene_profile/profile.c'],['-DSCENE_PROFILE_PAPER=1'] if fmt==1 else [])
  for mode in (('demand','retained-providers','no-ui') if name=='watch' else ('demand','retained-providers')):
   target=output/(name+'-'+mode);shutil.copytree(stage,target)
   assert hashlib.sha256((target/'calculator.elf').read_bytes()).hexdigest()==app_hash
   result=subprocess.run([str(executable),str(target),str(width),str(height),str(fmt),mode],env=env,text=True,capture_output=True)
   (target/'runtime.log').write_text(result.stdout+result.stderr);print(result.stdout,end='');print(result.stderr,end='')
   result.check_returncode();assert 'actual Runtime Calculator' in result.stdout and 'actual-unloads=2' in result.stdout
   results.append({'profile':name,'width':width,'height':height,'format':fmt,'activation':mode,'app_sha256':app_hash,'passed':True})
 (output/'results.json').write_text(json.dumps({'cases':results,'scope':'host Runtime, ELF loader, real scene provider and app; physical doubles','sanitize':sanitize,'hardware':'not tested'},indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve(),a.sanitize)
