#!/usr/bin/env python3
"""Exercise actual unloadable Battery/provider ELFs with the production Runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from scene_support import ROOT, sdk, time_sources


def link_replace(source, destination):
    if os.path.exists(destination): os.unlink(destination)
    os.link(source,destination)
    return destination

def run(runtime: Path, system: Path, output: Path, sanitize: bool) -> None:
    tracked = [ROOT/'Apps/battery_scene.c', ROOT/'Apps/battery_broadcast.h',ROOT/'Apps/battery_preference_adapter.h',
               ROOT/'Services/telemetry_broadcast/service.c', Path(__file__), ROOT/'scripts/scene_support.py', system/'scripts/scene_sdk.py']
    for directory in (ROOT/'lib/Alarm',ROOT/'lib/Bluetooth',ROOT/'test/shared_battery',runtime/'src',
                      runtime/'lib/ArduinoJson/src',runtime/'include',runtime/'test/drivers/stubs',
                      system/'Services/scene_host',system/'Services/scene_profile',
                      system/'lib/PortableApps/include',system/'lib/PortableApps/src',
                      system/'lib/PortableApps/time'):
        tracked += [p for p in directory.rglob('*') if p.is_file() and '__pycache__' not in str(p)]
    provenance = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(tracked))}
    if os.environ.get('BATTERY_REUSE_RUNTIME'):
        previous=json.loads((output/'sources.json').read_text())
        for path,digest in previous.items():
            if path.startswith(str(runtime)+'/') or path in (str(ROOT/'test/shared_battery/runtime_test.cpp'),str(ROOT/'test/shared_battery/hardware.c')):
                assert provenance.get(path)==digest, 'Runtime reuse input changed: '+path
    (output/'sources.json').write_text(json.dumps(provenance,indent=2)+'\n')
    previous_sdk={str(p.relative_to(output/'sdk')):hashlib.sha256(p.read_bytes()).hexdigest() for p in (output/'sdk').rglob('*') if p.is_file()} if os.environ.get('BATTERY_REUSE_RUNTIME') else {}
    include = sdk(runtime, system, output/'sdk')
    for name in ('RiscBatteryGaugeV1.h',):
        shutil.copyfile(system/'lib/PortableApps/include'/name, include/name)
    sdk_hashes={str(p.relative_to(output/'sdk')):hashlib.sha256(p.read_bytes()).hexdigest() for p in (output/'sdk').rglob('*') if p.is_file()}
    if previous_sdk: assert previous_sdk==sdk_hashes, 'Compiled Runtime SDK changed during reuse'
    (output/'sdk-inputs.json').write_text(json.dumps(sdk_hashes,indent=2)+'\n')
    (output/'compiler-tmp').mkdir(exist_ok=True)
    os.environ['TMPDIR']=str(output/'compiler-tmp')
    cc, cxx = os.environ.get('CC', 'cc'), os.environ.get('CXX', 'c++')
    san = (['-g0', '-pipe', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer'] if sanitize else ['-g0', '-pipe'])
    cflags = [*san, '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', '-I'+str(include)]
    shared = ['-fPIC', '-fvisibility=hidden', '-shared']
    if os.uname().sysname == 'Darwin':
        shared += ['-undefined', 'dynamic_lookup']
    def build(destination: Path, sources: list[Path | str], defines: list[str] = [], dso: bool = True):
        subprocess.run([cc, *cflags, *(shared if dso else ['-c']), *defines,
                        *map(str, sources), '-o', str(destination)], check=True)
    common = output/'common'
    common.mkdir(exist_ok=True)
    build(common/'battery.elf', [ROOT/'test/shared_battery/battery.c'])
    build(common/'scene-host.elf', [system/'Services/scene_host/host.c'])
    for index,(name,capability,api) in enumerate((('display','display.output',1),('touch','input.touch.raw',1),('navigation','input.navigation',1),('gauge','board.battery',1),('radio','bluetooth.telemetry',1))):
        build(common/(name+'.elf'),[ROOT/'test/shared_battery/provider.c'],[f'-DPROVIDER_INDEX={index if index<3 else index+5}',f'-DPROVIDER_ID="{name}"',f'-DPROVIDER_CAP="{capability}"',f'-DPROVIDER_API={api}'])
    build(common/'telemetry-broadcast.elf',[ROOT/'Services/telemetry_broadcast/service.c'])
    build(common/'launcher.elf', [ROOT/'test/shared_battery/launcher.c'])
    build(output/'hardware.o',[ROOT/'test/shared_battery/hardware.c'],dso=False)
    # Host helper's time conversion is linked once into the executable, not any
    # application. The product application itself remains exactly the same DSO.
    for index,source in enumerate(time_sources(system)):
        build(output/f'time{index}.o',[source],dso=False)
    sources = ('bootstrap/Json.cpp', 'bootstrap/Board.cpp', 'bootstrap/Runtime.cpp',
               'runtime/streams/AppStreamSessions.cpp', 'runtime/streams/ProviderQueueHost.cpp',
               'runtime/drivers/ProviderGraphV2.cpp', 'runtime/drivers/ProviderModuleV2.cpp',
               'runtime/storage/AppDataFiles.cpp')
    executable=output/'runtime-test'
    cppflags=[*san,'-std=c++17','-O0','-Wall','-Wextra','-Werror',
              '-Wno-missing-field-initializers','-DRISC_RUNTIME_PROVIDER_CAPACITY=28','-DRISC_RUNTIME_PREFERENCE_STORE=1','-DRISC_RUNTIME_ATOMIC_KEY_VALUE=1','-I'+str(include),
              *['-I'+str(runtime/path) for path in ('src','lib/ArduinoJson/src','test/drivers/stubs','test')]]
    runtime_build={'compiler':subprocess.check_output([cxx,'--version'],text=True).splitlines()[0],'flags':cppflags,'sanitize':sanitize}
    if os.environ.get('BATTERY_REUSE_RUNTIME'):
        assert json.loads((output/'runtime-build.json').read_text())==runtime_build, 'Runtime build flags changed'
    else:
        (output/'runtime-build.json').write_text(json.dumps(runtime_build,indent=2)+'\n')
    if not os.environ.get('BATTERY_REUSE_RUNTIME'):
        objects=[]
        for index,source in enumerate([*[runtime/'src'/source for source in sources],ROOT/'test/shared_battery/runtime_test.cpp']):
            obj=output/f'runtime{index}.o'
            subprocess.run([cxx,*cppflags,'-c',str(source),'-o',str(obj)],check=True)
            objects.append(obj)
        subprocess.run([cxx,*san,'-rdynamic',*map(str,objects),str(output/'hardware.o'),
                        *[str(output/f'time{i}.o') for i in range(3)],'-ldl','-o',str(executable)],check=True)
    env=dict(os.environ)
    env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    results=[]
    app_hash=hashlib.sha256((common/'battery.elf').read_bytes()).hexdigest()
    for profile,native_width,native_height in (('paper',800,480),('watch',240,240),('amoled',410,502)):
        staging=output/profile
        shutil.copytree(common,staging,dirs_exist_ok=True,copy_function=link_replace)
        profile_flags=['-DSCENE_PROFILE_PAPER=1','-DSCENE_DISPLAY_ROTATION=90'] if native_width==800 else []
        build(staging/'scene-presentation-profile.elf',[system/'Services/scene_profile/profile.c'],profile_flags)
        for contract in ('preferences','atomic'):
          for mode in ('demand','retained-providers','no-ui'):
              fixture=output/f'{profile}-{contract}-{mode}'
              contract_env=dict(env)
              contract_env.pop('BATTERY_ATOMIC',None)
              if contract=='atomic':contract_env['BATTERY_ATOMIC']='1'
              shutil.copytree(staging,fixture,dirs_exist_ok=True,copy_function=link_replace)
              assert hashlib.sha256((fixture/'battery.elf').read_bytes()).hexdigest()==app_hash
              subprocess.run([str(executable),str(fixture),str(native_width),str(native_height),mode],env=contract_env,check=True)
              results.append({'profile':profile,'activation':mode,'storage_contract':contract,'width':native_width,'height':native_height,'app_sha256':app_hash,'passed':True})
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in provenance.items()), 'Source changed during qualification'
    (output/'results.json').write_text(json.dumps({'cases':results,'sanitize':sanitize,'scope':'actual host Runtime/provider/app; physical devices doubled','hardware':'not tested'},indent=2)+'\n')
    print('Battery actual Runtime paths passed; hardware doubled, no physical acceptance.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--system-apps',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True,exist_ok=bool(os.environ.get('BATTERY_REUSE_RUNTIME')))
        run(args.runtime.resolve(),args.system_apps.resolve(),args.output.resolve(),args.sanitize)
    else:
        with tempfile.TemporaryDirectory(prefix='risc-scene-runtime-') as temporary:
            run(args.runtime.resolve(),args.system_apps.resolve(),Path(temporary),args.sanitize)
