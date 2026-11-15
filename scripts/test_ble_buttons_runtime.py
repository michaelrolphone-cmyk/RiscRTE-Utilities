#!/usr/bin/env python3
"""Exercise actual Buttons/scene/Runtime ELFs; only physical devices are doubled."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess
from pathlib import Path
from lifecycle_dependencies import Closure
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,system,output,sanitize=False,provider_system=None):
 provider_system=provider_system or system
 output.mkdir(parents=True,exist_ok=False)
 from buttons_support import sdk
 include=sdk(runtime,system,output/'sdk')
 for source in (system/'Services/scene_profile/SceneProfileV1.h',system/'Services/text_input/SceneKeyboardV1.h'):shutil.copyfile(source,include/source.name)
 cc,cxx=os.environ.get('CC','cc'),os.environ.get('CXX','c++')
 san=['-g0','-pipe']+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitize else [])
 cf=[*san,'-std=c11','-O1','-Wall','-Wextra','-Werror','-I'+str(include)]
 closure=Closure([('Runtime',runtime),('System',system),('ProviderSystem',provider_system),('Utilities',ROOT),('generated-sdk',output/'sdk')],output/'dependencies.json')
 def call(command):closure.record(command);subprocess.run(command,check=True)
 def build(path,sources,flags=(),shared=True):call([cc,*cf,*(['-fPIC','-fvisibility=hidden','-shared'] if shared else ['-c']),*flags,*map(str,sources),'-o',str(path)])
 common=output/'common';common.mkdir();fixture=ROOT/'test/ble_buttons_runtime'
 build(common/'ble_buttons.elf',[fixture/'app.c']);build(common/'scene-host.elf',[provider_system/'Services/scene_host/host.c'])
 build(common/'launcher.elf',[fixture/'launcher.c'])
 for i,(name,cap) in enumerate((('display','display.output'),('touch','input.touch.raw'),('navigation','input.navigation'),('hid','bluetooth.hid'))):
  build(common/(name+'.elf'),[fixture/'provider.c'],[f'-DPROVIDER_INDEX={i}',f'-DPROVIDER_ID="{name}"',f'-DPROVIDER_CAP="{cap}"','-DPROVIDER_API=1'])
 build(common/'alarm.elf',[fixture/'provider.c'],['-DPROVIDER_INDEX=5','-DPROVIDER_ID="alarm"','-DPROVIDER_CAP="alarm.service"','-DPROVIDER_API=2'])
 build(output/'hardware.o',[fixture/'hardware.c'],shared=False)
 sources=('bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp','runtime/storage/AppDataFiles.cpp')
 executable=output/'runtime-test'
 call([cxx,*san,'-std=c++17','-O0' if sanitize else '-O1','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-DRISC_RUNTIME_PROVIDER_CAPACITY=28','-DRISC_RUNTIME_PREFERENCE_STORE=1','-rdynamic','-I'+str(include),*['-I'+str(runtime/p) for p in ('src','lib/ArduinoJson/src','test/drivers/stubs','test')],*[str(runtime/'src'/p) for p in sources],str(fixture/'runtime_test.cpp'),str(output/'hardware.o'),'-ldl','-o',str(executable)])
 env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0');results=[]
 app_hash=hashlib.sha256((common/'ble_buttons.elf').read_bytes()).hexdigest()
 for name,width,height,fmt in (('paper',800,480,1),('watch',240,240,5),('amoled',410,502,5)):
  stage=output/name;shutil.copytree(common,stage)
  build(stage/'scene-presentation-profile.elf',[provider_system/'Services/scene_profile/profile.c'],['-DSCENE_PROFILE_PAPER=1'] if fmt==1 else [])
  for case in ('demand','retained-providers','no-ui','alarm'):
   mode='demand' if case=='alarm' else case
   case_env=dict(env);case_env.pop('BUTTONS_ALARM',None)
   if case=='alarm':case_env['BUTTONS_ALARM']='1'
   target=output/(name+'-'+case);shutil.copytree(stage,target)
   assert hashlib.sha256((target/'ble_buttons.elf').read_bytes()).hexdigest()==app_hash
   result=subprocess.run([str(executable),str(target),str(width),str(height),mode],env=case_env,text=True,capture_output=True)
   (target/'runtime.log').write_text(result.stdout+result.stderr);print(result.stdout,end='');print(result.stderr,end='')
   result.check_returncode();assert 'Buttons actual Runtime' in result.stdout and 'unloads=2' in result.stdout
   results.append({'profile':name,'width':width,'height':height,'format':fmt,'activation':mode,'alarm':case=='alarm','app_sha256':app_hash,'passed':True})
 for key,digest in closure.inputs.items():
  name,rel=key.split('/',1);base=dict(closure.roots)[name];assert hashlib.sha256((base/rel).read_bytes()).hexdigest()==digest, 'Source changed: '+key
 (output/'fixture-inputs.json').write_text(json.dumps({str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),ROOT/'scripts/buttons_support.py',ROOT/'scripts/lifecycle_dependencies.py',*fixture.glob('*')] if p.is_file()},indent=2)+'\n')
 (output/'binary-inputs.json').write_text(json.dumps({str(p.relative_to(output)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [executable,*output.rglob('*.elf')]},indent=2)+'\n')
 (output/'results.json').write_text(json.dumps({'cases':results,'scope':'actual Runtime/app/HOLD scene: native navigation edits exact HBv1, saves namespace11, reconnects, holds keyboard over650ms with400ms refresh, releases, stops, unloads/reopens persisted assignment; HID/physical/KV backend doubled, alarm reservation lane closes HID before next scheduler output; background/resident absent; no physical interoperability or durable-storage claim','sanitize':sanitize,'provider_system':str(provider_system),'sdk_system':str(system),'hardware':'not tested'},indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--provider-system',type=Path,help='Optional independently pinned scene/profile/text source root; SDK stays --system-apps')
 p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve(),a.sanitize,a.provider_system.resolve() if a.provider_system else None)
