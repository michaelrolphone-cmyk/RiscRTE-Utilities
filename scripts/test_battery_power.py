#!/usr/bin/env python3
"""Production power UI and generic adapter: status transitions, input and cleanup."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);a=p.parse_args();s=a.system_apps.resolve()
out=ROOT/'build/battery-power';out.mkdir(parents=True,exist_ok=True)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
for san in (False,True):
 flags=['-DPORTABLE_ALARM_CLIENT','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_NOVA_UI','-DPORTABLE_POWER_STATUS','-DPORTABLE_FORCE_FULL_FRAMES','-DBATTERY_RETURN_APP="springboard.elf"','-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps/battery.c'))]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 binary=out/f'test-{int(san)}'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(d) for d in (s/'lib/PortableApps/include',s/'lib/NativeApps/include',s/'test/native_apps')],str(ROOT/'test/native_apps/battery_power_test.c'),str(s/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(binary)],check=True)
 for case in range(20):
  frames=out/f'frames-{int(san)}-{case}';frames.mkdir(exist_ok=True)
  subprocess.run([str(binary),str(frames),str(case)],check=True,timeout=30)
