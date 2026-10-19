#!/usr/bin/env python3
"""Real Stopwatch/adapter raster and input at capability dimensions."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--snapshot',action='store_true');p.add_argument('--utc',action='store_true');p.add_argument('--quick-check',action='store_true');a=p.parse_args();s=a.system_apps.resolve();runtime=a.runtime.resolve()
out=ROOT/'build/stopwatch-native'/('utc' if a.utc else 'rtc')/('snapshot' if a.snapshot else 'direct');out.mkdir(parents=True,exist_ok=True)
from stopwatch_native_build import stage,flags as build_flags,source_paths,profile,hashes
headers=stage(s,runtime,out,a.utc);profile(a.utc)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
flags=build_flags(a.utc,a.snapshot)+['-DSTOPWATCH_APP_SOURCE='+json.dumps(str(ROOT/'Apps/stopwatch.c'))]
sources=[ROOT/'test/native_apps/stopwatch_native_test.c',*source_paths(s,a.utc)[1:],catalog]
rows=[]
for san in ((False,) if a.quick_check else (False,True)):
 sanitize=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 binary=out/f'test-{int(san)}';subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*sanitize,*['-I'+str(i) for i in (headers,s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'lib/Alarm/include',ROOT/'test/native_apps')],*map(str,sources),'-o',str(binary)],check=True,timeout=120)
 for width,height in (((160,160),) if a.quick_check else ((160,160),(240,240),(240,280),(320,240),(410,502))):
  for delay in (0,120):
   for scene in range(27):
    name=f'{int(san)}-{width}x{height}-{delay}-{scene}';frames=out/name;capture=not a.utc and not san and not delay and scene in (0,6,7,23)
    if capture:frames.mkdir(exist_ok=True)
    result=subprocess.run([str(binary),str(frames) if capture else '-',str(scene),str(width),str(height),str(delay)],capture_output=True,text=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
    (out/(name+'.log')).write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(f'{name}: {result.returncode}: {result.stdout}{result.stderr}')
    rows.append(dict(size=[width,height],latency=delay,scene=scene,sanitized=san,passed=True))
receipt=dict(source_sha256=hashes(ROOT),system_sha256=hashes(s),rows=rows,scenarios=len(rows),hardware_tested=False,utc=a.utc,snapshot=a.snapshot,quick_check=a.quick_check,flags=flags,system_commit=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip(),runtime_commit=subprocess.check_output(['git','-C',str(runtime),'rev-parse','HEAD'],text=True).strip())
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(f'PASS: {len(rows)} Stopwatch app/adapter scenarios; utc={a.utc}, snapshot={a.snapshot}')
