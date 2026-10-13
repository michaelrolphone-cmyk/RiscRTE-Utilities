#!/usr/bin/env python3
"""Bounded host tests of the real standalone Contexts controller and dispatcher."""
import argparse, os, subprocess, tempfile
from pathlib import Path
from build_contexts_scene import stage
ROOT=Path(__file__).resolve().parents[1]
CASES='one many oversized one-over-limit empty refresh long long-id medium select-stale unavailable missing partial release malformed duplicate generation snapshot-error snapshot-context snapshot-retained get-context get-retained get-error faces-actions absent launch launch-stale launch-removed launch-rejected launch-retained launch-catalog-retained launch-home launch-home-get picker-checkpoint picker-checkpoint-empty'.split()
def run(runtime,system,catalog_system,sanitize=False):
 with tempfile.TemporaryDirectory(prefix='contexts-catalog-') as tmp:
  tmp=Path(tmp);include=stage(runtime,system,tmp,catalog_system)
  flags=['-std=c11','-O1','-g0','-Wall','-Wextra','-Werror','-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=1','-I'+str(include),'-I'+str(system/'test/scene'),'-DSCENE_HOST_SOURCE="'+str(system/'Services/scene_host/host.c')+'"']
  for sanitized in ((False,True) if sanitize else (False,)):
   san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
   exe=tmp/'test'
   subprocess.run([os.environ.get('CC','cc'),*flags,*san,str(ROOT/'test/contexts/host_probe.c'),str(ROOT/'test/contexts/runtime_catalog_test.c'),'-o',str(exe)],check=True)
   symbols=subprocess.check_output(['nm',str(exe)],text=True)
   assert not {'portable_catalog','portable_catalog_count'} & {line.split()[-1] for line in symbols.splitlines() if line.split()}
   for case in CASES:subprocess.run([str(exe),case],check=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 print(f'PASS {len(CASES)*(2 if sanitize else 1)} standalone catalog cases; no compiled inventory symbols')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','catalog-system'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 run(a.runtime.resolve(),a.system_apps.resolve(),a.catalog_system.resolve(),a.sanitize)
