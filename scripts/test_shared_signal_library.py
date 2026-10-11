#!/usr/bin/env python3
"""Actual signal app training/storage/subtraction; deterministic PCM, no hardware."""
import argparse,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='shared-signals-') as tmp:
 for sanitized in (False,True):
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  target=Path(tmp)/('test-'+str(sanitized))
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(a.system_apps/'lib/PortableApps/include'),'-I'+str(a.system_apps/'lib/NativeApps/include'),root/'test/native_apps/shared_signal_library_test.c','-lm','-o',target],check=True)
  subprocess.run([target],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=60)
