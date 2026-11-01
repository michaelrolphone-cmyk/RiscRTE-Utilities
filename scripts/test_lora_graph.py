#!/usr/bin/env python3
"""Unchanged LoRa controller through production Runtime/Graph, scene and text DSOs."""
import argparse,hashlib,json,os,subprocess,shutil
from pathlib import Path
from scanner_support import sdk
from lifecycle_dependencies import Closure
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for k in ('runtime','system','output'):p.add_argument('--'+k,type=Path,required=True)
p.add_argument('--resident',action='store_true',help='Attempt production resident host; requires compatible display suffix SDK');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
runtime=a.runtime.resolve();system=a.system.resolve();inc=sdk(runtime,system,out/'sdk')
shutil.copytree(system/'lib/PortableApps/time',out/'sdk/time')
closure=Closure([('Runtime',runtime),('System',system),('Utilities',R),('generated-sdk',out/'sdk')],out/'dependencies.json')
commands=[];exits={}
source_files=[Path(__file__),R/'scripts/scanner_support.py',R/'scripts/lifecycle_dependencies.py',*sorted((R/'test/lora_runtime').glob('*'))]
source_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files if p.is_file()}
def run(c,name,required=True):
 c=list(map(str,c));commands.append(c);(out/'commands.json').write_text(json.dumps(commands,indent=2))
 if c[0] in ('cc','c++'):closure.record(c)
 r=subprocess.run(c,text=True,capture_output=True,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
 (out/(name+'.log')).write_text(r.stdout+r.stderr);exits[name]=r.returncode;(out/'exit-codes.json').write_text(json.dumps(exits,indent=2));
 if required:r.check_returncode()
 return r.stdout+r.stderr
flags=['-O0','-g','-Wall','-Wextra','-Werror']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
def build(src,name,extra=()):run(['cc','-std=c11',*flags,'-fPIC','-fvisibility=hidden','-shared','-I'+str(inc),'-I'+str(system/'lib/NativeApps/include'),'-I'+str(system/'Services/scene_profile'),'-I'+str(system/'Services/text_input'),*extra,src,'-o',out/name],'build-'+name)
build(R/'test/lora_runtime/app.c','default.elf')
if a.resident:
 host_flags=['-D'+x for x in ['PORTABLE_PAPER_PREFERENCES','PORTABLE_ALARM_TERMINAL_RETENTION','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_ALARM_CLIENT','ALARM_SERVICE_TAGGED_V2','PORTABLE_INPUT_NAVIGATION','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RESIDENT_POLICY','PORTABLE_RESIDENT_SHELL_HOST','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_CROWN_SLEEP_LOCAL','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_PAPER_TRANSITIONS','PORTABLE_X4_IDLE_POLICY','PORTABLE_LOW_BATTERY']]
 host_sources=[R/'test/lora_runtime/resident_host.c',*[system/'lib/PortableApps/src'/n for n in ['quick_actions.c','quick_session.c','quick_render.c','quick_radios.c']]]
 run(['cc','-std=c11',*flags,'-fPIC','-shared','-Wl,-Bsymbolic','-I'+str(inc),'-I'+str(system/'lib/NativeApps/include'),'-I'+str(R/'test/lora_runtime'),'-DHOST_SOURCE="'+str(system/'lib/PortableApps/src/adapter.c')+'"','-DPORTABLE_HOME_APP="host.elf"',*host_flags,*host_sources,'-o',out/'host.elf'],'build-host')
 
build(R/'test/lora_runtime/radio.c','radio.elf')
build(R/'test/lora_runtime/scene_probe.c','scene.elf',['-finstrument-functions','-DSCENE_SOURCE="'+str(system/'Services/scene_host/host.c')+'"'])
build(system/'Services/text_input/host.c','text.elf')
build(system/'Services/scene_profile/profile.c','profile.elf',['-DSCENE_PROFILE_ID="profile"'])
providers=[('display','display.output'),('touch','input.touch.raw'),('nav','input.navigation'),('keyboard','usb.hid.keyboard')]
for name,cap in providers:build(R/'test/lora_runtime/provider.c',name+'.elf',['-DTEST_ID="'+name+'"','-DTEST_CAP="'+cap+'"'])
def write(name,data):(out/name).write_text(json.dumps(data,indent=2)+'\n')
def req(cap):return {'capability':cap,'api':1}
def manifest(name,cap,requires=()):return {'type':'driver','id':name,'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':name+'.elf','requires':[req(c) for c in requires],'provides':[req(cap)]}
for name,cap in providers:write(name+'.json',manifest(name,cap))
m=json.loads((system/'Services/scene_host/manifest.json').read_text());m['file_name']='scene.elf';write('scene.json',m)
t=manifest('text','ui.text-input',['ui.scene','usb.hid.keyboard']);t['id']='text-input-host';write('text.json',t);write('profile.json',manifest('profile','ui.presentation-profile'))
radio=manifest('radio','radio.lora');radio['provides'][0]['api']=2;write('radio.json',radio)
write('board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':[]})
m=json.loads((R/'Apps/lora_scene.json').read_text());m['file_name']='default.elf';write('default.json',m)
units=[runtime/'src'/x for x in ['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']]
run(['c++','-std=c++17',*flags,'-Wno-missing-field-initializers','-fno-pie','-no-pie','-rdynamic',*['-I'+str(runtime/x) for x in ['src','lib/ArduinoJson/src','test/drivers/stubs','test']],'-I'+str(inc),*units,R/'test/lora_runtime/graph.cpp','-ldl','-o',out/'test'],'build-runtime')
results=[]
for activation in ['eager','demand']:
 for mode in ['no-ui','exit','no-storage','no-text','compose','update-input-boundary','profile','receive-history','short-back','cancel-failure','native-radio-cancel','send','no-radio','delayed-display','native-radio-send','native-info','native-status','malformed-radio','changing-radio','malformed-profile']+(['resident-exit','resident-compose','resident-send','resident-short-back'] if a.resident else []):
  drivers=[] if mode=='no-ui' else ['scene','profile',*[n for n,c in providers]]+([] if mode=='no-text' else ['text'])
  if mode not in ('no-ui','no-radio'):drivers+=['radio']
  grants=[] if mode=='no-ui' else [{'capability':'ui.scene','api':1,'instance_id':0}]+([] if mode=='no-text' else [{'capability':'ui.text-input','api':1,'instance_id':0}])
  if mode!='no-ui':
   if mode!='no-storage':grants += [{'capability':'storage.key-value','api':1,'instance_id':n} for n in (1,9)]
   if mode!='no-radio':grants += [{'capability':'radio.lora','api':2,'instance_id':0}]
  boot={'board':'board.json','default_app':'default.elf','provider_activation':activation,'drivers':[{'manifest':n+'.json'} for n in drivers],'app_capabilities':[{'manifest':'default.json','grants':grants}]}
  if mode.startswith('resident-'):
   host={'type':'application','id':'host','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'host.elf','entry':'app_main','requires':[],'optional':[req(c) for c in ['display.output','input.touch.raw','input.navigation','storage.key-value']]}
   write('host.json',host);boot['default_app']='host.elf';boot['resident_shell']={'api':1,'host':'host.elf','foreground':['default.elf']}
   boot['app_capabilities'].append({'manifest':'host.json','grants':[{**req(c),'instance_id':0} for c in ['display.output','input.touch.raw','input.navigation']]+[{'capability':'storage.key-value','api':1,'instance_id':1}]})
  write('boot.json',boot)
  result=run([out/'test',out,mode,activation],activation+'-'+mode,required=False);print(result,end='',flush=True);results.append(result)
assert source_hashes=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files if p.is_file()},'Source changed during execution'
write('receipt.json',{'passed':all(code==0 for code in exits.values()),'source_hashes':source_hashes,'exit_codes':exits,'results':results,'sanitize':a.sanitize,'runtime_mocked':False,'controller_sha256':hashlib.sha256((R/'Apps/lora_scene.c').read_bytes()).hexdigest(),'scope':'Actual Runtime/Graph and production scene/text; hardware endpoints doubled. No resident-host policy claim.'})
assert all(code==0 for code in exits.values()),'Integration failures; see per-case logs and receipt'
