#!/usr/bin/env python3
"""Private Buttons store composition from pinned published product inputs.

No firmware/boot-partition/release/feed operations. Preserves all unrelated
stored bytes, policies, namespaces, provider counts, and SD members. A supplied
scene package replaces only the existing scene provider, preserving its graph.
"""
import argparse, copy, hashlib, io, json, pathlib, sys, zipfile
from buttons_product_projection import project_buttons_boot, project_buttons_manifest, update_buttons_route

def sha(data): return hashlib.sha256(data).hexdigest()
def encode(value): return (json.dumps(value,sort_keys=True,indent=2)+'\n').encode()
def require(value,message):
    if not value: raise ValueError(message)
PINS={
 'idle_home':'6ab2384bf5a75c928f5656268d442a10c717965486c7aecf308bb3d87b53490a',
 'watch_artifact':'3a5aadb0eeb5c4b37640d606978162b5c0484893aada661baca4ca39a12d66f2',
 'wave_artifact':'aae8665d9240bd49dbdae89f250525ccea6cabc44215133248ee1a2c7b19d1d6',
 'x4_store':'8d38a77bdb3dd1439e8736d9c0363798f20803e02b44fc1aa5dab578207c4adc',
 'x4_sd':'2b6dce3efb468f3cd728b50707e0c9552d84e225b65e2b27e683d3d726bc6cef',
 'watch_rtc':'4ab38e133604991f2d09a2739f73dccc5dfa413e94527676795df9667378a671',
 'wave_rtc':'80fa5c1805f21eb97ee844a495bcde1e00c4c53d5eecf89ac204d64dbbaefa1b'}
SCENE_PINS={'watch':'cddbc3960dee8e8bcff1c92387d8dc773fee83f47dda91e2f690dad11a9bf91f','wave':'fa454876ccd71edd128359717aed4b13100db4a466fdeab0e81887977e423b36','x4':'38e9766cf359ebb4321d0b6f9771cae8324e32fe6fd474a5caea452d4a4a9930'}
TOOL_PINS={'current_bootfs.py':'6f0997a71fe6d2abbb97ed91e0e4180abf43b2056afbcb5b54d303437791576b','read_only_spiffs.py':'e48d3b2422c60f81fdae6371ff3c5b65c5cbaa9dd250b4a2925c74c0b45f040d'}
def unzip(data):
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        names=[x for x in z.namelist() if not x.endswith('/')]
        require(len(names)==len(set(names)),'Duplicate ZIP member')
        require(all(not n.startswith('/') and '\\' not in n and all(p not in ('','..','.') for p in n.split('/')) for n in names),'Unsafe ZIP member')
        return {n:z.read(n) for n in names}
def write_files(root,files):
    for name,data in files.items():
        require(not name.startswith('/') and all(p not in ('','..','.') for p in name.split('/')),'Unsafe store path')
        p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
def same_provider(old,new):
    require({k:v for k,v in old.items() if k!='version'}=={k:v for k,v in new.items() if k!='version'},'Provider graph/ABI/identity change')
HOME_PINS={'watch':('0.24.1','488c915da997573a2bb862c909e290e6290cc4a4c0c1cc15a47f4badffb54243'),'wave':('0.24.2','09a3e46e225737e660d6f9898509f9baadb0352c126246bc6199ecfadd646fda')}
def validate_idle_home(old,new,row,lane):
    require(lane in HOME_PINS,'No original idle host authority for this product')
    require(new.get('id')=='twatch-clock' and new.get('file_name')=='default.elf','Unexpected Home identity')
    require(new.get('version')==HOME_PINS[lane][0],'Home lacks qualified idle-only version')
    same_provider(old,new)
    require(row.get('manifest')=='default.json' and not row.get('image_source'),'Unexpected Home route')
    require(any(g.get('capability')=='power.sleep' for g in row.get('grants',[])),'Original Home sleep grant missing')
