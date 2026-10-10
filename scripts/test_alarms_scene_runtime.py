#!/usr/bin/env python3
"""Exercise actual unloadable Alarms/provider ELFs with the production Runtime."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from scene_support import ROOT, sdk, time_sources


def run(runtime: Path, system: Path, output: Path, sanitize: bool) -> None:
    include = sdk(runtime, system, output/'sdk')
    cc, cxx = os.environ.get('CC', 'cc'), os.environ.get('CXX', 'c++')
    san = (['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer'] if sanitize else ['-g'])
    cflags = [*san, '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', '-I'+str(include)]
    shared = ['-fPIC', '-fvisibility=hidden', '-shared']
    if os.uname().sysname == 'Darwin':
        shared += ['-undefined', 'dynamic_lookup']
    def build(destination: Path, sources: list[Path | str], defines: list[str] = [], dso: bool = True):
        subprocess.run([cc, *cflags, *(shared if dso else ['-c']), *defines,
                        *map(str, sources), '-o', str(destination)], check=True)
    common = output/'common'
    common.mkdir()
    build(common/'alarms.elf', [ROOT/'Apps/alarms_scene.c', ROOT/'test/scene/runtime_app_hooks.c'])
    build(common/'launcher.elf', [ROOT/'test/scene/runtime_launcher.c'])
    build(common/'scene-host.elf', [system/'Services/scene_host/host.c'])
    hardware = (('display','display.output',1),('touch','input.touch.raw',1),
                ('navigation','input.navigation',1),('rtc','rtc.clock',2),
                ('haptic','haptic.effect',1),('audio','audio.output',1))
    for index,(name,capability,api) in enumerate(hardware):
        build(common/(name+'.elf'), [ROOT/'test/scene/runtime_provider.c'],
              [f'-DPROVIDER_INDEX={index}',f'-DPROVIDER_ID="{name}"',
               f'-DPROVIDER_CAP="{capability}"',f'-DPROVIDER_API={api}'])
    build(output/'hardware.o',[ROOT/'test/scene/runtime_hardware.c'],dso=False)
    # Host helper's time conversion is linked once into the executable, not any
    # application. The product application itself remains exactly the same DSO.
    for index,source in enumerate(time_sources(system)):
        build(output/f'time{index}.o',[source],dso=False)
    sources = ('bootstrap/Json.cpp', 'bootstrap/Board.cpp', 'bootstrap/Runtime.cpp',
               'runtime/streams/AppStreamSessions.cpp', 'runtime/streams/ProviderQueueHost.cpp',
               'runtime/drivers/ProviderGraphV2.cpp', 'runtime/drivers/ProviderModuleV2.cpp',
               'runtime/storage/AppDataFiles.cpp')
    executable=output/'runtime-test'
    subprocess.run([cxx,*san,'-std=c++17','-O0' if sanitize else '-O1','-Wall','-Wextra','-Werror',
                    '-Wno-missing-field-initializers','-DRISC_RUNTIME_PROVIDER_CAPACITY=28','-rdynamic','-I'+str(include),
                    *['-I'+str(runtime/path) for path in ('src','lib/ArduinoJson/src','test/drivers/stubs')],
                    *[str(runtime/'src'/source) for source in sources],
                    str(ROOT/'test/scene/runtime_test.cpp'),str(output/'hardware.o'),
                    *[str(output/f'time{i}.o') for i in range(3)],'-ldl','-o',str(executable)],check=True)
    env=dict(os.environ)
    env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    for profile in ('watch','paper'):
        staging=output/profile
        shutil.copytree(common,staging)
        native=profile=='paper'
        profile_flags=['-DSCENE_PROFILE_PAPER=1','-DSCENE_DISPLAY_ROTATION=90'] if native else []
        build(staging/'scene-presentation-profile.elf',[system/'Services/scene_profile/profile.c'],profile_flags)
        control_flags=['-DALARM_NATIVE_UTC'] if native else ['-DPORTABLE_RTC_UTC8_DENVER']
        build(staging/'alarm-control.elf',[ROOT/'Services/alarm_control/control.c',*time_sources(system)],control_flags)
        scheduler_flags=['-DPOINTS_IN_TIME_SERVICE','-DALARM_DND_CONTROL']
        scheduler_flags += (['-DALARM_NATIVE_UTC','-DALARM_VISUAL_ONLY','-DALARM_SERVICE_TAGGED_V2']
                            if native else ['-DALARM_VOLUME_CONTROL'])
        build(staging/'alarm-service.elf',[ROOT/'Services/alarm_service/service.c',*time_sources(system)],scheduler_flags)
        for mode in ('demand','retained-providers','headless'):
            fixture=output/f'{profile}-{mode}'
            shutil.copytree(staging,fixture)
            try:
                subprocess.run([str(executable),str(fixture),profile,mode],env=env,check=True)
            except subprocess.CalledProcessError:
                # Generated fixtures contain no user data. Preserve the exact
                # admission inputs in CI evidence rather than hiding bootstrap failures.
                for manifest in sorted(fixture.glob('*.json')):
                    print(f'FAILED FIXTURE {manifest.name}: {manifest.read_bytes()!r}', flush=True)
                raise
    print('6 production Runtime/provider/app executions passed; real host ELF unload/reload, not hardware execution.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--system-apps',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True,exist_ok=False)
        run(args.runtime.resolve(),args.system_apps.resolve(),args.output.resolve(),args.sanitize)
    else:
        with tempfile.TemporaryDirectory(prefix='risc-scene-runtime-') as temporary:
            run(args.runtime.resolve(),args.system_apps.resolve(),Path(temporary),args.sanitize)
