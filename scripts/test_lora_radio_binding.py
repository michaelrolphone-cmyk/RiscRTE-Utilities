#!/usr/bin/env python3
"""Unchanged LoRa controller through production Runtime/Graph, scene and text DSOs."""
import argparse,hashlib,json,os,subprocess,shutil,sys,importlib.util
from pathlib import Path
from scanner_support import sdk
from lifecycle_dependencies import Closure
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--providers',type=Path,required=True)
p.add_argument('--scene-profile',choices=['watch-compact','color-dynamic'],required=True)
for k in ('runtime','system','watch','home-system','home-utilities','output'):p.add_argument('--'+k,type=Path,required=True)
p.add_argument('--radio-instance',type=int,required=True)
p.add_argument('--width',type=int,required=True);p.add_argument('--height',type=int,required=True)
p.add_argument('--civil-case',choices=['all','absent','summer','winter','rollover','invalid','loss'],default='absent')
p.add_argument('--profile',choices=['watch','wave'],required=True);p.add_argument('--case',action='append');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
assert 0<=a.radio_instance<=2147483647
out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
runtime=a.runtime.resolve();system=a.system.resolve();watch=a.watch.resolve()
sys.path.insert(0,str(watch/'scripts'))
from merged_sdk import headers
from build_inputs import verify_dependency
hs=a.home_system.resolve();hu=a.home_utilities.resolve()
lock=json.loads((watch/'dependencies.lock.json').read_text())
verify_dependency(hs,lock['system']['commit'],['lib/PortableApps','lib/elf_loader','test/native_apps'])
verify_dependency(hu,lock['utilities']['commit'],['Apps/alarm_writer.h','lib/Alarm/include','lib/Contexts/include','Services/alarm_service'])
hinc=headers(out/'home',hs,hu,watch)
inc=headers(out,system,R,watch)
spec=importlib.util.spec_from_file_location('selected_sdk',system/'scripts/scene_sdk.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);module.stage_sdk(runtime,system,out/'sdk')
extra=sdk(runtime,system,out/'lora-sdk')
for header in extra.glob('*.h'):
 if not (inc/header.name).exists():shutil.copyfile(header,inc/header.name)
closure=Closure([('Watch',watch),('Runtime',runtime),('System',system),('Utilities',R),('HomeSystem',hs),('HomeUtilities',hu),('SelectedSystem',a.providers.resolve()),('provider-sdk',out/'provider-sdk'),('generated-sdk',out/'sdk'),('home-sdk',out/'home')],out/'dependencies.json')
# Consumer compiled against the exact newer contracts used by canonical LoRa.
# Provider compiled separately with its own pinned canonical SDK and source.
import importlib.util
provider_system=a.providers.resolve()
sp=importlib.util.spec_from_file_location('provider_scene_sdk',provider_system/'scripts/scene_sdk.py');pm=importlib.util.module_from_spec(sp);sp.loader.exec_module(pm)
provider_inc=pm.stage_sdk(runtime,provider_system,out/'provider-sdk')
for n in ('RiscTextEntryV1.h',):shutil.copyfile(provider_system/'sdk/app'/n,provider_inc/n)
for n in ('RiscUsbHidV1.h','RiscUsbControllerV1.h','RiscUsbProviderV1.h'):shutil.copyfile(provider_system/'Services/text_input'/n,provider_inc/n)
for n in ('PortableTimeZone.h','PortableRtcClock.h','PortableTime.h'):shutil.copyfile(provider_system/'lib/PortableApps/include'/n,provider_inc/n)
shutil.copytree(provider_system/'lib/PortableApps/time',provider_inc.parent/'time')
scene_flags=['-DSCENE_HOST_TELEMETRY=0']+(['-DSCENE_HOST_WATCH_240=1'] if a.scene_profile=='watch-compact' else [])
commands=[];exits={}
source_files=[Path(__file__),R/'scripts/scanner_support.py',R/'scripts/lifecycle_dependencies.py',*sorted((R/'test/lora_panel_civil').glob('*')),*sorted((R/'test/lora_radio_binding').glob('*'))]
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
def build(src,name,extra=()):run(['cc','-std=c11',*flags,'-fPIC','-fvisibility=hidden','-shared','-I'+str(provider_inc if name in ('scene.elf','text.elf','profile.elf') else inc),'-I'+str(system/'lib/NativeApps/include'),'-I'+str(system/'Services/scene_profile'),'-I'+str(system/'Services/text_input'),*extra,src,'-o',out/name],'build-'+name)
build(R/'test/lora_panel_civil/app.c','default.elf')
# Parse the public build recipe without executing its argument parser.
import ast
module_ast=ast.parse((watch/'scripts/build.py').read_text())
flag_node=next(n.value for n in module_ast.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='FLAGS' for t in n.targets))
# FLAGS contains only constants and the four standard division export expressions.
profile_flags=eval(compile(ast.Expression(flag_node),'<published FLAGS>','eval'),{'__builtins__':{}},{})
profile_flags+=['-DWATCH_AUTOMATIC_PROVISION']
if a.profile=='wave':profile_flags+=['-DWATCH_SERIAL_TRANSFER_MODE']
assert '-DALARM_SERVICE_TAGGED_V2' not in profile_flags
(out/'profile.json').write_text(json.dumps({'profile':a.profile,'published_flags':profile_flags,'alarm_api':1,'dependency_lock':lock,'host_link_boundary':'native host compiler; target-only division exports omitted'},indent=2))
host_flags=[f for f in profile_flags if not f.startswith('-Wl,')]+['-Wno-unused-function','-Wno-misleading-indentation']
host_includes=['-I'+str(hinc),'-I'+str(hs/'lib/PortableApps/include'),'-I'+str(hs/'lib/PortableApps/src'),'-I'+str(watch/'include'),'-I'+str(watch)]
objects=[]
for i,src in enumerate([R/'test/lora_panel_civil/home.c',*[watch/'apps/clock'/n for n in ['nova/nova.c','viewport.c','capability_clients.c','quick_radios.c','points_projection.c']],*[hs/'lib/PortableApps/src'/n for n in ['quick_actions.c','quick_session.c','quick_render.c','PortableAutomaticProvision.c','PortableNetworkSession.c']]]):
 obj=out/('home-'+str(i)+'.o');run(['cc','-std=c11',*flags,'-fPIC','-fvisibility=hidden',*host_includes,*host_flags,'-DHOME_SOURCE="'+str(watch/'apps/clock/crown.c')+'"','-c',src,'-o',obj],'build-home-'+str(i));objects.append(obj)
