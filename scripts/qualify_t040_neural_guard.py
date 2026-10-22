#!/usr/bin/env python3
"""Non-distributable T040 software evidence; never replaces frozen release gates."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

import build_native_spectrum_qualification as frozen
import prepare_native_spectrum_release as frozen_package

ROOT = Path(__file__).resolve().parents[1]
CAPTURED = '390b8ee8580db7d7b3e37b99c38cb10378d6d413'
GUARD_SOURCE = '30867c87a7d3afb13252fd446784d484270b4ecf'
PARENT = '6b2047500905f8f45087cb49989103a926fc5c63'
SHARED_BASE = '65bffc27f96088db3c2f76ee95ae812b223459f7'
LEGACY_SYSTEM = 'fdc15395928a1fa8eee62a2823b3c8777dcf0fa0'
SHARED_SYSTEM = 'fd4b1e52fe2a49c3c4c22f9ccd4c98a91f88afa0'
PARITY_ERROR = 'Selector-off ELF differs from exact Utilities baseline; inspect retained receipt'
ADDITIONS = {'.github/workflows/t040-neural-guard.yml', 'scripts/qualify_t040_neural_guard.py',
             'tests/test_t040_neural_guard.py', 'docs/t040/candidate-qualification/README.md'}
FROZEN_FILES = ('scripts/build_native_spectrum_qualification.py',
                'scripts/test_spectrum_native_contract.py', 'scripts/test_spectrum_shared_input_safety.py',
                'docs/consolidation/20261022/frozen-prepare-native-spectrum-release.py', 'scripts/publish_native_spectrum.py',
                'docs/consolidation/20261022/frozen-native-spectrum.yml', 'docs/consolidation/20261022/frozen-watch-spectrum-shared-input.yml',
                'Apps/native/audio_spectrum.json', 'Apps/audio_spectrum.json')
FROZEN_SOURCE_SHA256 = {'scripts/build_native_spectrum_qualification.py': '857f92e510f363d8495cc994c6eeb95fb5e2a9558d4516a34b5800a4fc83c069', 'scripts/test_spectrum_native_contract.py': '4484af1ed888569a240d51272bdcb51acba7a17993a5e36019ac1ab085a7aa06', 'scripts/test_spectrum_shared_input_safety.py': '5c4273ebe7c2c4270a1f6c5df2abc8055e922267cfb8a1df10172353a37c18cf', 'docs/consolidation/20261022/frozen-prepare-native-spectrum-release.py': 'dfba398b73c5bdf850ed437fedc18fbb99cc4b653ec7cb035815f7f91ab5b949', 'scripts/publish_native_spectrum.py': '639493b155437fe44dd90dc4180a814271b567911ad337bfb3ade242521b0475', 'docs/consolidation/20261022/frozen-native-spectrum.yml': 'd686cfad81de396354c3876a28d1b7ad6d652437c937d979d1427b74f83f8dfd', 'docs/consolidation/20261022/frozen-watch-spectrum-shared-input.yml': '7f878934081931a592a055f69536272b5cc68647f297108c14a35df21ea9510e', 'Apps/native/audio_spectrum.json': '21e4be6d8d8ae15ec8d85b346ba1c2052b5282dc69897c01688a3b418058d01c', 'Apps/audio_spectrum.json': 'a1157a49b5b9e656b9f146481fb0a993210b1634d416f2518dce35e62044d9d4'}
REGRESSION_SOURCE_SHA256 = '01c70953e15ed388d8e598fcf09fd12f3e7860af2b267786de0626bd8fd88396'
COMPOSED_SIGNAL_FILES = ('Apps/context_fingerprint_app.inc', 'Apps/context_signal_library.inc')
GUARD_FILES = {'Apps/audio_spectrum.c': '974ba8acf763df90999140711bea6edff00765bbcd46e5221003fd1babe971d3',
               'Apps/spectrum_temporal_app.inc': '42853d31cba4a26976c6b0f0f5f5e21471d96acba5db860ae688e25466804d39'}
FIXTURE = 'test/native_apps/spectrum_neural_app_test.c'
FIXTURE_SHA = '6a5d8feeb1fc4875f4b0938ee68282e2f3dfb9d8c19a2a9b2f0c17f274e8e5d2'
NOTICE = ('QUALIFICATION EVIDENCE ONLY. NOT FOR INSTALLATION OR DISTRIBUTION AS AN APP.\n'
          'No candidate version or installable manifest is assigned. The frozen release gate remains failed.\n'
          'Host models and target structural admission do not qualify hardware, RF, target execution or device fit.\n')


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def digest(path):
    return sha(path.read_bytes())


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def safe(root, relative):
    p = PurePosixPath(relative)
    require(relative and not p.is_absolute() and '\\' not in relative and
            all(x not in ('', '.', '..') for x in relative.split('/')), 'Unsafe artifact path')
    result = root.joinpath(*p.parts)
    require(result.resolve().is_relative_to(root.resolve()), 'Artifact path escapes root')
    return result


def target_input(folder, owner, relative):
    require(owner in {*frozen.PINS, 'utilities', 'utilities-baseline', 'toolchain'},
            'Unknown target source owner')
    # Frozen SDK aliases may cross owner roots to preserve pragma-once identity,
    # but they must never resolve outside this particular target artifact.
    return safe(folder, 'inputs/'+owner+'/'+relative)


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], timeout=120)


def source_guard():
    require(not git(ROOT, 'status', '--porcelain').strip(), 'Tracked source must be clean')
    import consolidated_source_guard as composition
    if composition.available(ROOT):
        composition.validate(ROOT)
    else:
        changes = git(ROOT, 'diff', '--name-status', CAPTURED, 'HEAD').decode().splitlines()
        require(all(row.startswith('A\t') and row[2:] in ADDITIONS for row in changes),
                'Candidate lane may add qualification files only; captured source and all frozen gates must remain unchanged')
    require(all(digest(ROOT/n) == h for n,h in GUARD_FILES.items()) and digest(ROOT/FIXTURE) == FIXTURE_SHA, 'Captured guard or fixture changed')
    require(all(digest(ROOT/path) == value for path,value in FROZEN_SOURCE_SHA256.items()), 'Immutable historical source contract changed')
    require(digest(ROOT/'scripts/prepare_native_spectrum_release.py') == REGRESSION_SOURCE_SHA256, 'Approved Spectrum regression contract changed')
    return git(ROOT, 'rev-parse', 'HEAD').decode().strip()


def run(command, out, label, expected=0):
    command = list(map(str, command))
    result = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
                                                         'UBSAN_OPTIONS': 'halt_on_error=1'}, timeout=1800)
    log = out/'runs'/(label+'.log')
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(result.stdout)
    write(out/'runs'/(label+'.json'), {'command': command, 'exit_code': result.returncode,
                                      'log_sha256': digest(log)})
    print(label+': exit '+str(result.returncode), flush=True)
    if expected is not None:
        require(result.returncode == expected, label+' failed; see '+str(log))
    return result.returncode


TARGET_ATTEMPTS = 3
LINKER_ABORT = {'collect2: fatal error: ld terminated with signal 6 [Aborted], core dumped',
                'compilation terminated.', 'double free or corruption (out)'}
TARGET_LABELS = {'parent-frozen-target': 0, 'candidate-frozen-target': 1}


def linker_abort(text):
    lines = text.strip().splitlines()
    return len(lines) == 3 and set(lines) == LINKER_ABORT


def normalized_target_command(command):
    command = list(command)
    require(command.count('--output-dir') == 1, 'Target command must name one fresh output')
    command[command.index('--output-dir') + 1] = '<isolated-target>'
    return command


def run_target(command, out, label, folder):
    """Retry only the observed old ld allocator abort, never a target/test failure."""
    expected = TARGET_LABELS[label]
    evidence = out/'target-attempts'/label
    records = []
    require(not folder.exists() and not evidence.exists(), 'Stale target evidence')
    normalized_target_command(command)
    for attempt in range(1, TARGET_ATTEMPTS + 1):
        # The frozen builder, compiler, flags and inputs are unchanged. Its
        # existing output argument selects a clean, short temporary directory.
        with tempfile.TemporaryDirectory(prefix='t040-') as temporary:
            target = Path(temporary)/'target'
            invocation = list(command)
            invocation[invocation.index('--output-dir') + 1] = target
            name = f'{label}-attempt-{attempt}'
            code = run(invocation, evidence, name, expected=None)
            complete = (target/'qualification.json').is_file()
            record = {'attempt': attempt, 'exit_code': code, 'outcome': 'completed'}
            if complete and code == expected:
                # Preserve safe alias bytes before relocating the portable
                # target evidence; absolute internal aliases otherwise dangle.
                for path in sorted(target.rglob('*')):
                    if path.is_symlink():
                        require(path.resolve().is_relative_to(target), 'External target alias')
                        data = path.read_bytes(); path.unlink(); path.write_bytes(data)
                shutil.move(str(target), folder)
                (out/'runs').mkdir(exist_ok=True)
                for suffix in ('.json', '.log'):
                    shutil.copyfile(evidence/'runs'/(name+suffix), out/'runs'/(label+suffix))
                records.append(record)
                write(evidence/'attempts.json', records)
                return
            matches = [f'{mode}/retained-recipe-build.log' for mode in ('baseline','legacy','native')
                       if (target/mode/'retained-recipe-build.log').is_file() and
                       linker_abort((target/mode/'retained-recipe-build.log').read_text())]
            retryable = code == 1 and not complete and len(matches) == 1
            record['outcome'] = 'legacy-linker-abort' if retryable else 'unaccepted-failure'
            record['abort_log'] = matches[0] if retryable else None
            archive = evidence/f'attempt-{attempt}.tar.gz'
            # Preserve the complete failed tree, including link identities,
            # without following links or changing any failed compiler output.
            with tarfile.open(archive, 'w:gz', dereference=False) as tar:
                if target.exists(): tar.add(target, arcname='target')
            record['archive_sha256'] = digest(archive)
            records.append(record); write(evidence/'attempts.json', records)
            require(retryable and attempt < TARGET_ATTEMPTS,
                    label+' failed without an accepted complete target receipt; see '+str(evidence))


def check_target_attempts(out):
    for label, expected in TARGET_LABELS.items():
        evidence = out/'target-attempts'/label
        records = read(evidence/'attempts.json')
        require(1 <= len(records) <= TARGET_ATTEMPTS, 'Unbounded target attempts')
        final = read(out/'runs'/(label+'.json'))
        for index, record in enumerate(records, 1):
            name = f'{label}-attempt-{index}'
            run_record = read(evidence/'runs'/(name+'.json'))
            require(record['attempt'] == index and record['exit_code'] == run_record['exit_code'] and
                    digest(evidence/'runs'/(name+'.log')) == run_record['log_sha256'] and
                    normalized_target_command(run_record['command']) == normalized_target_command(final['command']),
                    'Target attempt identity changed')
            if index == len(records):
                require(record == {'attempt': index, 'exit_code': expected, 'outcome': 'completed'} and
                        run_record == final and digest(out/'runs'/(label+'.log')) == final['log_sha256'],
                        'Missing successful target protocol completion')
            else:
                require(record['outcome'] == 'legacy-linker-abort' and record['exit_code'] == 1 and
                        record['abort_log'] in {f'{mode}/retained-recipe-build.log' for mode in ('baseline','legacy','native')},
                        'Ordinary target failure must not be retried')
                archive = evidence/f'attempt-{index}.tar.gz'
                require(digest(archive) == record['archive_sha256'], 'Failed target archive changed')
                with tarfile.open(archive, 'r:gz') as tar:
                    member = tar.getmember('target/'+record['abort_log'])
                    require(member.isfile() and member.size < 4096 and
                            linker_abort(tar.extractfile(member).read().decode()), 'Wrong archived linker abort')


def copy_git(repo, ref, destination, selections):
    for relative in git(repo, 'ls-tree', '-rz', '--name-only', ref, '--', *selections).decode().rstrip('\0').split('\0'):
        if not relative:
            continue
        target = safe(destination, relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(git(repo, 'show', ref+':'+relative))


def validate_target(folder, *, source, parity):
    r = read(folder/'qualification.json')
    require(r['utilities_commit'] == source and not r['utilities_status'], 'Target source identity mismatch')
    require(r['script_sha256'] == digest(ROOT/'scripts/build_native_spectrum_qualification.py'), 'Frozen builder changed')
    require(r['utilities_base'] == frozen.BASE and r['exact_external_git_pins'] == frozen.PINS, 'Frozen target pins changed')
    require(r['selector_off_exact_baseline_bytes'] is parity and r['legacy_manifest_exact_base'] is True,
            'Unexpected selector-off or manifest result')
    require(r['native_opt_in'] is True and r['native_selectors'] == frozen.NATIVE and
            r['native_manifest'] is None and r['native_manifest_sha256'] is None,
            'Qualification-only outputs must not acquire a release identity')
    require(r['published'] is False and r['hardware_qualified'] is False and
            r['firmware_assembled'] is False, 'Qualification scope overstated')
    require(len(r['modules']) == 3 and {m['mode'] for m in r['modules']} == {'baseline', 'legacy', 'native'},
            'Incomplete target matrix')
    for m in r['modules']:
        mode = m['mode']; elf = folder/mode/'audio_spectrum.elf'
        require(digest(elf) == m['sha256'] and elf.stat().st_size == m['size_bytes'], 'Target bytes changed')
        require(frozen.elf_memory(elf.read_bytes()) == m['memory'], 'Target layout changed')
        require(m['structural_loader_admission'] == 'passed' and m['target_instructions_executed'] is False,
                'Missing structural target admission')
        require(set(m['imports']) <= frozen.IMPORTS and set(m['exports']) == frozen.EXPORTS, 'Target ABI mismatch')
        require(digest(folder/mode/'command.json') == m['command_sha256'] and
                digest(folder/mode/'source-closure.json') == m['source_closure_sha256'], 'Target binding changed')
        require(read(folder/mode/'source-closure.json') == m['source_sha256'], 'Target source closure differs')
        require(frozen.REQUIRED <= m['source_sha256'].keys(), 'Incomplete target closure')
        if mode == 'native':
            require({'utilities:Apps/'+n for n in ('spectrum_native_layout.h','spectrum_native_view.inc','spectrum_native_subpages.inc')} <= m['source_sha256'].keys(), 'Incomplete native closure')
        for identity, expected in m['source_sha256'].items():
            owner, relative = identity.split(':', 1)
            if mode == 'baseline' and owner == 'utilities':
                owner += '-baseline'
            require(digest(target_input(folder, owner, relative)) == expected, 'Changed target input '+identity)
    return r


def target_delta(out, source):
    parent = validate_target(out/'parent-target', source=PARENT, parity=True)
    candidate = validate_target(out/'target', source=source, parity=False)
    require(read(out/'target/failure.json') == {'type': 'ValueError', 'error': PARITY_ERROR, 'qualified': False},
            'Frozen failure was not the expected exact parity refusal')
    require(not (out/'parent-target/failure.json').exists(), 'Parent target unexpectedly failed')
    for name in ('tools', 'toolchain_inputs', 'retained_recipe_sha256', 'native_selectors', 'script_sha256'):
        require(parent[name] == candidate[name], 'Parent/candidate recipe mismatch: '+name)
    pm = {m['mode']:m for m in parent['modules']}; cm = {m['mode']:m for m in candidate['modules']}
    require(pm['baseline']['sha256'] == cm['baseline']['sha256'] == pm['legacy']['sha256'],
            'Parent no longer reproduces frozen selector-off bytes')
    changes = {}
    for mode in ('legacy', 'native'):
        a,b = pm[mode]['source_sha256'],cm[mode]['source_sha256']
        changed = sorted(k for k in set(a)|set(b) if a.get(k) != b.get(k))
        approved = set(GUARD_FILES) | set(COMPOSED_SIGNAL_FILES)
        expected = sorted('utilities:'+p for p in approved if a.get('utilities:'+p) != b.get('utilities:'+p))
        require(changed == expected, 'Unrelated compiled source change')
        require(all(b['utilities:'+p] == digest(ROOT/p) for p in COMPOSED_SIGNAL_FILES if 'utilities:'+p in b),
                'Composed RF source differs from admitted exact head')
        require(all(b['utilities:'+p] == h for p,h in GUARD_FILES.items()), 'Wrong guard bytes')
        require(pm[mode]['sha256'] != cm[mode]['sha256'], 'Expected guard target change absent')
        changes[mode] = changed
    return {'frozen_gate_status':'failed', 'frozen_gate_error':PARITY_ERROR,
            'parent_selector_off_exact':True, 'intentional_changed_inputs':changes,
            'parent_receipt_sha256':digest(out/'parent-target/qualification.json'),
            'candidate_receipt_sha256':digest(out/'target/qualification.json')}


def capture_toolchain(folder, cc):
    for m in read(folder/'qualification.json')['modules']:
        for identity,value in m['source_sha256'].items():
            if identity.startswith('toolchain:'):
                relative = identity.split(':',1)[1]; original = cc.absolute().parents[1]/relative
                require(digest(original) == value, 'Toolchain input changed')
                dest = safe(folder/'inputs/toolchain',relative); dest.parent.mkdir(parents=True,exist_ok=True)
                dest.write_bytes(original.read_bytes())


def pinned_checkout(repo, pin):
    require(git(repo,'rev-parse','HEAD').decode().strip() == pin and
            not git(repo,'status','--porcelain').strip() and
            not git(repo,'ls-files','--others','--ignored','--exclude-standard').strip(),
            'Dependency must be exact, clean and free of ignored shadow inputs: '+str(repo))


def neural_controls(out):
    # The identical new fixture is compiled against old and guarded production.
    old = out/'before-guard-fixture'
    copy_git(ROOT,PARENT,old,['Apps','lib','tests','test'])
    (old/FIXTURE).write_bytes((ROOT/FIXTURE).read_bytes())
    for kind,tree in [('before',old),('guarded',ROOT)]:
        for mode in ('normal','sanitized'):
            label = 'neural-'+kind+'-'+mode; binary = out/label
            flags = ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode == 'sanitized' else []
            headers = out/'source/legacy-system'
            run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
                 '-I'+str(headers/'lib/PortableApps/include'),'-I'+str(headers/'lib/NativeApps/include'),
                 tree/FIXTURE,'-lm','-o',binary],out,label+'-build')
            run([binary],out,label,expected=-6 if kind == 'before' else 0)
    check_neural(out)


def check_neural(out):
    for mode in ('normal','sanitized'):
        log = (out/'runs'/('neural-before-'+mode+'.log')).read_text()
        require('shared capture kind 1: neural ticks 0 -> 1839, checkpoint writes 0 -> 1, microphone closes 0 -> 1' in log
                and 'event_neural.ticks==ticks' in log and 'Assertion' in log, 'Wrong baseline regression failure')
        log = (out/'runs'/('neural-guarded-'+mode+'.log')).read_text()
        require('Neural app: shared room/event capture and pause fence idle training/promotion/checkpoint; cancel/save resume;' in log
                and 'unchanged banks and retained cleanup PASS' in log, 'Missing complete shared-capture matrix')
        require('ERROR: AddressSanitizer' not in log and 'runtime error:' not in log, 'Sanitizer failure')


def check_hosts(out,source):
    require(len(frozen_package.SCENARIOS) == 65, 'Wrong T040 host scenario contract')
    total = 0
    for profile in frozen_package.PROFILES:
        folder = out/'host'/profile; r = read(folder/'qualification.json')
        require(r['source_commit'] == source and len(r['results']) == 130 and
                {(x['mode'],x['sanitized']) for x in r['results']} ==
                {(case,mode) for case in frozen_package.SCENARIOS for mode in (False,True)} and
                all(x['returncode'] == 0 for x in r['results']), 'Incomplete host matrix')
        require(all(d in r['added_safety_defines'] for d in frozen.NATIVE), 'Not native host')
        w,h = profile.split('x')
        require('-DSPECTRUM_TEST_WIDTH='+w in r['added_safety_defines'] and
                '-DSPECTRUM_TEST_HEIGHT='+h in r['added_safety_defines'], 'Wrong host dimensions')
        for owner in ('system','runtime'):
            require(r['source_pins'][owner]['commit'] == frozen.PINS[owner] and
                    not r['source_pins'][owner]['status'], 'Wrong host dependency')
        for name,value in r['sources'].items():
            identity = frozen_package.source_identity(name,r['source_pins'])
            owner,relative = identity.split(':',1)
            require(digest(safe(out/'source'/owner,relative)) == value, 'Host source changed')
        for row in r['results']:
            p = folder/('sanitized' if row['sanitized'] else 'normal')/row['mode']/'run.log'
            require(p.is_file() and p.stat().st_size > 0, 'Missing or empty host run log')
        require(len(r['non_keyboard_views']) == 18 and len(r['normal_sanitized_pixel_identical']) >= 65,
                'Missing host pixel evidence')
        for name in r['normal_sanitized_pixel_identical']:
            require(digest(safe(folder/'normal',name)) == digest(safe(folder/'sanitized',name)), 'Host pixels differ')
        total += len(r['results'])
    return total


def required_runs():
    names = {'parent-frozen-target':0,'candidate-frozen-target':1,'native-hosts':0,
             'parent-native-parity':0,'candidate-native-parity':1,'parent-shared-parity':0,
             'candidate-shared-parity':1,'shared-current':0,'spectrum-suite':0,'storage-suite':0}
    for kind in ('before','guarded'):
        for mode in ('normal','sanitized'):
            label = 'neural-'+kind+'-'+mode
            names[label+'-build'] = 0; names[label] = -6 if kind == 'before' else 0
    return {'runs/'+k+'.json':v for k,v in names.items()}


def check_runs(out,codes):
    require(codes == required_runs(), 'Missing, extra or wrong-exit control runs')
    for relative,code in codes.items():
        record = read(safe(out,relative)); log = safe(out,relative.removesuffix('.json')+'.log')
        require(record['exit_code'] == code and digest(log) == record['log_sha256'], 'Run evidence changed')
    for name,error in [('candidate-native-parity',"AssertionError: ('selector-off token mismatch', [])"),
                       ('candidate-shared-parity','AssertionError: audio_spectrum.c opt-out preprocessor token mismatch: []')]:
        require(error in (out/'runs'/(name+'.log')).read_text(), 'Wrong frozen token failure')
    for name,text in [('spectrum-suite','Production Audio Spectrum normal and ASan/UBSan DSP and app fixtures passed'),
                      ('storage-suite','PASS'),('shared-current','Focused Spectrum shared-input safety passed')]:
        require(text in (out/'runs'/(name+'.log')).read_text(), 'Truncated successful control')
    parent = read(out/'parent-native-contract/qualification.json')
    require(len(parent['tests']) == 4 and len(parent['parity']) == 3 and
            all(x['identical'] for x in parent['parity']), 'Parent frozen native control incomplete')
    for label in ('parent-shared-evidence.json','shared-current-evidence.json'):
        r = read(out/label)
        require(len(r['tests']) == 2 and all(x['passed'] for x in r['tests']) and r['shared_local_keyboard_absent'],
                'Incomplete shared-input control')
    require(len(read(out/'parent-shared-evidence.json')['parity']) == 6 and
            all(x['token_identical'] for x in read(out/'parent-shared-evidence.json')['parity']),
            'Parent frozen shared parity incomplete')


def inventory(out):
    result = {}
    for p in sorted(out.rglob('*')):
        require(not p.is_symlink(), 'Transported evidence must not contain symlinks')
        if p.is_file() and p.relative_to(out).as_posix() != 'candidate-evidence.json':
            result[p.relative_to(out).as_posix()] = digest(p)
    return result


def validate(out,source):
    r = read(out/'candidate-evidence.json')
    require(r['source_commit'] == source and r['captured_head'] == CAPTURED and
            r['guard_source'] == GUARD_SOURCE and r['source_parent'] == PARENT, 'Wrong source evidence')
    require(r['qualification_only'] is True and r['distributable'] is False and r['version'] is None and
            r['hardware_qualified'] is False and r['published'] is False, 'Overstated scope')
    require((out/'NOT_FOR_INSTALLATION.txt').read_text() == NOTICE, 'Missing non-distributable notice')
    require(target_delta(out,source) == r['target_delta'], 'Changed target evidence')
    require(set(r['frozen_file_sha256']) == set(FROZEN_FILES), 'Missing frozen file bindings')
    for n,h in r['frozen_file_sha256'].items():
        require(digest(out/'source/utilities'/n) == h == digest(ROOT/n), 'Frozen gate changed')
    require(all(digest(out/'source/utilities'/p) == h for p,h in GUARD_FILES.items()) and
            digest(out/'source/utilities'/FIXTURE) == FIXTURE_SHA, 'Wrong guard source or regression fixture')
    check_neural(out); check_runs(out,r['run_exit_codes']); check_target_attempts(out)
    require(check_hosts(out,source) == r['host_executions'] == 650, 'Wrong host execution count')
    require(inventory(out) == r['artifact_sha256'], 'Absent, modified or extra artifact file')
    return r


def qualify(args):
    source = source_guard(); out = args.output.resolve()
    require(not out.exists(), 'Output must be new; refusing stale evidence')
    out.mkdir(parents=True); (out/'NOT_FOR_INSTALLATION.txt').write_text(NOTICE)
    repos = {name:getattr(args,name).resolve() for name in frozen.PINS}
    parent = args.parent_utilities.resolve()
    checks = [(parent,PARENT),(args.legacy_system,LEGACY_SYSTEM),(args.shared_system,SHARED_SYSTEM),
              *[(p,frozen.PINS[n]) for n,p in repos.items()]]
    for repo,pin in checks: pinned_checkout(repo,pin)
    for label,checkout,folder in [('parent-frozen-target',parent,out/'parent-target'),
                                   ('candidate-frozen-target',ROOT,out/'target')]:
        cmd = [sys.executable,checkout/'scripts/build_native_spectrum_qualification.py','--native-ui',
               '--xtensa-cc',args.xtensa_cc,'--output-dir',folder]
        for name,repo in repos.items(): cmd += ['--'+name,repo]
        run_target(cmd,out,label,folder)
        capture_toolchain(folder,args.xtensa_cc)
    delta = target_delta(out,source)
    copy_git(ROOT,source,out/'source/utilities',['Apps','lib','scripts','tests','test','profiles','LICENSE',
             '.github/workflows/native-spectrum.yml','.github/workflows/watch-spectrum-shared-input.yml',
             '.github/workflows/t040-neural-guard.yml','docs/t040/candidate-qualification','docs/consolidation/20261022'])
    for owner,repo,pin in [('legacy-system',args.legacy_system,LEGACY_SYSTEM),
                            ('shared-system',args.shared_system,SHARED_SYSTEM),
                            *[(n,p,frozen.PINS[n]) for n,p in repos.items()]]:
        copy_git(repo,pin,out/'source'/owner,['lib','Apps','Services','sdk','apps','include','profiles'])
    copy_git(ROOT,frozen.BASE,out/'native-baseline',['Apps','lib'])
    copy_git(ROOT,SHARED_BASE,out/'shared-baseline',['Apps','lib'])
    neural_controls(out)
    run([sys.executable,ROOT/'scripts/test_spectrum_native_ui.py','--system-apps',repos['system'],
         '--runtime',repos['runtime'],'--watch-profile',repos['watch'],'--output',out/'host'],out,'native-hosts')
    for label,checkout in [('parent-native-parity',parent),('candidate-native-parity',ROOT)]:
        run([sys.executable,checkout/'scripts/test_spectrum_native_contract.py','--legacy-system',out/'source/legacy-system',
             '--sdk-include',out/'host/240x240/sdk/include','--baseline',out/'native-baseline',
             '--output',out/('parent-native-contract' if checkout == parent else 'candidate-native-contract')],
            out,label,expected=0 if checkout == parent else 1)
    for label,checkout,baseline in [('parent-shared-parity',parent,True),('candidate-shared-parity',ROOT,True),
                                    ('shared-current',ROOT,False)]:
        cmd = [sys.executable,checkout/'scripts/test_spectrum_shared_input_safety.py',
               '--system-apps',out/'source/shared-system']
        if baseline: cmd += ['--baseline-utilities',out/'shared-baseline']
        run(cmd,out,label,expected=1 if label == 'candidate-shared-parity' else 0)
        if label != 'candidate-shared-parity':
            shutil.copyfile(checkout/'build/spectrum-shared-input-safety/evidence.json',
                            out/('parent-shared-evidence.json' if checkout == parent else 'shared-current-evidence.json'))
    for label,name,headers in [('spectrum-suite','test_audio_spectrum.py','legacy-system'),
                               ('storage-suite','test_spectrum_storage_lifecycle.py','system')]:
        run([sys.executable,ROOT/'scripts'/name,'--system-apps',out/'source'/headers],out,label)
    # Materialize the frozen builder's safe internal SDK aliases for transport.
    for p in sorted(out.rglob('*')):
        if p.is_symlink():
            require(p.resolve().is_relative_to(out), 'Symlink points outside evidence')
            data=p.read_bytes();p.unlink();p.write_bytes(data)
    require(source_guard() == source, 'Source changed during qualification')
    # Parent controls create ignored build products; only its tracked files must remain clean.
    require(not git(parent,'status','--porcelain').strip(), 'Parent source changed')
    for repo,pin in checks[1:]: pinned_checkout(repo,pin)
    codes={p.relative_to(out).as_posix():read(p)['exit_code'] for p in sorted((out/'runs').glob('*.json'))}
    report={'schema':1,'qualification_only':True,'distributable':False,'version':None,'published':False,
            'hardware_qualified':False,'source_commit':source,'source_tree':git(ROOT,'rev-parse','HEAD^{tree}').decode().strip(),
            'captured_head':CAPTURED,'guard_source':GUARD_SOURCE,'source_parent':PARENT,'target_delta':delta,
            'host_executions':check_hosts(out,source),'run_exit_codes':codes,
            'frozen_file_sha256':{n:digest(ROOT/n) for n in FROZEN_FILES},'artifact_sha256':inventory(out)}
    write(out/'candidate-evidence.json',report);validate(out,source)
    print('T040 qualification-only controls PASS. Frozen token/ELF gates remain FAILED. No distributable identity.',flush=True)


def main():
    p=argparse.ArgumentParser(description=__doc__); sub=p.add_subparsers(dest='action',required=True)
    q=sub.add_parser('qualify')
    for name in ('parent-utilities','system','legacy-system','shared-system','runtime','bridges','watch','wave','xtensa-cc','output'):
        q.add_argument('--'+name,type=Path,required=True)
    v=sub.add_parser('verify')
    v.add_argument('--artifact',type=Path,required=True);v.add_argument('--source-sha',required=True)
    v.add_argument('--receipt-sha256',required=True)
    a=p.parse_args()
    if a.action == 'qualify': qualify(a)
    else:
        require(source_guard() == a.source_sha, 'Verifier checkout differs from expected source')
        require(re.fullmatch('[0-9a-f]{64}',a.receipt_sha256) is not None and
                digest(a.artifact/'candidate-evidence.json') == a.receipt_sha256, 'Transported receipt digest changed')
        validate(a.artifact.resolve(),a.source_sha)
        print('Transported T040 exact-source qualification evidence verified; frozen gates remain failed.')


if __name__ == '__main__':main()
