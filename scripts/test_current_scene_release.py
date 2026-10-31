#!/usr/bin/env python3
"""Exercise actual unloadable Alarms/provider ELFs with the production Runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from scene_support import ROOT, sdk, time_sources


def run(runtime: Path, system: Path, output: Path, sanitize: bool, headless_only: bool = False, scheduler_source: Path | None = None) -> None:
    include = sdk(runtime, system, output/'sdk')
    cc, cxx = os.environ.get('CC', 'cc'), os.environ.get('CXX', 'c++')
    san = (['-g1', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer'] if sanitize else ['-g1'])
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
    build(common/'scene-host.elf', [system/'Services/scene_host/host.c'])
    for index,(name,capability,api) in enumerate((('display','display.output',1),('touch','input.touch.raw',1),('navigation','input.navigation',1))):
        build(common/(name+'.elf'),[ROOT/'test/scene/runtime_provider.c'],[f'-DPROVIDER_INDEX={index}',f'-DPROVIDER_ID="{name}"',f'-DPROVIDER_CAP="{capability}"',f'-DPROVIDER_API={api}'])
    build(common/'launcher.elf', [ROOT/'test/scene/runtime_launcher.c'])
    build(common/'headless.elf', [ROOT/'Apps/alarms_headless.c'], ['-DALARM_HEADLESS_TEST'])
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
    subprocess.run([cxx,*san,'-std=c++17','-O0','-Wall','-Wextra','-Werror',
                    '-Wno-missing-field-initializers','-DRISC_RUNTIME_PROVIDER_CAPACITY=28','-rdynamic','-I'+str(include),
                    *['-I'+str(runtime/path) for path in ('src','lib/ArduinoJson/src','test/drivers/stubs','test')],
                    *[str(runtime/'src'/source) for source in sources],
                    str(ROOT/'test/scene/current_runtime_test.cpp'),str(output/'hardware.o'),
                    *[str(output/f'time{i}.o') for i in range(3)],'-ldl','-o',str(executable)],check=True)
    env=dict(os.environ)
    if scheduler_source: env['ALARM_SCENE_SCHEDULES_ONLY']='1'
    env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    results=[]
    app_hash=hashlib.sha256((common/'alarms.elf').read_bytes()).hexdigest()
    profiles = (('paper',None,None),) if env.get('ALARM_SCENE_COLOR_WIDTH') else (('paper',None,None),('watch',240,240),('amoled',410,502),('watch-raw',240,240))
    for profile,native_width,native_height in profiles:
        if scheduler_source and profile=='watch-raw':continue
        profile_env=dict(env)
        if native_width:
            profile_env['ALARM_SCENE_COLOR_WIDTH']=str(native_width)
            profile_env['ALARM_SCENE_COLOR_HEIGHT']=str(native_height)
        staging=output/profile
        shutil.copytree(common,staging)
        native=profile!='watch-raw' # Clock/service composition is independent of app geometry.
        profile_flags=['-DSCENE_PROFILE_PAPER=1','-DSCENE_DISPLAY_ROTATION=90'] if native and not profile_env.get('ALARM_SCENE_COLOR_WIDTH') else []
        build(staging/'scene-presentation-profile.elf',[system/'Services/scene_profile/profile.c'],profile_flags)
        if not native:
            for index,name,capability,api in ((3,'rtc','rtc.clock',2),(4,'haptic','haptic.effect',1),(5,'audio','audio.output',1)):
                build(staging/(name+'.elf'),[ROOT/'test/scene/runtime_provider.c'],[f'-DPROVIDER_INDEX={index}',f'-DPROVIDER_ID="{name}"',f'-DPROVIDER_CAP="{capability}"',f'-DPROVIDER_API={api}'])
        control_flags=['-DALARM_NATIVE_UTC','-DALARM_SERVICE_CLOCK_EDIT_V2'] if native else ['-DPORTABLE_RTC_UTC8_DENVER']
        build(staging/'alarm-control.elf',[ROOT/'Services/alarm_control/control.c',*time_sources(system)],control_flags)
        scheduler_flags=['-DPOINTS_IN_TIME_SERVICE','-DALARM_DND_CONTROL']
        if native:scheduler_flags += ['-DALARM_SERVICE_CLOCK_EDIT_V2','-DPOINTS_SERVICE_CONTEXT','-DALARM_NATIVE_MIXED_POINTS']
        scheduler_flags += (['-DALARM_NATIVE_UTC','-DALARM_VISUAL_ONLY','-DALARM_SERVICE_TAGGED_V2']
                            if native else ['-DALARM_VOLUME_CONTROL'])
        if scheduler_source:
            scheduler_flags=['-DALARM_NATIVE_UTC','-DALARM_NATIVE_SCHEDULES_ONLY','-DALARM_SERVICE_TAGGED_V2','-DALARM_VISUAL_ONLY','-DALARM_DND_CONTROL','-DALARM_SERVICE_CLOCK_EDIT_V2']
        scheduler_root=scheduler_source or ROOT
        build(staging/'alarm-service.elf',[scheduler_root/'Services/alarm_service/service.c',*time_sources(system)],scheduler_flags)
        for mode in ('demand','retained-providers','no-ui'):
            fixture=output/f'{profile}-{mode}'
            shutil.copytree(staging,fixture)
            try:
                assert hashlib.sha256((fixture/'alarms.elf').read_bytes()).hexdigest()==app_hash
                subprocess.run([str(executable),str(fixture),'paper' if native else 'raw',mode],env=profile_env,check=True)
                results.append({'profile':profile,'activation':mode,'clock_binding':'utc' if native else 'raw-calendar','width':int(profile_env.get('ALARM_SCENE_COLOR_WIDTH','800')),'height':int(profile_env.get('ALARM_SCENE_COLOR_HEIGHT',profile_env.get('ALARM_SCENE_COLOR_WIDTH','480'))),'app_sha256':app_hash,'passed':True})
            except subprocess.CalledProcessError:
                # Generated fixtures contain no user data. Preserve the exact
                # admission inputs in CI evidence rather than hiding bootstrap failures.
                for manifest in sorted(fixture.glob('*.json')):
                    print(f'FAILED FIXTURE {manifest.name}: {manifest.read_bytes()!r}', flush=True)
                raise
    (output/'results.json').write_text(json.dumps({'cases':results,'sanitize':sanitize,'scope':'actual host Runtime/provider/app; physical devices doubled','hardware':'not tested'},indent=2)+'\n')
    print(('Selected schedules-only' if scheduler_source else 'Current mixed-service')+' GUI/unload/continuation paths passed; host Runtime integration only, not physical acceptance.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--system-apps',type=Path,required=True)
    parser.add_argument('--scheduler-source',type=Path)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--headless-only',action='store_true')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:
        args.output.mkdir(parents=True,exist_ok=False)
        run(args.runtime.resolve(),args.system_apps.resolve(),args.output.resolve(),args.sanitize,args.headless_only,args.scheduler_source.resolve() if args.scheduler_source else None)
    else:
        with tempfile.TemporaryDirectory(prefix='risc-scene-runtime-') as temporary:
            run(args.runtime.resolve(),args.system_apps.resolve(),Path(temporary),args.sanitize,args.headless_only,args.scheduler_source.resolve() if args.scheduler_source else None)
