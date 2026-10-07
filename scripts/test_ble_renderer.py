#!/usr/bin/env python3
"""Real scanner + actual adapter + Nova rasterizer over fake peripherals."""
import argparse,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--drivers',required=True,type=Path);a=p.parse_args();system=a.system_apps.resolve()
scenes={'scroll-list':[(3,50,210),(30,120,180),(31,120,140),(32,120,100),(33,120,60)],'scroll-details':[(3,50,210),(30,100,80),(50,120,170),(51,120,110),(52,120,65)],'idle':[],'scan-stop':[(3,50,210),(40,50,210)],'nested-back':[(3,50,210),(30,100,80),(50,20,20),(70,20,20)],'quick-controls':[(3,50,210),(30,120,5),(31,120,60),(32,120,140),(90,120,222)],'name-cancel':[(3,50,210),(30,100,80),(50,175,210),(60,50,86),(80,20,20)],'name-save':[(3,50,210),(30,100,80),(50,175,210),(60,50,86),(80,190,190)]}
for san in (False,True):
 out=ROOT/'build/ble-renderer'/str(int(san));out.mkdir(parents=True,exist_ok=True)
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RADIO_SESSION','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_RETURN_APP="springboard.elf"']
 if san:flags+=['-fsanitize='+os.environ.get('BLE_SANITIZERS','address,undefined'),'-fno-sanitize-recover=all','-fno-omit-frame-pointer',*(['-no-pie'] if sys.platform!='darwin' else [])]
 exe=out/'ble-renderer'
 sources=[a.drivers/'Drivers/ble_sensors/driver.c',ROOT/'test/native_apps/ble_renderer_test.c',ROOT/'Apps/ble_scanner.c',system/'lib/PortableApps/src/adapter.c',catalog]+[system/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in [ROOT/'Apps',system/'Apps',ROOT/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include',a.drivers/'sdk/driver']],*map(str,sources),'-o',str(exe)],check=True)
 for name,actions in scenes.items():
  folder=out/name;folder.mkdir(exist_ok=True)
  for stale in folder.glob('frame-*.ppm'):stale.unlink()
  path=folder/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions));env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
  if name!='idle':env['BLE_RENDER_SCAN']='1'
  if name.startswith('name-'):env['BLE_EXPECT_NAME']='Kitchen sensora' if name=='name-save' else 'Kitchen sensor'
  if name=='nested-back':env['BLE_RENDER_BACK']='1'
  if name=='quick-controls':env['BLE_RENDER_QUICK']='1'
  subprocess.run([str(exe),str(folder),str(path)],check=True,env=env,timeout=60)
  frames=sorted(folder.glob('frame-*.ppm'));assert frames,name
  for frame in frames:
   raw=frame.read_bytes();header=b'P6\n240 240\n255\n';assert raw.startswith(header) and len(raw)==len(header)+240*240*3
print('BLE actual app/adapter: scrolling,idle,scan-stop,nested Back,quick controls ordinary and configured sanitizers passed')
