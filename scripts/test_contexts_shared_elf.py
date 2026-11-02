#!/usr/bin/env python3
"""Exercise common app_main and optional-capability faults, not a model copy."""
import argparse,json,os,subprocess,tempfile
from pathlib import Path
from test_contexts_shared_input import build
def run(a):
 a.output.mkdir(parents=True,exist_ok=True)
 results=[]
 os.environ['CONTEXTS_SHARED_ELF_TEST']='1'
 with tempfile.TemporaryDirectory(prefix='contexts-shared-') as tmp:
  exe=build(a.runtime,a.system_apps,Path(tmp),'watch',True,a.sanitize,native=True)
  for case in ('normal','missing','partial','bad-alarm','revoke','release','limited','stale'):
   result=subprocess.run([str(exe),'watch',str(a.output),'shared-'+case],capture_output=True,text=True,
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
   (a.output/(case+'.log')).write_text(result.stdout+result.stderr)
   print(result.stdout+result.stderr,flush=True);result.check_returncode();results.append(case)
 (a.output/'results.json').write_text(json.dumps({'passed':results,'physical_testing':'not performed'},indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('runtime','system-apps','output'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');run(p.parse_args())
