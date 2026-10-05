#!/usr/bin/env python3
import argparse
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
out=ROOT/'build/frequency-generator';out.mkdir(parents=True,exist_ok=True)
for suffix,flags in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'])]:
    for name,count in [('tone_core',1),('frequency_generator',15)]:
        exe=out/(name+'-'+suffix)
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,
            '-I'+str(a.system_apps/'lib/PortableApps/include'),'-I'+str(a.system_apps/'lib/NativeApps/include'),
            '-I'+str(ROOT/'lib/Alarm/include'),str(ROOT/'test/native_apps'/(name+'_test.c')),'-lm','-o',str(exe)],check=True)
        for scenario in range(count):subprocess.run([str(exe),str(scenario)],check=True,timeout=15,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
