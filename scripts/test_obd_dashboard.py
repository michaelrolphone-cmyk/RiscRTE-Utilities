#!/usr/bin/env python3
"""Protocol and actual app/scene tests. Serial and board are deterministic fixtures."""
import argparse,os,subprocess
from pathlib import Path

# Native boot/catalog app schema, not the unrelated store catalog schema.
import json as _manifest_json
_manifest = _manifest_json.loads((Path(__file__).resolve().parents[1]/'Apps/obd_dashboard.json').read_text())
_required = {'type','id','version','architecture','file_name','entry','requires'}
_optional = {'description','display_name','icon','supported_file_types','runtime_lifecycle'}
assert _required <= _manifest.keys() <= _required | _optional
assert _manifest['architecture'] == 'xtensa-esp32s3' and _manifest['entry'] == 'app_main'
assert _manifest['runtime_lifecycle'] == 'foreground'
assert _manifest['requires'] == [{'capability':'ui.scene','api':1},{'capability':'bluetooth.serial','api':1},{'capability':'storage.volume','api':1}]

from build_obd_dashboard import stage
ROOT=Path(__file__).resolve().parents[1]
def run(runtime,system,drivers,out,sanitize):
 out.mkdir(parents=True,exist_ok=True);inc=stage(runtime,system,drivers,out)
 cc=os.environ.get('CC','cc');flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-format-truncation','-I'+str(inc),'-I'+str(ROOT/'Apps/obd_dashboard'),'-I'+str(system/'test/scene')]
 if sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
 exe=out/'math-test';subprocess.run([cc,*flags,'-D__udivdi3=obd_unsigned_divide','-D__divdi3=obd_signed_divide',str(ROOT/'Apps/obd_dashboard/math.c'),str(ROOT/'test/obd/math_test.c'),'-o',str(exe)],check=True);subprocess.run([str(exe)],env=env,check=True)
 exe=out/'elm-test';subprocess.run([cc,*flags,str(ROOT/'Apps/obd_dashboard/elm.c'),str(ROOT/'Apps/obd_dashboard/dashboard.c'),str(ROOT/'test/obd/diagnostics_test.c'),'-o',str(exe)],check=True);subprocess.run([str(exe)],env=env,check=True)
 exe=out/'readiness-test';subprocess.run([cc,*flags,str(ROOT/'Apps/obd_dashboard/elm.c'),str(ROOT/'test/obd/readiness_test.c'),'-o',str(exe)],check=True);subprocess.run([str(exe)],env=env,check=True)
 exe=out/'app-test';subprocess.run([cc,*flags,str(system/'test/scene/telemetry_probe.c'),str(ROOT/'Apps/obd_dashboard/elm.c'),str(ROOT/'Apps/obd_dashboard/dashboard.c'),str(ROOT/'test/obd/app_test.c'),'-o',str(exe)],check=True)
 for scenario in range(16):
  frames=out/'frames'/str(scenario);frames.mkdir(parents=True,exist_ok=True)
  subprocess.run([str(exe),str(scenario),str(frames)],env=env,check=True,timeout=60)
if __name__=='__main__':
 p=argparse.ArgumentParser()
 for n in ('runtime','system','drivers','output'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.runtime.resolve(),a.system.resolve(),a.drivers.resolve(),a.output.resolve(),a.sanitize)
