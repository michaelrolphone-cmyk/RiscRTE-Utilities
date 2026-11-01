#!/usr/bin/env python3
"""Private original Watch/Wave LoRa projection; assembly is not lifecycle/device proof."""
import argparse,copy,hashlib,io,json,sys,zipfile,gc
from pathlib import Path,PurePosixPath
from scanner_home_package import load_home_package,validate_home_pair
from lora_product_authority import project_authority
from lora_product_archives import file_sha256,selected_archive
p=argparse.ArgumentParser(description=__doc__)
for n in ('watch-artifact','wave-artifact','watch-rtc','wave-rtc','package','bootfs-tools','watch-home-first','watch-home-repeat','wave-home-first','wave-home-repeat','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args()
for n,v in vars(a).items():setattr(a,n,v.resolve())
sha=lambda b:hashlib.sha256(b).hexdigest()
enc=lambda x:(json.dumps(x,sort_keys=True,indent=2)+'\n').encode()
def require(ok,msg):
 if not ok:raise ValueError(msg)
expected={'watch_artifact':'3a5aadb0eeb5c4b37640d606978162b5c0484893aada661baca4ca39a12d66f2','wave_artifact':'aae8665d9240bd49dbdae89f250525ccea6cabc44215133248ee1a2c7b19d1d6','watch_rtc':'4ab38e133604991f2d09a2739f73dccc5dfa413e94527676795df9667378a671','wave_rtc':'80fa5c1805f21eb97ee844a495bcde1e00c4c53d5eecf89ac204d64dbbaefa1b'}
for name,h in expected.items():require(file_sha256(getattr(a,name))==h,'Published input differs: '+name)
for name,h in {'current_bootfs.py':'6f0997a71fe6d2abbb97ed91e0e4180abf43b2056afbcb5b54d303437791576b','read_only_spiffs.py':'e48d3b2422c60f81fdae6371ff3c5b65c5cbaa9dd250b4a2925c74c0b45f040d'}.items():require(sha((a.bootfs_tools/name).read_bytes())==h,'SPIFFS helper differs: '+name)
sys.path.insert(0,str(a.bootfs_tools));from read_only_spiffs import read_image
from current_bootfs import build
elf=(a.package/'lora_messages.elf').read_bytes();canonical=json.loads((a.package/'lora_messages.json').read_bytes())
require(sha(elf)=='b55ba8b0fa7036c889c81cd32c88e76d17b2d54b790bd10688934f966c3dc46b','Unqualified LoRa ELF')
require(canonical['id']=='lora_messages' and canonical['file_name']=='lora_messages.elf','Wrong LoRa identity')
RETIRED={'display.output','input.touch.raw','input.navigation','board.battery','net.wifi','motion.accel','input.power-key','power.sleep','display.power'}
def archive(raw):
 with zipfile.ZipFile(io.BytesIO(raw)) as z:
  require(len(z.namelist())==len(set(z.namelist())),'Duplicate archive member')
  result={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
 for n in result:
  q=PurePosixPath(n);require(not q.is_absolute() and '..' not in q.parts and '\\' not in n,'Unsafe archive member')
 return result
def route(row,old,new):
 source=row.get('image_source')
 if not source:return
 path=source['path'];q=PurePosixPath(path);require(not q.is_absolute() and '..' not in q.parts,'Unsafe image route')
 if '/image-' in path:
  marker='image-'+old;require(path.count(marker)==1,'Old image digest differs from route');source['path']=path.replace(marker,'image-'+new)
a.output.mkdir(parents=True,exist_ok=False);results={}
for lane in ('watch','wave'):
 artifact=selected_archive(getattr(a,lane+'_artifact'),{'candidate/store.bin'},{'candidate/sd-apps.zip'});raw=artifact.pop('candidate/store.bin');before=read_image(raw,len(raw));after=dict(before)
 sd=archive(artifact.pop('candidate/sd-apps.zip')) if 'candidate/sd-apps.zip' in artifact else {};sdold=dict(sd);del artifact
 boot=json.loads(before['boot.json']);nextboot=copy.deepcopy(boot)
 rows=[x for x in boot['app_capabilities'] if x.get('manifest')=='lora_messages.json'];require(len(rows)==1,'Original LoRa row missing/ambiguous')
 oldrow=rows[0];row=next(x for x in nextboot['app_capabilities'] if x.get('manifest')=='lora_messages.json')
 oldelf=before.get('lora_messages.elf') or sd[oldrow['image_source']['path']]
 selected=copy.deepcopy(canonical);alarms=[g for g in oldrow['grants'] if g['capability']=='alarm.service']
 require(len(alarms)<=1,'Ambiguous original alarm ABI')
 if alarms:
  require(alarms[0]['api'] in (1,2),'Unsupported original alarm ABI')
  next(d for d in selected['optional'] if d['capability']=='alarm.service')['api']=alarms[0]['api']
 selected,retained,legacy_optional=project_authority(selected,oldrow['grants'],RETIRED)
 for ns in (1,9):require({'capability':'storage.key-value','api':1,'instance_id':ns} in retained,'Original LoRa KV namespace differs: '+str(ns))
 # project_authority preserves validated radio presence or absence exactly.
 bindings={}
 for cap in ('ui.scene','ui.text-input','time.civil'):
  matches=[]
  for d in boot['drivers']:
   m=json.loads(before[d['manifest']])
   if {'capability':cap,'api':1} in m['provides']:matches.append((d,m))
  require(len(matches)==1,'Need an explicit qualified provider input for '+cap+'; no grant-only fallback')
  d,m=matches[0];g={'capability':cap,'api':1,'instance_id':d.get('instance_id',0)}
  existing=[x for x in retained if x['capability']==cap];require(not existing or existing==[g],'Existing service instance differs: '+cap)
  if not existing:retained.append(g)
  bindings[cap]={'grant':g,'driver':copy.deepcopy(d),'manifest':copy.deepcopy(m),'elf_sha256':sha(before.get(str(PurePosixPath(d['manifest']).parent/m['file_name'])) or sd[d['image_source']['path']])}
 require({'capability':'rtc.clock','api':2} in bindings['time.civil']['manifest']['requires'],'Original civil clock domain requires explicit review')
 keys=[(g['capability'],g['api'],g['instance_id'],g.get('file')) for g in retained];require(len(keys)==len(set(keys)),'Duplicate grant')
 row['grants']=retained;route(row,sha(oldelf),sha(elf))
 if 'lora_messages.elf' in before:after['lora_messages.elf']=elf
 if row.get('image_source'):sd[row['image_source']['path']]=elf
 require('lora_messages.elf' in before or row.get('image_source'),'No selected LoRa route')
 after['lora_messages.json']=enc(selected)
 # Use the exact owner-approved Home pair; the validator preserves baseline authority.
 homeelf,homemanifest,homeproof=validate_home_pair(load_home_package(getattr(a,lane+'_home_first')),load_home_package(getattr(a,lane+'_home_repeat')),before['default.json'],lane)
 after['default.elf']=homeelf;after['default.json']=homemanifest
 homerow=next(x for x in nextboot['app_capabilities'] if x['manifest']=='default.json');route(homerow,sha(before['default.elf']),sha(homeelf))
 if homerow.get('image_source'):sd[homerow['image_source']['path']]=homeelf
 # Exact published UTC8 RTC correction used by the selected civil clock domain.
 rtc=selected_archive(getattr(a,lane+'_rtc'),{'rtc-utc/package/driver.elf','rtc-utc/repeat/driver.elf','rtc-utc/receipt.json','rtc-utc/package/manifest.json'});rtcelf=rtc['rtc-utc/package/driver.elf'];require(rtcelf==rtc['rtc-utc/repeat/driver.elf'],'RTC repeat differs')
 rec=json.loads(rtc['rtc-utc/receipt.json']);require('-DRTC_UTC_OFFSET_SECONDS=28800' in rec['builds'][0]['defines'] and sha(rtcelf)==rec['builds'][0]['sha256'],'RTC clock domain/receipt differs')
 rm=json.loads(rtc['rtc-utc/package/manifest.json']);oldrm=json.loads(before['rtc/manifest.json']);require({k:v for k,v in rm.items() if k!='version'}=={k:v for k,v in oldrm.items() if k!='version'},'RTC authority changed')
 after['rtc/driver.elf']=rtcelf;after['rtc/manifest.json']=enc(rm)
 rtcrow=next(x for x in nextboot['drivers'] if x['manifest']=='rtc/manifest.json');route(rtcrow,sha(before['rtc/driver.elf']),sha(rtcelf))
 if rtcrow.get('image_source'):sd[rtcrow['image_source']['path']]=rtcelf
 # Undo only declared projection fields, proving every other policy byte value unchanged.
 check=copy.deepcopy(nextboot)
 for manifest in ('lora_messages.json','default.json'):
  idx=next(i for i,x in enumerate(boot['app_capabilities']) if x['manifest']==manifest);check['app_capabilities'][idx]=copy.deepcopy(boot['app_capabilities'][idx])
 idx=next(i for i,x in enumerate(boot['drivers']) if x['manifest']=='rtc/manifest.json');check['drivers'][idx]=copy.deepcopy(boot['drivers'][idx]);require(check==boot,'Unrelated authority changed')
 after['boot.json']=enc(nextboot);allowed={'boot.json','lora_messages.elf','lora_messages.json','default.elf','default.json','rtc/driver.elf','rtc/manifest.json'}
 require(before.keys()==after.keys(),'Unexpected store members');changed={n for n in before if before[n]!=after[n]};require(changed<=allowed,'Unrelated payload changed')
 allowed_sd={x['image_source']['path'] for x in (oldrow,next(x for x in boot['app_capabilities'] if x['manifest']=='default.json'),next(x for x in boot['drivers'] if x['manifest']=='rtc/manifest.json')) if x.get('image_source')}
 require({n for n in sdold if sd.get(n)!=sdold[n]}<=allowed_sd,'Unrelated SD payload changed')
 image,receipt=build(after,len(raw));require(read_image(image,len(raw))==after,'Store roundtrip differs');out=a.output/lane;out.mkdir()
 for label,files in [('before',before),('after',after)]:
  for n,b in files.items():dest=out/label/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(b)
 (out/'store.bin').write_bytes(image)
 if sd:
  with zipfile.ZipFile(out/'sd-apps.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
   for n,b in sorted(sd.items()):z.writestr(n,b)
  require(archive((out/'sd-apps.zip').read_bytes())==sd,'SD roundtrip differs')
 actual=after.get('lora_messages.elf') or sd[row['image_source']['path']];require(actual==elf,'Actual selected ELF differs')
 if row.get('image_source'):require(sd[row['image_source']['path']]==elf,'SD LoRa differs')
 results[lane]={'store':receipt,'changed_internal':sorted(changed),'unchanged_internal':len(before)-len(changed),'preserved_grants':[g for g in oldrow['grants'] if g['capability'] not in RETIRED],'retired_grants':[g for g in oldrow['grants'] if g['capability'] in RETIRED],'new_row':row,'preserved_legacy_optional_declarations':legacy_optional,'bindings':bindings,'home':homeproof,'original_sd_members':len(sdold),'sd_unchanged':sum(sd[n]==b for n,b in sdold.items()),'sd_added':sorted(set(sd)-set(sdold)),'app_sha256':sha(actual),'all_other_authority_unchanged':True,'full_home_lifecycle_qualification':'pending separate execution','hardware_tested':False}
 (a.output/'assembly.json').write_bytes(enc(results));print(lane,'assembly passed; full profile qualification remains separate',flush=True)
 # Release lane-owned image/archive dictionaries before opening the next artifact.
 del raw,before,after,files,b,sd,sdold,image,rtc,rtcelf,homeelf,homemanifest,oldelf,actual
 gc.collect()
