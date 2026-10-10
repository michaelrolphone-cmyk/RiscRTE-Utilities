#!/usr/bin/env python3
"""Build one intent-driven Alarms ELF and raw-RTC/native-UTC domain providers."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
from scene_support import ROOT,sdk,time_sources


def run(runtime: Path, system: Path, output: Path) -> None:
    if output.exists():
        raise FileExistsError(f'Use a new output directory: {output}')
    output.mkdir(parents=True)
    spec=importlib.util.spec_from_file_location('scene_build',system/'scripts/scene_build.py')
    if not spec or not spec.loader:raise ValueError('Missing scene target builder')
    builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
    compiler=builder.compiler_path()
    with tempfile.TemporaryDirectory(prefix='alarms-scene-sdk-') as temporary:
        include=sdk(runtime,system,Path(temporary))
        app=json.loads((ROOT/'Apps/alarms_scene.json').read_text())
        rows=[builder.build(compiler,include,output/'alarms',app,[ROOT/'Apps/alarms_scene.c'])]
        for name,native in (('alarm-control-raw',False),('alarm-control-utc',True)):
            service=json.loads((ROOT/'Services/alarm_control'/('utc.json' if native else 'raw.json')).read_text())
            if service['id']!=name:raise ValueError('Alarm control package identity mismatch')
            flags=['-DALARM_NATIVE_UTC'] if native else ['-DPORTABLE_RTC_UTC8_DENVER']
            flags += [f'-DALARM_CONTROL_ID="{name}"']
            row=builder.build(compiler,include,output/name,service,[ROOT/'Services/alarm_control/control.c',
                       *map(Path,time_sources(system))],flags);row['clock_policy']=name;rows.append(row)
        # The application is deliberately compiled without any profile flags.
        # Building a second time in another directory verifies reproducibility.
        duplicate=Path(temporary)/'rebuild'
        repeated=builder.build(compiler,include,duplicate,app,[ROOT/'Apps/alarms_scene.c'])
        if rows[0]['sha256']!=repeated['sha256']:raise ValueError('Alarms ELF is not reproducible')
        validator=Path(temporary)/'validate-elf'
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
                        '-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),
                        str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),
                        str(ROOT/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
        for elf in output.rglob('*.elf'):subprocess.run([str(validator),str(elf)],check=True)
        selected={}
        for base,label in ((runtime,'Runtime'),(system,'System'),(ROOT,'Utilities')):
            for prefix in ('sdk','lib/Alarm/include','Services/scene_host','Services/scene_profile','Services/alarm_control'):
                for path in (base/prefix).rglob('*'):
                    if path.is_file() and path.suffix in ('.c','.h','.inc','.json'):
                        selected[label+'/'+str(path.relative_to(base))]=hashlib.sha256(path.read_bytes()).hexdigest()
        for path in [ROOT/'Apps/alarms_scene.c',ROOT/'Apps/alarms_scene.json',*map(Path,time_sources(system))]:
            base,label=(ROOT,'Utilities') if path.is_relative_to(ROOT) else (system,'System')
            selected[label+'/'+str(path.relative_to(base))]=hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (system/'lib/PortableApps/include').glob('PortableTime*.h'):
            selected['System/'+str(path.relative_to(system))]=hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (system/'lib/PortableApps/time').rglob('*'):
            if path.is_file():selected['System/'+str(path.relative_to(system))]=hashlib.sha256(path.read_bytes()).hexdigest()
    builder.json_write(output/'packages.json',{'schema':1,'packages':rows,
                      'same_application_for_both_profiles':True,'source_sha256':selected})
    print('Built and validated the identical Alarms application plus two clock-policy services.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve())
