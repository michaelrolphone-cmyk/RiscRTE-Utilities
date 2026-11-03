#!/usr/bin/env python3
"""Decode exact published bytes; inspect all OBD identities and preserve inventory."""
import argparse,json,sys,hashlib,urllib.request,zipfile
from pathlib import Path
p=argparse.ArgumentParser()
for n in ('system','bootfs-tools','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
sys.path[:0]=[str(a.system.resolve()/'scripts'),str(a.bootfs_tools.resolve())]
from assemble_shared_usb_cohort import decode_store
from compose_shared_app_cohort import safe,write_tree
from read_only_spiffs import read_image
lock=json.loads((Path(__file__).resolve().parents[1]/'docs/shared-obd/baseline.json').read_text())
for x in lock['assets']:
 raw=urllib.request.urlopen(x['url']).read()
 assert len(raw)==x['bytes'] and hashlib.sha256(raw).hexdigest()==x['sha256']
 (a.output/x['name']).write_bytes(raw)
result={}
for lane in ('watch','wave','x4'):
 raw=(a.output/f'product-{lane}-store.bin').read_bytes();stored=read_image(raw,len(raw));canonical=decode_store(stored)
 with zipfile.ZipFile(a.output/f'product-{lane}-sd-apps.zip') as z:
  assert len(z.namelist())==len(set(z.namelist()))
  sd={safe(n):z.read(n) for n in z.namelist()}
 for label,tree in [('before',canonical),('before-stored',stored),('before-sd',sd)]:write_tree(a.output/lane/label,tree)
 boot=json.loads(canonical['boot.json']);matches=[]
 for row in boot['app_capabilities']:
  m=json.loads(canonical[row['manifest']])
  if 'obd' in json.dumps(m).lower():
   source=row.get('image_source')
   blob=sd[source['path']] if source else canonical[str(Path(row['manifest']).parent/m['file_name'])]
   matches.append({'row':row,'manifest':m,'bytes':len(blob),'sha256':hashlib.sha256(blob).hexdigest()})
 result[lane]={'obd':matches,'boot':boot}
 print('INSTALLED OBD '+lane+' '+json.dumps(matches),flush=True)
(a.output/'inventory.json').write_text(json.dumps(result,indent=2)+'\n')
