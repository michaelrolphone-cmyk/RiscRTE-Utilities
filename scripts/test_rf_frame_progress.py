#!/usr/bin/env python3
"""Real RF app/shared adapter: withheld frame token, continuing capture/input."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build-receipt',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=json.loads(a.build_receipt.read_text());cmd=r['compile_command'];a.output.mkdir(parents=True,exist_ok=True)
original=next(x for x in cmd if x.endswith('/Apps/waterfall.c'));oldroot=str(Path(original).parent.parent)
flags=[x.replace(oldroot,str(ROOT)) for x in cmd if x.startswith(('-D','-I'))]
sources=[x for x in cmd if x.endswith('.c') and x!=original]
stubs=a.output/'serial.c';stubs.write_text('void rf_serial_start(void){} void rf_serial_line(const char*s){(void)s;} void rf_serial_finish(int a,int b){(void)a;(void)b;}\n')
scene='''wait 4
set poll_ms 20
set frame_delay 60
nav 8
wait 2
check frame_pending eq 1
remember presents
nav 8
nav 8
tap 225 26
wait 2
nav 32
nav 32
check page eq 1
check scroll eq 96
same presents
wait 25
same presents
check history ge 8
check transforms ge 8
check canonical ge 8
check busy_checks ge 20
set frame_delay 0
wait 55
check frame_pending eq 0
check last_page eq 1
check last_scroll eq 96
check captures ge 15
nav 1
nav 1
'''
record=[]
for paper in (False,True):
 for san in (False,True):
  name=('paper' if paper else 'lcd60')+('-san' if san else '');out=a.output/name;out.mkdir(exist_ok=True)
  selected=flags if paper else [f for f in flags if f!='-DPORTABLE_DISPLAY_ROTATION=90']
  extra=['-DRF_RENDER_PAPER'] if paper else []
  sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  exe=out/'rf';command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sf,*selected,*extra,'-DRF_RENDER_RESIDENT','-DRF_RENDER_ORDERED_INPUT',str(ROOT/'test/native_apps/rf_renderer_test.c'),str(stubs),*sources,'-lm','-o',str(exe)]
  subprocess.run(command,check=True);actions=out/'actions.txt';actions.write_text(scene)
  result=subprocess.run([str(exe),str(out),str(actions)],capture_output=True,text=True,timeout=90,env={**os.environ,'RF_RENDER_ASYNC':'1','RF_RENDER_NO_IMAGES':'1','RF_RENDER_EXPECT_LAUNCH':'1','ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  (out/'run.log').write_text(result.stdout+result.stderr)
  if result.returncode:print(result.stdout+result.stderr)
  result.check_returncode();print(name,result.stdout.strip(),flush=True);record.append(dict(profile=name,command=command,result=result.stdout.strip()))
(a.output/'evidence.json').write_text(json.dumps(dict(hardware_verified=False,runs=record),indent=2)+'\n')
