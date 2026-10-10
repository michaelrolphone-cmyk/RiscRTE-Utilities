#!/usr/bin/env python3
"""Upgrade Contexts and ui.scene in an existing store, preserving saved namespaces.

The product cohort/image builder must validate and bind the resulting stage.
"""
import argparse,hashlib,json,shutil
from pathlib import Path

def encode(x):return (json.dumps(x,indent=2,sort_keys=True)+'\n').encode()
def digest(b):return hashlib.sha256(b).hexdigest()
def package(folder,name):
 m=json.loads((folder/name).read_bytes());r=json.loads((folder/'build.json').read_bytes());b=(folder/m['file_name']).read_bytes()
 if (m['id'],m['version'],len(b),digest(b))!=(r['id'],r['version'],r['size_bytes'],r['sha256']):raise ValueError('Package receipt mismatch')
 return m,r,b

def compose(store,app,scene,output):
 if output.exists():raise ValueError('Choose a new output directory')
 files={p.relative_to(store).as_posix():p.read_bytes() for p in store.rglob('*') if p.is_file()}
 if any(p.is_symlink() for p in store.rglob('*')):raise ValueError('Store links are not allowed')
 original=dict(files);boot=json.loads(files['boot.json']);row=next(r for r in boot['app_capabilities'] if r['manifest']=='contexts.json')
 required={('storage.shared-data',3,'context-fingerprints.cfp'),('storage.shared-data',4,'context-rules.ctx'),('storage.key-value',1,None),('storage.key-value',3,None)}
 if not required<={(r['capability'],r.get('instance_id'),r.get('file')) for r in row['grants']}:raise ValueError('Shared temporal namespaces are missing')
 providers={}
 for r in boot['drivers']:
  m=json.loads(files[r['manifest']])
  for cap in m['provides']:providers[(cap['capability'],cap['api'])]=(r,m)
 if ('time.civil',1) not in providers or ('ui.presentation-profile',1) not in providers:raise ValueError('Install the shared scene/civil providers through the product profile first')
 m,r,b=package(app,'contexts.json');app_receipt=r
 if m['id']!='contexts' or m['version']!='0.4.0':raise ValueError('Expected the corrected Contexts app')
 # Runtime manifest accepts these catalog fields only in the launcher inventory.
 for key in ('category','min_firmware_version','runtime_profile'):m.pop(key,None)
 for need in m['requires']:
  c=need['capability'];v=need['api']
  if c in ('ui.scene','time.civil'):
   if not any(g['capability']==c for g in row['grants']):row['grants'].append(dict(capability=c,api=v,instance_id=0))
  if not any(g['capability']==c and g['api']==v for g in row['grants']):raise ValueError('Missing admitted grant: '+c)
 declared={(r['capability'],r['api']) for r in m['requires']+m.get('optional',[])}
 row['grants']=[g for g in row['grants'] if (g['capability'],g['api']) in declared]
 files['contexts.json']=encode(m);files['contexts.elf']=b
 m,r,b=package(scene,'manifest.json');scene_receipt=r
 if m['id']!='scene-host' or m['version']!='0.3.0':raise ValueError('Expected scene-host 0.3.0')
 selected,old=providers[('ui.scene',1)];name=selected['manifest'];parent=str(Path(name).parent)
 files[name]=encode(m);files[parent+'/'+m['file_name']]=b
 files['boot.json']=encode(boot);files.pop('cohort.json',None)
 for name,b in files.items():p=output/'store'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
 receipt={'schema':'contexts.scene.stage.v1','app':app_receipt,'scene':scene_receipt,'source_boot_sha256':digest(original['boot.json']),'changed_files':sorted(n for n in files if files[n]!=original.get(n)),'provider_count':len(boot['drivers']),'physical_testing':'not performed','flashable':False}
 (output/'stage.json').write_bytes(encode(receipt));return receipt
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('store','app','scene','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(json.dumps(compose(a.store,a.app,a.scene,a.output),indent=2))
