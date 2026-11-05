#!/usr/bin/env python3
"""Prove deferred paper and unselected color pixels retain the predecessor.

Both source versions use the same pinned production adapter and ordered input.
The SDK include paths remain canonical; no header copies are made.
"""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--baseline-utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);system=a.system_apps.resolve()
s=(ROOT/'test/native_apps/rf_renderer_test.c').read_text().replace('#include "../../Apps/waterfall.c"','#include RF_RENDER_APP_SOURCE')
s=s.replace('V("native_canvas",rf_native_canvas);','').replace('V("status_hash",fx_region_hash(PLOT_Y+PLOT_H,PLOT_Y+PLOT_H+RF_NATIVE_STATUS_HEIGHT));','')
s+='\n_Static_assert(sizeof(spec_levels)==228*sizeof(*spec_levels),"legacy spectrum capacity");\n_Static_assert(sizeof(fall_levels)==438*sizeof(*fall_levels),"legacy waterfall capacity");\n'
fixture=out/'fixture.c';fixture.write_text(s);catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
scene='''wait 8
snapshot spectrum
nav 8
wait 4
snapshot waterfall
nav 32
wait 2
tap 225 26
wait 2
check page eq 1
snapshot controls
nav 32
wait 2
nav 32
wait 2
nav 32
wait 2
snapshot controls-scrolled
nav 1
wait 2
nav 8
wait 2
snapshot monitor
finish
''';actions=out/'actions.txt';actions.write_text(scene);runs=[]
flags=['PORTABLE_NOVA_UI','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RADIO_SESSION','PORTABLE_ALARM_CLIENT','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_FORCE_FULL_FRAMES','PORTABLE_INPUT_NAVIGATION','PORTABLE_INPUT_NAVIGATION_LOCAL','RF_RENDER_ORDERED_INPUT']
for paper in (False,True):
 for san in (False,True):
  captures=[]
  for name,root in [('candidate',ROOT),('predecessor',a.baseline_utilities.resolve())]:
   mode=('paper' if paper else 'color')+('-san' if san else '')+'/'+name;d=out/mode;d.mkdir(parents=True,exist_ok=True);exe=d/'rf'
   sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
   pf=['-DRF_RENDER_PAPER','-DPORTABLE_DISPLAY_ROTATION=90'] if paper else []
   cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sf,*pf,*['-D'+f for f in flags],'-DRF_RENDER_APP_SOURCE='+json.dumps(str(root/'Apps/waterfall.c')),*['-I'+str(x) for x in [root/'Apps',system/'lib/PortableApps/include',system/'lib/NativeApps/include',ROOT/'lib/Alarm/include']],str(fixture),str(system/'lib/PortableApps/src/adapter.c'),str(catalog),'-lm','-o',str(exe)]
   subprocess.run(cmd,check=True);r=subprocess.run([str(exe),str(d),str(actions)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});(d/'run.log').write_text(r.stdout+r.stderr);print(mode,r.stdout+r.stderr,flush=True);r.check_returncode();runs.append({'profile':mode,'command':cmd,'result':r.stdout.strip()});captures.append(d)
  for image in ['spectrum','waterfall','controls','controls-scrolled','monitor']:
   assert (captures[0]/(image+'.ppm')).read_bytes()==(captures[1]/(image+'.ppm')).read_bytes(),(paper,san,image)
(out/'evidence.json').write_text(json.dumps({'runs':runs,'predecessor_image_equalities':20,'legacy_capacities':[228,438],'hardware_verified':False},indent=2)+'\n')
