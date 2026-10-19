#!/usr/bin/env python3
"""Actual Touchpad application/adapter with native pixels and copied peripherals."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--sanitize',action='store_true');p.add_argument('--size',action='append');p.add_argument('--scene',action='append');p.add_argument('--slow',action='store_true');a=p.parse_args();system=a.system_apps.resolve()
out=ROOT/'build/ble-touchpad-native'/('sanitized' if a.sanitize else 'normal')/('slow' if a.slow else 'fast');out.mkdir(parents=True,exist_ok=True)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
defines=('PORTABLE_NOVA_UI','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RADIO_SESSION','PORTABLE_ALARM_CLIENT','PORTABLE_FORCE_FULL_FRAMES','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_INPUT_NAVIGATION','PORTABLE_INPUT_NAVIGATION_LOCAL','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_RETURN_APP="springboard.elf"','PORTABLE_PAPER_HID','PORTABLE_HOME_APP="default.elf"','PORTABLE_LOW_BATTERY','PORTABLE_BLE_TOUCHPAD_NATIVE_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_ASYNC_RGB565')
flags=['-D'+s for s in defines]+['-DBLE_TOUCHPAD_ADAPTER_SOURCE='+json.dumps(str(system/'lib/PortableApps/src/adapter.c'))]+['-I'+str(x) for x in (system/'Apps',ROOT/'Apps',ROOT/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include')]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
sources=[ROOT/'test/native_apps/ble_touchpad_native_test.c',ROOT/'test/native_apps/ble_touchpad_native_adapter.c',catalog,*[system/'lib/PortableApps/src'/s for s in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]]
exe=out/'fixture';subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*map(str,sources),'-Wl,--wrap=portable_nova_native_text','-Wl,--wrap=portable_nova_native_icon','-o',str(exe)],check=True)
results=[]
for w,h in ((128,128),(160,128),(240,240),(410,502),(320,180),(502,410)):
 if a.size and f'{w}x{h}' not in a.size:continue
 l=json.loads(subprocess.check_output([str(exe),'--layout','unused',str(w),str(h)],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}))
 def at(n,key):
  x,y,rw,rh=l[key];return (n,x+rw//2,y+rh//2)
 def seq(*keys):return [at(10+20*i,key) for i,key in enumerate(keys)]
 def checks(*keys,**values):return {**{k:'1' for k in keys},**{k:str(v) for k,v in values.items()}}
 px,py,pw,ph=l['pad'];x=px+pw//2;y=py+ph//2;scale=2 if w>=480 else 1;dy=min(12*scale,(ph//2-2)//2)
 reconnect=[at(10,'footer0')]
 cases={
  'idle':([],{}),'back':(seq('back'),checks('BACK')),'reconnect':(reconnect,checks('ACTIVE')),
  'left-tap':(reconnect+[(40,x,y)],checks('ACTIVE','MOUSE')),
  'right-tap':(reconnect+[(40,x-15,y),(40,x+15,y)],checks('ACTIVE','MOUSE')),
  'tap-drag':(reconnect+[(40,x,y),(42,x,y),(43,x+14*scale,y),(44,x+28*scale,y)],checks('ACTIVE','TAP_DRAG')),
  'scroll-x':(reconnect+[(40,x-30*scale,y),(40,x,y),(41,x-14*scale,y+1),(41,x+16*scale,y+1),(42,x+2*scale,y),(42,x+32*scale,y)],checks('ACTIVE','SCROLL_X')),
  'scroll-y':(reconnect+[(40,x-30*scale,y),(40,x,y),(41,x-30*scale+1,y+dy),(41,x+1,y+dy),(42,x-30*scale,y+2*dy),(42,x,y+2*dy)],checks('ACTIVE','SCROLL_Y')),
  'scroll-free':(reconnect+[(40,x-30*scale,y),(40,x,y),(41,x-18*scale,y+7*scale),(41,x+12*scale,y+7*scale),(42,x-6*scale,y+14*scale),(42,x+24*scale,y+14*scale)],checks('ACTIVE','SCROLL_FREE')),
  'raw-gap':(reconnect+[(38,x,y),(39,x,y),(40,x,y),(41,x,y)],checks('ACTIVE','GAP')),
  'pair-move':([at(10,'footer1'),at(30,'setting_pair0'),at(50,'footer1'),(51,l['footer1'][0]+2,l['footer1'][1]+l['footer1'][3]//2),at(80,'footer1')],checks('ACTIVE','PAIR','PAIR_ACCEPT','PAIR_RECOVERED','PAIR_MOVED')),
  'pair-multitouch':([at(10,'footer1'),at(30,'setting_pair0'),at(50,'footer1'),at(50,'footer0'),at(80,'footer1')],checks('ACTIVE','PAIR','PAIR_ACCEPT','PAIR_RECOVERED','PAIR_MULTITOUCH')),
  'pair-gap':([at(10,'footer1'),at(20,'setting_pair0'),at(38,'footer1'),at(39,'footer1'),at(40,'footer1'),at(80,'footer1')],checks('ACTIVE','PAIR','PAIR_ACCEPT','PAIR_RECOVERED','GAP')),
  'settings':(seq('footer1'),checks(EXPECT_VIEW=1)),
  'pair-accept':(seq('footer1','setting_pair0','footer1'),checks('ACTIVE','PAIR','PAIR_ACCEPT')),
  'pair-reject':(seq('footer1','setting_pair0','footer0'),checks('ACTIVE','PAIR','PAIR_REJECT')),
  'forget-cancel':(seq('footer1','setting_forget','footer0'),checks(EXPECT_VIEW=0,EXPECT_FORGETS=0)),
  'forget':(seq('footer1','setting_forget','footer1'),checks(EXPECT_VIEW=0,EXPECT_FORGETS=1)),
  'cleanup':(seq('footer0','back'),checks('ACTIVE','BACK','CLEANUP')),
  'home':(reconnect,checks('ACTIVE','HOME')),
  'repeat-entry':(seq('footer1'),checks('REPEAT',EXPECT_VIEW=1)),
  'previsible':([at(2,'footer1')],checks('PREVISIBLE',PRESENT_DELAY=120,STOP_POLLS=170,EXPECT_VIEW=0)),
 }
 for name,(actions,extra) in cases.items():
  if a.scene and name not in a.scene:continue
  folder=out/f'{w}x{h}'/name;folder.mkdir(parents=True,exist_ok=True);path=folder/'actions.txt';path.write_text(''.join(f'{n} {xx} {yy}\n' for n,xx,yy in actions))
  env={k:v for k,v in os.environ.items() if not k.startswith('HID_RENDER_')};env.update(ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1');env.update({f'HID_RENDER_{k}':str(v) for k,v in {'PRESENT_DELAY':120 if a.slow else 3,**extra}.items()})
  r=subprocess.run([str(exe),str(folder),str(path),str(w),str(h)],env=env,capture_output=True,text=True,timeout=60);(folder/'run.log').write_text(r.stdout+r.stderr);results.append({'size':[w,h],'case':name,'exit':r.returncode})
  if r.returncode:print(r.stdout+r.stderr);r.check_returncode()
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(f'Native Touchpad: {len(results)} actual app/adapter cases passed')
