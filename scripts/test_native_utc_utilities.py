#!/usr/bin/env python3
"""Bounded production utility/controller/adapter tests with native provider doubles."""
import argparse,ast,json,os,subprocess,tempfile
from pathlib import Path
from native_utility_build import *

def old_scenes(path):
 # Load only the existing local fixture's declarative scenes/functions. Its CLI
 # and build/run loops are deliberately excluded from this reuse.
 tree=ast.parse(path.read_text());selected=[]
 for node in tree.body:
  if isinstance(node,ast.FunctionDef) and node.name in ('check','tap','scene','key_tap','type_name','target_scenes'):selected.append(node)
  elif isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='scenes' for t in node.targets):selected.append(node)
 if not selected:raise ValueError('No declarative fixture scenes found: '+str(path))
 return execute(selected)
def execute(nodes):
 scope={};exec(compile(ast.Module(nodes,[]),'<existing scenes>','exec'),scope);return scope

def transformed_fixture(name,destination):
 if name=='waterfall':
  source=ROOT/'test/native_apps/rf_renderer_test.c';mapping=dict(TICKS='fx_ticks',GRANTS='fx_grants',KV='fx_legacy_kv',ALARM='fx_alarm_api',ACQUIRE='fx_acquire',RELEASE='fx_release',RUNTIME='fx_runtime',NAV='fx_nav_api',RASTER='fx_raster',FRAMES='fx_frames',SUBS='fx_subs',OPS='(fx_captures+fx_suspend+fx_writes+fx_app_writes+fx_diag_total)')
 else:
  source=ROOT/'test/native_apps'/('ble_renderer_test.c' if name=='ble_scanner' else 'hid_renderer_test.c');mapping=dict(TICKS='ticks',GRANTS='grants',KV='kv_api',ALARM='alarm_api',ACQUIRE='fake_acquire',RELEASE='fake_release',RUNTIME='runtime_api',NAV='nav_api',RASTER='pixels',FRAMES='frames',SUBS='subs',OPS='(radio_sends+radio_closes+radio_claims+radio_control_calls)' if name=='ble_scanner' else '(hid_mouse_reports+hid_keyboard_reports+hid_polls+hid_closes+radio_control_calls)')
 text=source.read_text().replace('#include "../../Apps/waterfall.c"','#include '+json.dumps(str(ROOT/'Apps/waterfall.c')))
 if name in ('ble_touchpad','ble_buttons'):
  text='#include "AlarmServiceV1.h"\ntypedef struct {alarm_service_v1 service;uint32_t output_modes;} alarm_service_outputs_v1;\n'+text
 lines=text.splitlines();lines=[line for line in lines if not line.startswith('const risc_runtime_api_v1 *risc_runtime_get_api(')]
 injection='\n'.join('#define NU_'+key+' '+value for key,value in mapping.items())+'\n#include '+json.dumps(str(ROOT/'test/native_apps/native_utility_peripherals.h'))+'\n'
 text='\n'.join(lines).replace('int main(',injection+'int main(').replace('assert(app_module_init()==0);','assert(app_module_init()==0);nu_check_clock();')
 text=text.replace('app_main();','app_main();if(nu_retained){nu_check_retained();return 0;}nu_check_motion();')
 text=text.replace('assert(!grants&&!frames&&!subs&&!radio_owned)', 'fprintf(stderr, "native retained=%u grants=%u frames=%u subs=%u radio=%u\\n",nu_retained,grants,frames,subs,radio_owned);assert(!grants&&!frames&&!subs&&!radio_owned)')
 destination.write_text(text+'\n');return source

