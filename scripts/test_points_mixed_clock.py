#!/usr/bin/env python3
"""Reproducible host service/ABI checks; no ELF admission or hardware claim."""
import argparse, hashlib, json, os
from pathlib import Path
import subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system',type=Path,required=True)
 p.add_argument('--receipt',type=Path,required=True);a=p.parse_args();checks=[]
 cases=[('tests/'+n+'_test.c',['-DALARM_NATIVE_UTC','-DALARM_SERVICE_TAGGED_V2','-DALARM_SERVICE_CLOCK_EDIT_V2'],False) for n in ('clock_edit_participants','clock_edit_schedule_participants','clock_edit_legacy_import','points_service_context')]
 cases += [('test/native_apps/'+n+'.c',[],True) for n in ('points_mixed_clock_edit_test','points_mixed_begin_storage_test','points_service_context_production_test','native_utc_alarm_service_test','native_clock_edit_service_test')]
 with tempfile.TemporaryDirectory(prefix='points-mixed-check-') as d:
  for source,defines,time in cases:
   for san in (False,True):
    target=Path(d)/'check';cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*defines]
    if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']
    cmd+=['-I'+str(ROOT/'lib/Alarm/include'),'-I'+str(a.runtime/'sdk/driver'),'-I'+str(a.runtime/'sdk/app'),'-I'+str(a.system/'lib/PortableApps/include'),str(ROOT/source)]
    if time:cmd += [str(a.system/'lib/PortableApps/src'/n) for n in ('PortableTimeZone.c','PortableTimeZoneCatalog.c')]
    cmd+=['-o',str(target)];subprocess.run(cmd,check=True)
    r=subprocess.run([str(target)],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    checks.append({'source':source,'sanitized':san,'command':cmd,'stdout':r.stdout,'stderr':r.stderr,'returncode':r.returncode});print(source,san,'PASS',flush=True)
 def head(path):return subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
 sources=[q for folder in ('Services/alarm_service','lib/Alarm/include','test/native_apps','tests') for q in (ROOT/folder).rglob('*') if q.is_file()]
 a.receipt.write_text(json.dumps({'scope':'Host service/codec/accessor tests only; not current product composition, ELF admission or physical proof','runtime_head':head(a.runtime),'system_head':head(a.system),'utilities_base':head(ROOT),'checks':checks,'sha256':{str(q.relative_to(ROOT)):hashlib.sha256(q.read_bytes()).hexdigest() for q in sources}},indent=2)+'\n')
if __name__=='__main__':main()
