#!/usr/bin/env python3
"""Native RF pixels and physical input over the production app/adapter.

Explicit display/radio/storage peripherals; this is not hardware qualification.
The minimum supported square, tall, landscape and delivered portrait dimensions
run the same controller, native painter and bounded history implementation.
"""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--system-apps',type=Path,required=True)
p.add_argument('--output',type=Path,default=ROOT/'build/rf-native-canvas')
a=p.parse_args();s=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
cat=out/'catalog.c';cat.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
flags=['PORTABLE_RF_NATIVE_CANVAS','PORTABLE_NOVA_UI','PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RADIO_SESSION','PORTABLE_ALARM_CLIENT','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_FORCE_FULL_FRAMES','PORTABLE_INPUT_NAVIGATION','PORTABLE_INPUT_NAVIGATION_LOCAL','RF_RENDER_MONITOR_STATE','RF_RENDER_ORDERED_INPUT']
runs=[]
for width,height in [(160,160),(200,200),(240,240),(410,502),(320,240),(640,240),(240,320)]:
 for san in (False,True):
  name=f'{width}x{height}'+('-san' if san else '');d=out/name;d.mkdir(exist_ok=True);exe=d/'rf'
  sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sf,*['-D'+f for f in flags],f'-DFX_W={width}u',f'-DFX_H={height}u',*['-I'+str(x) for x in [ROOT/'Apps',s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'lib/Alarm/include']],str(ROOT/'test/native_apps/rf_renderer_test.c'),str(s/'lib/PortableApps/src/adapter.c'),str(cat),'-lm','-o',str(exe)]
  subprocess.run(cmd,check=True)
  logical_h=max(240,(height*240+width-1)//width);scroll=max(0,624-(logical_h-94))
  logical=lambda x,y:(x*width//240,y*height//240)
  control=lambda x,y:(x*width//240,y*height//logical_h)
  extra='\n'.join(['nav 32']*13+[f'check scroll eq {scroll}', 'tap %d %d'%control(180,48+12*48-scroll+20),'check page eq 11','tap %d %d'%logical(120,211),'check page eq 12','tap %d %d'%logical(120,218),'check page eq 11','tap %d %d'%logical(195,20),'check page eq 6','nav 1','check page eq 1','tap %d %d'%control(120,30),'check page eq 10','tap %d %d'%logical(150,224),'check page eq 1','tap %d %d'%control(120,logical_h-23),'check page eq 0'])
  scene=f'''wait 8
check native_canvas eq 1
check plot_width eq {width-12}
check plot_height eq {height-110}
check running eq 1
snapshot spectrum
nav 8
wait 4
check view eq 1
snapshot waterfall
nav 32
check frozen eq 1
remember history
remember captures
wait 3
same history
same captures
set frame_delay 180
nav 8
nav 8
nav 8
check frame_pending eq 1
remember presents
tap {width//2} 46
check cursor_hz eq 2400000000
tap {width//2} {height-65}
tap {width//2} {height-65}
check cursor_hz eq 2480000000
nav 8
nav 8
tap 6 {min(100,height-85)}
tap 6 {min(100,height-85)}
check cursor_hz eq 2400000000
tap {width-7} {min(100,height-85)}
tap {width-7} {min(100,height-85)}
check cursor_hz eq 2480000000
tap {width-90} 63
check page eq 2
nav 1
check page eq 0
set frame_delay 90
nav 8
wait 3
check frame_pending eq 1
remember presents
remember history
tap {width-15} 26
check page eq 1
nav 32
check scroll eq 48
same presents
same history
release_frame
wait 8
check last_page eq 1
check last_scroll eq 48
snapshot controls
{extra}
native_seed
wait 5
native_pixels
status_state 0
wait 5
remember plot_hash
remember status_hash
status_state 1
wait 5
same plot_hash
different status_hash
status_state 2
wait 5
same plot_hash
different status_hash
status_state 0
wait 5
nav 8
status_state 0
wait 5
remember plot_hash
remember status_hash
status_state 1
wait 5
same plot_hash
different status_hash
status_state 2
wait 5
same plot_hash
different status_hash
status_state 0
wait 5
nav 8
nav 8
wait 5
snapshot native-pixels
finish
'''
  scene='\n'.join(line+('\nwait 2' if line.startswith(('tap ','nav ')) else '') for line in scene.splitlines())+'\n'
  actions=d/'actions.txt';actions.write_text(scene)
  result=subprocess.run([str(exe),str(d),str(actions)],capture_output=True,text=True,timeout=180,env={**os.environ,'RF_RENDER_ASYNC':'1','ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  (d/'run.log').write_text(result.stdout+result.stderr)
  if result.returncode:print(result.stdout+result.stderr)
  result.check_returncode();print(name,result.stdout.strip(),flush=True);runs.append({'dimensions':[width,height],'sanitized':san,'command':cmd,'result':result.stdout.strip()})
(out/'evidence.json').write_text(json.dumps({'hardware_verified':False,'history_bursts':228,'runs':runs},indent=2)+'\n')
