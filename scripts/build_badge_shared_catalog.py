#!/usr/bin/env python3
"""Reproduce the selected Badge Springboard before adding Shared Alarms."""
import argparse,hashlib,json,os,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def extract(capsule,output):
 output.mkdir(parents=True,exist_ok=False)
 with zipfile.ZipFile(capsule) as z:
  names=z.namelist()
  if len(names)!=len(set(names)) or any(Path(n).is_absolute() or '..' in Path(n).parts for n in names):raise ValueError('Unsafe capsule paths')
  recipe=json.loads(z.read('recipe.json'))
  if set(names)!=set(recipe['files_sha256'])|{'recipe.json'}:raise ValueError('Unexpected capsule contents')
  for n,h in recipe['files_sha256'].items():
   data=z.read(n)
   if hashlib.sha256(data).hexdigest()!=h:raise ValueError('Input hash mismatch: '+n)
   p=output/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 return recipe
def run(cc,output):
 capsule=ROOT/'test/release/badge-springboard-baseline.zip'
 recipe=extract(capsule,output)
 if sha(Path(cc))!=recipe['compiler_sha256']:raise ValueError('Exact baseline compiler required')
 version=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
 if version!=recipe['compiler']:raise ValueError('Compiler version differs')
 command=[cc,*recipe['command']]
 subprocess.run(command,cwd=output,check=True)
 elf=output/'generated/springboard.elf'
 if sha(elf)!=recipe['baseline_sha256']:raise ValueError('Baseline reproduction failed; do not package new catalog')
 baseline=sha(elf);(output/'baseline.elf').write_bytes(elf.read_bytes())
 apps=recipe['catalog']+[{'display_name':'Shared Alarms','file_name':'alarms_shared.elf','icon':'solid:f0f3'}]
 if len({a['file_name'] for a in apps})!=len(apps):raise ValueError('Duplicate app filename')
 text='#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={'
 text+=','.join('{'+','.join('.'+k+'='+json.dumps(a[k]) for k in ('display_name','file_name','icon'))+',.compatible=true}' for a in apps)
 text+='};\nconst unsigned portable_catalog_count='+str(len(apps))+';\n'
 (output/'generated/springboard-catalog.c').write_text(text)
 subprocess.run(command,cwd=output,check=True)
 (output/'catalog.json').write_text(json.dumps({'apps':apps},indent=2)+'\n')
 result={'schema':1,'capsule_sha256':sha(capsule),'baseline_sha256':baseline,'new_sha256':sha(elf),'compiler':version,'compiler_sha256':sha(Path(cc)),'command':command,'catalog':apps,'unchanged_flags':recipe['source_record']['build_defines'],'hardware_verified':False}
 (output/'catalog-build.json').write_text(json.dumps(result,indent=2)+'\n')
 print('Exact baseline reproduced; additive ten-entry catalog built.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cc',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();run(str(Path(a.cc).resolve()),a.output.resolve())
