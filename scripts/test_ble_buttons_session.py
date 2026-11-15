#!/usr/bin/env python3
"""Strict actual controllers/root with endpoint doubles, not device proof."""
import argparse,hashlib,json,os,pathlib,subprocess
from buttons_support import sdk
from lifecycle_dependencies import Closure
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','system','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
p.add_argument('--sanitize',action='store_true');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);include=sdk(a.runtime,a.system,a.output/'sdk')
closure=Closure([('Runtime',a.runtime),('System',a.system),('Utilities',ROOT),('generated-sdk',a.output/'sdk')],a.output/'dependencies.json')
flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-g0','-O0','-I'+str(ROOT/'Apps'),'-I'+str(include)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0' # ptrace environment cannot run LSan.
results=[]
for name in ('controller','independent','root'):
 source=ROOT/'test/ble_buttons_session'/(name+'_test.c');binary=a.output/name
 cmd=[os.environ.get('CC','cc'),*flags,str(source),'-o',str(binary)];closure.record(cmd)
 with (a.output/(name+'-build.log')).open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
 with (a.output/(name+'-run.log')).open('w') as log:subprocess.run([str(binary)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 results.append({'suite':name,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'result':'pass'})
for key,digest in closure.inputs.items():
 root,rel=key.split('/',1);assert hashlib.sha256((dict(closure.roots)[root]/rel).read_bytes()).hexdigest()==digest,key
(a.output/'receipt.json').write_text(json.dumps({'scope':'actual session/editor/root with HID/KV/Runtime and scene endpoint doubles; no physical claim','sanitize':a.sanitize,'leak_sanitizer':False,'suites':results,'result':'pass'},indent=2)+'\n')
print(a.output/'receipt.json')
