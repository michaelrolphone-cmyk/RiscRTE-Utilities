from pathlib import Path
import subprocess,shutil,json,os
import argparse
p=argparse.ArgumentParser()
for n in ['build','system','output']:p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--sanitizer',default='address,undefined',choices=['undefined','address,undefined']);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(exist_ok=True,parents=True)
inc=a.build.resolve()/'sdk/include'
flags=['-I'+str(inc),'-I'+str(a.system.resolve()/'lib/NativeApps/include'),'-I'+str(a.system.resolve()/'Apps'),'-I'+str(root/'lib/Bluetooth/include'),'-I'+str(root/'Apps')]
policy='accept cancel detached continuity presenting input-busy pending pending-exit busy-after-pending busy exit delayed-exit retained-close exit-after-poll runtime-exit exit-apply retained-pause retained-resume again-resume retained-capture retained-prepare retained-apply malformed-snapshot'.split()
atomic='name-cancel name-home name-home-retained name-repair name-stale name-busy name-copy name-tombstone name-delayed name-collect-error name-dirty-submit name-release-error name-bad-receipt name-exit name-cancel-pending name-cancel-race name-cancel-error name-same radio-success radio-airplane radio-rollback radio-missing-rollback radio-submit-fail radio-uncertain radio-revoked-before-hardware radio-stale-rollback radio-cancel-race radio-restore-fail'.split()
runs=[]
for sanitize in [False,True]:
 for name,cases,defs in [('scanner_policy_controller_test',policy,['-DPORTABLE_TEXT_INPUT_POLICY']),('utility_ble_atomic_test',atomic,[]),('utility_ble_atomic_test_policy',atomic,['-DPORTABLE_TEXT_INPUT_POLICY'])]:
  source='utility_ble_atomic_test' if name.endswith('_policy') else name
  exe=out/(name+('-san' if sanitize else ''))
  cmd=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-DPORTABLE_UTILITIES_ATOMIC_PREFERENCES','-DPORTABLE_UTILITIES_TARGET_X4',*defs,*flags,str(root/'tests'/f'{source}.c'),'-o',str(exe)]
  if sanitize:cmd+=['-fsanitize='+a.sanitizer,'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
  subprocess.run(cmd,check=True)
  for case in cases:
   r=subprocess.run([str(exe),case],capture_output=True,text=True,timeout=15,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
   if r.returncode:raise RuntimeError(name+'/'+case+': '+r.stdout+r.stderr)
   runs.append(dict(suite=name,case=case,sanitize=a.sanitizer if sanitize else "normal",passed=True));print(name,case,sanitize,'PASS',flush=True)
(out/'qualification.json').write_text(json.dumps(dict(runs=runs,hardware_tested=False,scope='Actual Scanner controller and shared client; deterministic provider/bridge boundary doubles'),indent=2)+'\n')
