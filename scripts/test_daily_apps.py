#!/usr/bin/env python3
"""Run production daily-app arithmetic, state, UI and provider fault fixtures."""
import argparse,os,subprocess
from pathlib import Path
from build_portable_apps import ROOT,inventory,verify_system
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);a=p.parse_args();system=a.system_apps.resolve()
inventory();verify_system(system)
out=ROOT/'build/daily-tests';out.mkdir(parents=True,exist_ok=True)
fixtures=['tests/calculator_core_failures.c','test/native_apps/calculator_test.c','tests/stopwatch_core_test.c','test/native_apps/stopwatch_test.c','tests/waterfall_core_test.c']
for fixture in fixtures:
 for sanitizer in (False,True):
  target=out/(Path(fixture).stem+('-san' if sanitizer else ''))
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitizer else []
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(system/'lib/PortableApps/include'),str(ROOT/fixture),'-o',str(target)],check=True,timeout=120)
  subprocess.run([str(target)],check=True,timeout=90)
print('Production Calculator/Stopwatch/Waterfall normal and ASan/UBSan fixtures passed')
