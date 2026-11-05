#!/usr/bin/env python3
"""Record exported source inputs for the current Utilities atomic controller tests."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for n in ('system','runtime','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();roots={'system':a.system.resolve(),'runtime':a.runtime.resolve(),'utilities':Path(__file__).resolve().parents[1]}
folders={'system':['Apps','lib','Services','test','scripts','sdk'],'runtime':['sdk'],'utilities':['Apps','lib','test','tests','scripts','experimental']}
rows=[]
for role,root in roots.items():
 for folder in folders[role]:
  for f in sorted((root/folder).rglob('*')):
   if f.is_file() and f.suffix in ('.h','.c','.cpp','.inc','.py','.json'):
    rows.append({'repo':role,'path':str(f.relative_to(root)),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
a.output.write_text(json.dumps({'heads':{k:subprocess.check_output(['git','-C',str(v),'rev-parse','HEAD'],text=True).strip() for k,v in roots.items()},'files':rows},indent=2)+'\n')
