#!/usr/bin/env python3
"""Run the shipping Contexts app_main against the production scene renderer."""
import argparse,os,subprocess,tempfile
from pathlib import Path
from build_contexts_scene import stage
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,system,frames):
 frames.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='contexts-scene-test-') as temporary:
  tmp=Path(temporary);include=stage(runtime,system,tmp)
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(include),'-I'+str(system/'test/scene')]
  for profile in ('watch','paper'):
   selected=['-DCONTEXTS_ALARM_API=2','-DCONTEXTS_SETTING_MASK=0x274u','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO'] if profile=='paper' else []
   exe=tmp/('app-'+profile);subprocess.run([os.environ.get('CC','cc'),*flags,*selected,'-DSCENE_HOST_SOURCE="'+str(system/'Services/scene_host/host.c')+'"',str(ROOT/'test/contexts/host_probe.c'),str(ROOT/'test/contexts/app_test.c'),'-o',str(exe)],check=True)
   for mode in ('normal','storage','retained','exit-home','exit-busy','exit-keyboard'):subprocess.run([str(exe),profile,str(frames),mode],check=True,timeout=60)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('runtime','system-apps','frames'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.frames.resolve())

