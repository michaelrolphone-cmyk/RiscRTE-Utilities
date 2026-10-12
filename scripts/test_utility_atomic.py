#!/usr/bin/env python3
"""Opt-in actual Utilities controller matrix; no product or hardware selection."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from build_contexts_scene import stage
ROOT=Path(__file__).resolve().parents[1]
PROFILE=json.loads((ROOT/'experimental/x4-atomic-preferences.json').read_text())
def exact(repo,pin):
 actual=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
 allowed={pin}
 if pin==PROFILE['system_sha']:
  allowed.update(PROFILE['system_local_equivalents'])
  tree=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD^{tree}'],text=True).strip()
  if tree!=PROFILE['system_tree']:raise ValueError('System published tree mismatch: '+str(repo))
 if actual not in allowed or subprocess.check_output(['git','-C',str(repo),'status','--porcelain','--untracked-files=no'],text=True).strip():raise ValueError('Clean exact source required: '+str(repo)+' @ '+pin)
 return actual

BLE='name-cancel name-repair name-stale name-busy name-copy name-tombstone name-delayed name-collect-error name-dirty-submit name-release-error name-bad-receipt name-exit name-cancel-pending name-cancel-race name-cancel-error name-same radio-success radio-airplane radio-rollback radio-missing-rollback radio-submit-fail radio-uncertain radio-revoked-before-hardware radio-stale-rollback radio-cancel-race radio-restore-fail'.split()
BATTERY='toggle same pause-failed busy delayed exit cancel-pending cancel-race collect-error release-error'.split()
CONTEXTS='enable sources timing busy delayed exit retained automatic-noop automatic-order automatic-low automatic-mask automatic-partial automatic-retained'.split()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('system-apps','runtime','output'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 exact(a.system_apps,PROFILE['system_sha']);exact(a.runtime,PROFILE['runtime_sdk_sha'])
 fonts=a.system_apps/'Services/scene_host/fonts/telemetry.inc'
 if not fonts.exists():subprocess.run(['python',str(a.system_apps/'scripts/generate_telemetry_fonts.py')],check=True)
 inc=stage(a.runtime.resolve(),a.system_apps.resolve(),a.output/'sdk')
 includes=[inc,a.system_apps/'Apps',a.system_apps/'lib/NativeApps/include',ROOT/'Apps',ROOT/'lib/Bluetooth/include',a.system_apps/'test/scene']
 flags=['-D'+define for define in PROFILE['defines']]
 cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in includes]]
 if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 results=[]
 for name,source,cases,extra in [('battery',ROOT/'tests/utility_battery_atomic_test.c',BATTERY,['-ffunction-sections','-fdata-sections','-Wl,--gc-sections']),('ble',ROOT/'tests/utility_ble_atomic_test.c',BLE,[]),('contexts',ROOT/'test/contexts/atomic_test.c',CONTEXTS,['-DCONTEXTS_SETTING_MASK=0x274u','-DCONTEXTS_ALARM_API=2','-DSCENE_HOST_SOURCE="'+str(a.system_apps/'Services/scene_host/host.c')+'"',str(ROOT/'test/contexts/host_probe.c')])]:
  exe=a.output/name;subprocess.run([*cmd,*extra,str(source),'-o',str(exe)],check=True)
  for case in cases:
   r=subprocess.run([str(exe),case],capture_output=True,text=True,timeout=20,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
   if r.returncode:raise RuntimeError(case+'\n'+r.stdout+r.stderr)
   print(r.stdout.strip());results.append({'app':name,'case':case,'result':r.stdout.strip()})
 (a.output/'receipt.json').write_text(json.dumps({'hardware_tested':False,'repository_sha':subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','-C',ROOT,'status','--porcelain','--untracked-files=no'],text=True).strip()),'sanitized':a.sanitize,'results':results,'system_public_sha':PROFILE['system_sha'],'system_tree':PROFILE['system_tree'],'system_actual_sha':subprocess.check_output(['git','-C',str(a.system_apps),'rev-parse','HEAD'],text=True).strip(),'source_sha256':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (ROOT/'Apps').glob('*') if f.is_file() and ('atomic' in f.name or f.name.startswith(('ble_scanner','battery_power','contexts')))}},indent=2)+'\n')
if __name__=='__main__':main()
