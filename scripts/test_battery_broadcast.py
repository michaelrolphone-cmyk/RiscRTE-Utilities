#!/usr/bin/env python3
"""Qualify optional shared Battery broadcast controller and unchanged provider prefix."""
import argparse,json,os,subprocess
from pathlib import Path
from scene_support import ROOT,sdk
from lifecycle_dependencies import Closure
p=argparse.ArgumentParser(description=__doc__)
for n in ('runtime','system-apps','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--sanitizers',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
include=sdk(a.runtime.resolve(),a.system_apps.resolve(),out/'sdk')
closure=Closure([('Runtime',a.runtime),('System',a.system_apps),('Utilities',ROOT),('generated-sdk',out/'sdk')],out/'dependencies.json')
common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(include)]
if a.sanitizers:common+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
rows=[]
for name,defs,sources in [('controller',[],[ROOT/'test/scene/battery_broadcast_test.c'])]+[(f'provider-default-{n}',[f'-DTELEMETRY_BROADCAST_DEFAULT_ENABLED={n}'],[ROOT/'test/scene/battery_broadcast_provider_test.c',ROOT/'Services/telemetry_broadcast/service.c']) for n in (0,1)]:
 exe=out/name;cmd=[*common,*defs,*map(str,sources),'-o',str(exe)];closure.record(cmd);subprocess.run(cmd,check=True)
 result=subprocess.run([str(exe)],check=True,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'));(out/(name+'.log')).write_text(result.stdout+result.stderr);rows.append({'name':name,'passed':True,'stdout':result.stdout})
(out/'results.json').write_text(json.dumps({'sanitizers':a.sanitizers,'runs':rows,'scope':'Host controller plus real service with deterministic capability doubles; no hardware or target instruction claim'},indent=2)+'\n');print(json.dumps(rows,indent=2))
