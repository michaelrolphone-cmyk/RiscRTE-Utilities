#!/usr/bin/env python3
"""Run actual BLE app_main and System adapter with exact X4 target receipt flags."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--drivers',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=json.loads((a.build/'ble_scanner/x4-native-app.json').read_text());cmd=r['compile_command'];system=Path(next(x for x in cmd if x.endswith('/lib/PortableApps/src/adapter.c'))).parents[3]
sys.path.insert(0,str(system/'scripts'));from build_scene_services import PROFILE_FLAGS
flags=[x for x in cmd if x.startswith(('-D','-I'))]+['-DBLE_PAPER_RENDER','-DBLE_RESIDENT_RENDER']
flags+=['-I'+str(x) for x in (system/'Services/scene_profile',system/'Services/text_input',system/'sdk/app',a.drivers/'sdk/driver')]
sources=[Path(x) for x in cmd if x.endswith('.c')]
for name,digest in r['compiled_dependencies_sha256'].items():
 label,relative=name.split('/',1);base={'Source':ROOT,'System':system,'CompiledSDK':a.build/'sdk/include'}[label]
 assert hashlib.sha256((base/relative).read_bytes()).hexdigest()==digest,name
scenes={'idle':[],'scan-stop':[(3,50,730),(40,50,730)],'nested-back':[(3,50,730),(30,100,140),(50,50,40),(70,50,40)],'enable-scan':[(3,50,730),(10,50,730),(20,50,730)]}
base=[(3,50,730),(30,100,140),(50,100,730)]
scenes.update({'name-save':base+[(60,100,400),(80,400,690)],'name-cancel':base+[(60,100,400),(80,20,20)],'name-navigation-back':base+[(60,100,400),(80,-1,0)],'name-unavailable':base,'name-retained':base+[(60,100,400),(80,20,20)],'name-back-return':base+[(60,100,400),(80,-1,0),(110,50,40),(140,50,40)],'name-repeat':base+[(60,100,400),(80,400,690),(110,100,730),(120,100,400),(140,20,20),(165,100,730),(180,100,400),(200,400,690)]})
scenes['home']=[(3,50,730),(30,-2,0)]
scenes['name-navigation-home']=base+[(60,100,400),(80,-2,0),(110,-2,0)]
scenes['resident-controls']=[(20,100,20),(21,100,60),(22,100,100)]
for boundary in ('acquire','open','poll','close','release','kv-acquire','kv-get','kv-put','kv-release','acquire-fail'):
 scenes['name-loss-'+boundary]=base+[(60,100,400),(80,400,690)]
results=[];a.output.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 out=a.output/str(int(san));out.mkdir(exist_ok=True)
 extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,*flags]
 objects=[]
 for host,entry in [('scene_host','ble_scene_driver_get'),('text_input','ble_text_driver_get'),('scene_profile','ble_profile_driver_get')]:
  obj=out/(host+'.o');objects.append(obj)
  subprocess.run([*command,* (PROFILE_FLAGS['portrait-monochrome'] if host=='scene_profile' else []),'-Dt5_driver_get='+entry,'-c',str(system/'Services'/host/('profile.c' if host=='scene_profile' else 'host.c')),'-o',str(obj)],check=True)
 exe=out/'ble-renderer';subprocess.run([*command,*map(str,[a.drivers/'Drivers/ble_sensors/driver.c',ROOT/'test/native_apps/ble_paper_renderer_test.c',*sources,*objects]),'-o',str(exe)],check=True)
 for scene,actions in scenes.items():
  folder=out/scene;folder.mkdir(exist_ok=True);path=folder/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions));env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
  if scene not in ('idle','resident-controls'):env['BLE_RENDER_SCAN']='1'
  if scene in ('home','name-navigation-home'):env['BLE_RENDER_HOME']='1'
  if scene=='resident-controls':env['BLE_RENDER_CONTROLS']='1'
  if scene in ('nested-back','name-back-return'):env['BLE_RENDER_BACK']='1'
  if scene=='enable-scan':env['BLE_RENDER_ENABLE']='1'
  if scene.startswith('name-'):env['BLE_RENDER_NAME']=scene[5:]
  run=subprocess.run([str(exe),str(folder),str(path)],env=env,capture_output=True,text=True,timeout=60);(folder/'run.log').write_text(run.stdout+run.stderr)
  if run.returncode:raise RuntimeError(f'{scene}: {run.stdout}{run.stderr}')
  frames=list(folder.glob('frame-*.ppm'));assert frames;header=b'P6\n480 800\n255\n'
  for frame in frames:
   data=frame.read_bytes();assert data.startswith(header) and len(data)==len(header)+480*800*3
  print('X4 exact resident',san,scene,'PASS',len(frames),'frames',flush=True);results.append(dict(sanitized=san,scenario=scene,frames=len(frames),passed=True))
record=dict(schema=1,scope='actual production app_main and exact target-receipt adapter/native-time/resident/custody flags with real shared hosts; peripheral and Runtime-table fakes',source_revision=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),target_elf_sha256=r['elf_sha256'],target_receipt_sha256=hashlib.sha256((a.build/'ble_scanner/x4-native-app.json').read_bytes()).hexdigest(),build_defines=r['build_defines'],system_revision=r['system_source_revision'],runtime_revision=r['runtime_source_revision'],driver_revision=subprocess.check_output(['git','-C',str(a.drivers),'rev-parse','HEAD'],text=True).strip(),physical_panel=[800,480],logical_viewport=[480,800],profile_flags=PROFILE_FLAGS['portrait-monochrome'],results=results,native_time_callback_checked_after_clean_app_return=True,hardware_verified=False)
(a.output/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
