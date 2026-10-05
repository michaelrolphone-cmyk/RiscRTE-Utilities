#!/usr/bin/env python3
"""Real current Points service + production shared client + app-audio ownership."""
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/points-audio-cues';out.mkdir(parents=True,exist_ok=True)
incs=[r/'lib/Alarm/include',a.system_apps/'lib/PortableApps/include',a.system_apps/'lib/NativeApps/include',a.system_apps/'test/native_apps',a.runtime/'sdk/driver']
for nova in (False,True):
 for volume in (False,True):
  for san in (False,True):
   exe=out/f'cue-{int(nova)}-{int(volume)}-{int(san)}';flags=['-DPOINTS_IN_TIME_SERVICE']
   if nova:flags+=['-DPORTABLE_NOVA_UI']
   if volume:flags+=['-DALARM_VOLUME_CONTROL']
   if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(x) for x in incs],str(r/'test/native_apps/audio_service_lifecycle_test.c'),str(a.system_apps/'Apps/settings.c'),str(r/'Services/alarm_service/service.c'),'-o',str(exe)],check=True)
   for case in range(10,13):subprocess.run([str(exe),str(case)],check=True,timeout=20)
print('24 real Points cue/shared-client/audio-ownership scenarios passed')
