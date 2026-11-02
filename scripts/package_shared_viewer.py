#!/usr/bin/env python3
"""Shared Viewer product reconstruction preserving unrelated bytes and authority."""
import argparse, hashlib, io, json, sys, zipfile
from pathlib import Path, PurePosixPath
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
INPUTS = {
 'watch.zip':'3a5aadb0eeb5c4b37640d606978162b5c0484893aada661baca4ca39a12d66f2',
 'wave.zip':'aae8665d9240bd49dbdae89f250525ccea6cabc44215133248ee1a2c7b19d1d6',
 'x4-store.bin':'8d38a77bdb3dd1439e8736d9c0363798f20803e02b44fc1aa5dab578207c4adc',
 'x4-sd.zip':'2b6dce3efb468f3cd728b50707e0c9552d84e225b65e2b27e683d3d726bc6cef'}
TOOLS = {'current_bootfs.py':'6f0997a71fe6d2abbb97ed91e0e4180abf43b2056afbcb5b54d303437791576b',
 'read_only_spiffs.py':'e48d3b2422c60f81fdae6371ff3c5b65c5cbaa9dd250b4a2925c74c0b45f040d'}
VERSION='1.4.0'
def sha(b): return hashlib.sha256(b).hexdigest()
def file_sha(p):
 with p.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def encode(v): return (json.dumps(v,sort_keys=True,indent=2)+'\n').encode()
def safe(n):
 p=PurePosixPath(n)
 if not n or p.is_absolute() or '..' in p.parts or '\\' in n: raise ValueError('Unsafe path: '+n)
 return n
def unzip(source):
 with zipfile.ZipFile(source) as z:
  names=z.namelist()
  if len(names)!=len(set(names)): raise ValueError('Duplicate ZIP entries')
  for n in names: safe(n)
  return {n:z.read(n) for n in names if not n.endswith('/')}
def loaded(data):
 e=ELFFile(io.BytesIO(data))
 if e.elfclass!=32 or not e.little_endian or e['e_machine']!='EM_XTENSA' or e['e_type']!='ET_DYN': raise ValueError('Not an Xtensa module')
 names=[s.name for s in e.iter_sections()]
 if len(names)!=len(set(names)): raise ValueError('Duplicate ELF sections')
 allocated={}
 for s in e.iter_sections():
  if s['sh_flags']&2:
   allocated[s.name]=(s['sh_type'],s['sh_flags'],s['sh_addr'],s['sh_size'],s['sh_addralign'],s['sh_entsize'],s.data())
 relocations={}
 for s in e.iter_sections():
  if isinstance(s,RelocationSection):
   table=e.get_section(s['sh_link'])
   relocations[s.name]=[(r['r_offset'],r['r_info_type'],r.entry.get('r_addend'),table.get_symbol(r['r_info_sym']).name) for r in s.iter_relocations()]
 return (e['e_entry'],e['e_flags'],allocated,relocations)

import copy
LOWER={'display.output','input.touch.raw','input.navigation'}
def locate(files,sd):
 boot=json.loads(files['boot.json']);rows=[r for r in boot['app_capabilities'] if r.get('manifest')=='model_viewer.json']
 if len(rows)!=1:raise ValueError('Ambiguous Viewer policy')
 row=rows[0];route=row.get('image_source',{}).get('path')
 blob=files.get('model_viewer.elf')
 if blob is None:
  if not route or route not in sd:raise ValueError('Viewer image absent')
  blob=sd[route]
 if route and route in sd and blob!=sd[route]:raise ValueError('Conflicting Viewer images')
 return blob,route,row
