#!/usr/bin/env python3
"""Build new shared daily tools against the pinned generic client adapter.

This is a distinct runtime profile from the preserved Reader migration cohort.
No source is copied into a Watch-specific application and no device is touched.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
from app_manifest import validate_manifest
ROOT=Path(__file__).resolve().parents[1]
PIN='911be9e8042f1bcc46038fb70189eebe4ca106c5'
IMPORTS={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'}
EXPORTS={'app_main','app_module_init','app_module_fini'}
def inventory():
    apps=json.loads((ROOT/'utilities-manifest.json').read_text())['portable_apps']
    if [a.get('id') for a in apps]!=['calculator','stopwatch']:
        raise ValueError('Unexpected portable daily tool inventory')
    for app in apps:
        name=app['id']
        if app.get('origin')!='original' or app.get('runtime_profile')!='portable-riscrte-v1' or app.get('source_path')!='Apps/'+name+'.c' or app.get('manifest_path')!='Apps/'+name+'.json' or app.get('file_name')!=name+'.elf':
            raise ValueError('Invalid portable app identity/provenance')
        validate_manifest(ROOT/app['source_path'],name+'.elf')
        manifest=json.loads((ROOT/app['manifest_path']).read_text())
        if manifest['version']!=app['version'] or manifest.get('runtime_profile')!='portable-riscrte-v1':
            raise ValueError('Portable manifest/version mismatch')
    return apps

def verify_system(system,revision=PIN):
    if len(revision)!=40 or any(c not in "0123456789abcdef" for c in revision):raise ValueError("Exact selected System revision required")
    if subprocess.check_output(['git','rev-parse','HEAD'],cwd=system,text=True).strip()!=revision or subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=system,text=True).strip():
        raise ValueError('Clean exact shared client source required: '+revision)
    pins=json.loads((system/'lib/PortableApps/SOURCES.json').read_text())
    for name,pin in pins.items():
        if hashlib.sha256((system/'lib/PortableApps/include'/name).read_bytes()).hexdigest()!=pin['sha256']:
            raise ValueError('Pinned SDK bytes changed: '+name)

def foreground_idle_module(system, args, parser):
    """Load the actual selected System builder; never fork its SDK/flag recipe."""
    if not any(getattr(args, name, None) is not None for name in (
            'foreground_idle_helper_dir', 'foreground_idle_runtime_dir',
            'foreground_idle_drivers_sdk_dir', 'foreground_idle_fallback_seconds')):
        return None
    source = system/'scripts/portable_foreground_idle_build.py'
    if not source.is_file():
        parser.error('Selected System revision has no foreground-idle builder: ' + str(source))
    try:
        committed = subprocess.check_output(['git', '-C', str(system), 'show',
            'HEAD:scripts/portable_foreground_idle_build.py'], stderr=subprocess.PIPE)
    except subprocess.CalledProcessError:
        parser.error('Foreground-idle builder must belong to the exact selected System revision')
    if source.read_bytes() != committed:
        parser.error('Foreground-idle builder differs from the exact selected System revision')
    spec = importlib.util.spec_from_file_location('selected_system_foreground_idle', source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.validate(args, parser)
    return module


def build(system,revision=PIN,safe_viewport=False,include_battery=False,output=None,native_stopwatch_runtime=None,navigation=False,
          foreground_idle_helper_dir=None,foreground_idle_runtime_dir=None,
          foreground_idle_drivers_sdk_dir=None,foreground_idle_fallback_seconds=None):
    apps=inventory();verify_system(system,revision)
    parser=argparse.ArgumentParser(description=__doc__)
    idle_args=argparse.Namespace(foreground_idle_helper_dir=foreground_idle_helper_dir,
        foreground_idle_runtime_dir=foreground_idle_runtime_dir,
        foreground_idle_drivers_sdk_dir=foreground_idle_drivers_sdk_dir,
        foreground_idle_fallback_seconds=foreground_idle_fallback_seconds,navigation=navigation)
    idle=foreground_idle_module(system,idle_args,parser)
    if include_battery:
        m=json.loads((ROOT/"Apps/battery.json").read_text());apps.append({"id":"battery","source_path":"Apps/battery.c","manifest_path":"Apps/battery.json","version":m["version"]})
    cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
    out=output or ROOT/'dist/portable-apps';out.mkdir(parents=True,exist_ok=True)
    include=system/'lib/PortableApps/include'
    if native_stopwatch_runtime:
        if subprocess.check_output(['git','status','--porcelain'],cwd=native_stopwatch_runtime,text=True).strip():raise ValueError('Clean native Runtime SDK required')
        include=out/'sdk';shutil.copytree(system/'lib/PortableApps/include',include,dirs_exist_ok=True)
        shutil.copytree(system/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
        for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:shutil.copyfile(native_stopwatch_runtime/'sdk/app'/name,include/name)
    idle_flags=[];idle_sources=[]
    if idle:
        include,idle_flags,idle_sources=idle.configure(idle_args,parser,system,out,include)
        if native_stopwatch_runtime and subprocess.check_output(
                ['git','rev-parse','HEAD'],cwd=native_stopwatch_runtime,text=True).strip()!=idle.RUNTIME_COMMIT:
            parser.error('Native Stopwatch and foreground idle must select the same exact canonical Runtime revision')
    catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    mapping=out/'exports.map';mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    validator=out/'validate-elf'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
    rows=[]
    for app in apps:
        name=app['id'];elf=out/(name+'.elf')
        native=name=='stopwatch' and native_stopwatch_runtime is not None
        extra=['-DPORTABLE_STOPWATCH_NATIVE_UTC','-DPORTABLE_STOPWATCH_NATIVE_STANDALONE','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_APP_LAUNCH_GUARD'] if native else []
        compile_command=[cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror','-DPORTABLE_FORCE_FULL_FRAMES',*(['-DPORTABLE_INPUT_NAVIGATION'] if navigation else []),*(['-DPORTABLE_SAFE_VIEWPORT'] if safe_viewport else []),*extra,*idle_flags,*['-I'+str(p) for p in (include,system/'lib/NativeApps/include')],str(ROOT/app['source_path']),str(system/'lib/PortableApps/src/adapter.c'),*([str(system/'lib/PortableApps/src'/n) for n in ['PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c']] if native else []),*map(str,idle_sources),str(catalog),'-lgcc','-o',str(elf)]
        subprocess.run(compile_command,check=True)
        syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
        imports={s.split()[-1] for s in syms.splitlines() if ' U ' in ' '+s}
        exports={s.split()[-1] for s in syms.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
        if not imports<=IMPORTS or exports!=EXPORTS:raise ValueError((name,imports-IMPORTS,exports))
        data=elf.read_bytes()
        if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Wrong target ABI')
        subprocess.run([str(validator),str(elf)],check=True)
        source_manifest=json.loads((ROOT/app['manifest_path']).read_text())
        requires=([{'capability':'display.output','api':1},{'capability':'input.touch.raw','api':1},{'capability':'board.battery','api':1}] if name=='battery' else [{'capability':row['capability'],'api':int(row['api'][2:])} for row in source_manifest['requires']])
        if navigation: requires.append({'capability':'input.navigation','api':1})
        if native: requires=[({'capability':'runtime.realtime','api':1} if d['capability']=='rtc.clock' else d) for d in requires]
        if idle:idle.requirements(idle_args,requires)
        manifest={'type':'application','id':name,'version':app['version'],'architecture':'xtensa-esp32s3','file_name':name+'.elf','entry':'app_main','requires':requires}
        elf.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
        row={'id':name,'version':app['version'],'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),'imports':sorted(imports)}
        if idle:
            row['requested_capabilities']=requires
            idle.record(idle_args,row,[flag for flag in compile_command if flag.startswith('-D')],
                        private_kv=(2,) if name=='stopwatch' else (),compile_command=compile_command)
            row['source_sha256']={str(path):hashlib.sha256(path.read_bytes()).hexdigest()
                for path in [ROOT/app['source_path'],system/'lib/PortableApps/src/adapter.c',*idle_sources]}
        rows.append(row)
    record={'schema':1,'purpose':'portable-runtime-development-not-install-catalog','repository_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'shared_system_apps_sha':revision,'safe_viewport':safe_viewport,'navigation':navigation,'native_stopwatch_runtime':subprocess.check_output(['git','rev-parse','HEAD'],cwd=native_stopwatch_runtime,text=True).strip() if native_stopwatch_runtime else None,'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'apps':rows}
    if idle:
        record['foreground_idle']=idle_args.foreground_idle_receipt
        record['builder_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n')
    for name in ('LICENSE-FontAwesome.txt','LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json'):
        (out/name).write_bytes((system/'lib/PortableApps/fonts'/name).read_bytes())
    print('Validated two portable application ELFs; no install or release')
def argument_parser():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps',required=True,type=Path)
    parser.add_argument('--system-apps-revision',default=PIN,help='Exact deliberately selected shared adapter revision; historical default preserved')
    parser.add_argument('--safe-viewport',action='store_true')
    parser.add_argument('--include-battery',action='store_true')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--navigation',action='store_true')
    parser.add_argument('--native-stopwatch-runtime',type=Path)
    # The selected System helper owns validation, defaults, staging and flags.
    parser.add_argument('--foreground-idle-helper-dir',type=Path,help='Opt-in generic returning foreground Light helper source directory')
    parser.add_argument('--foreground-idle-runtime-dir',type=Path,help='Exact canonical Runtime checkout selected by the System helper')
    parser.add_argument('--foreground-idle-drivers-sdk-dir',type=Path,help='Current Drivers sdk/driver capability headers')
    parser.add_argument('--foreground-idle-fallback-seconds',type=int,help='Missing-key fallback seconds; validated by selected System helper')
    return parser


if __name__=='__main__':
    a=argument_parser().parse_args()
    build(a.system_apps.resolve(),a.system_apps_revision,a.safe_viewport,a.include_battery,
          a.output.resolve() if a.output else None,
          a.native_stopwatch_runtime.resolve() if a.native_stopwatch_runtime else None,a.navigation,
          a.foreground_idle_helper_dir,a.foreground_idle_runtime_dir,
          a.foreground_idle_drivers_sdk_dir,a.foreground_idle_fallback_seconds)
