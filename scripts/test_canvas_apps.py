#!/usr/bin/env python3
import argparse,subprocess,tempfile,os
from pathlib import Path
from build_canvas_apps import stage,ROOT
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
 include=stage(a.runtime.resolve(),a.system_apps.resolve(),Path(tmp));env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
 for kind in (1,2):
  exe=a.output/('app-'+str(kind));subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(include),'-DCANVAS_KIND='+str(kind),str(ROOT/'test/canvas/app_test.c'),str(ROOT/'lib/CanvasApp/canvas.c'),'-o',str(exe)],check=True)
  for target in (['watch','x4','x4-native'] if kind==1 else ['x4']):
   for mode in (('normal','retain','sd','sd-legacy') if kind==1 and target=='x4-native' else ('normal','retain')):
    subprocess.run([str(exe),target,mode,str(a.output/f'{kind}-{target}-{mode}.ppm')],check=True,env=env,timeout=180)
