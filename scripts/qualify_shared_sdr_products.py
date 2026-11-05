#!/usr/bin/env python3
"""Offline product preservation, pack roundtrip and selected Runtime admission."""
import copy,json,os,subprocess,sys
from pathlib import Path,PurePosixPath
from shared_sdr_products import *
ROOT=Path(__file__).resolve().parents[1]
def command(cmd,log):
    q=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1'})
    log.write_text(q.stdout+q.stderr);check(q.returncode==0,'Command failed: '+str(log));return q.stdout
def preservation(before,before_sd,after,sd,elf,manifest):
    ob,oi,old,om=selected(before);nb,ni,new,nm=selected(after)
    check(oi==ni and old['manifest']==new['manifest'],'Selection identity changed')
    check(after[new['manifest']]==manifest,'Shared manifest differs')
    check(new['grants']==grants(before,old,json.loads(manifest)),'Existing grants changed')
    left=copy.deepcopy(old);right=copy.deepcopy(new)
    left.pop('grants');right.pop('grants')
    if 'image_source' in left and 'image_source' in right:
        left['image_source'].pop('path');right['image_source'].pop('path')
    check(left==right,'Non-grant app authority changed')
    ob['app_capabilities'][oi]=new;check(ob==nb,'Non-SDR boot state changed')
    oldimage,oldmeta=paths(old);image,meta=paths(new)
    changed={'boot.json',new['manifest']};changed_sd=set()
    if image:
        check(sd.get(image)==elf and sd.get(meta)==manifest,'Selected SD package differs')
        changed_sd.update((image,meta))
        check(PurePosixPath(image).parent==PurePosixPath(meta).parent,'Catalog image/manifest directory differs')
        if '/image-' in oldimage:
            check(PurePosixPath(image).parent.name=='image-'+sha(elf),'Successor content address differs')
            check(sd.get(oldimage)==before_sd[oldimage] and sd.get(oldmeta)==before_sd[oldmeta],'Content-addressed predecessor changed')
        else:check(image==oldimage and meta==oldmeta,'Stable package route changed')
    else:
        check(after.get('waterfall.elf')==elf,'Internal ELF differs');changed.add('waterfall.elf')
    check(set(before)<=set(after) and set(after)-set(before)<=changed,'Internal inventory changed')
    check(set(before_sd)<=set(sd) and set(sd)-set(before_sd)<=changed_sd,'SD inventory changed')
    for n,b in before.items():check(n in changed or after.get(n)==b,'Internal peer changed '+n)
    for n,b in before_sd.items():check(n in changed_sd or sd.get(n)==b,'SD peer changed '+n)
    return changed,meta
