#!/usr/bin/env python3
"""Real RF startup + production Contexts/custody + real RF-only service.

Only Runtime grant exclusivity and peripheral I/O are modeled. Contexts and RF
must alternate one native app/shared-data grant without losing custody.
"""
import argparse,json,pathlib,subprocess,os
U=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-receipt',type=pathlib.Path,required=True)
p.add_argument('--system-apps',type=pathlib.Path,required=True)
p.add_argument('--driver-sdk',type=pathlib.Path,required=True)
p.add_argument('--output',type=pathlib.Path,required=True)
a=p.parse_args();S=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
r=json.loads(a.build_receipt.read_text());r=r.get('waterfall',r);cmd=r.get('command',r.get('compile_command'))
old=next(x for x in cmd if x.endswith('/Apps/waterfall.c'));oldroot=str(pathlib.Path(old).parent.parent)
flags=[x.replace(oldroot,str(U)) for x in cmd if x.startswith(('-D','-I'))]+['-I'+str(S/'lib/PortableApps/src')]
record=[]
for san in [False,True]:
 tag='-san' if san else ''
 common=['cc','-std=c11','-O1','-g','-ffunction-sections','-fdata-sections']+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else [])
 objs=[]
 for src in ['rf_startup_contexts_test.c','rf_startup_contexts_adapter.c','rf_startup_contexts_service.c','service.c']:
  source=U/'Services/contexts/service.c' if src=='service.c' else U/'test/native_apps'/src
  selected=flags if src in ['rf_startup_contexts_test.c','rf_startup_contexts_adapter.c'] else ['-DCONTEXTS_RF_ONLY=1','-I'+str(U/'Apps'),'-I'+str(U/'lib/Contexts/include'),'-I'+str(a.driver_sdk.resolve()),'-I'+str(S/'lib/PortableApps/include')]
  obj=out/(src+tag+'.o');command=common+selected+['-c',str(source),'-o',str(obj)]
  record.append(command);subprocess.run(command,check=True);objs.append(str(obj))
 exe=out/('rf-startup'+tag);command=common+objs+['-Wl,--gc-sections','-lm','-o',str(exe)]
 record.append(command);subprocess.run(command,check=True)
 for mode in ['', 'checkpoint-first','retained-load']:
  run=subprocess.run([str(exe)]+([mode] if mode else []),capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'},timeout=15)
  (out/((mode or 'startup')+tag+'.log')).write_text(run.stdout+run.stderr)
  print(run.stdout+run.stderr);run.check_returncode()
 # Seeded files plus a real dirty Contexts rename/checkpoint exercise saved-data preservation.
 source=U/'test/native_apps/rf_startup_dirty_checkpoint_test.c';obj=out/('dirty-checkpoint'+tag+'.o')
 command=common+flags+['-c',str(source),'-o',str(obj)];record.append(command);subprocess.run(command,check=True)
 exe=out/('dirty-checkpoint'+tag);command=common+[str(obj),*objs[1:]]+['-Wl,--gc-sections','-lm','-o',str(exe)]
 record.append(command);subprocess.run(command,check=True)
 for mode in ['', 'checkpoint-first']:
  run=subprocess.run([str(exe)]+([mode] if mode else []),capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'},timeout=15)
  (out/('dirty-checkpoint-'+(mode or 'startup')+tag+'.log')).write_text(run.stdout+run.stderr)
  print(run.stdout+run.stderr);run.check_returncode()
(out/'evidence.json').write_text(json.dumps({'hardware_verified':False,'commands':record,'runs':10},indent=2)+'\n')
