#!/usr/bin/env python3
"""Exact X4 .52 app/adapter compile profile over deterministic peripheral APIs.
Runtime dispatch returns the production host's five-second policy request.
The shared host policy/Runtime integration is tested separately by System.
"""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--baseline-build',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--candidate',action='store_true');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
for app in ['ble_touchpad','ble_buttons']:
 r=json.loads((a.baseline_build/app/'x4-native-app.json').read_text());cmd=r['compile_command'];flags=[x for x in cmd if x.startswith(('-D','-I'))]
 flags=[x.replace('/workspace/shared/x4-fast-utility-clients-047',str(ROOT)) for x in flags]
 if a.candidate:flags+=['-DPORTABLE_RADIO_CONTINUOUS_CAPTURE']
 sources=[x for x in cmd if x.endswith('.c') and Path(x).name not in [app+'.c']]
 sources=[x.replace('/workspace/shared/system-fast-ui-047',str(a.system_apps)) for x in sources]
 exe=a.output/app
 san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
 command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,*flags,'-DHID_RENDER_PAPER','-DHID_RENDER_RESIDENT',ROOT/'test/native_apps/hid_renderer_test.c',ROOT/'Apps'/(app+'.c'),*sources,'-o',exe]
 subprocess.run(list(map(str,command)),check=True)
 if a.candidate:
  gate=a.output/(app+'-gate')
  includes=[x for x in flags if x.startswith('-I')]
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,*includes,'-DHID_TOUCHPAD='+str(int(app=='ble_touchpad')),str(ROOT/'tests/hid_resident_gate_test.c'),'-o',str(gate)],check=True)
  subprocess.run([str(gate)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 scenes=[
  ('accept','100 348 732\n180 60 50\n',['PAIR','PAIR_ACCEPT','REQUIRE_CONFIRM']),
  ('reject','100 110 732\n180 60 50\n',['PAIR','PAIR_REJECT','REQUIRE_CONFIRM']),
  ('reject-cleanup','100 110 732\n180 60 50\n',['PAIR','PAIR_REJECT','REQUIRE_CONFIRM','CLEANUP']),
  ('timeout-retry','100 110 732\n180 60 50\n',['PAIR_TIMEOUT']),
  ('error-retry','100 110 732\n180 60 50\n',['PAIR_ERROR']),
  ('cancel','100 60 50\n',['PENDING_CANCEL']),
 ]
 for name,events,checks in scenes:
  out=a.output/(app+'-'+name);out.mkdir(exist_ok=True)
  actions=out/'actions.txt';actions.write_text('3 348 732\n10 100 220\n'+events)
  env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','HID_RENDER_ACTIVE':'1',**{'HID_RENDER_'+check:'1' for check in checks}}
  result=subprocess.run([str(exe),str(out),str(actions)],env=env,capture_output=True,text=True,timeout=30)
  (out/'run.log').write_text(result.stdout+result.stderr)
  print(app,name,'exit',result.returncode,flush=True)
  if result.returncode:raise SystemExit(result.returncode)
print('HID resident policy: delayed accept/reject, timeout/error retry, explicit cancel and checked cleanup passed')
