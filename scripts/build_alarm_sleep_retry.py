#!/usr/bin/env python3
"""Build one capability-based color alarm for both watches with native GCC15.2."""
import argparse, hashlib, json, os, subprocess, sys
from pathlib import Path
from normalize_xtensa_relocations import normalize
ROOT=Path(__file__).resolve().parents[1]
def run(command,**kwargs):return subprocess.check_output(list(map(str,command)),text=True,**kwargs)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser()
    for name in ('runtime','system-apps','watch','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--cc',required=True);a=p.parse_args()
    runtime,system,watch,out=[x.resolve() for x in (a.runtime,a.system_apps,a.watch,a.output)]
    cc=Path(a.cc).absolute();version=run([cc,'--version']).splitlines()[0]
    if 'esp-15.2.0_20251204' not in version or run([cc,'-dumpfullversion']).strip()!='15.2.0':raise ValueError('Pinned native SDK GCC15.2 required')
    if out.exists():raise ValueError('Fresh output directory required')
    out.mkdir(parents=True)
    for key in ('CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','GCC_EXEC_PREFIX','COMPILER_PATH','LD_PRELOAD'):
        if os.environ.get(key):raise ValueError('Ambient compiler input is forbidden: '+key)
    mapping=out/'exports.map';mapping.write_text('{ global: t5_driver_get; local: *; };\n')
    elf=out/'driver.elf'
    defines=['POINTS_IN_TIME_SERVICE','PORTABLE_RTC_UTC8_DENVER','ALARM_VOLUME_CONTROL','ALARM_DND_CONTROL','ALARM_CLOCK_CHANGE_CONTROL','ALARM_RECONCILE_BUSY_RETRY']
    includes=[ROOT/'lib/Alarm/include',runtime/'sdk/driver',runtime/'sdk/app',system/'lib/PortableApps/include']
    command=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror',*['-D'+x for x in defines],*['-I'+str(x) for x in includes],ROOT/'Services/alarm_service/service.c','-lgcc','-o',elf]
    run(command)
    normalize(elf)
    sys.path.insert(0,str(watch/'scripts'))
    from compact_current_elf import compact
    compact(elf,str(cc),debug_path=out/'debug/driver.elf')
    symbols=run([str(cc).removesuffix('gcc')+'nm','-D',elf])
    imports={x.split()[-1] for x in symbols.splitlines() if ' U ' in ' '+x}
    exports={x.split()[-1] for x in symbols.splitlines() if len(x.split())>=3 and x.split()[-2] in ('T','D','R','B')}
    if not imports<={'memcpy','memset','memcmp','strcmp','strlen'} or exports!={'t5_driver_get'}:raise ValueError('Private or layout-sensitive ABI import/export: '+str((imports,exports)))
    data=elf.read_bytes()
    if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Wrong shared ELF architecture')
    (out/'manifest.json').write_bytes((ROOT/'Services/alarm_service/color-sleep-retry-manifest.json').read_bytes())
    loader=out/'target-loader'
    loader_command=[os.environ.get('CC','cc'),'-std=gnu11','-O1','-g','-I'+str(ROOT/'test/alarm-loader-stubs'),'-I'+str(runtime/'lib/elf_loader/include'),ROOT/'test/alarm_target_loader.c',runtime/'lib/elf_loader/src/esp_elf.c',runtime/'lib/elf_loader/src/arch/esp_elf_xtensa.c',runtime/'lib/elf_loader/src/esp_elf_validate.c','-o',loader]
    run(loader_command);loader_result=run([loader,elf]);print(loader_result,end='')
    roots={'utilities':ROOT,'runtime':runtime,'system':system,'watch':watch}
    sources={name:run(['git','-C',root,'rev-parse','HEAD']).strip() for name,root in roots.items()}
    source_hashes={str(x.relative_to(ROOT)):sha(x) for x in [ROOT/'Services/alarm_service/service.c',ROOT/'Services/alarm_service/color-sleep-retry-manifest.json',ROOT/'scripts/build_alarm_sleep_retry.py',ROOT/'scripts/normalize_xtensa_relocations.py',*sorted((ROOT/'lib/Alarm/include').glob('*.h'))]}
    receipt={'schema':1,'version':'0.4.12','compiler':version,'compiler_sha256':sha(cc.resolve()),'sources':sources,'source_hashes':source_hashes,'defines':defines,'command':list(map(str,command)),'loader_command':list(map(str,loader_command)),'loader_result':loader_result,'imports':sorted(imports),'exports':sorted(exports),'elf_bytes':len(data),'elf_sha256':sha(elf),'manifest_sha256':sha(out/'manifest.json'),'sdk_private_imports':False,'libc_layout_sensitive_imports':False,'hardware_tested':False}
    (out/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Shared color alarm0.4.12: GCC15.2, public ABI, exact Runtime loader, both-watch service PASS')
if __name__=='__main__':main()
