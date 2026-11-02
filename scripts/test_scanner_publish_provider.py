#!/usr/bin/env python3
"""Actual Scanner publish and scene provider regression; physical I/O doubled."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from scanner_support import sdk
from lifecycle_dependencies import Closure
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','system','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--app-root',type=Path,default=ROOT,help='Optional historical app source for negative reproduction');p.add_argument('--provider-system',type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
runtime,system,out=a.runtime.resolve(),a.system.resolve(),a.output.resolve();provider=(a.provider_system or system).resolve();out.mkdir(parents=True,exist_ok=False)
inc=sdk(runtime,system,out/'sdk');shutil.copyfile(system/'Services/scene_profile/SceneProfileV1.h',inc/'SceneProfileV1.h')
shutil.copyfile(system/'Services/text_input/SceneKeyboardV1.h',inc/'SceneKeyboardV1.h')
closure=Closure([('Runtime',runtime),('System',system),('ProviderSystem',provider),('Utilities',ROOT),('SelectedApp',a.app_root.resolve()),('generated-sdk',out/'sdk')],out/'dependencies.json')
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(inc),'-I'+str(a.app_root.resolve()/'Apps')]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']
commands=[]
def run(cmd):
 commands.append(list(map(str,cmd)));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 r=subprocess.run(cmd,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/('command-'+str(len(commands))+'.log')).write_text(r.stdout+r.stderr);print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
cc=os.environ.get('CC','cc');objects=[]
for name,source,extra in [('host',provider/'Services/scene_host/host.c',['-Drisc_runtime_get_api=scanner_provider_runtime']),('transport',ROOT/'test/scanner_scene/publish_provider_transport.c',['-Drisc_runtime_get_api=scanner_provider_runtime','-DSCANNER_SCENE_HOST_FIXTURE="'+str(provider/'test/scene/host_test.c')+'"']),('root',ROOT/'test/scanner_scene/publish_provider_test.c',[])]:
 obj=out/(name+'.o');cmd=[cc,*flags,*extra,'-c',str(source),'-o',str(obj)];closure.record(cmd);run(cmd);objects.append(str(obj))
exe=out/'test';run([cc,*flags,*objects,'-o',str(exe)])
for profile in ('watch','paper','gray'):run([str(exe),profile])
for key,digest in closure.inputs.items():
 name,rel=key.split('/',1);base=dict(closure.roots)[name]
 assert hashlib.sha256((base/rel).read_bytes()).hexdigest()==digest, 'Source changed: '+key
(out/'receipt.json').write_text(json.dumps({'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'passed':True,'profiles':['watch','paper','gray'],'sanitize':a.sanitize,'scope':'Actual Scanner publish plus production scene; physical transport and unrelated app endpoints doubled. No Runtime/hardware claim.','provider_system':str(provider)},indent=2)+'\n')
