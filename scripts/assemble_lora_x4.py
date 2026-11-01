#!/usr/bin/env python3
"""Add one private LoRa install to the exact original X4 product; no deployment."""
import argparse,copy,hashlib,json,sys,zipfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for n in ('store','sd','package','bootfs-tools','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
sha=lambda b:hashlib.sha256(b).hexdigest()
enc=lambda v:(json.dumps(v,sort_keys=True,indent=2)+'\n').encode()
raw=a.store.read_bytes();sd_raw=a.sd.read_bytes()
assert sha(raw)=='8d38a77bdb3dd1439e8736d9c0363798f20803e02b44fc1aa5dab578207c4adc'
assert sha(sd_raw)=='2b6dce3efb468f3cd728b50707e0c9552d84e225b65e2b27e683d3d726bc6cef'
for n,h in {'current_bootfs.py':'6f0997a71fe6d2abbb97ed91e0e4180abf43b2056afbcb5b54d303437791576b','read_only_spiffs.py':'e48d3b2422c60f81fdae6371ff3c5b65c5cbaa9dd250b4a2925c74c0b45f040d'}.items():assert sha((a.bootfs_tools/n).read_bytes())==h
sys.path.insert(0,str(a.bootfs_tools));from read_only_spiffs import read_image
from current_bootfs import build
before=read_image(raw,len(raw));after=dict(before);boot=json.loads(before['boot.json']);nextboot=copy.deepcopy(boot)
assert not any('lora' in x['manifest'] for x in boot['app_capabilities'])
assert not any(g['capability']=='storage.key-value' and g['instance_id']==9 for x in boot['app_capabilities'] for g in x['grants'])
assert not any(k['namespace']==9 for x in boot['drivers'] for k in x.get('key_value',[]))
# Select existing service instances from decoded product manifests, never invent providers.
selected={}
for d in boot['drivers']:
 m=json.loads(before[d['manifest']])
 for cap in m['provides']:
  if cap['capability'] in ('ui.scene','ui.text-input','time.civil','alarm.service','radio.lora'):
   assert cap['capability'] not in selected
   selected[cap['capability']]={'capability':cap['capability'],'api':cap['api'],'instance_id':d.get('instance_id',0)}
assert all(k in selected for k in ('ui.scene','ui.text-input','time.civil','alarm.service'))
assert 'radio.lora' not in selected # This original X4 product contains no LoRa radio provider.
elf=(a.package/'lora_messages.elf').read_bytes();manifest=(a.package/'lora_messages.json').read_bytes();m=json.loads(manifest)
assert sha(elf)=='b55ba8b0fa7036c889c81cd32c88e76d17b2d54b790bd10688934f966c3dc46b'
assert m['id']=='lora_messages' and m['file_name']=='lora_messages.elf'
grants=[selected[k] for k in ('ui.scene','ui.text-input','time.civil','alarm.service')]+[{'capability':'storage.key-value','api':1,'instance_id':n} for n in (1,9)]
declared={(v['capability'],v['api']) for v in m['optional']+m['requires']};assert all((g['capability'],g['api']) in declared for g in grants)
route='Apps/lora_messages/image-'+sha(elf)+'/lora_messages.elf'
row={'manifest':'lora_messages.json','grants':grants,'image_source':{'path':route,'volume_instance':9}}
nextboot['app_capabilities'].append(row);nextboot['resident_shell']['foreground'].append('lora_messages.elf')
check=copy.deepcopy(nextboot);assert check['app_capabilities'].pop()==row;assert check['resident_shell']['foreground'].pop()=='lora_messages.elf';assert check==boot
assert 'lora_messages.json' not in before;after['lora_messages.json']=manifest;after['boot.json']=enc(nextboot)
assert all(after[n]==b for n,b in before.items() if n!='boot.json')
assert after['default.elf']==before['default.elf'] and after['default.json']==before['default.json']
with zipfile.ZipFile(a.sd) as z:
 assert len(z.namelist())==len(set(z.namelist()))
 sd={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
assert route not in sd;original_sd=dict(sd);sd[route]=elf
image,receipt=build(after,len(raw));assert read_image(image,len(raw))==after
out=a.output/'x4';out.mkdir()
for label,files in [('before',before),('after',after)]:
 for name,data in files.items():dest=out/label/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
(out/'store.bin').write_bytes(image)
with zipfile.ZipFile(out/'sd-apps.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
 for name,data in sorted(sd.items()):z.writestr(name,data)
with zipfile.ZipFile(out/'sd-apps.zip') as z:
 assert {n:z.read(n) for n in z.namelist()}==sd
 assert z.read(route)==elf
assert all(sd[n]==b for n,b in original_sd.items())
result={'scope':__doc__,'baseline_store_sha256':sha(raw),'baseline_sd_sha256':sha(sd_raw),'store':receipt,'original_internal_files':len(before),'original_internal_bytes_unchanged_except_boot':True,'added_internal':['lora_messages.json'],'new_install_row':row,'existing_authority_unchanged':True,'original_sd_members':len(original_sd),'all_original_sd_members_byte_identical':True,'added_sd':[route],'canonical_elf_sha256':sha(elf),'home_unchanged':{'elf':sha(before['default.elf']),'manifest':sha(before['default.json'])},'civil_binding':selected['time.civil'],'radio_available':False,'source_sha256':sha(Path(__file__).read_bytes()),'physical_testing':False,'deployment':False}
(a.output/'assembly.json').write_bytes(enc(result));print(json.dumps(result))