def main():
 p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--drivers',type=Path,required=True);p.add_argument('--normal-only',action='store_true');p.add_argument('--app',action='append',choices=['ble_scanner','ble_touchpad','ble_buttons','waterfall']);motion.options(p);a=p.parse_args()
 system=a.system_apps.resolve();runtime=a.runtime.resolve();exact(system,SYSTEM);exact(runtime,RUNTIME);adapter=motion.select(a,p,system)
 out=ROOT/('build/native-paper-motion-utilities' if a.paper_transitions else 'build/native-utc-utilities');out.mkdir(parents=True,exist_ok=True);receipt={'system_sha':SYSTEM,'paper_motion':motion.receipt(adapter) if a.paper_transitions else None,'runtime_sha':RUNTIME,'source_sha':git(ROOT,'rev-parse','HEAD'),'source_dirty':bool(git(ROOT,'status','--porcelain')),'hardware_verified':False,'runs':[],'sources':{}}
 rf=old_scenes(ROOT/'scripts/test_rf_application.py')['target_scenes']('x4')
 selected_rf=['lifecycle','sidebands','controls','cursor-controls-drag','guard-keyboard','guard-label-draft','guard-event-review','home-touch','home-navigation-draft','home-storage-discard','quick-keyboard-draft','quick-event-review','quick-unavailable-wifi','events','frequency-labels']
 assert set(selected_rf)<=rf.keys(), 'Required RF scene missing'
 ble={'scan-complete':([(3,100,730)],{'BLE_RENDER_SCAN':'1','BLE_COMPLETE':'1'}),'name-save':([(3,100,730),(30,100,160),(45,100,730),(60,140,240),(80,360,600)],{'BLE_RENDER_SCAN':'1','BLE_EXPECT_NAME':'Kitchen sensorb'}),'back':([(3,100,730),(70,60,50)],{'BLE_RENDER_SCAN':'1','BLE_RENDER_BACK':'1'}),'idle':([],{'BLE_IDLE':'1'})}
 hid={'idle':([],{}),'reconnect':([(3,110,732)],{'HID_RENDER_ACTIVE':'1'}),'pair-accept':([(3,348,732),(10,100,220),(30,348,732)],{'HID_RENDER_ACTIVE':'1','HID_RENDER_PAIR':'1','HID_RENDER_PAIR_ACCEPT':'1'}),'quick-controls':([(3,110,732),(30,200,20),(31,200,100),(90,240,620),(91,240,560)],{'HID_RENDER_ACTIVE':'1','HID_RENDER_QUICK':'1'}),'home':([(3,110,732)],{'HID_RENDER_ACTIVE':'1','HID_RENDER_HOME':'1'}),'save-retry':([(3,348,732),(10,100,530),(20,405,345),(60,348,732),(80,348,732)],{'HID_RENDER_SAVED':'1','HID_RENDER_SAVE_FAIL':'1'}),'quick-edit':([(3,348,732),(10,100,530),(20,405,345),(30,200,20),(31,200,100),(90,240,620),(91,240,560),(110,348,732)],{'HID_RENDER_SAVED':'1'})}
 with tempfile.TemporaryDirectory(prefix='native-util-test-') as temp:
  folder=Path(temp);headers=stage(system,runtime,folder,out,adapter)
  catalog=folder/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
  for name in a.app or ['ble_scanner','ble_touchpad','ble_buttons','waterfall']:
   fixture=folder/(name+'-fixture.c');original=transformed_fixture(name,fixture);receipt['sources'][str(original.relative_to(ROOT))]=sha(original)
   for sanitized in ([False] if a.normal_only else [False,True]):
    dest=out/name/('sanitized' if sanitized else 'normal');dest.mkdir(parents=True,exist_ok=True)
    defs=flags(name,paper_transitions=a.paper_transitions);defs+=['-DRF_RENDER_PAPER'] if name=='waterfall' else ['-DBLE_PAPER'] if name=='ble_scanner' else ['-DHID_RENDER_PAPER']
    src=sources(name,system,adapter=adapter);src=[p for p in src if p.name!='SingleFloatDivisionCompat.c']
    if name=='waterfall':src=src[1:]
    src += [fixture,catalog]
    if name=='ble_scanner':src += [a.drivers/'Drivers/ble_sensors/driver.c']
    inc=include_paths(adapter,headers)+[a.drivers/'sdk/driver',ROOT/'test/native_apps']
    san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
    binary=dest/'test';run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*san,*defs,*['-I'+str(i) for i in inc],*src,'-Wl,--wrap=free','-lm','-o',binary])
    scenes={key:rf[key] for key in selected_rf} if name=='waterfall' else ble if name=='ble_scanner' else {key:v for key,v in hid.items() if name=='ble_buttons' or key not in ('save-retry','quick-edit')}
    if a.paper_transitions:
     if name=='waterfall':
      scenes['quick-repeat']=("wait 3\npoint 200 20\npoint 200 400\nwait 80\ncheck running eq 0\nremember captures\nraw 240 675\nwait 80\nsame captures\npoint 200 20\npoint 200 400\nwait 80\nraw 240 675\nwait 80\nsame captures\ncheck launches eq 0\nnav 2\ncheck running eq 1\nfinish\n",{'NATIVE_QUICK_EXPECT':'2'})
     else:
      repeat=[(20,200,20),(21,200,400),(80,200,670),(81,200,400),(140,200,20),(141,200,400),(200,200,670),(201,200,400)]
      extra={'NATIVE_QUICK_EXPECT':'2'}
      if name!='ble_scanner':repeat=[(3,110,732),*repeat];extra['HID_RENDER_ACTIVE']='1'
      scenes['quick-repeat']=(repeat,extra)
      if name!='ble_scanner':scenes['quick-interrupted']=( [(3,110,732),(20,200,20),(21,200,150),(21,220,160),(80,200,20),(81,200,400),(170,200,670),(171,200,400)],{'HID_RENDER_ACTIVE':'1','NATIVE_QUICK_EXPECT':'1'})
     scenes={key:(commands.replace('point 200 20\npoint 200 100\nwait 2\n','point 200 20\npoint 200 100\nwait 80\n').replace('raw 240 675\nwait 2\n','raw 240 675\nwait 80\n') if isinstance(commands,str) else [(170 if key=='quick-edit' and n==110 else n,x,500 if y==560 else y) for n,x,y in commands],extra) for key,(commands,extra) in scenes.items()}
    for key,(commands,extra) in scenes.items():
     scene_dir=dest/key;scene_dir.mkdir(exist_ok=True);input=scene_dir/'input.txt';input.write_text(commands if isinstance(commands,str) else ''.join(f'{n} {x} {y}\n' for n,x,y in commands));env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',RF_RENDER_NO_IMAGES='1',**extra)
     with (scene_dir/'test.log').open('w') as log:run([binary,scene_dir,input],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90)
     receipt['runs'].append(dict(app=name,sanitized=sanitized,case=key));print(name+' '+str(sanitized)+' '+key+' passed',flush=True)
    for mode in ['absent','unset','io','malformed','zone-missing','zone-invalid']:
     case=dest/('native-'+mode);case.mkdir(exist_ok=True);input=case/'input.txt';input.write_text('wait 3\ncheck running eq 1\nfinish\n' if name=='waterfall' else '')
     env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',NATIVE_TIME_CASE=mode,RF_RENDER_NO_IMAGES='1')
     with (case/'test.log').open('w') as log:run([binary,case,input],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90)
     receipt['runs'].append(dict(app=name,sanitized=sanitized,case='native-'+mode))
    for mode in ['context','release-failure','zone-context']:
     case=dest/('native-'+mode);case.mkdir(exist_ok=True);input=case/'input.txt';input.write_text('wait 3\npoint 200 20\npoint 200 100\nwait 5\nfinish\n' if name=='waterfall' else '3 110 732\n30 200 20\n31 200 100\n')
     env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',NATIVE_TIME_CASE=mode,RF_RENDER_NO_IMAGES='1')
     with (case/'test.log').open('w') as log:run([binary,case,input],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90)
     receipt['runs'].append(dict(app=name,sanitized=sanitized,case='native-'+mode))
 receipt['drivers_source_sha']=git(a.drivers,'rev-parse','HEAD');receipt['drivers_source_dirty']=bool(git(a.drivers,'status','--porcelain'));receipt['driver_sha256']=sha(a.drivers/'Drivers/ble_sensors/driver.c')
 receipt['sources']['test/native_apps/native_utility_peripherals.h']=sha(ROOT/'test/native_apps/native_utility_peripherals.h')
 (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Native utility composed host cases passed: '+str(len(receipt['runs'])))
if __name__=='__main__':main()