def run(inputs,tools,viewer,provider,output):
 if output.exists():raise ValueError('Output exists')
 for n,h in INPUTS.items():
  if file_sha(inputs/n)!=h:raise ValueError('Original input differs '+n)
 for n,h in TOOLS.items():
  if file_sha(tools/n)!=h:raise ValueError('Bootfs tool differs '+n)
 sys.path.insert(0,str(tools.resolve()))
 from read_only_spiffs import read_image
 from current_bootfs import build
 canonical=(viewer/'first/model_viewer.elf').read_bytes()
 manifest=(viewer/'first/model_viewer.json').read_bytes()
 if canonical!=(viewer/'repeat/model_viewer.elf').read_bytes():raise ValueError('Viewer repeat differs')
 if json.loads(manifest)['version']!=VERSION:raise ValueError('Wrong Viewer version')
 driver=(provider/'canvas-host/driver.elf').read_bytes()
 if driver!=(provider/'repeat/driver.elf').read_bytes():raise ValueError('Canvas repeat differs')
 dm=json.loads((provider/'canvas-host/manifest.json').read_text())
 output.mkdir(parents=True)
 (output/'model_viewer.elf').write_bytes(canonical);(output/'model_viewer.full.elf').write_bytes(canonical)
 (output/'model_viewer.json').write_bytes(manifest)
 results={}
 for lane in ('watch','wave','x4'):
  if lane=='x4':raw=(inputs/'x4-store.bin').read_bytes();sd=unzip(inputs/'x4-sd.zip')
  else:
   z=unzip(inputs/(lane+'.zip'));raw=z['candidate/store.bin'];sd=unzip(io.BytesIO(z['candidate/sd-apps.zip'])) if 'candidate/sd-apps.zip' in z else {}
  before=read_image(raw,len(raw));blob,route,row=locate(before,sd)
  oldmanifest=json.loads(before['model_viewer.json'])
  if oldmanifest['version']!=('1.3.0' if lane=='x4' else '1.3.1'):raise ValueError('Original Viewer version differs')
  boot=json.loads(before['boot.json']);after=dict(before);nextsd=dict(sd);nextboot=copy.deepcopy(boot)
  lower=[g for g in row['grants'] if g['capability'] in LOWER]
  if {g['capability'] for g in lower}!={'display.output','input.touch.raw'}|({'input.navigation'} if lane=='x4' else set()):raise ValueError('Original Viewer input authority differs')
  # Existing lower authority moves behind the presentation provider. No new
  # hardware, storage, namespace or selected-source authority is fabricated.
  profile=[n for n,b in before.items() if n.endswith('/manifest.json') and any(p.get('capability')=='ui.presentation-profile' for p in json.loads(b).get('provides',[]))]
  if len(profile)!=1:raise ValueError('Ambiguous existing presentation profile')
  provider_manifest=copy.deepcopy(dm)
  provider_manifest['requires']=[r for r in dm['requires'] if r['capability'] in {g['capability'] for g in lower}|{'ui.presentation-profile'}]
  binding={'manifest':'viewer-canvas/manifest.json','bindings':lower+[{'capability':'ui.presentation-profile','api':1,'instance_id':0}]}
  if any(r.get('manifest')==binding['manifest'] for r in boot['drivers']):raise ValueError('Provider already installed')
  nextboot['drivers'].append(binding)
  nextrow=next(r for r in nextboot['app_capabilities'] if r.get('manifest')=='model_viewer.json')
  nextrow['grants']=[g for g in row['grants'] if g['capability'] not in LOWER]+[{'capability':'ui.canvas','api':1,'instance_id':0}]
  after['model_viewer.json']=manifest
  if 'model_viewer.elf' in after:after['model_viewer.elf']=canonical
  if route:nextsd[route]=canonical
  after['viewer-canvas/driver.elf']=driver
  after['viewer-canvas/manifest.json']=encode(provider_manifest)
  after['boot.json']=encode(nextboot)
  for k,v in boot.items():
   if k not in ('drivers','app_capabilities') and nextboot[k]!=v:raise ValueError('Unrelated boot mutation')
  if nextboot['drivers'][:-1]!=boot['drivers']:raise ValueError('Existing provider mutation')
  if [r for r in nextboot['app_capabilities'] if r.get('manifest')!='model_viewer.json']!=[r for r in boot['app_capabilities'] if r.get('manifest')!='model_viewer.json']:raise ValueError('Other app policy mutation')
  original_without_grants={k:v for k,v in row.items() if k!='grants'}
  if {k:v for k,v in nextrow.items() if k!='grants'}!=original_without_grants:raise ValueError('Viewer namespace/image metadata mutation')
  allowed={'boot.json','model_viewer.json','model_viewer.elf','viewer-canvas/driver.elf','viewer-canvas/manifest.json'}
  changed={n for n in set(before)|set(after) if before.get(n)!=after.get(n)}
  if not changed<=allowed or set(before)-set(after):raise ValueError('Unrelated inventory mutation')
  if {n for n in set(sd)|set(nextsd) if sd.get(n)!=nextsd.get(n)}-({route} if route else set()):raise ValueError('Unrelated SD mutation')
  d=output/lane;d.mkdir();image,packing=build(after,len(raw))
  if read_image(image,len(raw))!=after:raise ValueError('Store readback differs')
  (d/'store.bin').write_bytes(image);(d/'original-model_viewer.elf').write_bytes(blob)
  for which,entries in (('before',before),('after',after),('before-sd',sd),('after-sd',nextsd)):
   for n,b in entries.items():
    p=d/which/safe(n);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
  if sd:
   with zipfile.ZipFile(d/'sd-apps.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
    for n,b in sorted(nextsd.items()):
     zi=zipfile.ZipInfo(n,date_time=(1980,1,1,0,0,0));zi.compress_type=zipfile.ZIP_DEFLATED;z.writestr(zi,b)
   if unzip(d/'sd-apps.zip')!=nextsd:raise ValueError('SD readback differs')
  if locate(after,nextsd)[0]!=canonical:raise ValueError('Product Viewer bytes differ')
  results[lane]={'original_sha256':sha(blob),'original_version':oldmanifest['version'],'viewer_sha256':sha(canonical),'viewer_bytes':len(canonical),'canvas_sha256':sha(driver),'changed_internal':sorted(changed),'changed_sd':[route] if route else [],'original_policy':row,'new_policy':nextrow,'provider_binding':binding,'provider_manifest':provider_manifest,'packing':packing,'unrelated_files_policies_namespaces_data_unchanged':True}
 receipt={'schema':1,'inputs':INPUTS,'tools':TOOLS,'canonical_sha256':sha(canonical),'canonical_bytes':len(canonical),'full_sidecar_identical':True,'products':results,'target_instructions_executed':False,'hardware_tested':False}
 (output/'receipt.json').write_bytes(encode(receipt));print(json.dumps(receipt,indent=2),flush=True)
if __name__=='__main__':
 p=argparse.ArgumentParser()
 for n in ('inputs','bootfs-tools','viewer','provider','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();run(a.inputs,a.bootfs_tools,a.viewer,a.provider,a.output)
