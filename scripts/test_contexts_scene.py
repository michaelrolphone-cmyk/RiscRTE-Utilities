#!/usr/bin/env python3
"""Run the shipping Contexts app_main against the production scene renderer."""
import argparse,os,subprocess,tempfile
from pathlib import Path
from build_contexts_scene import stage
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,system,frames,sanitize=False,atomic=False):
 if atomic:
  from test_utility_atomic import PROFILE,exact
  exact(system,PROFILE['system_sha']);exact(runtime,PROFILE['runtime_sdk_sha'])
 frames.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='contexts-scene-test-') as temporary:
  tmp=Path(temporary);include=stage(runtime,system,tmp)
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(include),'-I'+str(system/'test/scene')]
  if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  for profile in (('paper',) if atomic else ('watch','paper')):
   selected=['-DCONTEXTS_ALARM_API=2','-DCONTEXTS_SETTING_MASK=0x274u','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO'] if profile=='paper' else []
   if atomic:selected+=['-DPORTABLE_UTILITIES_ATOMIC_PREFERENCES','-DPORTABLE_UTILITIES_TARGET_X4']
   exe=tmp/('app-'+profile);subprocess.run([os.environ.get('CC','cc'),*flags,*selected,'-DSCENE_HOST_SOURCE="'+str(system/'Services/scene_host/host.c')+'"',str(ROOT/'test/contexts/host_probe.c'),str(ROOT/'test/contexts/app_test.c'),'-o',str(exe)],check=True)
   for mode in ('normal','storage','retained','no-input','event-no-input','delayed','save-failure','exit-home','exit-busy','exit-keyboard','exit-learning','exit-saving'):subprocess.run([str(exe),profile,str(frames),mode],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  # Production raster: partial ink must not become a solid monochrome circle.
  raw=(frames/'paper-learning.ppm').read_bytes();header,body=raw.split(b'255\n',1)
  assert header==b'P6\n480 800\n'
  def pixel(x,y):return body[(y*480+x)*3:(y*480+x+1)*3]
  assert pixel(331,236)==bytes((0,0,0)) and pixel(149,236)==bytes((255,255,255))
  assert pixel(240,145)==bytes((0,0,0))
  print('Actual app e-ink partial ring: filled arc / unfilled guide pixel checks PASS')

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('runtime','system-apps','frames'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');p.add_argument('--atomic-preferences',action='store_true')
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.frames.resolve(),a.sanitize,a.atomic_preferences)

