#!/usr/bin/env python3
"""Actual Frequency app/adapter at native dimensions, with async frame custody."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--system-apps',type=Path,required=True)
p.add_argument('--snapshot',action='store_true')
p.add_argument('--quick-check',action='store_true',help='Single size, ordinary mode; development only')
a=p.parse_args();s=a.system_apps.resolve()
out=ROOT/'build/frequency-native'/('snapshot' if a.snapshot else 'direct');out.mkdir(parents=True,exist_ok=True)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
sizes=((160,160),(240,240),(320,180),(410,502),(502,410));rows=[]
for sanitized in ((False,) if a.quick_check else (False,True)):
 flags=['-D'+f for f in ('PORTABLE_FREQUENCY_NATIVE_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_NOVA_UI','PORTABLE_FORCE_FULL_FRAMES','PORTABLE_AUDIO_SESSION','PORTABLE_ALARM_CLIENT','PORTABLE_INPUT_NAVIGATION','PORTABLE_INPUT_NAVIGATION_LOCAL','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_QUICK_NATIVE')]
 flags+=['-DPORTABLE_RETURN_APP="springboard.elf"','-DFREQUENCY_APP_SOURCE='+json.dumps(str(ROOT/'Apps/frequency_generator.c'))]
 if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_RETAINED_RGB565_HANDOFF']
 if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 binary=out/f'test-{int(sanitized)}'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(d) for d in (s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'lib/Alarm/include',s/'test/native_apps')],str(ROOT/'test/native_apps/frequency_native_test.c'),str(s/'lib/PortableApps/src/adapter.c'),*[str(s/'lib/PortableApps/src'/n) for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')],str(catalog),'-o',str(binary)],check=True,timeout=120)
 for width,height in (sizes[:1] if a.quick_check else sizes):
  for delay in (0,120):
   for scene in range(30):
    name=f'{int(sanitized)}-{width}x{height}-{delay}-{scene}';frames=out/name
    capture=not sanitized and not delay and scene in (0,4,12)
    if capture:frames.mkdir(exist_ok=True)
    result=subprocess.run([str(binary),str(frames) if capture else '-',str(scene),str(width),str(height),str(delay)],capture_output=True,text=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
    (out/(name+'.log')).write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(f'{name} failed ({result.returncode}): {result.stdout}{result.stderr}')
    rows.append({'size':[width,height],'latency_ms':delay,'sanitized':sanitized,'scene':scene,'passed':True})
paths=['Apps/frequency_generator.c','Apps/frequency_native.inc','Apps/tone_core.h','Apps/utility_frame.h','test/native_apps/frequency_native_test.c','test/native_apps/frequency_native_peripherals.h','scripts/test_frequency_native.py']
receipt={'scenarios':len(rows),'snapshot':a.snapshot,'quick_check':a.quick_check,'hardware_tested':False,'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},'system_commit':subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip(),'rows':rows}
(out/('quick-receipt.json' if a.quick_check else 'receipt.json')).write_text(json.dumps(receipt,indent=2)+'\n')
print(f'PASS: {len(rows)} actual Frequency app/adapter scenarios; snapshot={a.snapshot}')
