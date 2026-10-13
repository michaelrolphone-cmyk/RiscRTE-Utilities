import sys,json,subprocess,os,shutil
from pathlib import Path
import argparse,hashlib
p=argparse.ArgumentParser()
for n in ['build','hosts','drivers','output']:p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--sanitizer',default='normal',choices=['normal','undefined','address,undefined'])
p.add_argument('--regression-only',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1];system=a.hosts.resolve();drivers=a.drivers.resolve();build=a.build.resolve();out=a.output.resolve();out.mkdir(exist_ok=True,parents=True)
inc=out/'include';shutil.copytree(build/'sdk/include',inc,dirs_exist_ok=True)
for f in (root/'lib/Bluetooth/include').glob('*.h'):shutil.copyfile(f,inc/f.name)
r=json.loads((build/'ble_scanner/x4-native-app.json').read_text());cmd=[x.replace(str(build/'sdk/include'),str(inc)) for x in r['compile_command']]
flags=[s for s in cmd if s.startswith(('-D','-I'))]+['-DBLE_PAPER_RENDER','-DBLE_RESIDENT_RENDER']
flags+=['-I'+str(p) for p in [system/'Services/scene_profile',system/'Services/text_input',system/'sdk/app',drivers/'sdk/driver']]
sources=[s for s in cmd if s.endswith('.c')]
sys.path.insert(0,str(system/'scripts'));from build_scene_services import PROFILE_FLAGS
sys.path.insert(0,str(root/'scripts'));from test_x4_ble_shared_text import scenarios,tap,nav
objects=[]
if not (system/'Services/scene_host/fonts/telemetry.inc').exists():subprocess.run([sys.executable,str(system/'scripts/generate_telemetry_fonts.py')],check=True)
base=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',*flags]
if a.sanitizer!='normal':base+=['-fsanitize='+a.sanitizer,'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
for host,entry in [('scene_host','ble_scene_driver_get'),('text_input','ble_text_driver_get'),('scene_profile','ble_profile_driver_get')]:
 obj=out/(host+'.o');objects.append(str(obj));subprocess.run([*base,*(PROFILE_FLAGS['portrait-monochrome'] if host=='scene_profile' else []),'-Dt5_driver_get='+entry,'-c',str(system/'Services'/host/('profile.c' if host=='scene_profile' else 'host.c')),'-o',str(obj)],check=True)
for i,src in enumerate([str(drivers/'Drivers/ble_sensors/driver.c'),str(root/'test/native_apps/scanner_policy_x4_host_test.c'),*sources]):
 obj=out/(str(i)+'.o');objects.append(str(obj));subprocess.run([*base,'-c',src,'-o',str(obj)],check=True)
exe=out/'host';subprocess.run([*base,*objects,'-o',str(exe)],check=True)
scenes={k:v for k,v in scenarios().items() if not k.startswith('name-loss-kv-')}
scenes['name-navigation-home']=scenes['name-navigation-home'][:-1]
name=tap(200,50,730)+tap(500,100,140,'scan')+tap(250,100,730)
for case in ['detached','continuity','pause-again','pause-busy','pause-retained','resume-retained','resume-again','poll-exit','apply-exit','paused-home','missing','malformed','unsupported']:
 scenes['policy-'+case]=name+(tap(4000,100,400,'text')+tap(250,400,690,'text') if case in ['detached','continuity','pause-again','pause-busy'] else [])
results=[]
for scene,actions in scenes.items():
 if a.regression_only and scene.startswith('policy-'):continue
 folder=out/scene;folder.mkdir(exist_ok=True);path=folder/'actions.txt';path.write_text(''.join(' '.join(map(str,row))+'\n' for row in actions))
 env={k:v for k,v in os.environ.items() if not k.startswith(('BLE_RENDER_','BLE_POLICY'))}
 env['ASAN_OPTIONS']='detect_leaks=0'
 if scene not in ('idle','resident-controls'):env['BLE_RENDER_SCAN']='1'
 if scene in ('home','name-navigation-home'):env['BLE_RENDER_HOME']='1'
 if scene=='resident-controls':env['BLE_RENDER_CONTROLS']='1'
 if scene in ('nested-back','name-back-return'):env['BLE_RENDER_BACK']='1'
 if scene=='enable-scan':env['BLE_RENDER_ENABLE']='1'
 if scene.startswith('name-'):env['BLE_RENDER_NAME']=scene[5:]
 if scene.startswith('policy-'):
  env['BLE_POLICY']=scene[7:];env['BLE_RENDER_NAME']='save' if scene[7:] in ['detached','continuity','pause-again','pause-busy'] else 'cancel'
 rr=subprocess.run([str(exe),str(folder),str(path)],capture_output=True,text=True,env=env,timeout=60);(folder/'run.log').write_text(rr.stdout+rr.stderr)
 print(scene,rr.returncode,flush=True);results.append(dict(case=scene,passed=rr.returncode==0,frames={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in folder.glob('frame-*.ppm')}))
(out/'qualification.json').write_text(json.dumps(dict(results=results,hardware_tested=False,sanitizer=a.sanitizer,host_revision=subprocess.check_output(['git','-C',str(system),'rev-parse','HEAD'],text=True).strip(),target_sha256=r['elf_sha256']),indent=2)+'\n')
if any(not x['passed'] for x in results):raise SystemExit('Failures, see logs')
