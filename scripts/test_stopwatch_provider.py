#!/usr/bin/env python3
"""Run production Stopwatch documents against the selected real scene provider."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(system,include,output,sanitize=False):
 output.mkdir(parents=True,exist_ok=True)
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
 if sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 for p in [system/'sdk/app',system/'lib/PortableApps/include',system/'Services/scene_profile',system/'Services/text_input',include]:flags+=['-I'+str(p)]
 flags+=['-DSTOPWATCH_SCENE_HOST_FIXTURE="'+str(system/'test/scene/host_test.c')+'"']
 exe=output/'stopwatch-provider-test'
 subprocess.run([os.environ.get('CC','cc'),*flags,str(system/'Services/scene_host/host.c'),str(ROOT/'test/scene/provider_test.c'),str(ROOT/'test/scene/document_bridge.c'),'-o',str(exe)],check=True)
 env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
 for shape in range(3):
  env['STOPWATCH_PPM']=str(output/f'stopwatch-{shape}.ppm')
  subprocess.run([str(exe),str(shape)],env=env,check=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--include',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.system_apps.resolve(),a.include.resolve(),a.output.resolve(),a.sanitize)
