#!/usr/bin/env python3
"""Run one utility through the real shared adapter, not a replacement renderer."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--app',required=True,choices=['calculator']);p.add_argument('--snapshot',action='store_true');a=p.parse_args();s=a.system_apps.resolve();name=a.app
number={'calculator':1,'stopwatch':2,'battery':3,'alarms':4,'countdown':5}[name]
out=ROOT/'build/calculator-native'/('snapshot' if a.snapshot else 'direct');out.mkdir(parents=True,exist_ok=True);catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
owner='CALCULATOR_RETURN_APP' if name=='calculator' else 'ALARM_RETURN_APP' if name in ('alarms','countdown') else 'PORTABLE_RETURN_APP'
fixture=ROOT/'test/native_apps/calculator_native_test.c'
for san in (False,True):
 flags=['-DPORTABLE_CALCULATOR_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_NOVA_UI','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_APP_SLEEP_LOCAL','-D'+owner+'="springboard.elf"','-DNOVA_APP_ID='+str(number),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c')))]
 if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_RETAINED_RGB565_HANDOFF']
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 binary=out/f'test-{int(san)}';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(d) for d in (s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'lib/Alarm/include',s/'test/native_apps')],str(fixture),str(s/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(binary)],check=True,timeout=120)
 for width,height in ((128,128),(160,128),(240,240),(410,502),(502,410)):
  for delay in (0,120):
   for case in range(12):
    frames=out/f'native-snapshot{int(a.snapshot)}-{int(san)}-{width}x{height}-{delay}-{case}';frames.mkdir(exist_ok=True)
    subprocess.run([str(binary),str(frames),str(case),str(width),str(height),str(delay)],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})

paths=['Apps/calculator_nova.inc','Apps/calculator_native.inc','Apps/calculator_core.h','Apps/utility_frame.h','test/native_apps/calculator_native_test.c','test/native_apps/calculator_native_peripherals.h','scripts/test_calculator_native.py']
receipt={'cases':240,'sizes':[[128,128],[160,128],[240,240],[410,502],[502,410]],'latency_ms':[0,120],'modes':['normal','ASan/UBSan'],'snapshot':a.snapshot,'hardware_tested':False,'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},'system_commit':subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip(),'system_dirty':bool(subprocess.check_output(['git','-C',str(s),'status','--porcelain'],text=True))}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('PASS: 240 actual Calculator app/adapter scenarios; five sizes, two latencies, normal/sanitized')
