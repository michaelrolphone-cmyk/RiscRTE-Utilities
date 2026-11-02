#!/usr/bin/env python3
"""Strict host endpoint-double qualification, not native persistence/RF proof."""
import argparse,hashlib,json,pathlib,subprocess,os
p=argparse.ArgumentParser();p.add_argument('--system',required=True,type=pathlib.Path);p.add_argument('--runtime',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-O1','-I'+str(root/'Apps'),'-I'+str(a.system/'lib/PortableApps/include'),'-I'+str(a.runtime/'sdk/app'),'-MMD','-MF',str(out/'deps.d'),str(root/'test/scanner_preferences/test.c'),'-o',str(out/'test')]
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
with (out/'build.log').open('w') as f:subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True)
env=dict(os.environ)
if a.sanitize:env.update(ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
with (out/'test.log').open('w') as f:subprocess.run([str(out/'test')],stdout=f,stderr=subprocess.STDOUT,check=True,env=env)
paths=(out/'deps.d').read_text().replace('\\\n',' ').split(':',1)[1].split();paths+=[str(pathlib.Path(__file__).resolve())]
receipt={'cases':int((out/'test.log').read_text().split()[0]),'sanitize':a.sanitize,'leak_detection':False if a.sanitize else None,'command':cmd,'inputs':{str(pathlib.Path(x).resolve()):hashlib.sha256(pathlib.Path(x).read_bytes()).hexdigest() for x in paths},'binary_sha256':hashlib.sha256((out/'test').read_bytes()).hexdigest(),'scope':'host endpoint doubles; actual public preference/rollback/codec controllers'}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
