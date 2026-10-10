#!/usr/bin/env python3
"""Production paper app/adapter controller and raster with sanitizers."""
import argparse,os,subprocess,sys
from pathlib import Path
from PIL import Image
from build_contexts_paper import prepare
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--x4-idle-sdk',type=Path,required=True);a=p.parse_args();system=a.system_apps.resolve();out=ROOT/'build/contexts-paper';out.mkdir(parents=True,exist_ok=True);inc=prepare(system,a.runtime.resolve(),a.x4_idle_sdk,out)
flags=['PORTABLE_CONTEXTS_PAPER','PORTABLE_CONTEXTS_CLIENT','PORTABLE_CONTEXTS_EDITOR','PORTABLE_NOVA_UI','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_NATIVE_CUSTODY_FENCE','ALARM_SERVICE_TAGGED_V2','TEST_NATIVE_TOOLBAR_QUICK','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_APP_LAUNCH_GUARD','PORTABLE_PAPER_PREFERENCES','PORTABLE_PAPER_TRANSITIONS','PORTABLE_APP_TOUCH_SCROLL','PORTABLE_TOUCH_SCROLL','PORTABLE_X4_IDLE_POLICY','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_INPUT_NAVIGATION','PORTABLE_HOME_APP="default.elf"']
for san in (False,True):
 exe=out/('sanitized' if san else 'normal');extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 includes=[inc,ROOT/'Apps',ROOT/'lib/Contexts/include',ROOT/'lib/Alarm/include',system/'Apps',system/'lib/NativeApps/include']
 sources=[ROOT/'test/native_apps/contexts_paper_test.c',*[system/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_render.c')]]
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*extra,*['-D'+f for f in flags],'-DCONTEXTS_TOOLBAR_FIXTURE="'+str(system/'test/native_apps/portable_native_toolbar_test.c')+'"',*['-I'+str(p) for p in includes],*map(str,sources),'-Wl,--wrap=free','-o',str(exe)],check=True)
 for case in ('normal','flip'):
  dest=out/(case+'-'+str(int(san)));dest.mkdir(exist_ok=True)
  subprocess.run([exe,dest,case],check=True,timeout=60,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
  for image in dest.glob('*.pbm'):Image.open(image).rotate(90 if case=='flip' else 270,expand=True).save(image.with_suffix('.png'))
# Reader flip preserves the logical rendering byte for byte.
for f in (out/'normal-0').glob('*.png'):
 assert Image.open(f).tobytes()==Image.open(out/'flip-0'/f.name).tobytes(),f.name
print('Contexts actual 480x800 rasters + reader flip pixel equality PASS')
