#!/usr/bin/env python3
"""Focused ordinary-provider DND and shared volume regression."""
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=r/'build/dnd';out.mkdir(parents=True,exist_ok=True)
incs=[r/'lib/Alarm/include',a.runtime/'sdk/driver',a.system_apps/'lib/PortableApps/include',a.system_apps/'lib/NativeApps/include']
for sanitized in (False,True):
 flags=['-fsanitize='+os.environ.get('ALARM_TEST_SANITIZERS','address,undefined'),'-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitized else []
 exe=out/('service-'+str(int(sanitized)))
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_RTC_UTC8_DENVER',*flags,*['-I'+str(i) for i in incs],str(r/'test/native_apps/dnd_service_test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=20)
