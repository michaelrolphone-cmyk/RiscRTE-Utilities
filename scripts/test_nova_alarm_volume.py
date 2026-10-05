#!/usr/bin/env python3
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);s=p.parse_args().system_apps.resolve()
out=ROOT/'build/nova-volume';out.mkdir(parents=True,exist_ok=True);catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
for san in (False,True):
 flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_APP_SLEEP_LOCAL']
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 binary=out/f'test-{int(san)}';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(p) for p in (s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'lib/Alarm/include')],str(ROOT/'test/native_apps/nova_alarm_volume_test.c'),str(s/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(binary)],check=True,timeout=120)
 for case in range(9):
  frames=out/f'frames-{int(san)}-{case}';frames.mkdir(exist_ok=True);subprocess.run([str(binary),str(frames),str(case)],check=True,timeout=60)
print('18 production volume controller/persistence/real-render scenarios passed')
