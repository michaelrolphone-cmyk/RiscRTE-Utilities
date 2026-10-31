#!/usr/bin/env python3
"""Countdown semantic provider regression tests; no physical acceptance claim."""
import argparse,json,os,subprocess
from pathlib import Path
from scene_support import ROOT,sdk,time_sources
CASES=('arm-cancel','duration-bounds','elapsed-dst','minimum','stale','corrupt','unknown-commit','unwritten-retry','io-confirmed','expired','backward','bad-command','dismiss','blocked','get-context','put-context','service-retained','copied-retained','clock-context','other-occurrence','outputs','read-backward','native-sample-contract','native-unknown-status')
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);include=sdk(a.runtime.resolve(),a.system_apps.resolve(),out/'sdk');rows=[]
 flags=['-pipe','-std=c11','-O1','-g0','-Wall','-Wextra','-Werror','-I'+str(include)]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']
 def run(cmd):
  q=subprocess.run(list(map(str,cmd)),text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});rows.append({'command':list(map(str,cmd)),'returncode':q.returncode,'stdout':q.stdout,'stderr':q.stderr});(out/'results.json').write_text(json.dumps(rows,indent=2));q.check_returncode()
 for name,defines in [('raw',[]),('utc',['-DALARM_NATIVE_UTC'])]:
  exe=out/name;run(['cc',*flags,*defines,ROOT/'Services/countdown_control/control.c',ROOT/'test/scene/countdown_control_test.c',*time_sources(a.system_apps.resolve()),'-o',exe])
  for case in CASES:run([exe,case])
 obj=out/'control.o';run(['cc',*flags,'-DALARM_NATIVE_UTC','-DALARM_SERVICE_CLOCK_EDIT_V2','-Dt5_driver_get=control_driver_get','-c',ROOT/'Services/countdown_control/control.c','-o',obj])
 exe=out/'mixed-service';run(['cc',*flags,ROOT/'test/scene/countdown_current_service_test.c',obj,*time_sources(a.system_apps.resolve()),'-o',exe]);run([exe])
 print('Countdown provider:49 cases PASS ('+('sanitized' if a.sanitize else 'normal')+')')
if __name__=='__main__':main()
