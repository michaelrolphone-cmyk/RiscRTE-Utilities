#!/usr/bin/env python3
"""Exact standalone Frequency target composition with capability-backed navigation."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
from frequency_native_profile import load
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--system-apps',type=Path,required=True)
p.add_argument('--snapshot',action='store_true')
p.add_argument('--quick-check',action='store_true',help='Single size, ordinary mode; development only')
a=p.parse_args();s=a.system_apps.resolve();pin,profile=load()
out=ROOT/'build/frequency-native-standalone'/('snapshot' if a.snapshot else 'direct');out.mkdir(parents=True,exist_ok=True)
# Adapt only the hardware fixture to provide the ordinary granted navigation
# capability. The production app/adapter and original local-interface test stay
# unchanged; standalone does not claim a deployment-local sleep implementation.
fixture_source=ROOT/'test/native_apps/frequency_native_test.c'
fixture=out/'frequency_native_standalone_test.c'
original=fixture_source.read_text()
needle=' return fake_acquire(n,v,id,g);'
assert original.count(needle)==1
fixture.write_text(original.replace(needle,' if(!strcmp(n,"input.navigation")&&v==1){g->api=&nav_api;grants++;return true;}\n'+needle))
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
sizes=((160,160),(240,240),(320,180),(410,502),(502,410));rows=[]
for sanitized in ((False,) if a.quick_check else (False,True)):
 flags=['-D'+f for f in pin['defines']]
 flags+=['-DFREQUENCY_APP_SOURCE='+json.dumps(str(ROOT/'Apps/frequency_generator.c'))]
 if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_RETAINED_RGB565_HANDOFF']
 if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 binary=out/f'test-{int(sanitized)}'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(d) for d in (s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'lib/Alarm/include',s/'test/native_apps',ROOT/'test/native_apps')],str(fixture),str(s/'lib/PortableApps/src/adapter.c'),*[str(s/'lib/PortableApps/src'/n) for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')],str(catalog),'-o',str(binary)],check=True,timeout=120)
 for width,height in (sizes[:1] if a.quick_check else sizes):
  for delay in (0,120):
   for scene in (n for n in range(30) if n!=24):
    name=f'{int(sanitized)}-{width}x{height}-{delay}-{scene}';frames=out/name
    capture=not sanitized and not delay and scene in (0,4,12)
    if capture:frames.mkdir(exist_ok=True)
    result=subprocess.run([str(binary),str(frames) if capture else '-',str(scene),str(width),str(height),str(delay)],capture_output=True,text=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
    (out/(name+'.log')).write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(f'{name} failed ({result.returncode}): {result.stdout}{result.stderr}')
    rows.append({'size':[width,height],'latency_ms':delay,'sanitized':sanitized,'scene':scene,'passed':True})
paths=['Apps/frequency_generator.c','Apps/frequency_native.inc','Apps/tone_core.h','Apps/utility_frame.h','test/native_apps/frequency_native_test.c','test/native_apps/frequency_native_peripherals.h','scripts/test_frequency_native_standalone.py','scripts/build_frequency_native.py','sdk/frequency-native-sources.json','profiles/frequency-native-qualification.json','scripts/frequency_native_profile.py']
receipt={'scenarios':len(rows),'snapshot':a.snapshot,'quick_check':a.quick_check,'composition':'standalone capability navigation and explicit return; no local sleep hook','flags':flags,'generated_fixture_sha256':hashlib.sha256(fixture.read_bytes()).hexdigest(),'hardware_tested':False,'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},'system_commit':subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip(),'rows':rows}
(out/('quick-receipt.json' if a.quick_check else 'receipt.json')).write_text(json.dumps(receipt,indent=2)+'\n')
print(f'PASS: {len(rows)} exact standalone Frequency app/adapter scenarios; snapshot={a.snapshot}')
