#!/usr/bin/env python3
"""Build canonical, device-neutral app ELFs once; reuse bytes in device selections.

Providers and deployment bindings are separate inputs, not app compiler switches.
The selections prove byte reuse, not hardware compatibility or deployment.
"""
import argparse
import hashlib
import importlib.util
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from lifecycle_dependencies import Closure

ROOT = Path(__file__).resolve().parents[1]
TARGETS = ('x4', 'twatch', 'waveshare')

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if not spec or not spec.loader:
        raise ValueError('Missing selected builder: ' + str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def local_path(root, value):
    path = (root / value).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError('Invalid source path: ' + value)
    return path

def inventory(root=ROOT):
    rows = json.loads((root/'utilities-manifest.json').read_text())['shared_scene_apps']
    ids, files = set(), set()
    if not rows:
        raise ValueError('Empty shared app inventory')
    for row in rows:
        manifest = json.loads(local_path(root, row['manifest_path']).read_text())
        if (manifest.get('type') != 'application' or
            manifest.get('architecture') != 'xtensa-esp32s3' or
            manifest.get('entry') != 'app_main' or
            any(manifest.get(key) != row.get(key) for key in ('id', 'version', 'file_name'))):
            raise ValueError('Shared app manifest identity mismatch')
        if row.get('runtime_profile') != 'shared-scene-v1' or row.get('defines', []):
            raise ValueError('Shared apps cannot carry device compiler profiles')
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', row['id']) or row['id'] in ids or row['file_name'] in files or row['file_name']=='build.elf' or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.elf', row['file_name']):
            raise ValueError('Duplicate or unsafe shared app identity')
        ids.add(row['id']); files.add(row['file_name'])
        for source in row['sources']:
            local_path(root, source)
        exports = set(row['exports'])
        if 'app_main' not in exports or exports - {'app_main', 'risc_resident_app_descriptor_v1'}:
            raise ValueError('Unexpected shared app export contract')
    return rows

def distribute(package, manifest, output):
    """Copy the single compiled module; validate complete content at each destination."""
    original = package/manifest['file_name']
    sidecar = package/original.with_suffix('.json').name
    if json.loads(sidecar.read_text()) != manifest:
        raise ValueError('Built manifest differs from selected app manifest')
    expected = digest(original)
    result = {}
    for target in TARGETS:
        dest = output/'selections'/target/manifest['id']
        dest.mkdir(parents=True, exist_ok=False)
        shutil.copyfile(original, dest/original.name)
        shutil.copyfile(package/original.with_suffix('.json').name, dest/original.with_suffix('.json').name)
        actual = digest(dest/original.name)
        if actual != expected or (dest/original.name).read_bytes() != original.read_bytes():
            raise ValueError('Shared ELF bytes changed during selection')
        result[target] = {'path': str((dest/original.name).relative_to(output)), 'sha256': actual}
    return result

def run(runtime, system, output):
    if output.exists():
        raise FileExistsError('Use a new output directory')
    rows = inventory()
    output.mkdir(parents=True)
    sys.path.insert(0, str(system/'scripts'))
    try:
        builder = load('selected_scene_build', system/'scripts/scene_build.py')
        sdk = load('selected_scene_sdk', system/'scripts/scene_sdk.py')
    finally:
        sys.path.pop(0)
    compiler = builder.compiler_path()
    results = []
    with tempfile.TemporaryDirectory(prefix='shared-scene-sdk-') as temp:
        temp = Path(temp)
        include = sdk.stage_sdk(runtime, system, temp)
        # Extra optional contracts are canonical owning-source bytes, never adapter code.
        for directory in (system/'lib/PortableApps/include', ROOT/'lib/Alarm/include', ROOT/'lib/Bluetooth/include'):
            for source in directory.glob('*.h'):
                dest = include/source.name
                if not dest.exists():
                    shutil.copyfile(source, dest)
        closure = Closure([('Runtime', runtime), ('System', system), ('Utilities', ROOT),
                           ('generated-sdk', temp)], output/'dependencies.json')
        original_run = subprocess.run
        def recorded(command, *args, **kwargs):
            if isinstance(command, (list, tuple)) and command and str(command[0]) == compiler and '-o' in command:
                closure.record(command)
            return original_run(command, *args, **kwargs)
        subprocess.run = recorded
        try:
            for row in rows:
                manifest = json.loads((ROOT/row['manifest_path']).read_text())
                sources = [ROOT/path for path in row['sources']]
                package = output/'packages'/row['id']
                built = builder.build(compiler, include, package, manifest, sources,
                                      defines=[], exports=set(row['exports']))
                repeated = builder.build(compiler, include, temp/('repeat-'+row['id']), manifest, sources,
                                         defines=[], exports=set(row['exports']))
                if built['sha256'] != repeated['sha256']:
                    raise ValueError('Shared ELF is not reproducible: ' + row['id'])
                built['reproducibility'] = {'sha256': repeated['sha256'], 'size_bytes': repeated['size_bytes'], 'matches': True}
                if digest(package/manifest['file_name']) != built['sha256']:
                    raise ValueError('Builder receipt differs from actual ELF')
                built['selections'] = distribute(package, manifest, output)
                if any(value['sha256'] != built['sha256'] for value in built['selections'].values()):
                    raise ValueError('Selection differs from build receipt')
                results.append(built)
        finally:
            subprocess.run = original_run
        validator = temp/'validate-elf'
        validator_command = [os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-I'+str(ROOT/'test/native_apps/stubs'), '-I'+str(runtime/'lib/elf_loader/include'),
                        str(runtime/'lib/elf_loader/src/esp_elf_validate.c'), str(ROOT/'test/native_apps/validate_test.c'),
                        '-o', str(validator)]
        closure.record(validator_command)
        original_run(validator_command, check=True)
        for row in rows:
            elf = output/'packages'/row['id']/row['file_name']
            original_run([str(validator), str(elf)], check=True)
    builder.json_write(output/'packages.json', {'schema': 1, 'packages': results,
        'application_defines': [], 'selection_targets': list(TARGETS),
        'selection_semantics': 'identical module bytes; providers and deployment bindings qualified separately',
        'physical_testing': 'not performed', 'deployment': 'not performed',
        'validator': {'source': 'Runtime/lib/elf_loader/src/esp_elf_validate.c', 'sha256': digest(runtime/'lib/elf_loader/src/esp_elf_validate.c')},
        'selected_builder_sha256': {name: digest(system/'scripts'/name) for name in ('scene_build.py','scene_sdk.py','native_toolchain.py')},
        'builder_sha256': digest(Path(__file__)), 'inventory_sha256': digest(ROOT/'utilities-manifest.json')})
    print('Built reproducible shared app ELFs; three selections reuse identical bytes.')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'system-apps', 'output'):
        parser.add_argument('--'+name, required=True, type=Path)
    args = parser.parse_args()
    run(args.runtime.resolve(), args.system_apps.resolve(), args.output.resolve())
