#!/usr/bin/env python3
"""Production RF AppData retained helper: modern fence and legacy stack hold."""
import argparse,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='contexts-rf-retained-') as tmp:
 for sanitize in (False,True):
  out=Path(tmp)/('sanitized' if sanitize else 'normal')
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
  includes=[root/'Apps',root/'lib/Contexts/include',root/'lib/Alarm/include',a.system_apps/'lib/PortableApps/include',a.system_apps/'lib/NativeApps/include']
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*flags,*['-I'+str(i) for i in includes],str(root/'test/native_apps/contexts_rf_retained_test.c'),'-o',str(out)],check=True)
  subprocess.run([str(out)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=30)
