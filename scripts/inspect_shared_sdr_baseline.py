#!/usr/bin/env python3
"""Read exact public cumulative Home+Contexts stores; never select an older base."""
import argparse,hashlib,json,sys,urllib.request,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"System/scripts"))
from image_envelope import MAGIC,decode_image
from compose_shared_app_cohort import write_tree,safe
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--bootfs-tools',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);a=p.parse_args()
sys.path.insert(0,str(a.bootfs_tools.resolve()))
from read_only_spiffs import read_image
lock=json.loads((ROOT/'docs/shared-sdr/baseline.json').read_text())
a.output.mkdir(parents=True,exist_ok=True)
for row in lock['assets']:
 path=a.output/row['name']
 with urllib.request.urlopen(row['url'],timeout=180) as r:path.write_bytes(r.read())
 if len(path.read_bytes())!=row['bytes'] or hashlib.sha256(path.read_bytes()).hexdigest()!=row['sha256']:
  raise ValueError('Published cumulative baseline byte mismatch '+row['name'])
records={}
for lane in ('watch','wave','x4'):
 raw=(a.output/f'product-{lane}-store.bin').read_bytes()
 stored=read_image(raw,len(raw));canonical={n:decode_image(b) if b.startswith(MAGIC) else b for n,b in stored.items()}
 with zipfile.ZipFile(a.output/f'product-{lane}-sd-apps.zip') as z:
  sd={safe(n):z.read(n) for n in z.namelist()}
 write_tree(a.output/lane/'before',canonical);write_tree(a.output/lane/'before-stored',stored)
 write_tree(a.output/lane/'before-sd',sd)
 boot=json.loads(canonical['boot.json']);matches=[]
 for row in boot['app_capabilities']:
  manifest=json.loads(canonical[row['manifest']])
  if 'waterfall' in json.dumps(manifest).lower():
   image = sd[row['image_source']['path']] if 'image_source' in row else canonical[manifest['file_name']]
   matches.append({'row':row,'manifest':manifest,'elf_bytes':len(image),'elf_sha256':hashlib.sha256(image).hexdigest()})
 providers=[]
 for row in boot['drivers']:
  manifest=json.loads(canonical[row['manifest']])
  if any(x['capability'] in ('ui.canvas.raster','ui.canvas-input','ui.text-input','radio.iq','alarm.service','storage.key-value','storage.app-data','alarm.service.facade') for x in manifest.get('provides',[])):
   providers.append({'row':row,'manifest':manifest})
 records[lane]={'store_bytes':len(raw),'apps':matches,'providers':providers,'boot':boot,
 'files':{n:hashlib.sha256(b).hexdigest() for n,b in canonical.items()},
 'sd_files':{n:hashlib.sha256(b).hexdigest() for n,b in sd.items()}}
 print('SDR INVENTORY '+lane+' '+json.dumps({'apps':matches,'providers':providers,'resident_shell':boot.get('resident_shell')}),flush=True)
(a.output/'inventory.json').write_text(json.dumps(records,indent=2)+'\n')
