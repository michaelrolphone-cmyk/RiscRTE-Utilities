#!/usr/bin/env python3
"""Bind Spectrum's exact source/control evidence to a standalone development ELF.

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

import build_native_spectrum_qualification as recipe
from package_integrity import stamp_app_manifest

ROOT = recipe.ROOT
TAG = 'native-audio-spectrum-0.5.11-dev1'
VERSION = '0.5.11-dev1'
PACKAGE = 'app'
MANIFEST = 'Apps/native/audio_spectrum.json'
RECORD = 'audio_spectrum-build-record.json'
PROFILES = ('240x240', '410x502', '502x410', '480x800', '800x480')
SCENARIOS = {
    'native-pages', 'native-plot-history-toast', 'native-controls-scroll',
    'native-plot-delayed', 'native-repeated-entry',
    *(f'native-{editor}-{case}' for editor in ('frequency','signature','temporal','shared')
      for case in ('cancel','done','reopen','limit','delayed','no-present',
                   'live-delayed-done','live-delayed-back','live-delayed-home','explicit-save')),
    'missing-host','old-host','unavailable-open','retained','loss-close','loss-open','loss-poll',
    'policy-stopped-done','policy-stopped-cancel','policy-stopped-home','policy-active',
    'policy-unavailable','policy-retained','policy-missing-suffix','policy-paused-home','full-contexts-startup',
}
HOST_INPUTS = {
    *('utilities:Apps/'+name for name in ('spectrum_native_layout.h','spectrum_native_view.inc',
      'spectrum_native_subpages.inc','audio_spectrum.c','context_signal_library.inc',
      'spectrum_signature_app.inc','spectrum_temporal_app.inc','context_fingerprint_app.inc')),
    'utilities:test/native_apps/spectrum_shared_renderer_test.c',
    'system:lib/PortableApps/src/adapter.c','system:Services/scene_host/host.c',
    'system:Services/text_input/host.c','system:lib/PortableApps/include/PortableTextInputClient.h',
}
EXTRA_SOURCE = (MANIFEST, 'scripts/prepare_native_spectrum_release.py',
    'scripts/publish_native_spectrum.py', 'tests/test_native_spectrum_release.py',
    '.github/workflows/native-spectrum.yml')
CONTROL_SELECTIONS = ('Apps', 'lib', 'scripts', 'tests', 'test', 'profiles',
                      'docs/audio-spectrum-native', 'LICENSE', '.github/workflows/native-spectrum.yml')
LEGACY_SYSTEM = 'fdc15395928a1fa8eee62a2823b3c8777dcf0fa0'


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
    return {'architecture':'xtensa-esp32s3','display_name':'Audio Spectrum','entry':'app_main',
        'file_name':'audio_spectrum.elf','icon':'solid:f130','id':'audio_spectrum',
        'requires':[{'capability':c,'api':v} for c,v in recipe.CAPABILITIES],
        'type':'application','version':VERSION}


def native_module(target):
    require(len(target['modules']) == 3 and {m['mode'] for m in target['modules']} ==
            {'baseline','legacy','native'}, 'Incomplete target matrix')
    return next(m for m in target['modules'] if m['mode'] == 'native')


def source_identity(name, pins):
    """Resolve original absolute receipt paths using its recorded source roots."""
    for key, owner in (('legacy_utilities','utilities'),('system','system'),('runtime','runtime')):
        base = Path(pins[key]['path'])
        if Path(name).is_relative_to(base):
            return owner+':'+Path(name).relative_to(base).as_posix()
    raise ValueError('Unbound host source: '+name)


def verify_source(root, identity, expected):
    owner, relative = identity.split(':', 1)
    require(owner in {*recipe.PINS,'utilities','utilities-baseline','toolchain','legacy-system'}, 'Unknown source owner')
    path = checked_path(root/'source',owner+'/'+relative)
    require(digest(path) == expected, 'Changed source evidence: '+identity)


def validate_target(root):
    target = read(root/'target/qualification.json'); native = native_module(target)
    require(target['utilities_base'] == recipe.BASE and target['exact_external_git_pins'] == recipe.PINS,
            'Wrong target source pins')
    require(target['native_opt_in'] and target['native_selectors'] == recipe.NATIVE and
            target['selector_off_exact_baseline_bytes'] and target['legacy_manifest_exact_base'], 'Incomplete native/default contract')
    require(target['native_manifest'] == expected_manifest() and
            target.get('capability_contract',target.get('required_capabilities')) == expected_manifest()['requires'],
            'Wrong twenty-one capability contract')
    require(not target['hardware_qualified'] and not target['firmware_assembled'], 'Unsupported target hardware claim')
    require(target['retained_flags_removed'] == ['-Wl,--undefined=__divsf3'], 'Retained recipe flags drift')
    require(recipe.REQUIRED <= native['source_sha256'].keys(), 'Incomplete compiled source closure')
    require(set(native['exports']) == recipe.EXPORTS and set(native['imports']) <= recipe.IMPORTS,
            'Native import/export drift')
    legacy = next(m for m in target['modules'] if m['mode'] == 'legacy')
    require(native['build_defines'] == legacy['build_defines']+recipe.NATIVE and native['native_ui'], 'Native recipe selector drift')
    for module in target['modules']:
        folder = root/'target'/module['mode']; elf = folder/'audio_spectrum.elf'
        require(digest(elf) == module['sha256'] and elf.stat().st_size == module['size_bytes'], 'Changed target ELF')
        require(digest(folder/'command.json') == module['command_sha256'], 'Changed target command')
        require(module['structural_loader_admission'] == 'passed' and not module['target_instructions_executed'],
                'Wrong target qualification scope')
        for identity, sha in module['source_sha256'].items():
            if module['mode'] == 'baseline' and identity.startswith('utilities:'):
                identity = identity.replace('utilities:','utilities-baseline:',1)
            verify_source(root,identity,sha)
    require((root/'target/baseline/audio_spectrum.elf').read_bytes() ==
            (root/'target/legacy/audio_spectrum.elf').read_bytes(), 'Selector-off baseline bytes differ')
    verify_source(root,'utilities:scripts/build_native_spectrum_qualification.py',target['script_sha256'])
    verify_source(root,'wave:'+recipe.RECIPE,target['retained_recipe_sha256'])
    require(digest(root/'source/utilities'/MANIFEST) == target['native_manifest_sha256'] and
            read(root/'source/utilities'/MANIFEST) == expected_manifest(), 'Native source manifest changed')
    require(digest(root/'source/utilities/Apps/audio_spectrum.json') == target['legacy_manifest_sha256'] ==
            digest(root/'source/utilities-baseline/Apps/audio_spectrum.json'), 'Legacy manifest changed')
    return target, native


def validate_controls(root, target):
    proofs = {}
    for profile in PROFILES:
        folder = root/'host'/profile; path = folder/'qualification.json'; r = read(path)
        require(len(r['results']) == 2*len(SCENARIOS) and
                {(x['mode'],x['sanitized']) for x in r['results']} ==
                {(case,mode) for case in SCENARIOS for mode in (False,True)} and
                all(x['returncode'] == 0 for x in r['results']), 'Failed/incomplete host matrix: '+profile)
        require(all(d in r['added_safety_defines'] for d in recipe.NATIVE), 'Non-native host control')
        for owner in ('system','runtime'):
            require(r['source_pins'][owner]['commit'] == recipe.PINS[owner] and
                    not r['source_pins'][owner]['status'], 'Host external pin/dirty mismatch')
        require(HOST_INPUTS <= {source_identity(name,r['source_pins']) for name in r['sources']}, 'Missing host source hashes')
        w,h = profile.split('x')
        require('-DSPECTRUM_TEST_WIDTH='+w in r['added_safety_defines'] and
                '-DSPECTRUM_TEST_HEIGHT='+h in r['added_safety_defines'], 'Host dimensions differ')
        for name, sha in r['sources'].items(): verify_source(root,source_identity(name,r['source_pins']),sha)
        verify_source(root,'utilities:scripts/test_spectrum_shared_input.py',r['test_script_sha256'])
        for result in r['results']:
            log = folder/('sanitized' if result['sanitized'] else 'normal')/result['mode']/'run.log'
            require(log.is_file() and log.stat().st_size, 'Missing host run log')
            require(log.name == Path(result['log']).name, 'Host log name mismatch')
        require(len(r['normal_sanitized_pixel_identical']) >= len(SCENARIOS) and
                len(r['non_keyboard_views']) == 18, 'Incomplete native raster coverage')
        for name in r['normal_sanitized_pixel_identical']:
            require(digest(checked_path(folder/'normal',name)) == digest(checked_path(folder/'sanitized',name)),
                    'Normal/sanitized pixels differ')
        proofs['host/'+profile] = digest(path)
    path = root/'contracts/qualification.json'; r = read(path)
    tests = ('tests/spectrum_native_layout_test.c','test/native_apps/spectrum_native_subpages_test.c')
    require(len(r['tests']) == 4 and {(x['source'],x['sanitized']) for x in r['tests']} ==
            {(s,m) for s in tests for m in (False,True)} and all(x['returncode'] == 0 for x in r['tests']),
            'Incomplete geometry/subpage controls')
    require(len(r['parity']) == 3 and {tuple(x['defines']) for x in r['parity']} ==
            {(),('PORTABLE_NOVA_UI','PORTABLE_ALARM_CLIENT'),('PORTABLE_SPECTRUM_SHARED_TEXT_INPUT',)} and
            all(x['identical'] for x in r['parity']), 'Selector-off token parity failed')
    for name, sha in r['sources'].items(): verify_source(root,'utilities:'+name,sha)
    proofs['contracts'] = digest(path)
    path = root/'existing-controls.json'; r = read(path)
    require(r['system_commit'] == recipe.PINS['system'] and r['legacy_system_commit'] == LEGACY_SYSTEM,
            'Existing control pins differ')
    require(len(r['runs']) == 2 and {x['script'] for x in r['runs']} ==
            {'scripts/test_audio_spectrum.py','scripts/test_spectrum_storage_lifecycle.py'} and
            all(x['returncode'] == 0 for x in r['runs']), 'Existing model/storage controls failed')
    for run in r['runs']:
        require(digest(checked_path(root,run['log'])) == run['log_sha256'], 'Changed model/storage log')
    require(r['external_sources'] and all(owner+':lib/PortableApps/include/PortableApps.h' in r['external_sources'] for owner in ('system','legacy-system')), 'Missing model/storage external headers')
    for name, sha in r['source_files'].items(): verify_source(root,'utilities:'+name,sha)
    for identity, sha in r['external_sources'].items(): verify_source(root,identity,sha)
    proofs['existing'] = digest(path)
    return proofs


def run_controls(args):
    root = args.artifact.resolve(); root.mkdir(parents=True,exist_ok=True)
    # Read immutable objects; compile the staged headers, not either checkout.
    legacy_repo = args.legacy_system if (args.legacy_system/'.git').exists() else args.system
    names = recipe.git(ROOT,'ls-files','--',*CONTROL_SELECTIONS).decode().splitlines()
    hashes = {p:digest(ROOT/p) for p in names}
    external = {}
    for owner, repo, pin in [('system',args.system,recipe.PINS['system']),('legacy-system',legacy_repo,LEGACY_SYSTEM)]:
        for name in recipe.git(repo,'ls-tree','-r','--name-only',pin,'--','lib/PortableApps/include','lib/NativeApps/include').decode().splitlines():
            content = recipe.git(repo,'show',pin+':'+name)
            dest = root/'control-inputs'/owner/name; dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(content)
            external[owner+':'+name] = recipe.sha(content)
    runs = []
    for label, script, system in [('models','scripts/test_audio_spectrum.py',root/'control-inputs/legacy-system'),
                                  ('storage','scripts/test_spectrum_storage_lifecycle.py',root/'control-inputs/system')]:
        command = [os.environ.get('PYTHON','python3'),str(ROOT/script),'--system-apps',str(system.resolve())]
        p = subprocess.run(command,capture_output=True,text=True,timeout=600,cwd=ROOT,
            env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
        log = root/(label+'.log'); log.write_text(p.stdout+p.stderr)
        runs.append({'script':script,'command':command,'returncode':p.returncode,'log':log.name,'log_sha256':digest(log)})
        write(root/'existing-controls.json',{'system_commit':recipe.PINS['system'],
            'legacy_system_commit':LEGACY_SYSTEM,'source_files':hashes,'external_sources':external,'runs':runs})
        require(p.returncode == 0, 'Existing '+label+' controls failed; see '+str(log))
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
    spec = importlib.util.spec_from_file_location('spectrum_shared_admission',reporter)
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
    elf = root/'target/native/audio_spectrum.elf'; layout = shared.inspect(elf)
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
        'runtime_selection':'Explicit Spectrum target Runtime, not the shared reporter historical default',
        'admission_harness_sha256':digest(harness),'runs':runs,'budgets':budget,
        'elf_sha256':digest(elf),'layout_sha256':digest(app/'elf-layout.json'),
        'manifest_sha256':digest(root/'target/native-manifest.json'),
        'target_instructions_executed':False,'linked_device_memory_fit_qualified':False,
        'scope':'Production structural, import, role, lifecycle and allocation reserve/placement helpers; no relocation or device allocator execution',
        'unqualified_requirements':['linked-loader configuration and ABI','PSRAM allocator capacity/fragmentation',
            'concurrent Runtime/provider/cached-image memory','app dynamic allocations, stacks and frame/sleep buffers',
            'all twenty-one manifest requirements and deployment-owned bindings','installation-path compatibility']}
    write(app/'elf-memory-admission.json',admission)
    return admission


def validate_admission(root, native):
    app = root/PACKAGE; layout = read(app/'elf-layout.json'); admission = read(app/'elf-memory-admission.json')
    require(admission['runtime_commit'] == recipe.PINS['runtime'] and admission['elf_sha256'] == native['sha256'] and
            admission['layout_sha256'] == digest(app/'elf-layout.json') and
            admission['manifest_sha256'] == digest(app/'audio_spectrum.json'), 'Admission artifact binding mismatch')
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
    require(read(app/'audio_spectrum.json') == expected_manifest() and
            digest(app/'audio_spectrum.json') == target['native_manifest_sha256'], 'Standalone manifest differs')
    stamp_app_manifest(expected_manifest(),app/'audio_spectrum.elf')  # Budget/integrity check; preserve exact original manifest bytes.
    require((app/'audio_spectrum.elf').read_bytes() == (root/'target/native/audio_spectrum.elf').read_bytes() and
            digest(app/'command.json') == native['command_sha256'], 'Standalone target differs')
    admission = validate_admission(root,native)
    require(all('source/utilities/'+p in provenance['source_files'] for p in EXTRA_SOURCE), 'Missing release source')
    for name, sha in provenance['source_files'].items():
        require(digest(checked_path(root,name)) == sha, 'Changed staged source: '+name)
    evidence = {p.relative_to(root).as_posix():digest(p) for prefix in ('target','host','contracts','admission','control-inputs')
                for p in sorted((root/prefix).rglob('*')) if p.is_file()}
    evidence.update({name:digest(root/name) for name in ('existing-controls.json','models.log','storage.log','source-provenance.json')})
    return {'schema':1,'tag':TAG,'version':VERSION,'source_commit':source,'source_tree':provenance['source_tree'],
        'working_tree_dirty':False,'external_git_pins':recipe.PINS,'target_original_source_commit':target['utilities_commit'],
        'source_correspondence':'Every target compiled input and host recorded source matches the committed source closure; original receipts preserved',
        'required_capabilities':expected_manifest()['requires'],'target_receipt_sha256':digest(root/'target/qualification.json'),
        'controls':controls,'source_files':provenance['source_files'],'evidence_files':evidence,
        'elf_sha256':native['sha256'],'elf_size_bytes':native['size_bytes'],'manifest_sha256':digest(app/'audio_spectrum.json'),
        'command_sha256':native['command_sha256'],'layout_sha256':digest(app/'elf-layout.json'),
        'admission_sha256':digest(app/'elf-memory-admission.json'),'memory_budgets':admission['budgets'],
        'hardware_qualified':False,'firmware_replacement':False,'legacy_manifest_unchanged':True,
        'limitations':admission['unqualified_requirements']+['No Xtensa execution; host synthetic audio and software-only UI evidence',
            'Frozen .63 and every legacy release/catalog binding remain unchanged']}


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
            if owner in ('utilities','utilities-baseline'):
                content = recipe.git(ROOT,'show',(source if owner == 'utilities' else recipe.BASE)+':'+relative)
            elif owner == 'toolchain':
                staged = root/'target/inputs/toolchain'/relative
                if staged.is_file(): content = staged.read_bytes()
                else: content = (Path(target['tools']['cc']['path']).resolve().parent.parent/relative).read_bytes()
            else: content = recipe.git(repos[owner],'show',recipe.PINS[owner]+':'+relative)
            copy_source(owner,relative,content,sha)
    for relative in recipe.git(ROOT,'ls-tree','-r','--name-only',source,'--',*CONTROL_SELECTIONS).decode().splitlines():
        copy_source('utilities',relative,recipe.git(ROOT,'show',source+':'+relative))
    copy_source('utilities-baseline','Apps/audio_spectrum.json',recipe.git(ROOT,'show',recipe.BASE+':Apps/audio_spectrum.json'))
    for profile in PROFILES:
        host = read(root/'host'/profile/'qualification.json')
        for name,sha in host['sources'].items():
            owner,relative = source_identity(name,host['source_pins']).split(':',1)
            repo,pin = (ROOT,source) if owner == 'utilities' else (repos[owner],recipe.PINS[owner])
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
    copy_source('wave',recipe.RECIPE,recipe.git(args.wave,'show',recipe.PINS['wave']+':'+recipe.RECIPE))
    admission = read(root/'app/elf-memory-admission.json') if (root/'admission').exists() else stage_admission(args)
    copy_source('system','scripts/report_native_settings_memory.py',(root/'admission/report_native_settings_memory.py').read_bytes())
    for relative,sha in admission['runtime_source_sha256'].items():
        copy_source('runtime',relative,(root/'admission/runtime'/relative).read_bytes(),sha)
    app = root/PACKAGE
    for src,dst in [(root/'target/native/audio_spectrum.elf','audio_spectrum.elf'),
                    (ROOT/MANIFEST,'audio_spectrum.json'),(root/'target/native/command.json','command.json')]:
        shutil.copyfile(src,app/dst)
    write(root/'source-provenance.json',{'source_commit':source,
        'source_tree':recipe.git(ROOT,'rev-parse','HEAD^{tree}').decode().strip(),
        'working_tree_dirty':False,'source_files':source_files})
    record = qualified_record(root,source); write(app/RECORD,record)
    require(recipe.git(ROOT,'rev-parse','HEAD').decode().strip() == source and
            not recipe.git(ROOT,'status','--porcelain').strip(), 'Source changed during packaging')
    print('Native Spectrum 0.5.11-dev1: complete source/control package and production admission PASS')
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact',type=Path,required=True)
    for name in (*recipe.PINS,'legacy-system'): parser.add_argument('--'+name,type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--run-controls',action='store_true')
    mode.add_argument('--admission-only',action='store_true',help='Produce admission evidence only; not a releasable package')
    args = parser.parse_args()
    names = ('system','legacy-system') if args.run_controls else ('system','runtime') if args.admission_only else tuple(recipe.PINS)
    for name in names:
        if getattr(args,name.replace('-','_')) is None: parser.error('--'+name+' is required for this mode')
    if args.run_controls: run_controls(args)
    elif args.admission_only: stage_admission(args)
    else: qualify(args)


if __name__ == '__main__': main()
