#!/usr/bin/env python3
"""Real HID apps + production adapter + Nova pixels over copied peripheral APIs."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);a=p.parse_args();system=a.system_apps.resolve()
scenes={
 'idle':([],[]),
 'reconnect':([(3,50,218)],['ACTIVE']),
 'left-tap':([(3,50,218),(30,100,120)],['ACTIVE','MOUSE']),
 'right-tap':([(3,50,218),(30,100,120),(30,180,120)],['ACTIVE','MOUSE']),
 'keys':([(3,50,218),(30,40,160),(31,40,160),(32,40,160)],['ACTIVE','KEYS']),
 'pair-accept':([(3,180,218),(10,50,76),(30,180,218)],['ACTIVE','PAIR']),
 'pair-reject':([(3,180,218),(10,50,76),(30,50,218)],['ACTIVE','PAIR']),
 'disconnect':([(3,50,218),(30,40,160),(31,40,160)],['ACTIVE','DISCONNECT']),
 'sleep':([(3,50,218)],['ACTIVE','SLEEP']),
 'alarm':([(3,50,218),(38,40,160),(39,40,160),(60,120,166)],['ACTIVE','ALARM']),
 'raw-gap':([(3,50,218),(38,40,160),(39,40,160),(40,40,160),(41,40,160)],['ACTIVE','GAP']),
 'back':([(3,50,218),(30,20,20)],['ACTIVE','BACK']),
 'nested-back':([(3,180,218),(10,30,207),(20,120,174),(30,20,20),(40,20,20),(50,20,20)],['BACK']),
 'quick-controls':([(3,50,218),(30,120,5),(31,120,60),(32,120,140),(90,120,222)],['ACTIVE','QUICK']),
 'edit-save':([(3,180,218),(10,30,207),(20,205,124),(30,120,172),(40,40,75),(50,20,20),(60,180,218)],[]),
}
for san in (False,True):
 for name in ('ble_touchpad','ble_buttons'):
  out=ROOT/'build/hid-renderer'/str(int(san))/name;out.mkdir(parents=True,exist_ok=True)
  catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
  flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RADIO_SESSION','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_RETURN_APP="springboard.elf"']
  if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  exe=out/'renderer';sources=[ROOT/'test/native_apps/hid_renderer_test.c',ROOT/'Apps'/f'{name}.c',system/'lib/PortableApps/src/adapter.c',catalog]+[system/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in [ROOT/'Apps',ROOT/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include']],*map(str,sources),'-o',str(exe)],check=True)
  for scene,(actions,checks) in scenes.items():
   if name=='ble_touchpad' and scene in ('keys','nested-back','edit-save'):continue
   if name=='ble_buttons' and scene in ('left-tap','right-tap'):continue
   folder=out/scene;folder.mkdir(exist_ok=True)
   for f in folder.glob('frame-*.ppm'):f.unlink()
   path=folder/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions));env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',**{'HID_RENDER_'+c:'1' for c in checks})
   subprocess.run([str(exe),str(folder),str(path)],check=True,env=env,timeout=30)
   frames=sorted(folder.glob('frame-*.ppm'));assert frames,scene
   for frame in frames:
    raw=frame.read_bytes();header=b'P6\n240 240\n255\n';assert raw.startswith(header) and len(raw)==len(header)+240*240*3
print('HID production adapter: pairing, raw touches, gaps, disconnect, edits, Back and quick controls passed normally and under sanitizers')
