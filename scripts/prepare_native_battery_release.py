#!/usr/bin/env python3
"""Bind complete Battery qualification to a standalone development package.

No hardware, installation compatibility, or firmware replacement is implied.
The version belongs to the independently published manifest and exact ELF hash.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

import build_native_battery_qualification as recipe

TAG = 'native-battery-1.2.0-dev1'
VERSION = '1.2.0'
ROOT = recipe.ROOT
MANIFEST = 'Apps/native/battery.json'
REDUCED_PRODUCTION_INPUTS = {
    *('utilities:Apps/'+name for name in ('battery.c','battery_power.inc','battery_native_layout.h','battery_native_view.inc','utility_frame.h')),
    'system:lib/PortableApps/src/adapter.c', 'system:lib/PortableApps/src/battery_native_input.inc',
    'system:lib/PortableApps/include/PortableBatteryNative.h',
}


def require(ok, text):
    if not ok:
        raise ValueError(text)


def read(path):
    return json.loads(path.read_text())


def expected_manifest():
    return {'type': 'application', 'id': 'battery', 'version': VERSION,
            'architecture': 'xtensa-esp32s3', 'file_name': 'battery.elf', 'entry': 'app_main',
            'requires': [{'capability': name, 'api': api} for name, api in recipe.CAPABILITIES]}


def validate_controls(root, target):
    """Fail closed on missing, mismatched, incomplete or failed host controls."""
    target_sha = recipe.digest((root/'target/qualification.json').read_bytes())
    native = next(m for m in target['modules'] if m['mode'] == 'native')
    proofs = {}
    for name, original, off, snapshot, sanitized, count in [
        ('temporal-normal', False, False, False, False, 57),
        ('temporal-sanitized', False, False, False, True, 57),
        ('snapshot-normal', False, False, True, False, 57),
        ('snapshot-sanitized', False, False, True, True, 57),
        ('original-normal', True, False, False, False, 13),
        ('original-sanitized', True, False, False, True, 13),
        ('selector-off-normal', False, True, False, False, 13),
        ('selector-off-sanitized', False, True, False, True, 13),
    ]:
        path = root/name/'receipt.json'; r = read(path)
        require((r['original_failure_control'], r['selector_off'], r['snapshot'], r['sanitized']) ==
                (original, off, snapshot, sanitized), 'Wrong control mode: '+name)
        require(r['target_receipt_sha256'] == target_sha and r['source_unchanged'] and
                r['required_capabilities'] == target['required_capabilities'], 'Unbound temporal control: '+name)
        require(len(r['results']) == count and all(x['exit_code'] == 0 for x in r['results']), 'Failed/incomplete temporal control: '+name)
        expected = {(c, 240, 240) for c in (0,1,2,3,4,6,7,8,9,10,11,12,13)}
        if not (original or off):
            expected |= {(c,w,h) for c in range(14,25) for w,h in ((240,240),(410,502),(128,128),(320,180))}
        require({(x['case'], x['width'], x['height']) for x in r['results']} == expected,
                'Temporal coverage changed: '+name)
        for item in r['results']:
            require(recipe.digest((root/name/item['log']).read_bytes()) == item['log_sha256'], 'Changed temporal log')
        expected_pins = dict(recipe.PINS)
        if original:
            expected_pins['system'] = '9d2c55b32e88c0b12b1fa16200f399164767f5d1'
        require(r['external_git_pins'] == expected_pins, 'Temporal source pins differ: '+name)
        if not (original or off):
            require(native['source_sha256'].keys() <= r['compiled_dependency_sha256'].keys(),
                    'Missing full temporal production dependencies: '+name)
        if not original:
            for identity, sha in r['compiled_dependency_sha256'].items():
                if identity in native['source_sha256']:
                    require(sha == native['source_sha256'][identity], 'Temporal source differs: '+identity)
        proofs[name] = recipe.digest(path.read_bytes())
    native = next(m for m in target['modules'] if m['mode'] == 'native')
    for name, asynchronous, sanitized in [('host-normal',False,False), ('host-sanitized',False,True),
                                          ('async-normal',True,False), ('async-sanitized',True,True)]:
        path = root/name/'receipt.json'; r = read(path)
        require(r['sanitized'] == sanitized and r['async_rgb565'] == asynchronous and
                r['source_unchanged_during_run'] and len(r['results']) == 297, 'Incomplete UI matrix: '+name)
        expected_ui = {(c,w,h) for c in range(33) for w,h in
            ((128,128),(160,240),(240,240),(320,180),(410,502),(502,410),(320,480),(480,320),(800,480))}
        require({(x['case'],x['width'],x['height']) for x in r['results']} == expected_ui, 'UI coverage changed')
        require(REDUCED_PRODUCTION_INPUTS <= r['compiled_dependency_sha256'].keys(), 'Missing UI production dependencies')
        for identity, sha in r['compiled_dependency_sha256'].items():
            # Reduced UI fixtures use the same production app and adapter bytes.
            if identity in native['source_sha256']:
                require(sha == native['source_sha256'][identity], 'UI matrix source differs: '+identity)
        proofs[name] = recipe.digest(path.read_bytes())
    return proofs


def qualify(args):
    root = args.artifact.resolve(); target = read(root/'target/qualification.json')
    source = recipe.git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    require(target['utilities_commit'] == source and not target['working_tree_dirty'] and
            not recipe.git(ROOT, 'status', '--porcelain').strip(), 'Release requires a clean exact source')
    require(target['external_git_pins'] == recipe.PINS and target['legacy_exact_baseline_bytes'] and
            target['required_capabilities'] == expected_manifest()['requires'], 'Incomplete target recipe')
    require((root/'target/baseline/battery.elf').read_bytes() == (root/'target/legacy/battery.elf').read_bytes(),
            'Selector-off differs from baseline')
    for relative, sha in target['recipe_sha256'].items():
        require(recipe.digest((ROOT/relative).read_bytes()) == sha, 'Recipe changed: '+relative)
    controls = validate_controls(root, target)
    native = next(m for m in target['modules'] if m['mode'] == 'native')
    app = root/'app'; app.mkdir(exist_ok=False)
    elf = root/'target/native/battery.elf'
    require(recipe.digest(elf.read_bytes()) == native['sha256'] and elf.stat().st_size == native['size_bytes'],
            'Target ELF changed')
    require(native['build_defines'] == recipe.define_arguments(True), 'Native selectors changed')
    require(set(native['exports']) == recipe.EXPORTS and set(native['imports']) <= recipe.IMPORTS, 'Import/export drift')
    require(read(ROOT/MANIFEST) == expected_manifest(), 'Standalone manifest changed')
    shutil.copyfile(elf, app/'battery.elf'); shutil.copyfile(ROOT/MANIFEST, app/'battery.json')
    shutil.copyfile(root/'target/native/command.json', app/'command.json')
    require(recipe.digest((app/'command.json').read_bytes()) == native['command_sha256'], 'Command changed')
    # Use the exact existing shared production inspector, not a copied fork.
    reporter = root/'admission/report_native_settings_memory.py'; reporter.parent.mkdir()
    data = recipe.git(args.system, 'show', recipe.PINS['system']+':scripts/report_native_settings_memory.py')
    reporter.write_bytes(data)
    spec = importlib.util.spec_from_file_location('battery_shared_admission', reporter)
    shared = importlib.util.module_from_spec(spec); spec.loader.exec_module(shared)
    require(shared.RUNTIME_PIN == recipe.PINS['runtime'], 'Admission Runtime mismatch')
    layout = shared.inspect(app/'battery.elf')
    (app/'elf-layout.json').write_text(json.dumps(layout,indent=2)+'\n')
    runtime = reporter.parent/'runtime'; hashes = {}
    for relative in shared.RUNTIME_INPUTS:
        data = recipe.git(args.runtime,'show',recipe.PINS['runtime']+':'+relative)
        path = runtime/relative; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
        hashes[relative] = recipe.digest(data)
    (reporter.parent/'admit.cpp').write_text(shared.ADMISSION_SOURCE)
    includes = ['-I'+str(runtime/x) for x in ('test/native_apps/stubs','lib/elf_loader/include','src')]
    runs = []
    for sanitized in (False,True):
        flags = ['-O1','-g','-Wall','-Wextra','-Werror']
        if sanitized:
            flags += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
        obj = reporter.parent/'validate.o'; binary = reporter.parent/'admit'
        commands = [[os.environ.get('CC','cc'),'-std=c11',*flags,*includes,'-c',str(runtime/shared.RUNTIME_INPUTS[0]),'-o',str(obj)],
                    [os.environ.get('CXX','c++'),'-std=c++17',*flags,*includes,str(reporter.parent/'admit.cpp'),str(obj),'-o',str(binary)]]
        for command in commands:
            subprocess.run(command,check=True,timeout=120)
        p = subprocess.run([str(binary),str(app/'battery.elf')],check=True,capture_output=True,text=True,timeout=30,
                           env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
        result = json.loads(p.stdout)
        require(all(result.get(k) is True for k in ('ordinary_application_admitted','missing_import_rejected',
                'unsafe_state_rejected','driver_role_rejected')), 'Production admission failed')
        require(result['alignment_placements_checked'] > 0 and result['data_allocation_request_bytes'] ==
                layout['section_loader']['data_allocation_request_bytes'], 'Production placement mismatch')
        (reporter.parent/f'native-{int(sanitized)}.json').write_text(p.stdout)
        runs.append({'sanitized':sanitized,'commands':commands,'result':result})
        obj.unlink(); binary.unlink()
    admission = {'runtime_commit':recipe.PINS['runtime'],'runtime_source_sha256':hashes,
                 'shared_inspector_sha256':recipe.digest(reporter.read_bytes()),'runs':runs,
                 'elf_sha256':native['sha256'],'layout_sha256':recipe.digest((app/'elf-layout.json').read_bytes()),
                 'manifest_sha256':recipe.digest((app/'battery.json').read_bytes()),
                 'linked_device_memory_fit_qualified':False,'target_instructions_executed':False}
    (app/'elf-memory-admission.json').write_text(json.dumps(admission,indent=2)+'\n')
    # Preserve every compiled production dependency and exact recipe source.
    source_files = {}
    for identity, expected in native['source_sha256'].items():
        owner, relative = identity.split(':',1)
        path = ROOT/relative if owner == 'utilities' else root/'target/inputs'/owner/relative
        if owner == 'utilities' and not path.is_file():
            path = root/'target/inputs/utilities-baseline'/relative
        content = path.read_bytes()
        require(recipe.digest(content) == expected, 'Production dependency changed: '+identity)
        dest = root/'source'/owner/relative; dest.parent.mkdir(parents=True,exist_ok=True); dest.write_bytes(content)
        source_files[str(dest.relative_to(root))] = expected
    listing = recipe.git(ROOT,'ls-tree','-r','--name-only',source,'--','scripts','profiles','docs/native-battery','test/native_apps','tests','LICENSE').decode().splitlines()
    for relative in listing:
        content = recipe.git(ROOT,'show',source+':'+relative)
        dest = root/'source/utilities'/relative; dest.parent.mkdir(parents=True,exist_ok=True); dest.write_bytes(content)
        source_files[str(dest.relative_to(root))] = recipe.digest(content)
    for owner,pin in recipe.PINS.items():
        # External available notices were already staged by the target builder.
        notice = root/'target/inputs'/owner/'LICENSE'
        if notice.is_file():
            dest = root/'source'/owner/'LICENSE';dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(notice,dest)
            source_files[str(dest.relative_to(root))] = recipe.digest(dest.read_bytes())
    require(recipe.git(ROOT,'rev-parse','HEAD').decode().strip() == source and
            not recipe.git(ROOT,'status','--porcelain').strip(), 'Source changed during packaging')
    record = {'schema':1,'tag':TAG,'version':VERSION,'source_commit':source,'working_tree_dirty':False,
              'external_git_pins':recipe.PINS,'required_capabilities':expected_manifest()['requires'],
              'target_receipt_sha256':recipe.digest((root/'target/qualification.json').read_bytes()),
              'controls':controls,'source_files':source_files,'elf_sha256':native['sha256'],
              'elf_size_bytes':native['size_bytes'],'manifest_sha256':admission['manifest_sha256'],
              'admission_sha256':recipe.digest((app/'elf-memory-admission.json').read_bytes()),
              'layout_sha256':admission['layout_sha256'],'command_sha256':native['command_sha256'],
              'hardware_qualified':False,'firmware_replacement':False,'legacy_manifest_unchanged':True,
              'limitations':['No Xtensa execution, physical-device fit or installation-path qualification',
                'All 15 manifest providers and deployment-owned bindings required; preferences namespace1',
                'Optional app.catalog remains unavailable without a separately reviewed grant',
                'Existing color BLE entry remains deferred; paper-only behavior retained']}
    (app/'battery-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Native Battery1.2.0: exact-source standalone preparation and production admission PASS')
    return record


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact',type=Path,required=True)
    parser.add_argument('--system',type=Path,required=True)
    parser.add_argument('--runtime',type=Path,required=True)
    qualify(parser.parse_args())
