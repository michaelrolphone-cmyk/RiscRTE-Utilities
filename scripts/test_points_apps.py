#!/usr/bin/env python3
"""Focused recurring Points shared-core and production ELF source fixtures."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
out=ROOT/'build/points';out.mkdir(parents=True,exist_ok=True)
inc=[ROOT/'lib/Alarm/include',a.runtime/'sdk/driver',a.system_apps/'lib/PortableApps/include']
for fixture in ['tests/points_records_test.c','test/native_apps/points_service_test.c']:
 for denver in (False,True):
  for sanitized in (False,True):
   target=out/(Path(fixture).stem+f'-{int(denver)}-{int(sanitized)}')
   flags=(['-DPORTABLE_RTC_UTC8_DENVER'] if denver else [])+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitized else [])
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in inc],str(ROOT/fixture),'-o',str(target)],check=True,timeout=120)
   subprocess.run([str(target)],check=True,timeout=120)
print('Points normal/ASan/UBSan fixtures passed for raw and UTC+08/Denver policies')
