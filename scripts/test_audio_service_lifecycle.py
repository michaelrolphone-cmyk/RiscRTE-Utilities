#!/usr/bin/env python3
"""Real ordinary alarm service plus exact portable audio lifecycle adapter."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
out=ROOT/'build/audio-service';out.mkdir(parents=True,exist_ok=True)
extra=out/'include';extra.mkdir(exist_ok=True)
for name in ('RiscPlatformClockV1.h','RiscBoundKeyValueV1.h'):
    shutil.copyfile(a.runtime/'sdk/driver'/name,extra/name)
includes=[extra,a.system_apps/'lib/PortableApps/include',a.system_apps/'lib/NativeApps/include',
    a.system_apps/'test/native_apps',a.runtime/'sdk/driver',ROOT/'lib/Alarm/include']
for points in (False,True):
    for san in (False,True):
        exe=out/f'lifecycle-{int(points)}-{int(san)}'
        flags=(['-DPOINTS_IN_TIME_SERVICE'] if points else [])+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else [])
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-g','-O1','-Wall','-Wextra','-Werror',*flags,
            *['-I'+str(i) for i in includes],str(ROOT/'test/native_apps/audio_service_lifecycle_test.c'),
            str(a.system_apps/'Apps/settings.c'),str(ROOT/'Services/alarm_service/service.c'),'-o',str(exe)],check=True)
        for scenario in range(10):subprocess.run([str(exe),str(scenario)],check=True,timeout=10,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Exact production service: 10 audio lifecycle cases x Alarm/Points x plain/sanitized passed')
