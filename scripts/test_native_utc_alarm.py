#!/usr/bin/env python3
"""Production-source native UTC profile behavior, normal and ASan/UBSan."""
import argparse,os,subprocess
from pathlib import Path
from build_native_utc_alarm import ROOT,PIN,verify,inputs
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args();system=a.system_apps.resolve();runtime=a.runtime.resolve();verify(system,runtime)
out=ROOT/'build/native-utc-alarm';out.mkdir(parents=True,exist_ok=True)
for sanitized in (False,True):
 target=out/('test-san' if sanitized else 'test');flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitized else []
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(p) for p in inputs(system,runtime)],str(ROOT/'test/native_apps/native_utc_alarm_service_test.c'),*[str(system/p) for p in PIN['system_sources'] if p.endswith('.c')],'-o',str(target)],check=True)
 subprocess.run([str(target)],check=True)
print('Native UTC visual alarm normal + ASan/UBSan passed')