run(['c++','-std=c++11',*flags,'-fPIC','-fvisibility=hidden','-I'+str(hinc),*host_flags,'-shared',watch/'apps/clock/effects/boot.cpp',*objects,'-o',out/'host.elf'],'link-home')
build(R/'test/lora_panel_civil/launcher.c','springboard.elf')
build(hu/'Services/alarm_service/service.c','alarm.elf',['-I'+str(hu/'lib/Alarm/include')])
for name,cap in [('haptic','haptic.effect'),('audio','audio.output')]:build(R/'test/lora_panel_civil/provider.c',name+'.elf',['-DTEST_ID=\"'+name+'\"','-DTEST_CAP=\"'+cap+'\"'])
build(R/'test/lora_panel_civil/provider.c','rtc.elf',['-DTEST_ID="rtc"','-DTEST_CAP="rtc.clock"','-DTEST_API=2'])
run(['cc','-std=c11',*flags,'-fPIC','-fvisibility=hidden','-shared','-I'+str(provider_inc),'-DPORTABLE_RTC_UTC8_DENVER','-DCIVIL_CLOCK_ID="civil-clock-watch"',provider_system/'Services/civil_clock/clock.c',provider_system/'lib/PortableApps/src/PortableTimeZone.c',provider_system/'lib/PortableApps/src/PortableTimeZoneCatalog.c','-o',out/'civil.elf'],'build-civil')
build(R/('test/lora_radio_binding/radio.c' if a.radio_instance else 'test/lora_panel_civil/radio.c'),'radio.elf',['-DTEST_RADIO_INSTANCE='+str(a.radio_instance)])
build(R/'test/lora_panel_civil/scene_probe.c','scene.elf',[*scene_flags,'-finstrument-functions','-DSCENE_SOURCE="'+str(provider_system/'Services/scene_host/host.c')+'"'])
build(provider_system/'Services/text_input/host.c','text.elf')
build(provider_system/'Services/scene_profile/profile.c','profile.elf',['-DSCENE_PROFILE_ID="profile"'])
providers=[('display','display.output'),('touch','input.touch.raw'),('nav','input.navigation'),('keyboard','usb.hid.keyboard'),('power-key','input.power-key'),('power-sleep','power.sleep'),('display-power','display.power')]
for name,cap in providers:build(R/'test/lora_panel_civil/provider.c',name+'.elf',['-DTEST_ID="'+name+'"','-DTEST_CAP="'+cap+'"'])
def write(name,data):(out/name).write_text(json.dumps(data,indent=2)+'\n')
def req(cap):return {'capability':cap,'api':1}
def manifest(name,cap,requires=()):return {'type':'driver','id':name,'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':name+'.elf','requires':[req(c) for c in requires],'provides':[req(cap)]}
for name,cap in providers:write(name+'.json',manifest(name,cap))
m=json.loads((provider_system/'Services/scene_host/manifest-watch-compact.json').read_text());m['file_name']='scene.elf';write('scene.json',m)
t=manifest('text','ui.text-input',['ui.scene','usb.hid.keyboard']);t['id']='text-input-host';write('text.json',t);write('profile.json',manifest('profile','ui.presentation-profile'))
radio=manifest('radio','radio.lora');radio['provides'][0]['api']=2
if a.radio_instance:
 radio['requires']=[req('hardware.device')];radio['hardware_compatibility']=[{'compatible':'test,radio-endpoint','revisions':['unspecified'],'config_type':'gpio.bank','config_version':1}]