def install_idle_home(before,after,boot,lane,archive):
    with zipfile.ZipFile(archive) as z:
        elf=z.read(lane+'-first/default.elf')
        require(elf==z.read(lane+'-repeat/default.elf') and sha(elf)==HOME_PINS[lane][1],'Qualified idle Home repeat/digest differs')
        data=z.read(lane+'-first/default.json');require(data==z.read(lane+'-repeat/default.json'),'Home manifest repeat differs')
        manifest=json.loads(data)
    row=next(x for x in boot['app_capabilities'] if x['manifest']=='default.json')
    validate_idle_home(json.loads(before['default.json']),manifest,row,lane)
    after['default.elf']=elf;after['default.json']=data
    return {'source':'b58a67cd926add5f9effc83ed07bdd337076e927','version':manifest['version'],'sha256':sha(elf),'policy_unchanged':True}
def assemble(a):
    inputs={}
    for name,h in PINS.items():
        path=getattr(a,name)
        if path is None:
            raise ValueError('Missing pinned input: '+name)
        actual=sha(path.read_bytes());require(actual==h,'Published input differs: '+name);inputs[name]={'path':str(path.resolve()),'sha256':actual}
    for name,h in TOOL_PINS.items():require(sha((a.bootfs_tools/name).read_bytes())==h,'SPIFFS helper differs: '+name)
    generator=a.bootfs_tools.parent/'vendor/esp-idf-spiffs/spiffsgen.py'
    require(sha(generator.read_bytes())=='5779792a4d98a12383233267511735769a201b5c2c79b582186a40c22b7c6466','SPIFFS generator differs')
    sys.path.insert(0,str(a.bootfs_tools.resolve()))
    from read_only_spiffs import read_image
    from current_bootfs import build
    app=(a.package/'ble_buttons.elf').read_bytes();require(sha(app)==a.expected_app_sha256,'Final app digest differs')
    manifest_bytes=(a.package/'ble_buttons.json').read_bytes();require(sha(manifest_bytes)==a.expected_manifest_sha256,'Final manifest digest differs')
    canonical=json.loads(manifest_bytes)
    require(not a.output.exists(),'Use a fresh composition directory')
    a.output.mkdir(parents=True);results={}
    for lane in ('watch','wave','x4'):
        if lane=='x4':raw=a.x4_store.read_bytes();sd_raw=a.x4_sd.read_bytes()
        else:
            artifact=unzip(getattr(a,lane+'_artifact').read_bytes());raw=artifact['candidate/store.bin'];sd_raw=artifact.get('candidate/sd-apps.zip')
        before=read_image(raw,len(raw));after=dict(before);boot=json.loads(before['boot.json']);oldrow=next(x for x in boot['app_capabilities'] if x['manifest']=='ble_buttons.json')
        sd=unzip(sd_raw) if sd_raw else {};sd_before=dict(sd)
        selected=project_buttons_manifest(canonical,oldrow,json.loads(before['ble_buttons.json']))
        nextboot,grant_receipt=project_buttons_boot(boot,oldrow,selected)
        row=next(x for x in nextboot['app_capabilities'] if x['manifest']=='ble_buttons.json')
        oldapp=before.get('ble_buttons.elf') or sd[oldrow['image_source']['path']]
        nextboot=update_buttons_route(nextboot,row,sha(oldapp),sha(app));row=next(x for x in nextboot['app_capabilities'] if x['manifest']=='ble_buttons.json')
        if 'ble_buttons.elf' in after:after['ble_buttons.elf']=app
        after['ble_buttons.json']=encode(selected)
        if row.get('image_source'):
            sd[row['image_source']['path']]=app
            sd_manifest=str(pathlib.PurePosixPath(oldrow['image_source']['path']).parent/'manifest.json')
            require(sd_manifest in sd_before,'Original SD app manifest missing')
            require(json.loads(sd_before[sd_manifest]).get('id')=='ble_buttons','SD app manifest identity differs')
            sd[sd_manifest]=encode(selected)
        package=getattr(a,lane+'_scene');scene=(package/'driver.elf').read_bytes();manifest=json.loads((package/'manifest.json').read_text())
        require(sha(scene)==SCENE_PINS[lane],'Qualified scene profile bytes differ')
        same_provider(json.loads(before['ui-scene/manifest.json']),manifest)
        require(manifest['version']=='0.4.14','Unexpected held-input scene version')
        after['ui-scene/driver.elf']=scene;after['ui-scene/manifest.json']=encode(manifest)
        scene_row=next(x for x in nextboot['drivers'] if x['manifest']=='ui-scene/manifest.json')
        if scene_row.get('image_source'):
            source=scene_row['image_source'];oldpath=source['path'];oldhash=sha(before['ui-scene/driver.elf'])
            require(oldpath.count('image-'+oldhash)==1 and source.get('internal_fallback') is True,'Unexpected scene source/fallback')
            source['path']=oldpath.replace('image-'+oldhash,'image-'+sha(scene));sd[source['path']]=scene
        rtcpath=getattr(a,lane+'_rtc',None)
        allowed={'boot.json','ble_buttons.elf','ble_buttons.json','ui-scene/driver.elf','ui-scene/manifest.json'}
        if rtcpath:
            rtc=unzip(rtcpath.read_bytes());elf=rtc['rtc-utc/package/driver.elf'];require(elf==rtc['rtc-utc/repeat/driver.elf'],'RTC repeat differs')
            rec=json.loads(rtc['rtc-utc/receipt.json']);require('-DRTC_UTC_OFFSET_SECONDS=28800' in rec['builds'][0]['defines'] and sha(elf)==rec['builds'][0]['sha256'],'RTC UTC basis/proof differs')
            rm=json.loads(rtc['rtc-utc/package/manifest.json']);same_provider(json.loads(before['rtc/manifest.json']),rm)
            after['rtc/driver.elf']=elf;after['rtc/manifest.json']=encode(rm);allowed|={'rtc/driver.elf','rtc/manifest.json'}
        idle_home=None
        if lane in HOME_PINS:
            idle_home=install_idle_home(before,after,nextboot,lane,a.idle_home);allowed|={'default.elf','default.json'}
        after['boot.json']=encode(nextboot)
        require(before.keys()==after.keys(),'Store membership changed')
        changed={n for n in before if before[n]!=after[n]};require(changed<=allowed,'Unrelated stored payload changed')
        original_sd_path=oldrow.get('image_source',{}).get('path')
        require(all(sd[n]==b for n,b in sd_before.items() if n not in ({original_sd_path,sd_manifest} if original_sd_path else set())),'Unrelated SD member changed')
        image,store_receipt=build(after,len(raw));require(read_image(image,len(raw))==after,'Independent SPIFFS decode differs')
        out=a.output/lane;out.mkdir();write_files(out/'before',before);write_files(out/'after',after);(out/'store.bin').write_bytes(image)
        if sd_raw:
            with zipfile.ZipFile(out/'sd-apps.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
                for n,b in sorted(sd.items()):z.writestr(n,b)
            require(unzip((out/'sd-apps.zip').read_bytes())==sd,'SD round trip differs');write_files(out/'sd',sd)
        results[lane]={'baseline_store_sha256':sha(raw),'store':store_receipt,'changed_files':sorted(changed),'unchanged_internal':len(before)-len(changed),'driver_count':len(boot['drivers']),'app_policy_count':len(boot['app_capabilities']),'grants':grant_receipt,'sd_original_count':len(sd_before),'sd_unchanged':sum(sd.get(n)==b for n,b in sd_before.items()),'sd_added':sorted(set(sd)-set(sd_before)),'app_sha256':sha(app),'scene_sha256':sha(scene),'profile_package':str(package.resolve()),'all_other_authority_unchanged':True,'idle_home':idle_home}
    receipt={'inputs':inputs,'app_sha256':sha(app),'products':results,'scope':'Private stores/SD and metadata only; no firmware execution or external publication'}
    (a.output/'assembly.json').write_bytes(encode(receipt));return receipt
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('idle-home','watch-artifact','wave-artifact','x4-store','x4-sd','watch-scene','wave-scene','x4-scene','package','bootfs-tools','output'):p.add_argument('--'+n,type=pathlib.Path,required=True)
    for n in ('watch-rtc','wave-rtc'):p.add_argument('--'+n,type=pathlib.Path,required=True)
    p.add_argument('--expected-app-sha256',required=True)
    p.add_argument('--expected-manifest-sha256',required=True)
    args=p.parse_args();result=assemble(args)
    print(json.dumps({k:v['store']['sha256'] for k,v in result['products'].items()},indent=2))
