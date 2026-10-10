#!/usr/bin/env python3
"""Prove selected app edits preserve prior flag-off target ELF bytes."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
rows=[]
for receipt in sorted(a.build.glob('*/x4-native-app.json')):
 r=json.loads(receipt.read_text());name=r['app'];original=r['compile_command'];system=Path(next(x for x in original if x.endswith('/lib/PortableApps/src/adapter.c'))).parents[3]
 current=[x for x in original if not x.startswith(('-DPORTABLE_RESIDENT_','-DPORTABLE_UNPADDED_HOURS'))]
 current+=['-DPORTABLE_QUICK_ACTIONS',*[str(system/'lib/PortableApps/src'/n) for n in ('quick_actions.c','quick_session.c','quick_render.c')]]
 for label,root in [('baseline',a.baseline),('current',a.source)]:
  folder=a.output/label/name;folder.mkdir(parents=True,exist_ok=True);command=[x.replace(str(a.source),str(root)) for x in current];command[command.index('-o')+1]=str(folder/(name+'.elf'))
  subprocess.run(command,check=True,cwd=folder,stdout=subprocess.DEVNULL)
 old=(a.output/'baseline'/name/(name+'.elf')).read_bytes();new=(a.output/'current'/name/(name+'.elf')).read_bytes()
 if old!=new:raise ValueError('Flag-off target bytes changed: '+name)
 rows.append(dict(app=name,flag_off_elf_identical=True,sha256=hashlib.sha256(new).hexdigest(),bytes=len(new)))
 print(name+': flag-off target bytes identical',flush=True)
(a.output/'evidence.json').write_text(json.dumps(rows,indent=2)+'\n')
