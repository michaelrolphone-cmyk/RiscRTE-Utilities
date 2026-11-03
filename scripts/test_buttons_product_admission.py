#!/usr/bin/env python3
"""Prepare exact private products with published native binding fixtures.
This verifies metadata/authority and retained X4 SD graph only: no target ELF
execution, firmware rebuild, hardware operations or native cohort ELF admission.
"""
import argparse,pathlib,json,subprocess,hashlib,shlex,os
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','x4-runtime','fixtures','assembly','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
p.add_argument('--sanitize',action='store_true');p.add_argument('--reuse-receipt',type=pathlib.Path);args=p.parse_args();args.output.mkdir(parents=True,exist_ok=False)
fixtures=args.fixtures.resolve();templates=json.loads((fixtures/'commands.json').read_text());results={}
sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
reuse=json.loads(args.reuse_receipt.read_text()) if args.reuse_receipt else None
if reuse and reuse['sanitized']!=args.sanitize:raise ValueError('Reused sanitizer mode differs')
for lane,template in templates.items():
 cmd=[x.format(runtime=args.runtime,x4_runtime=args.x4_runtime,fixtures=fixtures,output=args.output) for x in template]
 if args.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 sources=[x for x in cmd if x.endswith(('.c','.cpp'))];base=[];skip=False
 for x in cmd[1:]:
  if skip:skip=False;continue
  if x=='-o':skip=True;continue
  if x in sources or x in ['-ldl','-rdynamic']:continue
  base.append(x)
 files=set();per={}
 for src in sources:
  q=subprocess.run([cmd[0],*base,'-MM','-MT','source',src],capture_output=True,text=True,check=True);deps=shlex.split(q.stdout.replace('\\\n',' ').split(':',1)[1]);per[src]=deps;files.update(deps)
 before={f:sha(f) for f in sorted(files)}
 executable=args.output/lane
 if reuse:
  original=reuse['products'][lane];executable=args.reuse_receipt.parent/lane
  if original['inputs']!=before or sha(executable)!=original['binary_sha256']:raise ValueError('Reused compiler closure/binary differs')
  original_cmd=original['command'];wanted=[str(executable) if x==str(args.output/lane) else x for x in cmd]
  if wanted!=original_cmd:raise ValueError('Reused exact compiler command differs')
 else:
  q=subprocess.run(cmd,capture_output=True);(args.output/(lane+'-compile.log')).write_bytes(q.stdout+q.stderr)
  if q.returncode:raise RuntimeError(lane+' compile failed; see retained log')
 rows=[]
 for which in ('before','after'):
  q=subprocess.run([str(executable),str(args.assembly/lane/which)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  (args.output/(lane+'-'+which+'.log')).write_text(q.stdout+q.stderr)
  if q.returncode:raise RuntimeError(lane+' '+which+' preparation failed')
  result=json.loads(q.stdout)
  if not result['prepared'] or result['hardware_calls'] or result['storage_calls']:raise RuntimeError(lane+' invalid preparation')
  if lane=='x4' and not result.get('retained_sd_serial_graph_bound'):raise RuntimeError('X4 retained SD graph missing')
  rows.append({'input':which,'result':result})
 if {f:sha(f) for f in before}!=before:raise RuntimeError('Compiler input mutated')
 results[lane]={'command':cmd,'inputs':before,'per_tu_dependencies':per,'results':rows,'binary_sha256':sha(executable),'binary_path':str(executable),'reused_receipt':str(args.reuse_receipt) if reuse else None}
 (args.output/'receipt.json').write_text(json.dumps({'schema':1,'sanitized':args.sanitize,'scope':__doc__,'products':results},indent=2)+'\n');print(lane,'PASS',flush=True)
