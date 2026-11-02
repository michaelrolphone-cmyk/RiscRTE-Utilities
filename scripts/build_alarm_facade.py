#!/usr/bin/env python3
"""Build both forwarding facades; no scheduler or product policy replacement."""
import argparse, hashlib, json, os, subprocess, sys
from pathlib import Path
from normalize_xtensa_relocations import normalize
ROOT=Path(__file__).resolve().parents[1]
def run(command,**kwargs):return subprocess.check_output(list(map(str,command)),text=True,**kwargs)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser()
    for name in ('runtime','watch','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--cc',required=True);a=p.parse_args()
    runtime,watch,out=[x.resolve() for x in (a.runtime,a.watch,a.output)]
    cc=Path(a.cc).absolute();version=run([cc,'--version']).splitlines()[0]
    if 'esp-15.2.0_20251204' not in version or run([cc,'-dumpfullversion']).strip()!='15.2.0':raise ValueError('Pinned native SDK GCC15.2 required')
    if out.exists():raise ValueError('Fresh output directory required')
    out.mkdir(parents=True)
    for key in ('CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','GCC_EXEC_PREFIX','COMPILER_PATH','LD_PRELOAD'):
        if os.environ.get(key):raise ValueError('Ambient compiler input is forbidden: '+key)
    sys.path.insert(0,str(watch/'scripts'))
    from compact_current_elf import compact
    includes=[ROOT/'lib/Alarm/include',runtime/'sdk/driver',runtime/'sdk/app']
    sources={name:run(['git','-C',root,'rev-parse','HEAD']).strip() for name,root in {'utilities':ROOT,'runtime':runtime,'watch':watch}.items()}
    for profile,define in [('legacy-clock-change','ALARM_FACADE_LEGACY_CLOCK_CHANGE'),('tagged-owner','ALARM_FACADE_TAGGED_OWNER')]:
        target=out/profile;target.mkdir()
        mapping=target/'exports.map';mapping.write_text('{ global: t5_driver_get; local: *; };\n')
        elf=target/'driver.elf'
        command=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror','-D'+define,*['-I'+str(x) for x in includes],ROOT/'Services/alarm_facade/facade.c','-lgcc','-o',elf]
        run(command);normalize(elf)
        compact(elf,str(cc),debug_path=target/'debug/driver.elf')
        symbols=run([str(cc).removesuffix('gcc')+'nm','-D',elf])
        imports={x.split()[-1] for x in symbols.splitlines() if ' U ' in ' '+x}
        exports={x.split()[-1] for x in symbols.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','R','B')}
        if not imports<={'memcpy','memset','memcmp','strcmp','strlen'} or exports!={'t5_driver_get'}:raise ValueError(str((imports,exports)))
        data=elf.read_bytes()
        if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Wrong target ABI')
        manifest=ROOT/('Services/alarm_facade/'+profile+'-manifest.json')
        (target/'manifest.json').write_bytes(manifest.read_bytes())
        loader=target/'target-loader'
        loader_command=[os.environ.get('CC','cc'),'-std=gnu11','-O1','-g','-I'+str(ROOT/'test/alarm-loader-stubs'),'-I'+str(runtime/'lib/elf_loader/include'),ROOT/'test/alarm_target_loader.c',runtime/'lib/elf_loader/src/esp_elf.c',runtime/'lib/elf_loader/src/arch/esp_elf_xtensa.c',runtime/'lib/elf_loader/src/esp_elf_validate.c','-o',loader]
        run(loader_command);loader_result=run([loader,elf]);print(loader_result,end='')
        paths=[ROOT/'Services/alarm_facade/facade.c',manifest,ROOT/'scripts/build_alarm_facade.py',ROOT/'scripts/normalize_xtensa_relocations.py',*sorted((ROOT/'lib/Alarm/include').glob('*.h'))]
        receipt={'schema':1,'profile':profile,'compiler':version,'compiler_sha256':sha(cc.resolve()),'sources':sources,'source_hashes':{str(x.relative_to(ROOT)):sha(x) for x in paths},'defines':[define],'command':list(map(str,command)),'loader_command':list(map(str,loader_command)),'loader_result':loader_result,'imports':sorted(imports),'exports':sorted(exports),'elf_bytes':len(data),'elf_sha256':sha(elf),'manifest_sha256':sha(target/'manifest.json'),'owner_scheduler_replaced':False,'hardware_tested':False}
        (target/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        print('Alarm facade '+profile+': target ELF and exact Runtime loader PASS')
if __name__=='__main__':main()
