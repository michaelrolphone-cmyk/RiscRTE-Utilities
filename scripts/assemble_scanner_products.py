#!/usr/bin/env python3
"""Rebuild three private Scanner stores from exact public artifact inputs.
No firmware, settings, boot partition or external publication operation.
"""
import sys,json,zipfile,pathlib,hashlib,io,copy,argparse
p=argparse.ArgumentParser(description=__doc__)
for name in ('watch-artifact','wave-artifact','x4-store','x4-sd','watch-rtc','wave-rtc','package','bootfs-tools','watch-home-first','watch-home-repeat','wave-home-first','wave-home-repeat','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
args=p.parse_args()
expected={
'watch_artifact':'3a5aadb0eeb5c4b37640d606978162b5c0484893aada661baca4ca39a12d66f2',
'wave_artifact':'aae8665d9240bd49dbdae89f250525ccea6cabc44215133248ee1a2c7b19d1d6',
'x4_store':'8d38a77bdb3dd1439e8736d9c0363798f20803e02b44fc1aa5dab578207c4adc',
'x4_sd':'2b6dce3efb468f3cd728b50707e0c9552d84e225b65e2b27e683d3d726bc6cef',
'watch_rtc':'4ab38e133604991f2d09a2739f73dccc5dfa413e94527676795df9667378a671',
'wave_rtc':'80fa5c1805f21eb97ee844a495bcde1e00c4c53d5eecf89ac204d64dbbaefa1b'}
for name,h in expected.items():
 if hashlib.sha256(getattr(args,name).read_bytes()).hexdigest()!=h:raise ValueError('Published input differs: '+name)
for name,h in {'current_bootfs.py': '6f0997a71fe6d2abbb97ed91e0e4180abf43b2056afbcb5b54d303437791576b', 'read_only_spiffs.py': 'e48d3b2422c60f81fdae6371ff3c5b65c5cbaa9dd250b4a2925c74c0b45f040d'}.items():
 if hashlib.sha256((args.bootfs_tools/name).read_bytes()).hexdigest()!=h:raise ValueError('Published SPIFFS helper differs: '+name)
sys.path.insert(0,str(args.bootfs_tools))
from scanner_product_projection import project_scanner_boot,update_scanner_route,project_scanner_manifest
from scanner_home_package import validate_home_pair,load_home_package
from read_only_spiffs import read_image
from current_bootfs import build
OUT=args.output;OUT.mkdir(parents=True,exist_ok=False)
sha=lambda b:hashlib.sha256(b).hexdigest()
enc=lambda j:(json.dumps(j,sort_keys=True,indent=2)+'\n').encode()
app=(args.package/'ble_scanner.elf').read_bytes();manifest=(args.package/'ble_scanner.json').read_bytes();canonical=json.loads(manifest)
homes={lane:(load_home_package(getattr(args,lane+'_home_first')),load_home_package(getattr(args,lane+'_home_repeat'))) for lane in ('watch','wave')}
results={}
for lane in ['watch','wave','x4']:
 if lane=='x4':
  raw=args.x4_store.read_bytes();sdraw=args.x4_sd.read_bytes()
 else:
  z=zipfile.ZipFile(args.watch_artifact if lane=='watch' else args.wave_artifact);raw=z.read('candidate/store.bin');sdraw=z.read('candidate/sd-apps.zip') if lane=='wave' else None
 before=read_image(raw,len(raw));after=dict(before);boot=json.loads(before['boot.json']);oldrow=next(x for x in boot['app_capabilities'] if x['manifest']=='ble_scanner.json')
 sd={}
 if sdraw:
  with zipfile.ZipFile(io.BytesIO(sdraw)) as z:sd={n:z.read(n) for n in z.namelist() if not n.endswith('/')}
 sdold=dict(sd)
 selected_manifest=project_scanner_manifest(canonical,oldrow)
 nextboot,grantreceipt=project_scanner_boot(boot,oldrow,selected_manifest,preferences=lane=='watch');row=next(x for x in nextboot['app_capabilities'] if x['manifest']=='ble_scanner.json')
 nextboot=update_scanner_route(nextboot,row,sha(before.get('ble_scanner.elf') or sd[oldrow['image_source']['path']]),sha(app));row=next(x for x in nextboot['app_capabilities'] if x['manifest']=='ble_scanner.json')
 if 'ble_scanner.elf' in after:after['ble_scanner.elf']=app
 after['ble_scanner.json']=enc(selected_manifest)
 if row.get('image_source'):sd[row['image_source']['path']]=app
 homereceipt=None
 if lane!='x4':
  homeelf,homemanifest,homereceipt=validate_home_pair(*homes[lane],before['default.json'],lane)
  after['default.elf']=homeelf;after['default.json']=homemanifest
 newscene=before['ui-scene/driver.elf'] # Existing selected provider remains unchanged.
 if lane!='x4':
  rtc=zipfile.ZipFile(args.watch_rtc if lane=='watch' else args.wave_rtc);elf=rtc.read('rtc-utc/package/driver.elf');assert elf==rtc.read('rtc-utc/repeat/driver.elf');rec=json.loads(rtc.read('rtc-utc/receipt.json'));assert '-DRTC_UTC_OFFSET_SECONDS=28800' in rec['builds'][0]['defines'];assert sha(elf)==rec['builds'][0]['sha256']
  m=json.loads(rtc.read('rtc-utc/package/manifest.json'));oldm=json.loads(before['rtc/manifest.json']);assert {k:v for k,v in m.items() if k!='version'}=={k:v for k,v in oldm.items() if k!='version'}
  after['rtc/driver.elf']=elf;after['rtc/manifest.json']=enc(m)
 after['boot.json']=enc(nextboot)
 allowed={'boot.json','ble_scanner.elf','ble_scanner.json'}|({'rtc/driver.elf','rtc/manifest.json','default.elf','default.json'} if lane!='x4' else set())
 if lane=='x4':assert after['default.elf']==before['default.elf'] and after['default.json']==before['default.json']
 changed={k for k in before if before[k]!=after[k]};assert changed<=allowed and before.keys()==after.keys()
 image,receipt=build(after,len(raw));assert read_image(image,len(raw))==after
 out=OUT/lane;out.mkdir(exist_ok=True)
 for name,store in [('before',before),('after',after)]:
  for n,b in store.items():p=out/name/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
 (out/'store.bin').write_bytes(image)
 if sdraw:
  with zipfile.ZipFile(out/'sd-apps.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
   for n,b in sorted(sd.items()):z.writestr(n,b)
  with zipfile.ZipFile(out/'sd-apps.zip') as z:assert {n:z.read(n) for n in z.namelist()}==sd
  changed_sd={n for n in sdold if sdold[n]!=sd.get(n)};assert changed_sd<=({oldrow['image_source']['path']} if lane=='wave' else set())
  for n,b in sd.items():p=out/'sd'/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
 results[lane]={'baseline_store_sha256':sha(raw),'store':receipt,'changed_files':sorted(changed),'unchanged_internal':len(before)-len(changed),'driver_count':len(boot['drivers']),'app_policy_count':len(boot['app_capabilities']),'grants':grantreceipt,'sd_original_count':len(sdold),'sd_unchanged':sum(sd.get(n)==b for n,b in sdold.items()),'sd_added':sorted(set(sd)-set(sdold)),'home':homereceipt,'app_sha256':sha(app),'scene_sha256':sha(newscene),'all_other_authority_unchanged':True}
 print(lane,receipt['sha256'],len(before),flush=True)
(OUT/'assembly.json').write_bytes(enc(results))
