#!/usr/bin/env python3
"""Retain legacy DSP checks; full RF controller coverage is test_rf_application.py."""
import argparse,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='waterfall-test-') as d:
 for source in ['waterfall_core_test.c']:
  for sanitizer in [False,True]:
   target=Path(d)/(source+str(sanitizer))
   flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitizer else []
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-DPORTABLE_RETURN_APP="springboard.elf"','-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.system_apps/'lib/PortableApps/include'),str(ROOT/'tests'/source),'-o',str(target)],check=True)
   subprocess.run([str(target)],check=True,timeout=30)
