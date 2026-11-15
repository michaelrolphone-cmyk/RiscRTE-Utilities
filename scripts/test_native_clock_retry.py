#!/usr/bin/env python3
"""Original native UTC lifecycle suite plus read-phase recovery guard controls."""
import argparse,json,os,subprocess
from pathlib import Path
from scene_support import ROOT,sdk,time_sources
from lifecycle_dependencies import Closure
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
runtime=a.runtime.resolve();system=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);include=sdk(runtime,system,out/'sdk')
closure=Closure([('candidate',ROOT),('runtime',runtime),('system',system),('generated-sdk',include.parent)],out/'DEPENDENCIES.json')
cases=[]
for editor in (False,True):
 flags=['-DPOINTS_SERVICE_CONTEXT']+(['-DALARM_NATIVE_MIXED_POINTS','-DALARM_SERVICE_CLOCK_EDIT_V2'] if editor else [])
 prefix=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(include)]
 if a.sanitize:prefix+=['-fsanitize=undefined','-fno-sanitize-recover=all']
 cmd=[*prefix,str(ROOT/'test/native_apps/native_utc_clock_retry_test.c'),*time_sources(system),'-o',str(out/('editor' if editor else 'ordinary'))]
 closure.record(cmd);subprocess.run(cmd,check=True)
 result=subprocess.run([cmd[-1]],text=True,capture_output=True);cases.append({'editor':editor,'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr});assert result.returncode==0,cases[-1]
(out/'RESULT.json').write_text(json.dumps({'sanitizer':'undefined' if a.sanitize else 'none','cases':cases,'scope':'Actual service plus original accepted lifecycle suite; host-modeled dependencies, no device execution.'},indent=2)+'\n')
print('Both original native UTC profiles and 10/12 recovery guard controls passed')
