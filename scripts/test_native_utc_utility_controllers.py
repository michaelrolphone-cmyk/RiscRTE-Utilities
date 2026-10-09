#!/usr/bin/env python3
"""Actual native Stopwatch, Calculator and Battery controllers plus production adapter."""
import argparse,json,os,tempfile
from pathlib import Path
from native_utility_build import *
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--normal-only',action='store_true');a=p.parse_args();system=a.system_apps.resolve();runtime=a.runtime.resolve();exact(system,SYSTEM);exact(runtime,RUNTIME)
out=ROOT/'build/native-utc-controllers';out.mkdir(parents=True,exist_ok=True);receipt={'system_sha':SYSTEM,'runtime_sha':RUNTIME,'source_sha':git(ROOT,'rev-parse','HEAD'),'source_dirty':bool(git(ROOT,'status','--porcelain')),'hardware_verified':False,'runs':[]}
with tempfile.TemporaryDirectory(prefix='native-controller-') as temp:
 stage_dir=Path(temp);headers=stage(system,runtime,stage_dir,out)
 catalog=stage_dir/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 for name,number in [('calculator',1),('stopwatch',2),('battery',3)]:
  for sanitized in ([False] if a.normal_only else [False,True]):
   dest=out/(name+('-san' if sanitized else ''));san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
   defines=flags(name)+['-DNOVA_APP_ID='+str(number),'-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps'/(name+'.c')))];inc=include_paths(system,headers)
   run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,*defines,*['-I'+str(i) for i in inc],ROOT/'test/native_apps/native_utility_controller_test.c',*sources(name,system)[1:],catalog,'-Wl,--wrap=free','-o',dest])
   for paper in [0,1]:
    for case in range(25 if number==2 else 8):
     if (case==20 if number==2 else case==1) and not paper:continue
     log=out/f'{name}-{int(sanitized)}-{paper}-{case}.log'
     with log.open('w') as stream:run([dest,str(case),str(paper)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),stdout=stream,stderr=subprocess.STDOUT,timeout=15)
     receipt['runs'].append(dict(app=name,sanitized=sanitized,paper=bool(paper),case=case));print(name+' '+str(sanitized)+' '+str(paper)+' '+str(case)+' passed',flush=True)
(out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
