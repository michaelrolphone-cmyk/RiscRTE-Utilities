#!/usr/bin/env python3
"""Bind complete Alarms qualification to a standalone development package.

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

import build_native_alarms_qualification as recipe

TAG = 'native-alarms-0.3.0-dev1'
VERSION = '0.3.0'
ROOT = recipe.ROOT
MANIFEST = 'Apps/native/alarms.json'
REDUCED_PRODUCTION_INPUTS = {
    *('utilities:Apps/'+name for name in ('alarms.c', 'countdown.c', 'alarm_app_shared.h',
      'alarm_nova.inc', 'alarm_writer.h', 'alarms_native_layout.h', 'alarms_native_view.inc', 'utility_frame.h')),
    'system:lib/PortableApps/src/adapter.c', 'system:lib/PortableApps/src/alarms_native_input.inc',
    'system:lib/PortableApps/include/PortableAlarmsNative.h',
}
CONTROL_PROFILE = 'profiles/native-alarms-controls.json'
RELEASE_WORKFLOW = '.github/workflows/native-alarms.yml'
HOST_SIZES = ((120,128),(128,128),(160,128),(160,240),(240,240),(410,502),(502,410),(320,180),(800,480))
HOST_CASES = ('geometry-editor-persistence',)
CAPTURES = {f'{app}-{w}x{h}-page{page}.png' for app in recipe.APPLICATIONS
            for w,h in ((128,128),(240,240),(410,502),(320,180))
            for page in (range(4) if app == 'alarms' else range(3))}


def require(ok, text):
    if not ok:
        raise ValueError(text)


def read(path):
    return json.loads(path.read_text())


def expected_manifest():
    return {'type': 'application', 'id': 'alarms', 'version': VERSION,
            'architecture': 'xtensa-esp32s3', 'file_name': 'alarms.elf', 'entry': 'app_main',
            'requires': [{'capability': name, 'api': api} for name, api in recipe.CAPABILITIES]}


def target_modules(target):
    records = target['modules']
    expected = {(app, mode) for app in recipe.APPLICATIONS for mode in ('baseline','legacy','native')}
    require(len(records) == len(expected) and {(r['app'],r['mode']) for r in records} == expected,
            'Incomplete/duplicate Alarms and Countdown target matrix')
    return {(r['app'],r['mode']):r for r in records}


def checked_path(root, relative):
    path = Path(relative)
    require(not path.is_absolute() and '..' not in path.parts and bool(path.parts), 'Invalid evidence path')
    resolved = (root/path).resolve()
    require(resolved.is_relative_to(root.resolve()), 'Evidence escapes artifact root')
    return resolved


def validate_target(root, target):
    modules = target_modules(target)
    require(target['external_git_pins'] == recipe.PINS and target['legacy_exact_baseline_bytes'] and
            target['legacy_applications'] == list(recipe.APPLICATIONS) and
            target['required_capabilities'] == expected_manifest()['requires'], 'Incomplete target recipe')
    require(target['external_working_trees_used'] is False and target['historical_dot59_elf_reproduction'] is False
            and target['hardware_qualified'] is False and target['firmware_assembled'] is False and
            target['published'] is False, 'Unsupported target qualification claim')
    for (app,mode), record in modules.items():
        elf = root/'target'/mode/(app+'.elf')
        data = elf.read_bytes(); recipe.elf_contract(data)
        require(recipe.digest(data) == record['sha256'] and len(data) == record['size_bytes'], 'Target ELF changed: '+app+'/'+mode)
        require(record['native_ui'] == (mode == 'native') and record['build_defines'] == recipe.define_arguments(mode == 'native'),
                'Native selectors changed: '+app+'/'+mode)
        require(set(record['exports']) == recipe.EXPORTS and set(record['imports']) <= recipe.IMPORTS and
                record['structural_loader_admission'] == 'passed' and record['target_instructions_executed'] is False,
                'Target admission changed: '+app+'/'+mode)
        require(recipe.required_closure(app) <= record['source_sha256'].keys(), 'Missing full target dependency closure')
        if mode == 'native':
            require(recipe.NATIVE_CLOSURE <= record['source_sha256'].keys(), 'Missing native target dependency closure')
        command = root/'target'/mode/(app+'-command.json')
        require(recipe.digest(command.read_bytes()) == record['command_sha256'] and
                [x for x in read(command) if x.startswith('-D')] == record['build_defines'], 'Target command changed')
        symbols = (root/'target'/mode/(app+'-dynamic-symbols.txt')).read_text()
        require(recipe.symbol_contract(symbols) == (record['imports'], record['exports']), 'Target symbol evidence changed')
    for app in recipe.APPLICATIONS:
        require((root/'target/baseline'/(app+'.elf')).read_bytes() == (root/'target/legacy'/(app+'.elf')).read_bytes(),
                'Selector-off differs from baseline: '+app)
    return modules


def validate_controls(root, target):
    """Fail closed on absent cases, source drift, detached receipts or failed logs."""
    target_sha = recipe.digest((root/'target/qualification.json').read_bytes())
    modules = target_modules(target)
    native_sources = {}
    for app in recipe.APPLICATIONS:
        for key, sha in modules[(app,'native')]['source_sha256'].items():
            require(key not in native_sources or native_sources[key] == sha, 'Conflicting native source closure')
            native_sources[key] = sha
    contract = read(ROOT/CONTROL_PROFILE)
    require(contract['schema'] == 1 and contract['temporal'], 'Full-feature temporal coverage contract is not finalized')
    proofs = {}
    expectations = []
    for asynchronous in (False,True):
        for sanitized in (False,True):
            name = ('async' if asynchronous else 'host')+'-'+('sanitized' if sanitized else 'normal')
            expected = {(app,c,w,h) for app in recipe.APPLICATIONS for c in HOST_CASES for w,h in HOST_SIZES}
            expectations.append((name, asynchronous, sanitized, expected, False, None))
    for control in contract['temporal']:
        require(control['cases'] and control['sizes'] and control['applications'] == list(recipe.APPLICATIONS),
                'Incomplete temporal coverage contract')
        require(control['mode'] in ('temporal','snapshot','selector-off','original'), 'Unknown temporal mode')
        for sanitized in (False,True):
            expected = {(app,c,w,h) for app in control['applications'] for c in control['cases'] for w,h in control['sizes']}
            name = control['mode']+'-'+('sanitized' if sanitized else 'normal')
            expectations.append((name, True, sanitized, expected, True, control))
    require(len({x[0] for x in expectations}) == len(expectations), 'Duplicate control receipts')
    for name, asynchronous, sanitized, expected, full, control in expectations:
        path = root/name/'receipt.json'; r = read(path)
        require(r['sanitized'] == sanitized and r['async_rgb565'] == asynchronous and r['source_unchanged'] is True,
                'Wrong or incomplete control mode: '+name)
        require(r['target_receipt_sha256'] == target_sha and r['required_capabilities'] == target['required_capabilities'],
                'Unbound control receipt: '+name)
        pins = dict(recipe.PINS)
        if control is not None:
            require(r['mode'] == control['mode'], 'Wrong temporal mode: '+name)
            pins.update(control.get('external_pin_overrides', {}))
        require(r['external_git_pins'] == pins, 'Control source pins differ: '+name)
        results = r['results']
        require(len(results) == len(expected) and all(x['exit_code'] == 0 for x in results), 'Failed/incomplete control: '+name)
        require({(x['app'],x['case'],x['width'],x['height']) for x in results} == expected, 'Control coverage changed: '+name)
        for item in results:
            log = checked_path(root/name, item['log'])
            require(recipe.digest(log.read_bytes()) == item['log_sha256'], 'Changed control log: '+name)
        if name == 'host-normal':
            captures = r.get('capture_sha256', {})
            require(set(captures) == CAPTURES, 'Incomplete provider captures')
            for relative, sha in captures.items():
                require(recipe.digest(checked_path(root/name,relative).read_bytes()) == sha, 'Changed provider capture')
        compiled = r['compiled_dependency_sha256']
        runner = 'utilities:scripts/test_alarms_native'+('_temporal' if full else '')+'.py'
        require(runner in compiled, 'Missing qualification runner provenance')
        expected_sources = native_sources
        if control is not None and control['mode'] == 'selector-off':
            expected_sources = {}
            for app in recipe.APPLICATIONS:
                expected_sources.update(modules[(app,'legacy')]['source_sha256'])
        if control is not None and control['mode'] == 'original':
            require(control.get('source_sha256'), 'Original control lacks exact source contract')
            expected_sources = control['source_sha256']
        required = set(expected_sources) if full else REDUCED_PRODUCTION_INPUTS
        require(required <= compiled.keys(), 'Missing full production dependencies: '+name)
        for key, sha in compiled.items():
            if key in expected_sources:
                require(sha == expected_sources[key], 'Control production source differs: '+key)
            if key.startswith(('utilities:test/', 'utilities:tests/', 'utilities:scripts/', 'utilities:profiles/')):
                relative = key.split(':',1)[1]
                bundled = checked_path(root/'source/utilities',relative)
                data = bundled.read_bytes() if bundled.is_file() else recipe.git(ROOT,'show',target['utilities_commit']+':'+relative)
                require(recipe.digest(data) == sha, 'Control fixture source differs: '+key)
        proofs[name] = recipe.digest(path.read_bytes())
    return proofs


def qualify(args):
    root = args.artifact.resolve(); target = read(root/'target/qualification.json')
    source = recipe.git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    require(target['utilities_commit'] == source and not target['working_tree_dirty'] and
            not recipe.git(ROOT, 'status', '--porcelain').strip(), 'Release requires a clean exact source')
    modules = validate_target(root, target)
    for relative, sha in target['recipe_sha256'].items():
        require(recipe.digest((ROOT/relative).read_bytes()) == sha, 'Recipe changed: '+relative)
    controls = validate_controls(root, target)
    native = modules[('alarms','native')]
    app = root/'app'; app.mkdir(exist_ok=False)
    elf = root/'target/native/alarms.elf'
    require(recipe.digest(elf.read_bytes()) == native['sha256'] and elf.stat().st_size == native['size_bytes'],
            'Target ELF changed')
    require(native['build_defines'] == recipe.define_arguments(True), 'Native selectors changed')
    require(set(native['exports']) == recipe.EXPORTS and set(native['imports']) <= recipe.IMPORTS, 'Import/export drift')
    require(read(ROOT/MANIFEST) == expected_manifest(), 'Standalone manifest changed')
    shutil.copyfile(elf, app/'alarms.elf'); shutil.copyfile(ROOT/MANIFEST, app/'alarms.json')
    shutil.copyfile(root/'target/native/alarms-command.json', app/'command.json')
    require(recipe.digest((app/'command.json').read_bytes()) == native['command_sha256'], 'Command changed')
    # Use the exact existing shared production inspector, not a copied fork.
    reporter = root/'admission/report_native_settings_memory.py'; reporter.parent.mkdir()
    data = recipe.git(args.system, 'show', recipe.PINS['system']+':scripts/report_native_settings_memory.py')
    reporter.write_bytes(data)
    spec = importlib.util.spec_from_file_location('alarms_shared_admission', reporter)
    shared = importlib.util.module_from_spec(spec); spec.loader.exec_module(shared)
    require(shared.RUNTIME_PIN == recipe.PINS['runtime'], 'Admission Runtime mismatch')
    layouts = {}
    for app_name in recipe.APPLICATIONS:
        layouts[app_name] = shared.inspect(root/'target/native'/(app_name+'.elf'))
        (reporter.parent/(app_name+'-elf-layout.json')).write_text(json.dumps(layouts[app_name],indent=2)+'\n')
    shutil.copyfile(reporter.parent/'alarms-elf-layout.json', app/'elf-layout.json')
    runtime = reporter.parent/'runtime'; hashes = {}
    for relative in shared.RUNTIME_INPUTS:
        data = recipe.git(args.runtime,'show',recipe.PINS['runtime']+':'+relative)
        path = runtime/relative; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
        hashes[relative] = recipe.digest(data)
    (reporter.parent/'admit.cpp').write_text(shared.ADMISSION_SOURCE)
    includes = ['-I'+str(runtime/x) for x in ('test/native_apps/stubs','lib/elf_loader/include','src')]
    runs = {app_name:[] for app_name in recipe.APPLICATIONS}
    for sanitized in (False,True):
        flags = ['-O1','-g','-Wall','-Wextra','-Werror']
        if sanitized:
            flags += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
        obj = reporter.parent/'validate.o'; binary = reporter.parent/'admit'
        commands = [[os.environ.get('CC','cc'),'-std=c11',*flags,*includes,'-c',str(runtime/shared.RUNTIME_INPUTS[0]),'-o',str(obj)],
                    [os.environ.get('CXX','c++'),'-std=c++17',*flags,*includes,str(reporter.parent/'admit.cpp'),str(obj),'-o',str(binary)]]
        for command in commands:
            subprocess.run(command,check=True,timeout=120)
        for app_name in recipe.APPLICATIONS:
            p = subprocess.run([str(binary),str(root/'target/native'/(app_name+'.elf'))],check=True,capture_output=True,text=True,timeout=30,
                               env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
            result = json.loads(p.stdout)
            require(all(result.get(k) is True for k in ('ordinary_application_admitted','missing_import_rejected',
                    'unsafe_state_rejected','driver_role_rejected')), 'Production admission failed: '+app_name)
            require(result['alignment_placements_checked'] > 0 and result['data_allocation_request_bytes'] ==
                    layouts[app_name]['section_loader']['data_allocation_request_bytes'], 'Production placement mismatch: '+app_name)
            (reporter.parent/(app_name+'-native-'+str(int(sanitized))+'.json')).write_text(p.stdout)
            runs[app_name].append({'sanitized':sanitized,'commands':commands,'result':result})
        obj.unlink(); binary.unlink()
    memory_admissions = {}
    for app_name in recipe.APPLICATIONS:
        admission = {'app':app_name,'runtime_commit':recipe.PINS['runtime'],'runtime_source_sha256':hashes,
                     'shared_inspector_sha256':recipe.digest(reporter.read_bytes()),'runs':runs[app_name],
                     'elf_sha256':modules[(app_name,'native')]['sha256'],
                     'layout_sha256':recipe.digest((reporter.parent/(app_name+'-elf-layout.json')).read_bytes()),
                     'required_capabilities':expected_manifest()['requires'],
                     'linked_device_memory_fit_qualified':False,'target_instructions_executed':False}
        if app_name == 'alarms':
            admission['manifest_sha256'] = recipe.digest((app/'alarms.json').read_bytes())
        path = reporter.parent/(app_name+'-elf-memory-admission.json')
        path.write_text(json.dumps(admission,indent=2)+'\n')
        memory_admissions[app_name] = {'admission_sha256':recipe.digest(path.read_bytes()),
            'layout_sha256':admission['layout_sha256']}
    shutil.copyfile(reporter.parent/'alarms-elf-memory-admission.json',app/'elf-memory-admission.json')
    admission = read(app/'elf-memory-admission.json')
    # Preserve every compiled production dependency and exact recipe source.
    source_files = {}
    native_sources = {}
    for app_name in recipe.APPLICATIONS:
        native_sources.update(modules[(app_name,'native')]['source_sha256'])
    for identity, expected in native_sources.items():
        owner, relative = identity.split(':',1)
        path = ROOT/relative if owner == 'utilities' else root/'target/inputs'/owner/relative
        if owner == 'utilities' and not path.is_file():
            path = root/'target/inputs/utilities-baseline'/relative
        content = path.read_bytes()
        require(recipe.digest(content) == expected, 'Production dependency changed: '+identity)
        dest = root/'source'/owner/relative; dest.parent.mkdir(parents=True,exist_ok=True); dest.write_bytes(content)
        source_files[str(dest.relative_to(root))] = expected
    listing = recipe.git(ROOT,'ls-tree','-r','--name-only',source,'--','scripts','profiles','Apps/native/alarms.json','docs/native-alarms','test/native_apps','tests','LICENSE',RELEASE_WORKFLOW).decode().splitlines()
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
              'controls':controls,'controls_profile_sha256':recipe.digest((ROOT/CONTROL_PROFILE).read_bytes()),'source_files':source_files,'elf_sha256':native['sha256'],
              'elf_size_bytes':native['size_bytes'],'manifest_sha256':admission['manifest_sha256'],
              'admission_sha256':recipe.digest((app/'elf-memory-admission.json').read_bytes()),
              'memory_admissions':memory_admissions,
              'layout_sha256':admission['layout_sha256'],'command_sha256':native['command_sha256'],
              'hardware_qualified':False,'firmware_replacement':False,'legacy_manifest_unchanged':True,
              'limitations':['No Xtensa execution, physical-device fit or installation-path qualification',
                'All 15 manifest providers and deployment-owned bindings required; preferences namespace 1',
                'Optional app.catalog remains unavailable without a separately reviewed grant',
                'Countdown target and host controls are evidence only; no Countdown release/catalog entry is published']}
    (app/'alarms-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Native Alarms 0.3.0: exact-source standalone preparation and production admission PASS')
    return record


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact',type=Path,required=True)
    parser.add_argument('--system',type=Path,required=True)
    parser.add_argument('--runtime',type=Path,required=True)
    qualify(parser.parse_args())
