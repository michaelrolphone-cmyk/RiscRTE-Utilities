#!/usr/bin/env python3
"""Exact Watch Scanner profile, actual app/adapter/scene/text hosts, synthetic devices.
Runs normal and ASan/UBSan workflows; frame bytes are hash-recorded and gzip-kept.
"""
import argparse,gzip,hashlib,json,os,shlex,subprocess,shutil
from pathlib import Path
from scanner_policy_cases import scenarios,tap
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['system-apps','runtime','watch','drivers','output']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--provisional-system',action='store_true',help='Diagnostic only: allow tracked System edits at the pinned base');p.add_argument('--normal-only',action='store_true');p.add_argument('--policy-only',action='store_true');p.add_argument('--legacy-only',action='store_true');p.add_argument('--no-leak-checks',action='store_true');p.add_argument('--keep-binaries',action='store_true');a=p.parse_args()
 roots={'system-apps':a.system_apps.resolve(),'utilities':ROOT,'runtime':a.runtime.resolve(),'watch':a.watch.resolve(),'drivers':a.drivers.resolve(),'generated':ROOT/'tests/scanner_policy_profile'};out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 pins={'system-apps':'9957e7c975f7b57ce9ea50d57b13c1d9bb75cf43','drivers':'aac1cb342696e96345d4546e2c69023834c6c8d4','runtime':'267aab6d9382ce4921a1536fab1620749b2f5301','watch':'089868395268b696c05651c33816345bfe0425df'}
 for owner,pin in pins.items():
  root=roots[owner]
  assert subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()==pin,owner
  assert (a.provisional_system and owner=='system-apps') or not subprocess.check_output(['git','-C',str(root),'status','--porcelain','--untracked-files=no'],text=True).strip(),owner
 profile=json.loads((roots['generated']/'profile.json').read_text());mapped=lambda text:text.format(**{k:str(v) for k,v in roots.items()});s=roots['system-apps'];r=roots['runtime']
 defs=list(map(mapped,profile['defines']))+['-DSCANNER_TEST_ALARM_HEADER="'+str(ROOT/'lib/Alarm/include/AlarmServiceV1.h')+'"'];incs=list(map(mapped,profile['includes']));fixture=ROOT/'tests/scanner_policy_watch_host_test.c'
 fixture_sdk=out/'fixture-sdk';fixture_sdk.mkdir(exist_ok=True);shutil.copyfile(ROOT/'lib/Bluetooth/include/RiscBluetoothSensorsV1.h',fixture_sdk/'RiscBluetoothSensorsV1.h')
 service=['-I'+str(s/'Services/text_input'),'-I'+str(s/'sdk/app'),'-I'+str(s/'lib/PortableApps/include'),'-I'+str(r/'sdk/driver'),'-I'+str(s/'Services/scene_profile')]
 sources=[(s/'Services/scene_host/host.c',service+['-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=1','-Dt5_driver_get=ble_scene_driver_get']),(s/'Services/text_input/host.c',service+['-Dt5_driver_get=ble_text_driver_get']),(s/'Services/scene_profile/profile.c',service+['-Dt5_driver_get=ble_profile_driver_get']),(roots['drivers']/'Drivers/ble_sensors/driver.c',['-I'+str(roots['drivers']/'sdk/driver'),'-I'+str(roots['drivers']/'Drivers/ble_sensors'),'-I'+str(r/'sdk/driver')]),(fixture,['-I'+str(fixture_sdk)]+defs+incs+['-DBLE_WATCH_RENDER',*service,'-I'+str(roots['drivers']/'sdk/driver'),'-I'+str(ROOT/'lib/Bluetooth/include')]),*[(Path(mapped(x)),defs+incs) for x in profile['sources']]]
 legacy=scenarios();runs=[] if a.policy_only else [(name,None,actions) for name,actions in legacy.items()]
 if not a.legacy_only:
  name=legacy['name-unavailable'];key=tap(250,47,97,'text');done=tap(250,194,188,'text')
  for mode in ['idle','low-battery','pause-again','pause-busy','pause-retained','resume-retained','resume-again','paused-home','missing','malformed','unsupported','paused-back','sleep-refused','sleep-error','sleep-retained']:
   actions=name if mode in ['missing','malformed','unsupported'] else name+key+[(100,'text','mark',0,0),(12000 if mode=='low-battery' else 65000,'text','mark',0,0)]+done
   if mode=='paused-back':actions=name+key+[(100,'text','mark',0,0)]
   runs.append(('name-cancel' if mode in ['missing','malformed','unsupported','pause-retained','resume-retained','resume-again','paused-home','paused-back','sleep-error','sleep-retained'] else 'name-save',mode,actions))
 records=[];commands=[];dependencies={};frame_sets={}
 for san in [False] if a.normal_only else [False,True]:
  build=out/('asan-ubsan' if san else 'normal');build.mkdir(exist_ok=True);objects=[]
  common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror']+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if san else [])
  for i,(src,flags) in enumerate(sources):
   obj=build/(str(i)+'.o');dep=build/(str(i)+'.d');cmd=[*common,*flags,'-MMD','-MF',str(dep),'-c',str(src),'-o',str(obj)];commands.append(cmd);subprocess.run(cmd,check=True);objects.append(obj)
   for x in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):dependencies[x]=sha(x)
  binary=build/'scanner';cmd=[*common,*map(str,objects),'-o',str(binary)];commands.append(cmd);subprocess.run(cmd,check=True);binary_hash=sha(binary)
  for case,mode,actions in runs:
   label='policy-'+mode if mode else case;folder=build/label;folder.mkdir(exist_ok=True);f=folder/'actions.txt';f.write_text(''.join(' '.join(map(str,row))+'\n' for row in actions))
   env={k:v for k,v in os.environ.items() if not k.startswith(('BLE_RENDER_','BLE_POLICY','BLE_NO_FRAMES'))};env.update(UBSAN_OPTIONS='halt_on_error=1')
   if case!='idle':env['BLE_RENDER_SCAN']='1'
   if case.startswith('name-'):env['BLE_RENDER_NAME']=case[5:]
   if a.no_leak_checks:env['ASAN_OPTIONS']='detect_leaks=0'
   if mode:env.update(BLE_POLICY=mode,BLE_NO_FRAMES='1')
   if case in ['nested-back','name-back-return']:env['BLE_RENDER_BACK']='1'
   if case=='name-navigation-home':env['BLE_RENDER_HOME']='1'
   if case in ['name-save-delayed','name-rapid-repeat']:env['BLE_RENDER_DELAY']='1'
   run=subprocess.run([str(binary),str(folder),str(f)],env=env,capture_output=True,text=True,timeout=90);(folder/'run.log').write_text(run.stdout+run.stderr)
   frames=[]
   for frame in sorted(folder.glob('frame-*.ppm')):
    data=frame.read_bytes();z=frame.with_suffix('.ppm.gz');z.write_bytes(gzip.compress(data,mtime=0));assert gzip.decompress(z.read_bytes())==data;frames.append({'name':frame.name,'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'gzip_sha256':sha(z)});frame.unlink()
   frames_hash=[x['sha256'] for x in frames]
   if san:assert frame_sets.get(label)==frames_hash,(label,'normal/sanitized frame mismatch')
   else:frame_sets[label]=frames_hash
   records.append(dict(case=label,sanitized=san,exit_code=run.returncode,binary_sha256=binary_hash,frames=frames,stdout=run.stdout,stderr=run.stderr))
   evidence=dict(schema='scanner.modal-policy.host.v1',runs=records,commands=commands,dependencies=dependencies,pins=pins,provisional_system=a.provisional_system,hardware_verified=False,target_instructions_executed=False,scope='Actual Watch Scanner app, PortableApps adapter, scene host, shared text host and scene profile and BLE sensors driver; synthetic native transports, local sleep and bounded Contexts/broadcast provider')
   (out/'result.json').write_text(json.dumps(evidence,indent=2)+'\n');print(('PASS' if run.returncode==0 else 'FAIL'),san,label,flush=True)
   if run.returncode:print(run.stdout+run.stderr,flush=True);raise SystemExit(1)
  if not a.keep_binaries:
   binary.unlink()
   for obj in objects:obj.unlink()
 print(len(records),'workflows PASS'+('; normal/sanitized frame parity verified' if not a.normal_only else '; normal-only diagnostic'))
if __name__=='__main__':main()