def main():
    a=arguments(__doc__,('runtime','candidate'));pin(a.runtime,RUNTIME)
    elf,manifest,lanes,build,read_image,magic,decode,encode_image=load(a)
    a.output.mkdir(parents=True)
    receipt={'schema':1,'status':'running','runtime':RUNTIME,'system':SYSTEM,'bootfs':BOOTFS,'elf_sha256':sha(elf),'manifest_sha256':sha(manifest),'products':{},'physical_testing':False,'target_execution':False,'installed_predecessor_qualified':False}
    metadata={}
    for lane,(size,stored,before,before_sd) in lanes.items():
        after=tree(a.candidate/lane/'after');sd=tree(a.candidate/lane/'after-sd')
        changed,meta=preservation(before,before_sd,after,sd,elf,manifest);metadata[lane]=meta
        packed,image,packing=pack(after,stored,changed,size,build,read_image,magic,decode,encode_image)
        out=a.output/lane;out.mkdir();write(out/'before',before);write(out/'stored',packed)
        (out/'store.bin').write_bytes(image);zip_roundtrip(out/'sd-apps.zip',sd)
        receipt['products'][lane]={'packing':packing,'canonical_files':{n:sha(b) for n,b in after.items()},'stored_files':{n:sha(b) for n,b in packed.items()},'sd_files':{n:sha(b) for n,b in sd.items()},'encoded_images':sum(b.startswith(magic) for b in packed.values())}
    (a.output/'roundtrip.json').write_bytes(encode(receipt))
    fixtures=a.system/'test/files_products/native';templates=json.loads((fixtures/'commands.json').read_text())
    for mode in ('normal','sanitized'):
        out=a.output/mode;out.mkdir()
        san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*(['-no-pie'] if sys.platform=='linux' else [])] if mode=='sanitized' else []
        inc=['-I'+str(a.runtime/x) for x in ('src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','lib/elf_loader/include','test/native_bank_stubs')]
        validator=out/'validator.o'
        command(['cc','-std=c11',*san,*inc,'-c',a.runtime/'lib/elf_loader/src/esp_elf_validate.c','-o',validator],out/'validator.log')
        command(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function',*san,*inc,'-I'+str(fixtures),ROOT/'Apps/shared_sdr/shared_sdr_elf_admission.cpp',validator,'-o',out/'elf-admission'],out/'elf-compile.log')
        command([out/'elf-admission',a.shared/'waterfall.elf'],out/'elf-admission.log')
        for lane,template in templates.items():
            cmd=[x.format(runtime=a.runtime,x4_runtime=a.runtime,fixtures=fixtures,output=out) for x in template]
            cmd=['-DRISC_APP_REQUIREMENT_ROWS=32' if x=='-DRISC_APP_REQUIREMENT_ROWS=24' else '-DRISC_PROVIDER_CAPACITY=48' if lane!='x4' and x=='-DRISC_PROVIDER_CAPACITY=40' else x for x in cmd]
            cmd[1:1]=[*san,'-DSTORE_ADMISSION_COHORT','-Wno-misleading-indentation',*(['-DSTORE_ADMISSION_ENCODED_IMAGES'] if lane!='x4' else [])]
            if lane=='x4':
                # Include the exact Runtime source unchanged; append only a host
                # context selector so its actual installationAdmit can run.
                wrapper=out/'runtime-catalog-probe.cpp'
                wrapper.write_text('#include "'+str(a.runtime/'src/bootstrap/Runtime.cpp')+'"\nextern "C" RiscBoot::Runtime* sdr_test_select_runtime(RiscBoot::Runtime* r){auto* old=currentRuntime;currentRuntime=r;return old;}\n')
                source=(fixtures/'x4.cpp').read_text()
                def replace(old,new):
                    nonlocal source
                    check(source.count(old)==1,'Pinned X4 fixture changed');source=source.replace(old,new)
                replace('if (argc != 3) return 2;','if (argc != 5) return 2;')
                replace('int main(int argc, char** argv) {','#include "shared_sdr_catalog_probe.h"\nint main(int argc, char** argv) {')
                replace('if(validated && !retainedSdGraphBound)return 4;','if(validated && !retainedSdGraphBound)return 4;\n  if(validated && !sdr_catalog_probe(candidate,argv[3],argv[4]))return 5;')
                replace('printPlatformReceipt(validated?candidate:runtime);','printPlatformReceipt(validated?candidate:runtime);\n  printf("\\"shared_sdr_catalog_admitted\\":%s,",validated?"true":"false");')
                fixture=out/'x4-catalog.cpp';fixture.write_text(source)
                cmd=[str(wrapper) if x==str(a.runtime/'src/bootstrap/Runtime.cpp') else str(fixture) if x==str(fixtures/'x4.cpp') else x for x in cmd]
            cmd.extend([*inc,'-I'+str(ROOT/'Apps/shared_sdr'),str(validator)])
            (out/(lane+'-command.json')).write_bytes(encode(cmd));command(cmd,out/(lane+'-compile.log'))
            argv=[out/lane,a.output/lane/'before',a.output/lane/'stored']
            if lane=='x4':argv.extend([metadata[lane],a.candidate/lane/'after-sd'/metadata[lane]])
            r=json.loads(command(argv,out/(lane+'-admission.log')))
            check(r['prepared'] and r['cohort_validated'] and r['elf_count'] and not r['hardware_calls'] and not r['storage_calls'],'Runtime admission failed '+lane)
            check(r.get('encoded_image_count',0)==receipt['products'][lane]['encoded_images'],'Envelope count mismatch')
            if lane=='x4':check(r.get('retained_sd_serial_graph_bound') and r.get('shared_sdr_catalog_admitted'),'X4 catalog/SD admission missing')
            receipt['products'][lane][mode]=r
            (a.output/'qualification.json').write_bytes(encode(receipt))
    pin(a.runtime,RUNTIME);pin(a.system,SYSTEM);pin(a.bootfs,BOOTFS)
    receipt['status']='passed';(a.output/'qualification.json').write_bytes(encode(receipt))
    print('PASS: shared SDR product preservation, roundtrips, selected Runtime and catalog metadata admission')
if __name__=='__main__':main()
