#!/usr/bin/env python3
"""Bind BLE Buttons's exact source/control evidence to a standalone development ELF.

Historical receipts remain intact: consumed bytes, not mutable checkout paths or
old HEAD labels, establish correspondence. No device or installation fit claim.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
from collections import defaultdict
import test_ble_buttons_native as host_control

import build_native_ble_buttons_qualification as recipe
from package_integrity import stamp_app_manifest

ROOT = recipe.ROOT
TAG = 'native-ble-buttons-0.1.30-dev1'
VERSION = '0.1.30-dev1'
PACKAGE = 'app'
MANIFEST = 'Apps/native/ble_buttons.json'
RECORD = 'ble_buttons-build-record.json'
PROFILES = ('128x128', '160x128', '240x240', '410x502', '320x180', '502x410', '480x800')
SCENARIOS = set(host_control.full_scenes(defaultdict(lambda: (0,0,1,1))))
HOST_MODES = ('normal-sync', 'sanitized-sync', 'normal-async', 'sanitized-async')
TEMPORAL_SCENARIOS = set(host_control.TEMPORAL_SCENARIOS)
CONTROL_RUNS = [(f'host/{mode}',mode.startswith('sanitized'),mode.endswith('async'),False,SCENARIOS) for mode in HOST_MODES]
CONTROL_RUNS += [(f'temporal/{mode}',mode == 'sanitized',True,True,TEMPORAL_SCENARIOS) for mode in ('normal','sanitized')]
HOST_INPUTS = {
    'utilities:Apps/ble_buttons.c', 'utilities:Apps/ble_hid_app.inc',
    'utilities:Apps/ble_buttons_native_layout.h', 'utilities:Apps/ble_buttons_native_view.inc',
    'system:lib/PortableApps/src/adapter.c',
}
EXTRA_SOURCE = (MANIFEST, 'scripts/build_native_ble_buttons_qualification.py',
    'scripts/prepare_native_ble_buttons_release.py', 'scripts/publish_native_ble_buttons.py',
    'scripts/test_ble_buttons_native.py', 'scripts/test_ble_buttons_native_contract.py',
    'tests/hid_buttons_native_test.c', 'tests/test_native_ble_buttons_release.py',
    '.github/workflows/native-ble-buttons.yml', 'docs/ble-buttons-native/packaging.md')
CONTROL_SELECTIONS = ('Apps', 'lib', 'scripts', 'tests', 'test', 'profiles',
                      'docs/ble-buttons-native', 'LICENSE', '.github/workflows/native-ble-buttons.yml')
LEGACY_SYSTEM = 'f1db70fcc3ba9df2a3551a711a9d3b0b714a5f8a'
DEPLOYED = {
    'version':'0.1.29', 'elf_bytes':154028,
    'elf_sha256':'b3f4633d8bc0d0612d6410eae3f41d8c66a0586025f076a26dd0f277cac7232d',
    'manifest_sha256':'95c2d0e393abffb9323fbb99723ccf419cb05a4fafc72608779ff6ba23369b68',
    'product_origin':'0.1.48', 'currently_delivered_product':'0.1.64',
    'historical_recipe_is_exact_deployed_build_provenance':False,
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def read(path):
    return json.loads(path.read_text())


def digest(path):
    return recipe.sha(path.read_bytes())


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2)+'\n')


def checked_path(root, relative):
    path = PurePosixPath(relative)
    require(not path.is_absolute() and relative and '\\' not in relative and
            all(p not in ('.','..','') for p in relative.split('/')), 'Unsafe evidence path: '+relative)
    result = root.joinpath(*path.parts)
    require(result.resolve().is_relative_to(root.resolve()), 'Evidence escapes bundle')
    return result


def expected_manifest():
    return {'architecture':'xtensa-esp32s3','display_name':'BLE Buttons','entry':'app_main',
        'file_name':'ble_buttons.elf','icon':'solid:f11c','id':'ble_buttons',
        'requires':[{'capability':c,'api':v} for c,v in recipe.CAPABILITIES],
        'type':'application','version':VERSION}


def native_module(target):
    require(len(target['modules']) == 3 and {m['mode'] for m in target['modules']} ==
            {'baseline','legacy','native'}, 'Incomplete target matrix')
    return next(m for m in target['modules'] if m['mode'] == 'native')


def source_identity(name, roots):
    """Accept owner-relative identity or a recorded absolute producer path."""
    if ':' in name and not name.startswith('/'):
        owner, relative = name.split(':',1)
        require(owner in {*recipe.PINS,'utilities','utilities-baseline','system-baseline','legacy-system','generated'}, 'Unknown host source owner')
        checked_path(Path('/source'),relative)
        return name
    if not name.startswith('/'):
        checked_path(Path('/source'),name)
        return 'utilities:'+name
    for owner, value in sorted(roots.items(),key=lambda item:len(Path(item[1]).parts),reverse=True):
        base = Path(value)
        if Path(name).is_relative_to(base):
            return owner+':'+Path(name).relative_to(base).as_posix()
    raise ValueError('Unbound host source: '+name)


def verify_source(root, identity, expected):
    owner, relative = identity.split(':', 1)
    require(owner in {*recipe.PINS,'utilities','utilities-baseline','toolchain','legacy-system','generated','system-baseline'}, 'Unknown source owner')
    path = checked_path(root/'source',owner+'/'+relative)
    require(digest(path) == expected, 'Changed source evidence: '+identity)


def validate_target(root):
    target = read(root/'target/qualification.json'); native = native_module(target)
    require(target['utilities_base'] == recipe.BASE and target['baseline_system_pin'] == recipe.BASE_SYSTEM and target['exact_external_git_pins'] == recipe.PINS,
            'Wrong target source pins')
    require(target['native_opt_in'] and target['native_selectors'] == recipe.NATIVE and
            target['selector_off_exact_baseline_bytes'] and target['legacy_manifest_exact_base'], 'Incomplete native/default contract')
    require(target['native_manifest'] == expected_manifest() and
            target.get('capability_contract',target.get('required_capabilities')) == expected_manifest()['requires'],
            'Wrong sixteen capability contract')
    require(not target['hardware_qualified'] and not target['firmware_assembled'], 'Unsupported target hardware claim')
    require(target['retained_flags_removed'] == [], 'Retained recipe flags drift')
    require(recipe.REQUIRED | recipe.NATIVE_REQUIRED <= native['source_sha256'].keys(), 'Incomplete compiled source closure')
    require(set(native['exports']) == recipe.EXPORTS and set(native['imports']) <= recipe.IMPORTS,
            'Native import/export drift')
    require(target.get('historical_recipe_current_deployed_bytes_proven') is False and
            target.get('predecessor_version') == recipe.PREDECESSOR_VERSION, 'Historical recipe provenance overstated')
    definitions = {}
    for module in target['modules']:
        folder = root/'target'/module['mode']; elf = folder/'ble_buttons.elf'
        require(digest(elf) == module['sha256'] and elf.stat().st_size == module['size_bytes'], 'Changed target ELF')
        require(digest(folder/'command.json') == module['command_sha256'], 'Changed target command')
        command = read(folder/'command.json')
        require(isinstance(command,list) and all(isinstance(arg,str) for arg in command), 'Invalid target command')
        definitions[module['mode']] = [arg for arg in command if arg.startswith('-D')]
        # The producer records the command itself. Historical reconciled local
        # receipts may additionally expose these derived fields; check them if
        # present, without requiring fields the real producer does not emit.
        require('build_defines' not in module or module['build_defines'] == definitions[module['mode']],
                'Recorded build defines differ from command')
        require('native_ui' not in module or module['native_ui'] == (module['mode'] == 'native'),
                'Recorded native mode differs')
        require(module['structural_loader_admission'] == 'passed' and not module['target_instructions_executed'],
                'Wrong target qualification scope')
        for identity, sha in module['source_sha256'].items():
            if module['mode'] == 'baseline' and identity.startswith('utilities:'):
                identity = identity.replace('utilities:','utilities-baseline:',1)
            if module['mode'] == 'baseline' and identity.startswith('system:'):
                identity = identity.replace('system:','system-baseline:',1)
            verify_source(root,identity,sha)
    require(definitions['baseline'] == definitions['legacy'] and
            definitions['native'] == definitions['legacy']+recipe.NATIVE, 'Native recipe selector drift')
    retained = read(root/'source/system'/recipe.RECIPE)['ble_buttons']
    require(definitions['baseline'] == [d for d in retained['command'] if d.startswith('-D')], 'Deployed feature selectors differ')
    require((root/'target/baseline/ble_buttons.elf').read_bytes() ==
            (root/'target/legacy/ble_buttons.elf').read_bytes(), 'Selector-off baseline bytes differ')
    verify_source(root,'utilities:scripts/build_native_ble_buttons_qualification.py',target['script_sha256'])
    verify_source(root,'system:'+recipe.RECIPE,target['retained_recipe_sha256'])
    require(target['retained_recipe_sha256'] == recipe.RECIPE_SHA256, 'Retained replay recipe changed')
    for identity, sha in target['recipe_sources'].items(): verify_source(root,identity,sha)
    require(set(target['generated_sources']) == set(recipe.GENERATED_NAMES), 'Missing replay fixtures')
    for name, proof in target['generated_sources'].items():
        owner,relative=proof['source'].split(':',1)
        row=read(root/'source'/owner/relative)[proof['key']]
        require(recipe.sha(row['text'].encode()) == row['template_sha256'] == row['sha256'] == proof['sha256'],
                'Generated fixture provenance differs')
        verify_source(root,'generated:'+name,proof['sha256'])
    require(digest(root/'source/utilities'/MANIFEST) == target['native_manifest_sha256'] and
            read(root/'source/utilities'/MANIFEST) == expected_manifest(), 'Native source manifest changed')
    require(digest(root/'source/utilities/Apps/ble_buttons.json') == target['legacy_manifest_sha256'] ==
            digest(root/'source/utilities-baseline/Apps/ble_buttons.json'), 'Legacy manifest changed')
    return target, native


def validate_controls(root, target):
    proofs = {}
    matrices = []
    for mode, sanitized, asynchronous, temporal, required_scenarios in CONTROL_RUNS:
        folder = root/mode; path = folder/'receipt.json'; r = read(path)
        require(r.get('sanitized') == sanitized and r.get('async_rgb565') == asynchronous and r.get('temporal') == temporal,
                'Host sanitizer/presentation mode differs')
        require(r.get('system_git_pin') == recipe.PINS['system'] and r.get('source_unchanged_during_run') is True,
                'Host pin/source stability differs')
        require(r.get('hardware_tested') is False and r.get('target_instructions_executed') is False,
                'Unsupported host hardware claim')
        expected_defines = [d for d in read(root/'target/native/command.json') if d.startswith('-D')]
        if not asynchronous: expected_defines = [d for d in expected_defines if d != '-DPORTABLE_ASYNC_RGB565']
        require(r.get('full_profile') is True and r.get('full_feature_profile') is True and
                r.get('system_working_tree_allowed') is False and r.get('target_qualification_sha256') == digest(root/'target/qualification.json'),
                'Host is not bound to complete deployed-feature target')
        require(r['build_defines'] == expected_defines, 'Full-feature host selectors differ')
        results = r['results']
        matrix = {(str(x['width'])+'x'+str(x['height']),x['scene']) for x in results}
        require(results and len(matrix) == len(results) and all(x['exit_code'] == 0 for x in results),
                'Failed/duplicate/incomplete host matrix: '+mode)
        require({p for p,_ in matrix} == set(PROFILES), 'Missing host profiles')
        scenarios = set(r['required_scenarios'])
        require(scenarios == required_scenarios and matrix == {(p,s) for p in PROFILES for s in required_scenarios}, 'Missing host scenarios')
        require(r.get('counterexamples') == [], 'Unresolved host temporal counterexamples')
        if not temporal: matrices.append(matrix)
        roots = r.get('source_roots',{})
        sources = {source_identity(n,roots):h for n,h in r['source_sha256'].items()}
        dependencies = {source_identity(n,roots):h for n,h in r['compiled_dependency_sha256'].items()}
        require(HOST_INPUTS <= dependencies.keys() and dependencies, 'Incomplete actual-adapter host dependencies')
        for name, sha in {**sources,**dependencies}.items():
            verify_source(root,name,sha)
            if name.startswith('generated:'):
                require(digest(checked_path(root/'target/inputs/generated',name.split(':',1)[1])) == sha, 'Changed host generated input')
        for result in results:
            log = checked_path(folder,result['log'])
            require(log.stat().st_size and digest(log) == result['log_sha256'], 'Changed/missing host log')
            require(temporal or result['captures'], 'Missing host captures')
            for capture in result['captures']:
                require(capture in r['capture_sha256'], 'Unbound host capture')
        for name,sha in r['capture_sha256'].items():
            require(digest(checked_path(folder,name)) == sha, 'Changed host capture')
        require(r['capture_sha256'], 'Missing host raster evidence')
        audit=r['native_pixel_audit']; title=audit['title_raster']
        require(len(title)==5 and title[1]>0 and title[2]>0 and title[3]>0 and audit['icon_rasters'], 'Incomplete native pixel audit')
        require(all(len(v)==2 and v[0]>0 for v in audit['icon_rasters'].values()), 'Incomplete icon pixel audit')
        proofs[mode] = digest(path)
    require(all(m == matrices[0] for m in matrices), 'Host matrices differ')
    path=root/'contracts/qualification.json'; r=read(path)
    require(r.get('system_git_pin') == recipe.PINS['system'] and r.get('stable_source_verified') is True,
            'Geometry/controller pin or source stability differs')
    require(r.get('hardware_tested') is False and r.get('physical_radio') is False, 'Unsupported contract hardware claim')
    require(len(r['cases'])==2 and {c['sanitized'] for c in r['cases']}=={False,True} and
            all(c['passed'] and all(profile in c['output'] for profile in PROFILES) for c in r['cases']),
            'Incomplete geometry/controller matrix')
    require(len(r['parity'])==4 and {(c['touchpad'],c['paper']) for c in r['parity']} ==
            {(pad,paper) for pad in (0,1) for paper in (False,True)} and
            all(c['identical'] for c in r['parity']), 'Incomplete selector-off token parity')
    for name,sha in r['source_hashes'].items(): verify_source(root,'utilities:'+name,sha)
    require(r['compiled_dependency_sha256'], 'Missing contract dependencies')
    for identity,sha in r['compiled_dependency_sha256'].items(): verify_source(root,identity,sha)
    require({'normal.log','sanitized.log'} <= r['log_sha256'].keys(), 'Missing contract run logs')
    for name,sha in r['log_sha256'].items():
        require(digest(checked_path(root/'contracts',name)) == sha, 'Changed contract log')
    require(r['command_sha256'], 'Missing contract commands')
    for name,sha in r['command_sha256'].items():
        require(digest(checked_path(root/'contracts',name)) == sha, 'Changed contract command')
    proofs['contracts']=digest(path)
    path = root/'existing-controls.json'; r = read(path)
    require(r['legacy_system_commit'] == LEGACY_SYSTEM, 'Legacy HID control pin differs')
    require(len(r['runs']) == 3 and {x['id'] for x in r['runs']} == {'models','legacy-renderer','paper-renderer'} and
            all(x['returncode'] == 0 for x in r['runs']), 'Existing HID controls failed')
    for run in r['runs']:
        require(digest(checked_path(root,run['log'])) == run['log_sha256'], 'Changed existing HID log')
    require(r['external_sources'], 'Missing legacy control source hashes')
    for name,sha in r['source_files'].items(): verify_source(root,'utilities:'+name,sha)
    for name,sha in r['external_sources'].items(): verify_source(root,name,sha)
    proofs['existing'] = digest(path)
    return proofs


def run_controls(args):
    root = args.artifact.resolve(); root.mkdir(parents=True,exist_ok=True)
    legacy_repo = args.legacy_system or args.system
    names = set()
    for selection in ('Apps','lib/Bluetooth/include','tests/hid*','test/native_apps/hid*',
                      'scripts/test_hid_apps.py','scripts/test_hid_renderer.py',
                      'scripts/prepare_native_ble_buttons_release.py'):
        for path in ROOT.glob(selection):
            files = [path] if path.is_file() else path.rglob('*')
            names.update(str(p.relative_to(ROOT)) for p in files if p.is_file() and '__pycache__' not in p.parts)
    hashes = {p:digest(ROOT/p) for p in sorted(names)}
    external = {}; staged = root/'control-inputs/legacy-system'
    for name in recipe.git(legacy_repo,'ls-tree','-r','--name-only',LEGACY_SYSTEM,'--',
                           'lib/PortableApps','lib/NativeApps','Apps').decode().splitlines():
        content = recipe.git(legacy_repo,'show',LEGACY_SYSTEM+':'+name)
        dest = staged/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(content)
        external['legacy-system:'+name] = recipe.sha(content)
    runs = []
    for label, script, extra in [('models','scripts/test_hid_apps.py',[]),
                                ('legacy-renderer','scripts/test_hid_renderer.py',[]),
                                ('paper-renderer','scripts/test_hid_renderer.py',['--paper'])]:
        command = [os.environ.get('PYTHON','python3'),str(ROOT/script),'--system-apps',str(staged),*extra]
        p = subprocess.run(command,capture_output=True,text=True,timeout=900,cwd=ROOT,
            env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
        log = root/(label+'.log');log.write_text(p.stdout+p.stderr)
        runs.append({'id':label,'script':script,'command':command,'returncode':p.returncode,
                     'log':log.name,'log_sha256':digest(log)})
        write(root/'existing-controls.json',{'legacy_system_commit':LEGACY_SYSTEM,
            'source_files':hashes,'external_sources':external,'runs':runs})
        require(p.returncode == 0,'Existing '+label+' controls failed; see '+str(log))
    require(all(digest(ROOT/p) == sha for p,sha in hashes.items()), 'Source changed during existing controls')


def memory_budgets(layout):
    loader = layout['section_loader']; sections = layout['sections']
    values = {'file_bytes':layout['file_size_bytes'],
        'code_bytes':loader['executable_allocation_request_bytes'],
        'bss_bytes':sum(s['size_bytes'] for s in sections if s['type'] == 8 and s['flags'] & 2),
        'data_with_alignment_reserve_bytes':loader['data_allocation_request_bytes'],
        'resident_section_allocation_bytes':loader['resident_section_allocation_request_bytes'],
        'allocated_section_bytes':layout['shf_alloc_size_sum_bytes']}
    limits = {'package_file_bytes':1024*1024,'ordinary_application_file_bytes':2097152,
              'structural_image_bytes':8*1024*1024,'manifest_bytes':2048}
    require(52 <= values['file_bytes'] <= limits['package_file_bytes'], 'Application package file budget exceeded')
    require(values['file_bytes'] <= limits['ordinary_application_file_bytes'], 'Ordinary application file limit exceeded')
    require(all(0 <= values[k] <= limits['structural_image_bytes'] for k in values if k != 'file_bytes'),
            'Code/BSS/data/combined structural allocation budget exceeded')
    require(values['code_bytes'] > 0 and values['resident_section_allocation_bytes'] ==
            values['code_bytes']+values['data_with_alignment_reserve_bytes'], 'Invalid loader allocation accounting')
    require(all(c['alignment'] <= 16 for c in loader['data_allocation_components']),
            'Admission harness does not cover every alignment residue')
    return {'observed':values,'limits':limits,'passed':True,
        'separate_code_or_bss_budget_defined':False,
        'scope':'Existing package/admission/structural caps; these are not an application-specific PSRAM reservation or physical-device-fit budget.'}


def stage_admission(args):
    root = args.artifact.resolve(); out = root/'admission'; out.mkdir(exist_ok=False)
    reporter = out/'report_native_settings_memory.py'
    reporter.write_bytes(recipe.git(args.system,'show',recipe.PINS['system']+':scripts/report_native_settings_memory.py'))
    spec = importlib.util.spec_from_file_location('ble_buttons_shared_admission',reporter)
    shared = importlib.util.module_from_spec(spec); spec.loader.exec_module(shared)
    # Reuse the inspector/harness, explicitly selecting this app's Runtime. The
    # helper's historical default is NOT the Runtime being qualified here.
    runtime = out/'runtime'; hashes = {}
    for relative in shared.RUNTIME_INPUTS:
        content = recipe.git(args.runtime,'show',recipe.PINS['runtime']+':'+relative)
        path = runtime/relative; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(content)
        hashes[relative] = recipe.sha(content)
    header = (runtime/'lib/elf_loader/include/private/esp_elf_data_layout.h').read_text()
    ordinary = (runtime/'src/runtime/packages/ElfAdmission.h').read_text()
    require('#define ESP_ELF_MAX_IMAGE_BYTES (8u * 1024u * 1024u)' in header and
            'n>2097152u' in ordinary, 'Existing Runtime budget contract changed')
    app = root/PACKAGE; app.mkdir(exist_ok=True)
    elf = root/'target/native/ble_buttons.elf'; layout = shared.inspect(elf)
    budget = memory_budgets(layout); write(app/'elf-layout.json',layout)
    harness = out/'admit.cpp'; harness.write_text(shared.ADMISSION_SOURCE)
    includes = ['-I'+str(runtime/x) for x in ('test/native_apps/stubs','lib/elf_loader/include','src')]
    runs = []
    for sanitized in (False,True):
        flags = ['-O1','-g','-Wall','-Wextra','-Werror']
        if sanitized: flags += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
        obj = out/'validate.o'; binary = out/'admit'
        commands = [[os.environ.get('CC','cc'),'-std=c11',*flags,*includes,'-c',str(runtime/shared.RUNTIME_INPUTS[0]),'-o',str(obj)],
                    [os.environ.get('CXX','c++'),'-std=c++17',*flags,*includes,str(harness),str(obj),'-o',str(binary)]]
        for command in commands: subprocess.run(command,check=True,timeout=120)
        command = [str(binary),str(elf)]
        p = subprocess.run(command,capture_output=True,text=True,timeout=30,
            env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
        (out/f'admit-{int(sanitized)}.log').write_text(p.stdout+p.stderr)
        require(p.returncode == 0,'Production admission failed; see '+str(out))
        result = json.loads(p.stdout)
        require(all(result.get(k) is True for k in ('ordinary_application_admitted','missing_import_rejected',
            'unsafe_state_rejected','driver_role_rejected')), 'Production admission control failed')
        require(result['data_allocation_request_bytes'] == budget['observed']['data_with_alignment_reserve_bytes'] and
                result['alignment_placements_checked'] == 16*len(layout['section_loader']['data_allocation_components']),
                'Production alignment/size differs from inspector')
        runs.append({'sanitized':sanitized,'commands':commands+[command],'returncode':p.returncode,'result':result})
        obj.unlink(); binary.unlink()
    admission = {'runtime_commit':recipe.PINS['runtime'],'runtime_source_sha256':hashes,
        'shared_inspector_source':'system:scripts/report_native_settings_memory.py',
        'shared_inspector_sha256':digest(reporter),'shared_inspector_default_runtime':shared.RUNTIME_PIN,
        'runtime_selection':'Explicit BLE Buttons target Runtime, not the shared reporter historical default',
        'admission_harness_sha256':digest(harness),'runs':runs,'budgets':budget,
        'elf_sha256':digest(elf),'layout_sha256':digest(app/'elf-layout.json'),
        'manifest_sha256':digest(root/'target/native-manifest.json'),
        'target_instructions_executed':False,'linked_device_memory_fit_qualified':False,
        'scope':'Production structural, import, role, lifecycle and allocation reserve/placement helpers; no relocation or device allocator execution',
        'unqualified_requirements':['linked-loader configuration and ABI','PSRAM allocator capacity/fragmentation',
            'concurrent Runtime/provider/cached-image memory','app dynamic allocations, stacks and frame/sleep buffers',
            'all sixteen manifest requirements and deployment-owned bindings','installation-path compatibility']}
    write(app/'elf-memory-admission.json',admission)
    return admission


def validate_admission(root, native):
    app = root/PACKAGE; layout = read(app/'elf-layout.json'); admission = read(app/'elf-memory-admission.json')
    require(admission['runtime_commit'] == recipe.PINS['runtime'] and admission['elf_sha256'] == native['sha256'] and
            admission['layout_sha256'] == digest(app/'elf-layout.json') and
            admission['manifest_sha256'] == digest(app/'ble_buttons.json'), 'Admission artifact binding mismatch')
    require(layout['sha256'] == native['sha256'] and layout['file_size_bytes'] == native['size_bytes'] and
            admission['budgets'] == memory_budgets(layout), 'Memory budget evidence differs')
    require(not admission['target_instructions_executed'] and not admission['linked_device_memory_fit_qualified'],
            'Unsupported admission hardware claim')
    require(len(admission['runs']) == 2 and {x['sanitized'] for x in admission['runs']} == {False,True}, 'Missing admission modes')
    for run in admission['runs']:
        result = run['result']
        require(run['returncode'] == 0 and all(result.get(k) is True for k in
            ('ordinary_application_admitted','missing_import_rejected','unsafe_state_rejected','driver_role_rejected')),
            'Admission failed')
        require(result['data_allocation_request_bytes'] == layout['section_loader']['data_allocation_request_bytes'] and
                result['alignment_placements_checked'] == 16*len(layout['section_loader']['data_allocation_components']),
                'Admission allocation evidence differs')
    for relative, sha in admission['runtime_source_sha256'].items():
        require(digest(root/'admission/runtime'/relative) == sha, 'Admission Runtime source changed')
        verify_source(root,'runtime:'+relative,sha)
    verify_source(root,admission['shared_inspector_source'],admission['shared_inspector_sha256'])
    require(digest(root/'admission/admit.cpp') == admission['admission_harness_sha256'] and
            digest(root/'admission/report_native_settings_memory.py') == admission['shared_inspector_sha256'], 'Admission harness changed')
    return admission


def qualified_record(root, source):
    provenance = read(root/'source-provenance.json')
    require(provenance['source_commit'] == source and not provenance['working_tree_dirty'], 'Wrong clean source identity')
    target,native = validate_target(root)
    controls = validate_controls(root,target)
    app = root/PACKAGE
    require(read(app/'ble_buttons.json') == expected_manifest() and
            digest(app/'ble_buttons.json') == target['native_manifest_sha256'], 'Standalone manifest differs')
    stamp_app_manifest(expected_manifest(),app/'ble_buttons.elf')  # Budget/integrity check; preserve exact original manifest bytes.
    require((app/'ble_buttons.elf').read_bytes() == (root/'target/native/ble_buttons.elf').read_bytes() and
            digest(app/'command.json') == native['command_sha256'], 'Standalone target differs')
    admission = validate_admission(root,native)
    require(all('source/utilities/'+p in provenance['source_files'] for p in EXTRA_SOURCE), 'Missing release source')
    for name, sha in provenance['source_files'].items():
        require(digest(checked_path(root,name)) == sha, 'Changed staged source: '+name)
    evidence = {p.relative_to(root).as_posix():digest(p) for prefix in ('target','host','temporal','contracts','admission','control-inputs')
                for p in sorted((root/prefix).rglob('*')) if p.is_file()}
    evidence.update({name:digest(root/name) for name in ('existing-controls.json','models.log','legacy-renderer.log','paper-renderer.log','source-provenance.json')})
    return {'schema':1,'tag':TAG,'version':VERSION,'source_commit':source,'source_tree':provenance['source_tree'],
        'working_tree_dirty':False,'external_git_pins':recipe.PINS,'target_original_source_commit':target['utilities_commit'],
        'source_correspondence':'Every target compiled input and host recorded source matches the committed source closure; original receipts preserved',
        'required_capabilities':expected_manifest()['requires'],'target_receipt_sha256':digest(root/'target/qualification.json'),
        'controls':controls,'source_files':provenance['source_files'],'evidence_files':evidence,
        'elf_sha256':native['sha256'],'elf_size_bytes':native['size_bytes'],'manifest_sha256':digest(app/'ble_buttons.json'),
        'command_sha256':native['command_sha256'],'layout_sha256':digest(app/'elf-layout.json'),
        'admission_sha256':digest(app/'elf-memory-admission.json'),'memory_budgets':admission['budgets'],
        'hardware_qualified':False,'firmware_replacement':False,'legacy_manifest_unchanged':True,
        'predecessor':DEPLOYED,
        'limitations':admission['unqualified_requirements']+['No Xtensa execution; host simulated BLE/peripherals and software-only UI evidence',
            'Frozen .64 and every legacy release/catalog binding remain unchanged']}


def qualify(args):
    root = args.artifact.resolve(); source = recipe.git(ROOT,'rev-parse','HEAD').decode().strip()
    require(not recipe.git(ROOT,'status','--porcelain').strip(), 'Release requires a clean exact source')
    target = read(root/'target/qualification.json'); native = native_module(target)
    require(read(ROOT/MANIFEST) == expected_manifest(), 'Native release manifest changed')
    source_files = {}
    def copy_source(owner,relative,content,expected=None):
        require(expected is None or recipe.sha(content) == expected, 'Source closure differs: '+owner+':'+relative)
        path = checked_path(root/'source',owner+'/'+relative); path.parent.mkdir(parents=True,exist_ok=True)
        if path.exists(): require(path.read_bytes() == content, 'Conflicting source identity')
        path.write_bytes(content); source_files[path.relative_to(root).as_posix()] = recipe.sha(content)
    repos = {key:getattr(args,key) for key in recipe.PINS}
    # Retain original staging plus a compact, indexed closure, checked against
    # immutable Git objects (toolchain headers against recorded target bytes).
    for module in target['modules']:
        for identity, sha in module['source_sha256'].items():
            owner,relative = identity.split(':',1)
            if module['mode'] == 'baseline' and owner == 'utilities': owner = 'utilities-baseline'
            if module['mode'] == 'baseline' and owner == 'system': owner = 'system-baseline'
            if owner == 'system-baseline':
                content = recipe.git(repos['system'],'show',recipe.BASE_SYSTEM+':'+relative)
            elif owner in ('utilities','utilities-baseline'):
                content = recipe.git(ROOT,'show',(source if owner == 'utilities' else recipe.BASE)+':'+relative)
            elif owner == 'generated':
                content = (root/'target/inputs/generated'/relative).read_bytes()
            elif owner == 'toolchain':
                staged = root/'target/inputs/toolchain'/relative
                if staged.is_file(): content = staged.read_bytes()
                else: content = (Path(target['tools']['cc']['path']).resolve().parent.parent/relative).read_bytes()
            else: content = recipe.git(repos[owner],'show',recipe.PINS[owner]+':'+relative)
            copy_source(owner,relative,content,sha)
    for relative in recipe.git(ROOT,'ls-tree','-r','--name-only',source,'--',*CONTROL_SELECTIONS).decode().splitlines():
        copy_source('utilities',relative,recipe.git(ROOT,'show',source+':'+relative))
    copy_source('utilities-baseline','Apps/ble_buttons.json',recipe.git(ROOT,'show',recipe.BASE+':Apps/ble_buttons.json'))
    for mode, *_ in CONTROL_RUNS:
        host = read(root/mode/'receipt.json')
        for name,sha in {**host['source_sha256'],**host['compiled_dependency_sha256']}.items():
            owner,relative = source_identity(name,host.get('source_roots',{})).split(':',1)
            if owner == 'generated':
                copy_source(owner,relative,checked_path(root/'target/inputs/generated',relative).read_bytes(),sha)
            else:
                repo,pin = (ROOT,source) if owner == 'utilities' else (repos[owner],recipe.PINS[owner])
                copy_source(owner,relative,recipe.git(repo,'show',pin+':'+relative),sha)
    contract=read(root/'contracts/qualification.json')
    for identity,sha in {**{'utilities:'+name:h for name,h in contract['source_hashes'].items()},
                         **contract['compiled_dependency_sha256']}.items():
        owner,relative=identity.split(':',1)
        if owner == 'utilities-baseline': repo,pin=ROOT,recipe.BASE
        elif owner == 'system-baseline': repo,pin=repos['system'],recipe.BASE_SYSTEM
        else: repo,pin=(ROOT,source) if owner == 'utilities' else (repos[owner],recipe.PINS[owner])
        copy_source(owner,relative,recipe.git(repo,'show',pin+':'+relative),sha)
    controls = read(root/'existing-controls.json')
    for identity,sha in controls['external_sources'].items():
        owner,relative = identity.split(':',1)
        copy_source(owner,relative,(root/'control-inputs'/owner/relative).read_bytes(),sha)
    # Include all existing pinned license/copyright/patent notices; do not invent
    # notices or silently copy from unpinned working trees.
    for owner,repo in repos.items():
        pin = recipe.PINS[owner]
        listing = recipe.git(repo,'ls-tree','-r','--name-only',pin).decode().splitlines()
        for relative in listing:
            if re.search(r'(^|/)(LICENSE[^/]*|COPYING[^/]*|NOTICE[^/]*|PATENTS[^/]*|AUTHORS[^/]*)$',relative,re.I):
                copy_source(owner,relative,recipe.git(repo,'show',pin+':'+relative))
    toolchain_root = Path(target['tools']['cc']['path']).resolve().parent.parent
    for notice in sorted((toolchain_root/'share/licenses').rglob('*')):
        if notice.is_file(): copy_source('toolchain',notice.relative_to(toolchain_root).as_posix(),notice.read_bytes())
    for identity,sha in target['recipe_sources'].items():
        owner,relative=identity.split(':',1)
        copy_source(owner,relative,recipe.git(repos[owner],'show',recipe.PINS[owner]+':'+relative),sha)
    for name,proof in target['generated_sources'].items():
        copy_source('generated',name,(root/'target/inputs/generated'/name).read_bytes(),proof['sha256'])
    admission = read(root/'app/elf-memory-admission.json') if (root/'admission').exists() else stage_admission(args)
    copy_source('system','scripts/report_native_settings_memory.py',(root/'admission/report_native_settings_memory.py').read_bytes())
    for relative,sha in admission['runtime_source_sha256'].items():
        copy_source('runtime',relative,(root/'admission/runtime'/relative).read_bytes(),sha)
    app = root/PACKAGE
    for src,dst in [(root/'target/native/ble_buttons.elf','ble_buttons.elf'),
                    (ROOT/MANIFEST,'ble_buttons.json'),(root/'target/native/command.json','command.json')]:
        shutil.copyfile(src,app/dst)
    write(root/'source-provenance.json',{'source_commit':source,
        'source_tree':recipe.git(ROOT,'rev-parse','HEAD^{tree}').decode().strip(),
        'working_tree_dirty':False,'source_files':source_files})
    record = qualified_record(root,source); write(app/RECORD,record)
    require(recipe.git(ROOT,'rev-parse','HEAD').decode().strip() == source and
            not recipe.git(ROOT,'status','--porcelain').strip(), 'Source changed during packaging')
    print('Native BLE Buttons 0.1.30-dev1: complete source/control package and production admission PASS')
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact',type=Path,required=True)
    for name in (*recipe.PINS,'legacy-system'): parser.add_argument('--'+name,type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--run-controls',action='store_true')
    mode.add_argument('--admission-only',action='store_true',help='Produce admission evidence only; not a releasable package')
    args = parser.parse_args()
    names = ('system',) if args.run_controls else ('system','runtime') if args.admission_only else tuple(recipe.PINS)
    for name in names:
        if getattr(args,name.replace('-','_')) is None: parser.error('--'+name+' is required for this mode')
    if args.run_controls: run_controls(args)
    elif args.admission_only: stage_admission(args)
    else: qualify(args)


if __name__ == '__main__': main()
