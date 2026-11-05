#!/usr/bin/env python3
"""Capability-pixel scanner through the actual adapter and BLE sensor driver."""
import argparse,json,os,subprocess
from pathlib import Path
from ble_test_support import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--drivers',required=True,type=Path);a=p.parse_args();system=a.system_apps.resolve()
out=ROOT/'build/ble-scanner-native';out.mkdir(parents=True,exist_ok=True)
layout=out/'layout.c';layout.write_text('#include "ble_scanner_native_layout.h"\n#include <stdio.h>\n#include <stdlib.h>\nint main(int argc,char**argv){scan_native_layout l;if(argc!=3||!scan_native_layout_make(&l,atoi(argv[1]),atoi(argv[2])))return 1;scan_native_rect r[]={l.back,l.footer[0],l.footer[1],l.list,scan_native_row(l,0)};printf("[");for(unsigned i=0;i<5;i++)printf("%s[%d,%d,%d,%d]",i?",":"",r[i].x,r[i].y,r[i].w,r[i].h);puts("]");}\n')
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'Apps'),str(layout),'-o',str(out/'layout')],check=True)
sdk=stage_sdk(system,out);catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
flags=['-DPORTABLE_TEXT_INPUT_CLIENT','-DPORTABLE_APP_LAUNCH_GUARD','-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RADIO_SESSION','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_RETURN_APP="springboard.elf"','-DPORTABLE_BLE_SCANNER_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS']
total=0
for san in (False,True):
 exe=out/f'fixture-{int(san)}';extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 geometry=out/f'geometry-{int(san)}'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',*extra,'-I'+str(ROOT/'Apps'),str(ROOT/'test/native_apps/ble_scanner_native_layout_test.c'),'-o',str(geometry)],check=True)
 subprocess.run([str(geometry)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 sources=[a.drivers/'Drivers/ble_sensors/driver.c',ROOT/'test/native_apps/ble_scanner_native_test.c',ROOT/'test/native_apps/ble_scanner_native_entry.c',system/'lib/PortableApps/src/adapter.c',catalog]+[system/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*extra,*['-I'+str(i) for i in [ROOT/'Apps',system/'Apps',sdk,system/'lib/NativeApps/include',a.drivers/'sdk/driver']],*map(str,sources),'-o',str(exe)],check=True)
 for w,h in [(128,128),(160,128),(240,240),(410,502),(320,180),(502,410)]:
  rects=json.loads(subprocess.check_output([str(out/'layout'),str(w),str(h)],text=True))
  def at(n,i):
   x,y,rw,rh=rects[i];return (n,x+rw//2,y+rh//2)
  cases={'idle':([],{}),'scan-stop':([at(20,1),at(100,1)],{'BLE_RENDER_SCAN':'1'}),'detail':([at(20,1),at(100,4)],{'BLE_RENDER_SCAN':'1','SCAN_EXPECT_0':'1'}),'detail-drag':([at(20,1),at(100,4),(160,w//2,rects[3][1]+40),(161,w//2,rects[3][1]+10)],{'BLE_RENDER_SCAN':'1','SCAN_EXPECT_0':'1','SCAN_EXPECT_SCROLL_STEP':'1'}),'detail-back':([at(20,1),at(100,4),at(160,0)],{'BLE_RENDER_SCAN':'1','SCAN_EXPECT_0':'0'}),'filter':([at(20,1),at(100,2)],{'BLE_RENDER_SCAN':'1','SCAN_EXPECT_4':'1'}),'previsible':([at(2,1)],{'SCAN_FORCE_TOUCH':'1'}),'scan-stale-row':([at(20,1),at(22,4)],{'BLE_RENDER_SCAN':'1','SCAN_FORCE_TOUCH':'1','SCAN_EXPECT_0':'0'}),'footer-gap':([(20,w//2,h-10)],{}),'policy-off':([at(20,1)],{'BLE_POLICY_OFF':'1'}),'airplane':([at(20,1)],{'BLE_AIRPLANE':'1'})}
  for name,(actions,checks) in list(cases.items()):
   if name not in ('previsible','scan-stale-row'):cases[name+'-slow']=(actions,{**checks,'SCAN_DELAY':'120'})
  for name,(actions,checks) in cases.items():
   folder=out/f'{int(san)}-{w}x{h}-{name}';folder.mkdir(exist_ok=True);path=folder/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions))
   env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','SCAN_WIDTH':str(w),'SCAN_HEIGHT':str(h),'SCAN_DELAY':'120' if name=='previsible' else '3',**checks}
   subprocess.run([str(exe),str(folder),str(path)],env=env,check=True,timeout=60,stdout=subprocess.DEVNULL)
   total+=1
print(f'Native scanner: {total} actual app/adapter/sensor cases passed; dimensions, hit actions, async pixels and cleanup checked.')
from test_ble_shared_renderer import run as shared_renderer
shared_renderer(system,a.drivers,native=True)
shared_renderer(system,a.drivers,paper=True,native=True)
