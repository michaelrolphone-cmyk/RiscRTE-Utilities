#!/usr/bin/env python3
"""Production OBD-only overlay; preserve OBDTest and every unrelated byte."""
import argparse,copy,json,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for n in ('system','baseline','application','bootfs-tools','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args()
sys.path[:0]=[str(a.system.resolve()/'scripts'),str(a.bootfs_tools.resolve())]
from compose_shared_app_cohort import encode,digest,safe,write_tree
from assemble_shared_home_cohort import inventory,fingerprint,pack_candidate,providers
from assemble_shared_usb_cohort import decode_store,storage_record
from image_envelope import encode_image
from current_bootfs import build
from read_only_spiffs import read_image
lock=json.loads((ROOT/'docs/shared-obd/baseline.json').read_text())
for asset in lock['assets']:
 raw=(a.baseline/asset['name']).read_bytes()
 assert len(raw)==asset['bytes'] and digest(raw)==asset['sha256']
elf=(a.application/'obd_dashboard.elf').read_bytes()
manifest=(a.application/'obd_dashboard.json').read_bytes();common=json.loads(manifest)
proof=json.loads((a.application/'build.json').read_text())
assert proof['sha256']==digest(elf) and common['id']=='obd-dashboard'
assert common['requires']==[{'capability':'ui.scene','api':1}]
assert common['optional']==[{'capability':'bluetooth.serial','api':1},{'capability':'storage.volume','api':1}]
assert proof['exports']==['app_main','risc_resident_app_descriptor_v1']
assert not a.output.exists();a.output.mkdir(parents=True);results={}
for lane in ('watch','wave','x4'):
 raw=(a.baseline/f'product-{lane}-store.bin').read_bytes()
 stored=read_image(raw,len(raw));before=decode_store(stored)
 with zipfile.ZipFile(a.baseline/f'product-{lane}-sd-apps.zip') as z:
  assert len(z.namelist())==len(set(z.namelist()))
  sd={safe(n):z.read(n) for n in z.namelist()}
 out=dict(before);packed=dict(stored);after_sd=dict(sd)
 boot=json.loads(before['boot.json']);original_boot=copy.deepcopy(boot);available=providers(before,boot)
 assert ('ui.scene',1,0) in available
 rows=[r for r in boot['app_capabilities'] if r['manifest']=='obd_dashboard.json']
 if lane=='x4':
  assert len(rows)==1
  row=copy.deepcopy(rows[0]);old=json.loads(before['obd_dashboard.json'])
  assert old['id']=='obd-dashboard' and old['version']=='0.2.2'
  assert row['image_source']=={'path':'Apps/obd-dashboard/obd_dashboard.elf','volume_instance':9}
  assert digest(sd[row['image_source']['path']])=='ecf5c2b7f61bb7de2091c7a2b37c434604dbb640ada51111b8ae9ec19d4e977c'
  assert digest(sd['Apps/obd-test/obd_test.elf'])=='9acdf0593b4b3131f4341dbece0e2b178412df16c94abf17bc4657bfd852ec0e'
  new=copy.deepcopy(old)
  for key in ('version','requires','optional'):new[key]=copy.deepcopy(common[key])
  after_sd[row['image_source']['path']]=elf
  after_sd['Apps/obd-dashboard/manifest.json']=encode(new)
 else:
  assert not rows and 'obd_dashboard.json' not in before and 'obd_dashboard.elf' not in before
  new=copy.deepcopy(common)
  row={'manifest':'obd_dashboard.json','grants':[{'capability':'ui.scene','api':1,'instance_id':0}]}
  for decl in common['optional']:
   matches=[k for k in available if k[:2]==(decl['capability'],decl['api'])]
   if len(matches)>1 and decl['capability']=='storage.volume':
    external={r['image_source']['volume_instance'] for r in boot['app_capabilities'] if 'image_source' in r}
    matches=[k for k in matches if k[2] in external]
   assert len(matches)<=1,('Ambiguous optional provider',lane,matches)
   if matches:row['grants'].append({'capability':matches[0][0],'api':matches[0][1],'instance_id':matches[0][2]})
  boot['app_capabilities'].append(row)
  out['obd_dashboard.elf']=elf;packed['obd_dashboard.elf']=elf
 for g in row['grants']:assert (g['capability'],g['api'],g['instance_id']) in available
 if 'resident_shell' in boot:
  rs=boot['resident_shell'];assert isinstance(rs.get('foreground'),list)
  if 'obd_dashboard.elf' not in rs['foreground']:rs['foreground'].append('obd_dashboard.elf')
 assert {k:v for k,v in boot.items() if k not in ('app_capabilities','resident_shell')}=={k:v for k,v in original_boot.items() if k not in ('app_capabilities','resident_shell')}
 assert [r for r in boot['app_capabilities'] if r['manifest']!='obd_dashboard.json']==[r for r in original_boot['app_capabilities'] if r['manifest']!='obd_dashboard.json']
 out['boot.json']=encode(boot);out['obd_dashboard.json']=encode(new)
 packed['boot.json']=out['boot.json'];packed['obd_dashboard.json']=out['obd_dashboard.json']
 allowed={'boot.json','obd_dashboard.json'}|({'obd_dashboard.elf'} if lane!='x4' else set())
 sd_allowed={'Apps/obd-dashboard/obd_dashboard.elf','Apps/obd-dashboard/manifest.json'} if lane=='x4' else set()
 assert not(set(before)-set(out)) and not(set(sd)-set(after_sd))
 assert all(out[n]==b for n,b in before.items() if n not in allowed)
 assert all(after_sd[n]==b for n,b in sd.items() if n not in sd_allowed)
 representation_changes=[]
 def fit():
  try:build(packed,len(raw));return True
  except Exception as error:
   if type(error).__name__=='SpiffsFullError':return False
   if isinstance(error,ValueError) and str(error)=='Current bootfs lacks SPIFFS free-block reserve':return False
   raise
 if not fit():
  assert lane!='x4','No unqualified decoder or geometry change'
  packed['obd_dashboard.elf']=encode_image(elf)
  if not fit():
   driver_images={json.loads(out[r['manifest']])['file_name'] for r in boot.get('drivers',[])}
   choices=[]
   for app in boot['app_capabilities']:
    if 'image_source' in app:continue
    m=json.loads(out[app['manifest']]);name=safe(str(Path(app['manifest']).parent/m['file_name']))
    if m.get('type')!='application' or m.get('provides') or name in driver_images:continue
    if name not in out or packed[name]!=out[name] or not out[name].startswith(b'\x7fELF'):continue
    encoded=encode_image(out[name]);saving=len(out[name])-len(encoded)
    if saving>0:choices.append((-saving,name,encoded))
   for _,name,encoded in sorted(choices):
    packed[name]=encoded;representation_changes.append(name)
    if fit():break
   else:raise ValueError('Ordinary app encoding cannot preserve fixed SPIFFS reserve')
 assert all(packed[n]==data for n,data in stored.items() if n not in allowed and n not in representation_changes)
 assert decode_store(packed)==out
 encoding=storage_record(out,packed)
 if encoding:encoding['selection']='Preserve existing envelopes; OBD first, then descending positive savings among raw ordinary internal applications; stop at first fixed-size pack preserving reserve'
 path=a.output/lane
 for name,entries in [('before',before),('before-stored',stored),('before-sd',sd),('after',out),('stored',packed),('after-sd',after_sd)]:write_tree(path/name,entries)
 selected=after_sd[row['image_source']['path']] if lane=='x4' else decode_store(packed)['obd_dashboard.elf']
 assert selected==elf
 result={'inventory_sha256':fingerprint(inventory(before,sd)),'shared_elf_sha256':digest(selected),'installed_manifest_sha256':digest(out['obd_dashboard.json']),
  'baseline_store_sha256':digest(raw),'image_storage':encoding,'changed_internal':[n for n in sorted(out) if out[n]!=before.get(n)],
  'changed_stored':[n for n in sorted(packed) if packed[n]!=stored.get(n)],'changed_sd':[n for n in sorted(after_sd) if after_sd[n]!=sd.get(n)],
  'unrelated_canonical_bytes_preserved':True,'unrelated_stored_bytes_preserved':not bool(representation_changes),
  'ordinary_application_representation_changes':representation_changes,'unrelated_sd_bytes_preserved':True,'test_identity_preserved':True,
  'provider_and_saved_data_ownership_preserved':True,'installed_policy':row,'resident_policy':boot.get('resident_shell')}
 result.update(pack_candidate(path,packed,after_sd,len(raw),build,read_image));results[lane]=result
 print('OBD PRODUCT '+lane+' '+json.dumps(result),flush=True)
(a.output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'packed':True,'shared_elf_sha256':digest(elf),'baseline_tag':lock['tag'],'physical_testing':False,'scope':'Production OBD only; OBDTest, Home, Contexts, Bringup and unpublished-change exclusion preserved'}))
