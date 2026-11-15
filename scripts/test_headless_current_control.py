#!/usr/bin/env python3
"""Current editor-aware control and historical policies; host tests, not hardware."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from scene_support import ROOT,sdk,time_sources
CONTROL_CASES=('arm-cancel', 'tomorrow', 'gap', 'fold', 'stale', 'corrupt', 'unknown-commit', 'unwritten-retry', 'io-confirmed', 'expired', 'backward', 'bad-command', 'volume', 'dismiss', 'blocked', 'get-context', 'put-context', 'service-retained', 'copied-retained', 'clock-context', 'bad-zone')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);runtime=a.runtime.resolve();system=a.system_apps.resolve();include=sdk(runtime,system,out/'sdk');rows=[]
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(include)]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
 def execute(cmd):
  result=subprocess.run(cmd,text=True,capture_output=True,env=env);rows.append({'command':list(map(str,cmd)),'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr});(out/'results.json').write_text(json.dumps(rows,indent=2));result.check_returncode()
 for name,defines,fixture in [('raw',['-DPORTABLE_RTC_UTC8_DENVER'],'control_test.c'),('utc',['-DALARM_NATIVE_UTC'],'control_test.c'),('editor',['-DALARM_NATIVE_UTC','-DALARM_SERVICE_CLOCK_EDIT_V2'],'control_editor_test.c')]:
  target=out/name;execute(['cc',*flags,*defines,str(ROOT/'Services/alarm_control/control.c'),str(ROOT/'test/scene'/fixture),*time_sources(system),'-o',str(target)])
  cases=CONTROL_CASES+('editor-intent','editor-busy',*['editor-malformed-'+n for n in ('tag','size','phase','feature')]) if name=='editor' else CONTROL_CASES
  for case in cases:execute([str(target),case])
 obj=out/'control.o';execute(['cc',*flags,'-DALARM_NATIVE_UTC','-DALARM_SERVICE_CLOCK_EDIT_V2','-Dt5_driver_get=control_driver_get','-c',str(ROOT/'Services/alarm_control/control.c'),'-o',str(obj)])
 target=out/'mixed-service';execute(['cc',*flags,str(ROOT/'test/scene/current_service_control_test.c'),str(obj),*time_sources(system),'-o',str(target)]);execute([str(target)])
 print('Historical policies, editor refusals and actual current mixed scheduler controls passed; no hardware claim.')
if __name__=='__main__':main()
