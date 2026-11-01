#!/usr/bin/env python3
"""Guarded Spectrum storage boundaries; real scope/proxy with endpoint doubles."""
import argparse,hashlib,json,os,shlex,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for key in ('runtime','system','output'):p.add_argument('--'+key,required=True,type=Path)
p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);inc=out/'include';inc.mkdir()
origins={}
for directory in [a.system/'lib/PortableApps/include',a.system/'sdk/app',a.runtime/'sdk/app',a.runtime/'sdk/driver',a.runtime/'sdk/hardware']:
 for header in directory.glob('*.h'):origins[header.name]=header.resolve()
for name,path in origins.items():shutil.copyfile(path,inc/name)
source=ROOT/'test/spectrum_scene_io/io_test.c';binary=out/'test';dep=out/'inputs.d'
flags=['cc','-std=c11','-O0','-g0','-pipe','-Wall','-Wextra','-Werror','-I'+str(ROOT/'Apps'),'-I'+str(inc)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
subprocess.run([*flags,'-MM','-MF',str(dep),str(source)],check=True)
paths=shlex.split(dep.read_text().split(':',1)[1].replace('\\\n',' '));closure={str(Path(v).resolve()):hashlib.sha256(Path(v).read_bytes()).hexdigest() for v in paths}
cmd=[*flags,str(source),'-o',str(binary)];r=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(r.stdout+r.stderr);r.check_returncode()
r=subprocess.run([str(binary)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/'test.log').write_text(r.stdout+r.stderr);print(r.stdout,end='');r.check_returncode()
for path,digest in closure.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,'Changed input: '+path
(out/'receipt.json').write_text(json.dumps({'result':r.stdout.strip(),'sanitize':a.sanitize,'command':cmd,'inputs':closure,'sdk_origins':{k:str(v) for k,v in origins.items()},'scope':'Production storage scope/proxy with deterministic endpoint doubles; no native durable backend/device execution.'},indent=2)+'\n')
