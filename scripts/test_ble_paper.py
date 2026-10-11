#!/usr/bin/env python3
"""Native landscape MONO1 + portrait touch + actual scanner/adapter, no radio hardware."""
import argparse,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--drivers',required=True,type=Path);a=p.parse_args();system=a.system_apps.resolve()
scenes={
'scan-complete':([(3,100,730)],{'BLE_RENDER_SCAN':'1','BLE_COMPLETE':'1'}),
'name-save':([(3,100,730),(30,100,160),(45,100,730),(60,140,240),(80,360,600)],{'BLE_RENDER_SCAN':'1','BLE_EXPECT_NAME':'Kitchen sensorb'}),
'name-cancel':([(3,100,730),(30,100,160),(45,100,730),(60,140,240),(80,60,50)],{'BLE_RENDER_SCAN':'1','BLE_EXPECT_NAME':'Kitchen sensor'}),
'sensors':([(3,100,730),(35,360,730),(50,100,160)],{'BLE_RENDER_SCAN':'1'}),
'idle':([],{'BLE_IDLE':'1'}),
'footer-gap':([(3,240,730)],{'BLE_IDLE':'1'}),
'scan-stop':([(3,100,730),(40,100,730)],{'BLE_RENDER_SCAN':'1'}),
'details':([(3,100,730),(30,100,160),(45,360,730),(60,360,730),(80,60,50)],{'BLE_RENDER_SCAN':'1'}),
'swipe':([(3,100,730),(30,120,610),(31,120,400),(32,120,200),(50,120,170),(51,120,300),(52,120,590)],{'BLE_RENDER_SCAN':'1'}),
'back':([(3,100,730),(70,60,50)],{'BLE_RENDER_SCAN':'1','BLE_RENDER_BACK':'1'}),
'enable-only':([(3,100,730),(15,100,730)],{'BLE_POLICY_OFF':'1','BLE_ENABLE':'1'}),
'enable-scan':([(3,100,730),(15,100,730),(30,100,730),(70,100,730)],{'BLE_POLICY_OFF':'1','BLE_ENABLE':'1','BLE_RENDER_SCAN':'1'}),
'airplane':([(3,100,730),(15,100,730)],{'BLE_AIRPLANE':'1'}),
}
for san in (False,True):
 out=ROOT/'build/ble-paper'/str(int(san));out.mkdir(parents=True,exist_ok=True)
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 flags=['-DBLE_PAPER','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RADIO_SESSION','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_RETURN_APP="springboard.elf"']
 if san:flags+=['-fsanitize='+os.environ.get('BLE_SANITIZERS','address,undefined'),'-fno-sanitize-recover=all','-fno-omit-frame-pointer',*(['-no-pie'] if sys.platform!='darwin' else [])]
 exe=out/'ble-paper';sources=[a.drivers/'Drivers/ble_sensors/driver.c',ROOT/'test/native_apps/ble_renderer_test.c',ROOT/'Apps/ble_scanner.c',system/'lib/PortableApps/src/adapter.c',catalog]
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in [ROOT/'Apps',ROOT/'lib/Bluetooth/include',system/'Apps',system/'lib/PortableApps/include',system/'lib/NativeApps/include',a.drivers/'sdk/driver']],*map(str,sources),'-o',str(exe)],check=True)
 for name,(actions,extra) in scenes.items():
  folder=out/name;folder.mkdir(exist_ok=True)
  for stale in folder.glob('frame-*.ppm'):stale.unlink()
  path=folder/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions))
  subprocess.run([str(exe),str(folder),str(path)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',**extra),timeout=60)
  frames=sorted(folder.glob('frame-*.ppm'));assert frames,name
  for frame in frames:
   raw=frame.read_bytes();header=b'P6\n480 800\n255\n';assert raw.startswith(header) and len(raw)==len(header)+480*800*3
print('BLE Nova7: native MONO1 rotation, snapshot input, enable/Airplane, scan/stop, details, swipe, Back and static idle passed normal and configured sanitizers')
