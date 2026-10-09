#!/usr/bin/env python3
"""Run production cooperative publisher with deterministic capability providers."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--drivers',required=True,type=Path);a=p.parse_args()
out=ROOT/'build/telemetry-broadcast';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(ROOT/'lib/Broadcast/include'),'-I'+str(a.system_apps/'lib/PortableApps/include'),'-I'+str(a.drivers/'sdk/driver')]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/f'service-{int(san)}'
 subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'test/native_apps/telemetry_broadcast_test.c'),str(ROOT/'Services/telemetry_broadcast/service.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
