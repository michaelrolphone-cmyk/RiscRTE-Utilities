#!/usr/bin/env python3
"""Real HID apps + production adapter + Nova pixels over copied peripheral APIs."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--watch',type=Path,help='Link the production Watch FT6336U driver over simulated I2C reports');p.add_argument('--runtime',type=Path,help='Link the production Runtime serial logger with its existing USB host shim');p.add_argument('--watch-touch-source',type=Path,help='Explicit Watch touch source, relative to --watch (defaults to the current provider if present)');p.add_argument('--low-battery',action='store_true',help='Use the delivered low-battery lifecycle policy');p.add_argument('--scene',action='append',help='Run only the named scene (repeatable)');a=p.parse_args();system=a.system_apps.resolve();watch=a.watch.resolve() if a.watch else None
if a.watch_touch_source and not watch:p.error('--watch-touch-source requires --watch')
touch_source=(watch/a.watch_touch_source if a.watch_touch_source else watch/'drivers/current/twatch_touch/driver.c') if watch else None
if watch and not a.watch_touch_source and not touch_source.is_file():touch_source=watch/'drivers/twatch_touch/driver.c'
scenes={
 'idle':([],[]),
 'reconnect':([(3,50,218)],['ACTIVE']),
 'left-tap':([(3,50,218),(30,100,120)],['ACTIVE','MOUSE']),
 'right-tap':([(3,50,218),(30,100,120),(30,180,120)],['ACTIVE','MOUSE']),
 'keys':([(3,50,218),(30,40,160),(31,40,160),(32,40,160)],['ACTIVE','KEYS']),
 'pair-accept':([(3,180,218),(10,50,76),(30,180,218)],['ACTIVE','PAIR','PAIR_ACCEPT']),
 'pair-reject':([(3,180,218),(10,50,76),(30,50,218)],['ACTIVE','PAIR','PAIR_REJECT']),
 'pair-move':([(3,180,218),(10,50,76),(30,180,218),(31,160,218),(60,180,218)],['ACTIVE','PAIR','PAIR_ACCEPT','PAIR_RECOVERED','PAIR_MOVED']),
 'pair-multitouch':([(3,180,218),(10,50,76),(30,180,218),(30,160,218),(60,180,218)],['ACTIVE','PAIR','PAIR_ACCEPT','PAIR_RECOVERED','PAIR_MULTITOUCH']),
 'pair-gap':([(3,180,218),(10,50,76),(38,180,218),(39,180,218),(40,180,218),(60,180,218)],['ACTIVE','PAIR','PAIR_ACCEPT','PAIR_RECOVERED','GAP']),
 'disconnect':([(3,50,218),(30,40,160),(31,40,160)],['ACTIVE','DISCONNECT']),
 'mouse-reconnect':([(3,50,218),(30,100,120),(31,110,120),(62,100,120),(80,100,120)],['ACTIVE','MOUSE','MOUSE_RECONNECT']),
 'transport-reconnect':([(3,50,218),(30,100,120),(31,110,120),(60,50,218),(80,100,120)],['ACTIVE','MOUSE','TRANSPORT_RECONNECT']),
 'low-battery':([(3,50,218)],['ACTIVE','LOW_BATTERY']),
 'sleep':([(3,50,218)],['ACTIVE','SLEEP']),
 'alarm':([(3,50,218),(38,40,160),(39,40,160),(60,120,166)],['ACTIVE','ALARM']),
 'raw-gap':([(3,50,218),(38,40,160),(39,40,160),(40,40,160),(41,40,160)],['ACTIVE','GAP']),
 'back':([(3,50,218),(30,20,20)],['ACTIVE','BACK']),
 'nested-back':([(3,180,218),(10,30,207),(20,120,174),(30,20,20),(40,20,20),(50,20,20)],['BACK']),
 'quick-controls':([(3,50,218),(30,120,5),(31,120,60),(32,120,140),(90,120,222)],['ACTIVE','QUICK']),
 'edit-save':([(3,180,218),(10,30,207),(20,205,124),(30,120,172),(40,40,75),(50,20,20),(60,180,218)],[]),
}
if not a.low_battery:del scenes['low-battery']
if a.scene:
 unknown=set(a.scene)-scenes.keys()
 if unknown:p.error('Unknown scenes: '+', '.join(sorted(unknown)))
 scenes={name:scene for name,scene in scenes.items() if name in a.scene}
for san in (False,True):
 for name in ('ble_touchpad','ble_buttons'):
  out=ROOT/'build'/('hid-watch-renderer' if watch else 'hid-renderer')/str(int(san))/name;out.mkdir(parents=True,exist_ok=True)
  catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
  flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RADIO_SESSION','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_RETURN_APP="springboard.elf"']
  if a.low_battery:flags+=['-DPORTABLE_LOW_BATTERY']
  if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  exe=out/'renderer';sources=[ROOT/'test/native_apps/hid_renderer_test.c',ROOT/'Apps'/f'{name}.c',system/'lib/PortableApps/src/adapter.c',catalog]+[system/'lib/PortableApps/src'/s for s in ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]
  extra=[]
  if watch:
   flags+=['-DHID_RENDER_WATCH_TOUCH']
   extra=['-I'+str(watch/'sdk/driver'),'-I'+str(watch/'include')]
   sources+=[ROOT/'test/native_apps/hid_watch_touch_backend.c',touch_source]
  if a.runtime:
   runtime=a.runtime.resolve();flags+=['-DHID_RENDER_RUNTIME_DIAGNOSTICS']
   for index,source in enumerate([ROOT/'test/native_apps/hid_runtime_diagnostics.cpp',runtime/'src/ports/esp32s3/SleepDiagnostics.cpp']):
    obj=out/f'diagnostics-{index}.o'
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-DRISC_SLEEP_DIAGNOSTICS=1',*flags,'-I'+str(runtime/'src'),'-I'+str(runtime/'test/diagnostic_shim'),'-c',str(source),'-o',str(obj)],check=True)
    sources.append(obj)
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*extra,*['-I'+str(i) for i in [ROOT/'Apps',ROOT/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include']],*map(str,sources),*(['-lstdc++'] if a.runtime else []),'-o',str(exe)],check=True)
  for scene,(actions,checks) in scenes.items():
   if name=='ble_touchpad' and scene in ('keys','nested-back','edit-save'):continue
   if name=='ble_buttons' and scene in ('left-tap','right-tap','mouse-reconnect','transport-reconnect'):continue
   folder=out/scene;folder.mkdir(exist_ok=True)
   for f in folder.glob('frame-*.ppm'):f.unlink()
   path=folder/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions));env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',**{'HID_RENDER_'+c:'1' for c in checks})
   subprocess.run([str(exe),str(folder),str(path)],check=True,env=env,timeout=30)
   frames=sorted(folder.glob('frame-*.ppm'));assert frames,scene
   for frame in frames:
    raw=frame.read_bytes();header=b'P6\n240 240\n255\n';assert raw.startswith(header) and len(raw)==len(header)+240*240*3
print('HID production adapter: pairing, raw touches, gaps, disconnect, edits, Back and quick controls passed normally and under sanitizers')
