#!/usr/bin/env python3
"""Preserve published peers while assembling one shared SDR package."""
import copy,json,re
from pathlib import PurePosixPath
from shared_sdr_products import *
def assemble(before,before_sd,elf,manifest):
    boot,index,row,old_manifest=selected(before)
    oldrow=copy.deepcopy(row);after=dict(before);sd=dict(before_sd)
    row['grants']=grants(before,row,json.loads(manifest))
    after[row['manifest']]=manifest
    changed={'boot.json',row['manifest']};changed_sd=set()
    old_image,old_meta=paths(oldrow)
    if old_image:
        check(old_image in sd and old_meta in sd,'Selected SD image/manifest absent')
        check(json.loads(sd[old_meta])==old_manifest,'Published SD manifest differs from boot manifest')
        match=re.fullmatch(r'(.*)/image-([0-9a-f]{64})/([^/]+)',old_image)
        if match:
            check(match.group(2)==sha(sd[old_image]),'Published content-addressed image differs')
            image=match.group(1)+'/image-'+sha(elf)+'/waterfall.elf'
            meta=str(PurePosixPath(image).parent/'manifest.json')
            for n,b in ((image,elf),(meta,manifest)):check(n not in sd or sd[n]==b,'Successor address collision')
            row['image_source']['path']=image
        else:image,meta=old_image,old_meta
        sd[image]=elf;sd[meta]=manifest;changed_sd.update((image,meta))
    else:
        check(old_manifest['file_name']=='waterfall.elf','Internal identity changed')
        after['waterfall.elf']=elf;changed.add('waterfall.elf')
    after['boot.json']=encode(boot)
    return after,sd,changed,changed_sd,oldrow,row
def main():
    a=arguments(__doc__);elf,manifest,lanes,build,read_image,magic,decode,encode_image=load(a)
    staged={};receipt={'schema':1,'elf_sha256':sha(elf),'manifest_sha256':sha(manifest),'products':{},'published':False}
    for lane,(size,stored,before,sd) in lanes.items():
        after,new_sd,changed,changed_sd,oldrow,newrow=assemble(before,sd,elf,manifest)
        packed,image,packing=pack(after,stored,changed,size,build,read_image,magic,decode,encode_image)
        staged[lane]=(after,new_sd)
        receipt['products'][lane]={'alarm_migration':alarm_migration(before,oldrow),'before_selection':oldrow,'after_selection':newrow,'changed_internal':sorted(changed),'changed_sd':sorted(changed_sd),'packing':packing}
    a.output.mkdir(parents=True)
    for lane,(after,sd) in staged.items():write(a.output/lane/'after',after);write(a.output/lane/'after-sd',sd)
    (a.output/'assembly.json').write_bytes(encode(receipt))
    print('Shared SDR assembled; qualification remains required')
if __name__=='__main__':main()