write('radio.json',radio)
alarm=manifest('alarm','alarm.service',['storage.key-value.bound','platform.clock','rtc.clock','haptic.effect','audio.output']);alarm['id']='alarm-service';alarm['provides'][0]['api']=1;alarm['requires'][2]['api']=2;write('alarm.json',alarm)
write('haptic.json',manifest('haptic','haptic.effect'));write('audio.json',manifest('audio','audio.output'))
rtc=manifest('rtc','rtc.clock');rtc['provides'][0]['api']=2;write('rtc.json',rtc)
civil=json.loads((provider_system/'Services/civil_clock/watch.json').read_text());civil['file_name']='civil.elf';write('civil.json',civil)
write('board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':([{'instance_id':a.radio_instance,'chip':{'vendor':'test','model':'radio-endpoint','revision':'unspecified'},'compatible':'test,radio-endpoint','config_type':'gpio.bank','config_version':1,'config':{'pins':[8],'active_high':True,'pull_up':False,'debounce_us':0,'long_press_us':0,'click_min_us':0}}] if a.radio_instance else [])})
m=json.loads((R/'Apps/lora_scene.json').read_text());m['file_name']='default.elf';write('default.json',m)
units=[runtime/'src'/x for x in ['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']]
run(['c++','-std=c++17',*flags,'-Wno-missing-field-initializers','-fno-pie','-no-pie','-rdynamic',*['-I'+str(runtime/x) for x in ['src','lib/ArduinoJson/src','test/drivers/stubs','test']],'-I'+str(inc),*units,R/'test/lora_panel_civil/graph.cpp','-ldl','-o',out/'test'],'build-runtime')
results=[]
for civil_case in (['summer','winter','rollover','invalid','loss'] if a.civil_case=='all' else [a.civil_case]):
 for activation in ['eager','demand']:
  for mode in (a.case or ['exit','compose','send','short-back','update-input-boundary','profile','receive-history','delayed-display','cancel-failure','native-radio-send','short-key','long-key','controls','idle','text-idle','text-short-key','alarm','text-alarm','receive-alarm','receive-controls','alarm-delayed','receive-controls-delayed','text-idle-alarm']):
   drivers=[] if mode=='no-ui' else ['scene','profile',*[n for n,c in providers]]+([] if mode=='no-text' else ['text'])
   if mode not in ('no-ui','no-radio'):drivers+=['radio']
   grants=[] if mode=='no-ui' else [{'capability':'ui.scene','api':1,'instance_id':0}]+([] if mode=='no-text' else [{'capability':'ui.text-input','api':1,'instance_id':0}])
   if mode!='no-ui':
    if mode!='no-storage':grants += [{'capability':'storage.key-value','api':1,'instance_id':n} for n in (1,9)]
    if mode!='no-radio':grants += [{'capability':'radio.lora','api':2,'instance_id':a.radio_instance}]
   boot={'board':'board.json','default_app':'default.elf','provider_activation':activation,'drivers':[{'manifest':n+'.json'} for n in drivers],'app_capabilities':[{'manifest':'default.json','grants':grants}]}
   if True:
    host={'type':'application','id':'host','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'host.elf','entry':'app_main','requires':[],'optional':[req(c) for c in ['runtime.realtime-control','display.output','input.touch.raw','input.navigation','storage.key-value','input.power-key','power.sleep','display.power']]}
    write('host.json',host);boot['default_app']='host.elf';boot['resident_shell']={'api':1,'host':'host.elf','foreground':['default.elf'],'legacy':['springboard.elf']}
    boot['app_capabilities'].append({'manifest':'host.json','grants':[{**req(c),'instance_id':0} for c in ['runtime.realtime-control','display.output','input.touch.raw','input.navigation','input.power-key','power.sleep','display.power']]+[{'capability':'storage.key-value','api':1,'instance_id':1}]})
   if mode in ('alarm','text-alarm','receive-alarm','alarm-delayed','text-idle-alarm'):
    boot['drivers'] += [{'manifest':'haptic.json'},{'manifest':'audio.json'},{'manifest':'rtc.json'},{'manifest':'alarm.json','key_value':[{'key':k,'namespace':ns,'access':access} for k,ns,access in [('alarm_cfg',3,'read'),('timer_cfg',3,'read'),('alarm_occ',4,'read-write'),('timer_occ',4,'read-write'),('alert_mode',1,'read')]]}]
    for cap in ('rtc.clock','alarm.service'):
     api=2 if cap=='rtc.clock' else 1;host['optional'].append({'capability':cap,'api':api});boot['app_capabilities'][1]['grants'].append({'capability':cap,'api':api,'instance_id':0})
    write('host.json',host)
   write('springboard.json',{'type':'application','id':'springboard','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'springboard.elf','entry':'app_main','requires':[]})
   boot['app_capabilities'].append({'manifest':'springboard.json','grants':[]})
   if civil_case!='absent':
    if not any(d['manifest']=='rtc.json' for d in boot['drivers']):boot['drivers'].append({'manifest':'rtc.json'})
    boot['drivers'].append({'manifest':'civil.json'})
    grants.append({'capability':'time.civil','api':1,'instance_id':0})
   for driver in boot['drivers']:
    if driver['manifest']=='radio.json' and a.radio_instance:driver['instance_id']=a.radio_instance
   write('boot.json',boot)
   result=run([out/'test',out,'resident-'+mode,activation,a.width,a.height,civil_case],civil_case+'-'+activation+'-'+mode,required=False);print(result,end='',flush=True);results.append(result)
assert source_hashes=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files if p.is_file()},'Source changed during execution'
write('receipt.json',{'radio_instance':a.radio_instance,'application_request_instance':0,'geometry':[a.width,a.height],'civil_case':a.civil_case,'passed':all(code==0 for code in exits.values()),'source_hashes':source_hashes,'exit_codes':exits,'results':results,'sanitize':a.sanitize,'runtime_mocked':False,'controller_sha256':hashlib.sha256((R/'Apps/lora_scene.c').read_bytes()).hexdigest(),'scope':'Actual Runtime/Graph and production scene/text; hardware endpoints doubled. Production Watch Home resident host; physical endpoints doubled.'})
assert all(code==0 for code in exits.values()),'Integration failures; see per-case logs and receipt'
