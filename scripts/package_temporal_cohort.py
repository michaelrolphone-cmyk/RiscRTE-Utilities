#!/usr/bin/env python3
"""Assemble and admit a full-initial temporal cohort from explicit build inputs.

No device access or update/migration qualification. The baseline owns every
unmodified byte, including the partition map, bootloader and unrelated drivers.
"""
import argparse, hashlib, json, re, subprocess, sys
from pathlib import Path

def digest(raw): return hashlib.sha256(raw).hexdigest()
def encoded(value): return (json.dumps(value, indent=2, sort_keys=True)+'\n').encode()
def files(root): return {p.relative_to(root).as_posix():p.read_bytes() for p in root.rglob('*') if p.is_file()}
def revision(repo): return subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--target',choices=('watch','x4'),required=True)
 for name in ('baseline','baseline-image','apps','service','iq','firmware','native-elf','runtime','packaging','product','output'):
  p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--home',type=Path);p.add_argument('--version',required=True);p.add_argument('--runtime-version',required=True)
 p.add_argument('--product-revision',help='Published commit with the same verified source tree as the local product')
 a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 sys.path.insert(0,str(a.packaging.resolve()/'scripts'))
 from current_bootfs import build
 from read_only_spiffs import read_image
 from check_runtime_store_admission import admit_cohort
 sys.path.insert(0,str(a.runtime.resolve()/'scripts'))
 from paired_bank_images import initial_otadata,initial_bank_state,parse_record
 baseline=files(a.baseline);store=dict(baseline);image=bytearray(a.baseline_image.read_bytes())
 assert len(image)==0x1000000 and read_image(bytes(image[0x2f0000:0x800000]),0x510000)==baseline
 apps=json.loads((a.apps/'build.json').read_text())
 for name,record in apps.items():
  raw=(a.apps/(name+'.elf')).read_bytes();assert digest(raw)==record['sha256']
  store[name+'.elf']=raw;store[name+'.json']=(a.apps/(name+'.json')).read_bytes()
 if a.home:
  for ext in ('elf','json'):store['default.'+ext]=(a.home/('default.'+ext)).read_bytes()
  apps['default']={'sha256':digest(store['default.elf'])}
 for folder,source in [('contexts',a.service),('s3-radio-iq' if a.target=='watch' else 'iq',a.iq)]:
  for name in ('driver.elf','manifest.json'):store[folder+'/'+name]=(source/name).read_bytes()
 boot=json.loads(store['boot.json']);source_ids=(2,3) if a.target=='watch' else (3,)
 for name in apps:
  manifest=json.loads(store[name+'.json']);policy=next(x for x in boot['app_capabilities'] if x['manifest']==name+'.json')
  if apps[name].get('role')=='default-forwarder':
   assert name=='clock' and not manifest['requires']
   policy['grants']=[]
   continue
  def grant(cap,api,instance,**extra):
   req={'capability':cap,'api':api};g=dict(req,instance_id=instance,**extra)
   if req not in manifest['requires']:manifest['requires'].append(req)
   if g not in policy['grants']:policy['grants'].append(g)
  grant('contexts.service',1,0)
  for instance in source_ids:grant('storage.shared-data',1,instance,file='context-fingerprints.cfp')
  grant('storage.shared-data',1,4,file='context-rules.ctx')
  grant('storage.key-value',1,3)
  assert len(policy['grants'])<=24 and len(manifest['requires'])<=24
  store[name+'.json']=encoded(manifest)
 store['boot.json']=encoded(boot)
 firmware=a.firmware.read_bytes();native=a.native_elf.read_bytes()
 assert 32<=len(firmware)<=0x260000
 for marker in (b'RISC_PAIRED_STORE_ABI:2\0',b'RISC_APP_POLICY_ROWS:24\0',b'RISC_RUNTIME_VERSION:'+a.runtime_version.encode()+b'\0'):
  assert marker in firmware and marker in native,marker
 product_revision=a.product_revision or revision(a.product)
 assert re.fullmatch('[0-9a-f]{40}',product_revision)
 cohort=json.loads(store['cohort.json']);cohort.update(version=a.version,runtime_version=a.runtime_version,firmware_sha256=digest(firmware),firmware_size=len(firmware),source_revision=product_revision)
 store['cohort.json']=encoded(cohort)
 # Admission uses the production Runtime and exact native import profile.
 qualification=admit_cohort(a.runtime,native,store,store,app_policy_rows=24)
 admission={k:qualification[k] for k in ('prepared','hardware_calls','storage_calls')}
 bootfs,geometry=build(store)
 image[0x10000:0x270000]=firmware+b'\xff'*(0x260000-len(firmware))
 image[0x2f0000:0x800000]=bootfs
 image[0xff0000:0xff2000]=initial_otadata()
 state=initial_bank_state(firmware,bootfs,True);parse_record(state[:96],True)
 image[0xff2000:0xff4000]=state
 original=a.baseline_image.read_bytes()
 for lo,hi in ((0,0x10000),(0x270000,0x2f0000),(0x800000,0xff0000),(0xff4000,0x1000000)):assert image[lo:hi]==original[lo:hi]
 changed=sorted(k for k in set(store)|set(baseline) if store.get(k)!=baseline.get(k))
 allowed={'boot.json','cohort.json',*{n+'.'+e for n in apps for e in ('elf','json')},'contexts/driver.elf','contexts/manifest.json',('s3-radio-iq' if a.target=='watch' else 'iq')+'/driver.elf',('s3-radio-iq' if a.target=='watch' else 'iq')+'/manifest.json'}
 assert set(changed)<=allowed
 name=('TWatch-S3' if a.target=='watch' else 'X4')+'-'+a.version+'-Temporal-Contexts-FULL-INITIAL-ERASES-DATA.bin'
 (a.output/name).write_bytes(image);(a.output/'bootfs.bin').write_bytes(bootfs)
 for key,value in store.items():
  dest=a.output/'store'/key;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(value)
 proof=dict(target=a.target,version=a.version,runtime_version=a.runtime_version,firmware_sha256=digest(firmware),native_elf_sha256=digest(native),full_image_sha256=digest(image),full_image_bytes=len(image),baseline_image_sha256=digest(original),changed_store_members=changed,store_admission=admission,cohort_admission=qualification,bootfs=geometry,hardware_verified=False,update_migration_qualified=False,source_revisions={'native':revision(a.runtime),'product':revision(a.product)},files={k:{'sha256':digest(v),'bytes':len(v)} for k,v in sorted(store.items())})
 proof['source_revisions']['published_product']=product_revision
 (a.output/'integration-proof.json').write_bytes(encoded(proof))
 print(name, 'PASS',len(image),digest(image),flush=True)

if __name__=='__main__':main()
