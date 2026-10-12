#!/usr/bin/env python3
"""RF startup/reentry over exact target flags and the production shared adapter.

Runtime/peripherals are explicit host fixtures. The optional Contexts service is
unavailable here; test_rf_startup_contexts.py covers the actual shared-data path.
"""
import argparse, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-receipt',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();r=json.loads(a.build_receipt.read_text());r=r.get('waterfall',r);cmd=r.get('command',r.get('compile_command'))
old=next(x for x in cmd if x.endswith('/Apps/waterfall.c'));oldroot=str(Path(old).parent.parent)
flags=[x.replace(oldroot,str(ROOT)) for x in cmd if x.startswith(('-D','-I'))]
assert '-DPORTABLE_CONTEXTS_CLIENT' in flags and '-DPORTABLE_RESIDENT_POLICY' in flags
sources=[x for x in cmd if x.endswith('.c') and x!=old]
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
stubs=out/'serial.c';stubs.write_text('void rf_serial_start(void){} void rf_serial_line(const char*s){(void)s;} void rf_serial_finish(int a,int b){(void)a;(void)b;}\n')
scenes={
 'normal':('wait 8\ncheck running eq 1\ncheck captures ge 1\ncheck diag_startup eq 13\ncheck diag_total le 21\nnav 1\n',{}),
 'reentry':('wait 8\ncheck running eq 1\ncheck captures ge 1\nnav 1\n',{'RF_RENDER_REENTRY':'1','RF_RENDER_EXPECT_LAUNCH':'2'}),
 'unread-settings':('wait 8\ncheck load_errors eq 1\ncheck writes eq 0\nnav 1\n',{'RF_RENDER_KV_FAILURE':'1'}),
 'missing-storage':('wait 8\ncheck event_files_ready eq 0\ncheck app_writes eq 0\nnav 1\n',{'RF_RENDER_NO_STORAGE':'1'}),
 'airplane':('wait 8\ncheck running eq 0\ncheck captures eq 0\ncheck radio_acquires eq 0\nnav 1\n',{'RF_RENDER_POLICY':'airplane'}),
 'no-logger':('wait 8\ncheck running eq 1\ncheck captures ge 1\nnav 1\n',{'RF_RENDER_NO_DIAGNOSTIC':'1'})}
record={'hardware_verified':False,'runtime_and_peripherals':'simulated','runs':[]}
for sanitized in (True,False):
 mode='sanitized' if sanitized else 'normal';sf=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 exe=out/('rf-'+mode);command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*sf,*flags,'-DRF_RENDER_PAPER','-DRF_RENDER_RESIDENT',str(ROOT/'test/native_apps/rf_renderer_test.c'),str(stubs),*sources,'-lm','-o',str(exe)]
 subprocess.run(command,check=True)
 for name,(script,extra) in scenes.items():
  d=out/(mode+'-'+name);d.mkdir(exist_ok=True);actions=d/'actions.txt';actions.write_text(script+'finish\n')
  result=subprocess.run([str(exe),str(d),str(actions)],capture_output=True,text=True,timeout=60,env={**os.environ,'RF_RENDER_NO_IMAGES':'1','RF_RENDER_EXPECT_LAUNCH':'1','ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1',**extra})
  (d/'run.log').write_text(result.stdout+result.stderr)
  if result.returncode:print(result.stdout+result.stderr)
  result.check_returncode();print(mode,name,'PASS',flush=True);record['runs'].append({'mode':mode,'scene':name,'command':command,'result':'pass'})
(out/'evidence.json').write_text(json.dumps(record,indent=2)+'\n')
