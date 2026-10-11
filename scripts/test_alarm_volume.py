#!/usr/bin/env python3
"""Production alarm gain and unchanged non-modal Points cues, no physical I/O."""
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/alarm-volume';out.mkdir(parents=True,exist_ok=True)
inc=[r/'lib/Alarm/include',a.runtime/'sdk/driver',a.system_apps/'lib/PortableApps/include',a.system_apps/'lib/NativeApps/include']
for san in (False,True):
 for fixture in ('alarm_volume_service_test','points_service_test'):
  exe=out/f'{fixture}-{int(san)}';flags=['-DALARM_VOLUME_CONTROL'] if fixture=='points_service_test' else []
  if san:flags+=['-fsanitize='+os.environ.get('ALARM_TEST_SANITIZERS','address,undefined'),'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(x) for x in inc],str(r/'test/native_apps'/f'{fixture}.c'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)

# Keep service and app as separate production translation units: this catches
# provider gain that survives one app's close and changes the next app's PCM.
for san in (False,True):
 exe=out/f'alarm_frequency_gain-{int(san)}'
 flags=['-DALARM_VOLUME_CONTROL','-DPOINTS_IN_TIME_SERVICE']
 if san:flags+=['-fsanitize='+os.environ.get('ALARM_TEST_SANITIZERS','address,undefined'),'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 sources=[r/'test/native_apps/alarm_frequency_gain_test.c',r/'Services/alarm_service/service.c',r/'Apps/frequency_generator.c']
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(x) for x in inc],*map(str,sources),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=30)
