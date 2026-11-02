#!/usr/bin/env python3
"""Canonical Lists packaging from exact published products; no app rewrite."""
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
VERSION='0.1.3'
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

def inspect(inputs,tools,output):
 for n,h in INPUTS.items():
  if file_sha(inputs/n)!=h: raise ValueError('Original input differs: '+n)
 for n,h in TOOLS.items():
  if file_sha(tools/n)!=h: raise ValueError('Bootfs tool differs: '+n)
 sys.path.insert(0,str(tools.resolve()))
 from read_only_spiffs import read_image
 output.mkdir(parents=True,exist_ok=False)
 report={}
 for lane in ('watch','wave','x4'):
  if lane=='x4':
   raw=(inputs/'x4-store.bin').read_bytes();sd=unzip(inputs/'x4-sd.zip')
  else:
   z=unzip(inputs/(lane+'.zip'));raw=z['candidate/store.bin']
   sd=unzip(io.BytesIO(z['candidate/sd-apps.zip'])) if 'candidate/sd-apps.zip' in z else {}
  files=read_image(raw,len(raw));boot=json.loads(files['boot.json'])
  rows=[r for r in boot['app_capabilities'] if 'viewer' in r.get('manifest','')]
  manifests={n:json.loads(b) for n,b in files.items() if n.endswith('.json') and ('viewer' in n or 'profile' in n or n=='boot.json')}
  images={n:{'bytes':len(b),'sha256':sha(b)} for n,b in {**files,**sd}.items() if 'viewer' in n and n.endswith('.elf')}
  report[lane]={'viewer_rows':rows,'images':images,'selected_manifests':manifests,'store_payload_bytes':sum(map(len,files.values())),'store_bytes':len(raw)}
 (output/'inventory.json').write_bytes(encode(report));print(json.dumps(report,indent=2),flush=True)
if __name__=='__main__':
 p=argparse.ArgumentParser()
 for n in ('inputs','bootfs-tools','output'): p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();inspect(a.inputs,a.bootfs_tools,a.output)
