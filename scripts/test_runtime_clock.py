#!/usr/bin/env python3
"""Canonical byte identity plus owning Canvas/Contexts clock behavior."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
from build_canvas_apps import stage as canvas_stage
from build_contexts_scene import stage as contexts_stage
from runtime_clock_sdk import ROOT,stage_runtime_clock

def run(runtime,system,canonical):
 record=json.loads((ROOT/'sdk/runtime-clock-source.json').read_text())
 assert record['source_commit']=='37fca3e2244bf71ac64bd8cd7e62eb94f4c09a9d'
 header=ROOT/record['path'];assert header.read_bytes()==canonical.read_bytes()
 assert hashlib.sha256(header.read_bytes()).hexdigest()==record['sha256']=='76584fdab224b184918d9f9cf675d5c4443adef7161e8518d456bc25277dc30f'
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
 with tempfile.TemporaryDirectory(prefix='utilities-clock-test-') as tmp:
  tmp=Path(tmp)
  for consumer,stage,toolbar in [('canvas',canvas_stage,False),('contexts',contexts_stage,False),('contexts',contexts_stage,True)]:
   inc=stage(runtime,system,tmp/(consumer+('-toolbar' if toolbar else '')))
   assert (inc/header.name).read_bytes()==header.read_bytes()
   for san in (False,True):
    exe=tmp/'probe'
    flags=['-std=c11','-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(inc)]
    if toolbar:flags+=['-DPORTABLE_NATIVE_TIME_TOOLBAR']
    if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    sources=[str(ROOT/f'test/{consumer}/clock_test.c')]
    if consumer=='contexts':
     flags+=['-I'+str(system/'test/scene'),'-DSCENE_HOST_SOURCE="'+str(system/'Services/scene_host/host.c')+'"']
     sources+=[str(ROOT/'test/contexts/host_probe.c')]
    subprocess.run(['cc',*flags,*sources,'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env=env);exe.unlink()
  # Compile unchanged helper against both current and pre-clock SDK prefixes.
  text=(runtime/'sdk/app/RiscRuntimeV1.h').read_text()
  for legacy in (False,True):
   helper_inc=tmp/('legacy' if legacy else 'current');helper_inc.mkdir()
   (helper_inc/header.name).write_bytes(header.read_bytes())
   selected=text
   if legacy:
    start=selected.index('  bool (*monotonic_ms)(uint32_t* out);')
    end=selected.index('} risc_runtime_api_v1;',start)
    selected=selected[:start]+selected[end:]
    selected='\n'.join(line for line in selected.splitlines() if not line.startswith('#define RISC_RUNTIME_MONOTONIC_V1_SIZE') and not line.startswith('#define RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE'))+'\n'
   (helper_inc/'RiscRuntimeV1.h').write_text(selected)
   for compiler,language,std in [('cc','c','c11'),('c++','c++','c++17')]:
    for san in (False,True):
     exe=tmp/'reader';flags=['-x',language,'-std='+std,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-I'+str(helper_inc)]
     if san:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
     subprocess.run([compiler,*flags,str(ROOT/'tests/runtime_clock_reader_test.c'),'-o',str(exe)],check=True)
     subprocess.run([str(exe)],check=True,env=env);exe.unlink()
  # Refuse divergent same-name canonical headers rather than silently replacing.
  (inc/header.name).write_text('different\n')
  try:stage_runtime_clock(inc)
  except ValueError:pass
  else:raise AssertionError('conflicting SDK helper accepted')
 print('Canonical equality/hash, both stages, 6 owning plus 8 C/C++ current/legacy normal/sanitized configurations PASS')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('runtime','system-apps','canonical-header'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.canonical_header.resolve())
