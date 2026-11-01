#!/usr/bin/env python3
"""Production spectrum root lifecycle with scene/canvas/text endpoint doubles."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for key in ['runtime','system','output']:p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);inc=out/'include';inc.mkdir(exist_ok=True)
origins={}
for d in [ROOT/'lib/Bluetooth/include',a.system/'lib/PortableApps/include',a.system/'sdk/app',a.runtime/'sdk/app',a.runtime/'sdk/driver',a.runtime/'sdk/hardware']:
 for header in d.glob('*.h'):origins[header.name]=header.resolve()
for name,path in origins.items():shutil.copyfile(path,inc/name)
(out/'sdk-origins.json').write_text(json.dumps({k:str(v) for k,v in origins.items()},indent=2)+'\n')
commands=[];results=[];closure={}
for name in ['root']:
 flags=['cc','-std=c11','-O0','-g0','-pipe','-Wall','-Wextra','-Werror','-I'+str(ROOT/'Apps'),'-I'+str(inc)]
 if name=='root':flags+=['-Wno-unused-function','-Wno-unused-variable','-ffunction-sections','-fdata-sections','-Wl,--gc-sections']
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 source=ROOT/'test/spectrum_scene_lifecycle'/(name+'_test.c');dep=out/(name+'.d');binary=out/name
 cmd=flags+['-MM','-MF',str(dep),str(source)];commands.append(cmd);r=subprocess.run(cmd,capture_output=True,text=True);(out/(name+'-deps.log')).write_text(r.stdout+r.stderr);r.check_returncode()
 paths=dep.read_text().replace('\\\n',' ').split(':',1)[1].split()
 for path in paths:closure[str(Path(path).resolve())]=hashlib.sha256(Path(path).read_bytes()).hexdigest()
 cmd=flags+[str(source),'-o',str(binary)];commands.append(cmd);r=subprocess.run(cmd,capture_output=True,text=True);(out/(name+'-build.log')).write_text(r.stdout+r.stderr);r.check_returncode()
 cmd=[str(binary)]+([str(out/'waterfall-410.ppm')] if name=='canvas' else []);commands.append(cmd);r=subprocess.run(cmd,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/(name+'-test.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode();results.append(r.stdout.strip())
for path,digest in closure.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,'Source changed: '+path
closure[str(Path(__file__).resolve())]=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
(out/'receipt.json').write_text(json.dumps({'results':results,'sanitize':a.sanitize,'commands':commands,'source_sha256':closure,'scope':'Production root lifecycle and actual background adapter; scene/canvas/text/Runtime endpoints doubled. No real Runtime/native provider or physical claim.'},indent=2)+'\n')
