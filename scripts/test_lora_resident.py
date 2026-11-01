#!/usr/bin/env python3
"""Unchanged LoRa controller through production Runtime/Graph, scene and text DSOs."""
import argparse,hashlib,json,os,subprocess,shutil,sys,importlib.util
from pathlib import Path
from scanner_support import sdk
from lifecycle_dependencies import Closure
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for k in ('runtime','system','watch','output'):p.add_argument('--'+k,type=Path,required=True)
p.add_argument('--case',action='append');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
runtime=a.runtime.resolve();system=a.system.resolve();watch=a.watch.resolve()
sys.path.insert(0,str(watch/'scripts'))
from merged_sdk import headers
inc=headers(out,system,R,watch)
spec=importlib.util.spec_from_file_location('selected_sdk',system/'scripts/scene_sdk.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);module.stage_sdk(runtime,system,out/'sdk')
extra=sdk(runtime,system,out/'lora-sdk')
for header in extra.glob('*.h'):
 if not (inc/header.name).exists():shutil.copyfile(header,inc/header.name)
closure=Closure([('Watch',watch),('Runtime',runtime),('System',system),('Utilities',R),('generated-sdk',out/'sdk')],out/'dependencies.json')
commands=[];exits={}
source_files=[Path(__file__),R/'scripts/scanner_support.py',R/'scripts/lifecycle_dependencies.py',*sorted((R/'test/lora_resident').glob('*'))]
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
build(R/'test/lora_resident/app.c','default.elf')
host_flags=['-D'+x for x in ['WATCH_MOTION_WAKE','WATCH_CLOCK_ALARMS','ALARM_SERVICE_TAGGED_V2','WATCH_RESIDENT_HOST','WATCH_CLOCK_LAUNCHER','WATCH_QUICK_ACTIONS','WATCH_SHARED_PREFERENCE_HEADERS','PORTABLE_WATCH_NATIVE_HEIGHT','WATCH_COMPRESSED_CALENDAR_ALPHA','WATCH_COMPRESSED_FACE_ALPHA']]+['-Wno-unused-function','-Wno-misleading-indentation']
host_includes=['-I'+str(inc),'-I'+str(system/'lib/PortableApps/include'),'-I'+str(system/'lib/PortableApps/src'),'-I'+str(watch/'include'),'-I'+str(watch)]
objects=[]
for i,src in enumerate([R/'test/lora_resident/home.c',*[watch/'apps/clock'/n for n in ['nova/nova.c','viewport.c','capability_clients.c']],*[system/'lib/PortableApps/src'/n for n in ['quick_actions.c','quick_session.c','quick_render.c']]]):
 obj=out/('home-'+str(i)+'.o');run(['cc','-std=c11',*flags,'-fPIC','-fvisibility=hidden',*host_includes,*host_flags,'-DHOME_SOURCE="'+str(watch/'apps/clock/crown.c')+'"','-c',src,'-o',obj],'build-home-'+str(i));objects.append(obj)
run(['c++','-std=c++11',*flags,'-fPIC','-fvisibility=hidden','-I'+str(inc),'-shared',watch/'apps/clock/effects/boot.cpp',*objects,'-o',out/'host.elf'],'link-home')
build(R/'test/lora_resident/launcher.c','springboard.elf')
build(R/'Services/alarm_service/service.c','alarm.elf',['-DALARM_VISUAL_ONLY','-DALARM_SERVICE_TAGGED_V2'])
build(R/'test/lora_resident/provider.c','rtc.elf',['-DTEST_ID="rtc"','-DTEST_CAP="rtc.clock"','-DTEST_API=2'])
build(R/'test/lora_resident/radio.c','radio.elf')
build(R/'test/lora_resident/scene_probe.c','scene.elf',['-finstrument-functions','-DSCENE_SOURCE="'+str(system/'Services/scene_host/host.c')+'"'])
build(system/'Services/text_input/host.c','text.elf')
build(system/'Services/scene_profile/profile.c','profile.elf',['-DSCENE_PROFILE_ID="profile"'])
providers=[('display','display.output'),('touch','input.touch.raw'),('nav','input.navigation'),('keyboard','usb.hid.keyboard'),('power-key','input.power-key'),('power-sleep','power.sleep'),('display-power','display.power')]
for name,cap in providers:build(R/'test/lora_resident/provider.c',name+'.elf',['-DTEST_ID="'+name+'"','-DTEST_CAP="'+cap+'"'])
def write(name,data):(out/name).write_text(json.dumps(data,indent=2)+'\n')
def req(cap):return {'capability':cap,'api':1}
def manifest(name,cap,requires=()):return {'type':'driver','id':name,'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':name+'.elf','requires':[req(c) for c in requires],'provides':[req(cap)]}
for name,cap in providers:write(name+'.json',manifest(name,cap))
m=json.loads((system/'Services/scene_host/manifest.json').read_text());m['file_name']='scene.elf';write('scene.json',m)
t=manifest('text','ui.text-input',['ui.scene','usb.hid.keyboard']);t['id']='text-input-host';write('text.json',t);write('profile.json',manifest('profile','ui.presentation-profile'))
radio=manifest('radio','radio.lora');radio['provides'][0]['api']=2;write('radio.json',radio)
alarm=manifest('alarm','alarm.service',['storage.key-value.bound','platform.clock','rtc.clock']);alarm['id']='alarm-service';alarm['provides'][0]['api']=2;alarm['requires'][-1]['api']=2;write('alarm.json',alarm)
rtc=manifest('rtc','rtc.clock');rtc['provides'][0]['api']=2;write('rtc.json',rtc)
write('board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':[]})
m=json.loads((R/'Apps/lora_scene.json').read_text());m['file_name']='default.elf';write('default.json',m)
units=[runtime/'src'/x for x in ['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']]
run(['c++','-std=c++17',*flags,'-Wno-missing-field-initializers','-fno-pie','-no-pie','-rdynamic',*['-I'+str(runtime/x) for x in ['src','lib/ArduinoJson/src','test/drivers/stubs','test']],'-I'+str(inc),*units,R/'test/lora_resident/graph.cpp','-ldl','-o',out/'test'],'build-runtime')
results=[]
for activation in ['eager','demand']:
 for mode in (a.case or ['exit','compose','send','short-back','update-input-boundary','profile','receive-history','delayed-display','cancel-failure','native-radio-send','short-key','long-key','controls','idle','text-idle','text-short-key','alarm','text-alarm','receive-alarm','receive-controls','alarm-delayed','receive-controls-delayed','text-idle-alarm']):
  drivers=[] if mode=='no-ui' else ['scene','profile',*[n for n,c in providers]]+([] if mode=='no-text' else ['text'])
  if mode not in ('no-ui','no-radio'):drivers+=['radio']
  grants=[] if mode=='no-ui' else [{'capability':'ui.scene','api':1,'instance_id':0}]+([] if mode=='no-text' else [{'capability':'ui.text-input','api':1,'instance_id':0}])
  if mode!='no-ui':
   if mode!='no-storage':grants += [{'capability':'storage.key-value','api':1,'instance_id':n} for n in (1,9)]
   if mode!='no-radio':grants += [{'capability':'radio.lora','api':2,'instance_id':0}]
  boot={'board':'board.json','default_app':'default.elf','provider_activation':activation,'drivers':[{'manifest':n+'.json'} for n in drivers],'app_capabilities':[{'manifest':'default.json','grants':grants}]}
  if True:
   host={'type':'application','id':'host','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'host.elf','entry':'app_main','requires':[],'optional':[req(c) for c in ['display.output','input.touch.raw','input.navigation','storage.key-value','input.power-key','power.sleep','display.power']]}
   write('host.json',host);boot['default_app']='host.elf';boot['resident_shell']={'api':1,'host':'host.elf','foreground':['default.elf'],'legacy':['springboard.elf']}
   boot['app_capabilities'].append({'manifest':'host.json','grants':[{**req(c),'instance_id':0} for c in ['display.output','input.touch.raw','input.navigation','input.power-key','power.sleep','display.power']]+[{'capability':'storage.key-value','api':1,'instance_id':1}]})
  if mode in ('alarm','text-alarm','receive-alarm','alarm-delayed','text-idle-alarm'):
   boot['drivers'] += [{'manifest':'rtc.json'},{'manifest':'alarm.json','key_value':[{'key':k,'namespace':ns,'access':access} for k,ns,access in [('alarm_cfg',3,'read'),('timer_cfg',3,'read'),('alarm_occ',4,'read-write'),('timer_occ',4,'read-write'),('alert_mode',1,'read')]]}]
   for cap in ('rtc.clock','alarm.service'):
    host['optional'].append({'capability':cap,'api':2});boot['app_capabilities'][1]['grants'].append({'capability':cap,'api':2,'instance_id':0})
   write('host.json',host)
  write('springboard.json',{'type':'application','id':'springboard','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'springboard.elf','entry':'app_main','requires':[]})
  boot['app_capabilities'].append({'manifest':'springboard.json','grants':[]})
  write('boot.json',boot)
  result=run([out/'test',out,'resident-'+mode,activation],activation+'-'+mode,required=False);print(result,end='',flush=True);results.append(result)
assert source_hashes=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files if p.is_file()},'Source changed during execution'
write('receipt.json',{'passed':all(code==0 for code in exits.values()),'source_hashes':source_hashes,'exit_codes':exits,'results':results,'sanitize':a.sanitize,'runtime_mocked':False,'controller_sha256':hashlib.sha256((R/'Apps/lora_scene.c').read_bytes()).hexdigest(),'scope':'Actual Runtime/Graph and production scene/text; hardware endpoints doubled. Production Watch Home resident host; physical endpoints doubled.'})
assert all(code==0 for code in exits.values()),'Integration failures; see per-case logs and receipt'
