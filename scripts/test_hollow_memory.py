#!/usr/bin/env python3
"""Check bounded real-entry launch, allocation cleanup and raster scratch parity."""
import argparse, os, shutil, subprocess, tempfile
from pathlib import Path
from build_canvas_apps import ROOT, stage
p=argparse.ArgumentParser()
for n in ('runtime','system-apps','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--baseline',type=Path,help='Optional clean 1.2.1 Utilities source for the failing-entry reproduction')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
 include=stage(a.runtime.resolve(),a.system_apps.resolve(),Path(tmp))
 flags=['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(include),'-DCANVAS_KIND=2']
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','CANVAS_ALLOC_LIMIT':str(4*1024*1024)}
 canvas=a.output/'canvas-budget.o'
 subprocess.run([*flags,'-include',str(ROOT/'test/canvas/allocation_budget.h'),'-Dmalloc=canvas_test_malloc','-Dcalloc=canvas_test_calloc','-Dfree=canvas_test_free','-c',str(ROOT/'lib/CanvasApp/canvas.c'),'-o',str(canvas)],check=True)
 exe=a.output/'hollow-entry-budget'
 subprocess.run([*flags,'-DCANVAS_TEST_BUDGET',str(ROOT/'test/canvas/app_test.c'),str(ROOT/'test/canvas/allocation_budget.c'),str(canvas),'-o',str(exe)],check=True)
 for mode in ('normal','retain'):
  subprocess.run([str(exe),'x4',mode,str(a.output/f'hollow-budget-{mode}.ppm')],check=True,env=env,timeout=180)
 subprocess.run([str(exe),'x4','normal'],check=True,env={**env,'CANVAS_ALLOC_LIMIT':'3780000'},timeout=180)
 subprocess.run([str(exe),'x4','normal'],check=True,env={**env,'CANVAS_ALLOC_LIMIT':'3713140','CANVAS_EXPECT_MEMORY_FAILURE':'journal'},timeout=180)
 if a.baseline:
  baseline=a.baseline.resolve();test=Path(tmp)/'baseline/test/canvas';test.mkdir(parents=True)
  for name in ('app_test.c','allocation_budget.h'):shutil.copyfile(ROOT/'test/canvas'/name,test/name)
  (test.parents[1]/'Apps').symlink_to(baseline/'Apps',target_is_directory=True)
  (test.parents[1]/'lib').symlink_to(baseline/'lib',target_is_directory=True)
  old_canvas=a.output/'canvas-baseline.o'
  subprocess.run([*flags,'-include',str(ROOT/'test/canvas/allocation_budget.h'),'-Dmalloc=canvas_test_malloc','-Dcalloc=canvas_test_calloc','-Dfree=canvas_test_free','-c',str(baseline/'lib/CanvasApp/canvas.c'),'-o',str(old_canvas)],check=True)
  old_exe=a.output/'hollow-baseline-entry'
  subprocess.run([*flags,'-DCANVAS_TEST_BUDGET',str(test/'app_test.c'),str(ROOT/'test/canvas/allocation_budget.c'),str(old_canvas),'-o',str(old_exe)],check=True)
  subprocess.run([str(old_exe),'x4','normal'],check=True,env={**env,'CANVAS_EXPECT_MEMORY_FAILURE':'native B'},timeout=180)
  baseline_image=a.output/'hollow-baseline-normal.ppm'
  subprocess.run([str(old_exe),'x4','normal',str(baseline_image)],check=True,env={**env,'CANVAS_ALLOC_LIMIT':str(8*1024*1024)},timeout=180)
  assert baseline_image.read_bytes()==(a.output/'hollow-budget-normal.ppm').read_bytes(), 'Delivered entrypoint pixels changed'
  print('Original versus fixed entrypoint snapshot: byte-identical PASS',flush=True)
 exe=a.output/'hollow-memory'
 subprocess.run([*flags,str(ROOT/'test/canvas/hollow_memory_test.c'),str(ROOT/'lib/CanvasApp/canvas.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=env,timeout=180)
