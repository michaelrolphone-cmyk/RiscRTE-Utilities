#!/usr/bin/env python3
"""Run the existing actual controller atomic cases in the common app build."""
import argparse,json,os,subprocess
from pathlib import Path
from build_contexts_scene import stage
ROOT=Path(__file__).resolve().parents[1]
CASES='legacy-enable enable sources timing busy delayed exit retained automatic-noop automatic-order automatic-low automatic-mask automatic-partial automatic-retained automatic-rgb'.split()
def main():
 p=argparse.ArgumentParser()
 for n in ('runtime','system-apps','output'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 inc=stage(a.runtime.resolve(),a.system_apps.resolve(),a.output/'sdk')
 cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
 '-DCONTEXTS_SHARED_APPLICATION','-I'+str(inc),'-I'+str(a.system_apps/'test/scene'),
 '-DSCENE_HOST_SOURCE="'+str((a.system_apps/'Services/scene_host/host.c').resolve())+'"']
 if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=a.output/'atomic'
 subprocess.run([*cmd,str(ROOT/'test/contexts/host_probe.c'),str(ROOT/'test/contexts/atomic_test.c'),'-o',str(exe)],check=True)
 results=[]
 for case in CASES:
  r=subprocess.run([str(exe),case],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
  (a.output/(case+'.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,flush=True);r.check_returncode();results.append(case)
 (a.output/'results.json').write_text(json.dumps({'passed':results,'physical_testing':'not performed'},indent=2)+'\n')
if __name__=='__main__':main()
