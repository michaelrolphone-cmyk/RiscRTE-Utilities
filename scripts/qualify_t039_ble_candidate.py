#!/usr/bin/env python3
"""Non-distributable T039 software evidence; never replaces frozen release gates."""
import argparse
from collections import defaultdict
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys

import build_native_ble_buttons_qualification as frozen
import test_ble_buttons_native as host

ROOT = Path(__file__).resolve().parents[1]
CAPTURED = '39e9247528b39519397e3857d75074dddc4cac82'
PARENT = '6b2047500905f8f45087cb49989103a926fc5c63'
WATCH = '851b8998e804c57b2c9f42d9f8fd0a69e79f2b41'
LEGACY_SYSTEM = 'f1db70fcc3ba9df2a3551a711a9d3b0b714a5f8a'
CONTROLLER = 'd5e9447899c97d1f7bad337f349e62526a81d481a721041b1706aebad3861cdf'
PARITY_ERROR = 'Selector-off ELF differs from exact Utilities baseline; inspect retained receipt'
ADDITIONS = {'.github/workflows/t039-ble-candidate.yml', 'scripts/qualify_t039_ble_candidate.py',
             'tests/test_t039_ble_candidate.py', 'docs/t039/candidate-qualification/README.md'}
FROZEN_FILES = ('scripts/build_native_ble_buttons_qualification.py',
                'scripts/test_ble_buttons_native_contract.py',
                'scripts/prepare_native_ble_buttons_release.py', 'scripts/publish_native_ble_buttons.py',
                '.github/workflows/native-ble-buttons.yml', 'Apps/native/ble_buttons.json',
                'Apps/ble_buttons.json')
HOST_INPUTS = {'utilities:Apps/ble_buttons.c', 'utilities:Apps/ble_hid_app.inc',
               'utilities:Apps/ble_buttons_native_layout.h', 'utilities:Apps/ble_buttons_native_view.inc',
               'system:lib/PortableApps/src/adapter.c'}
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


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], timeout=120)


def source_guard():
    require(not git(ROOT, 'status', '--porcelain').strip(), 'Tracked source must be clean')
    changes = git(ROOT, 'diff', '--name-status', CAPTURED, 'HEAD').decode().splitlines()
    require(all(row.startswith('A\t') and row[2:] in ADDITIONS for row in changes),
            'Candidate lane may add qualification files only; captured source and all frozen gates must remain unchanged')
    require(digest(ROOT/'Apps/ble_hid_app.inc') == CONTROLLER, 'T039 captured controller changed')
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


def copy_git(repo, ref, destination, selections):
    for relative in git(repo, 'ls-tree', '-r', '--name-only', ref, '--', *selections).decode().splitlines():
        target = safe(destination, relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(git(repo, 'show', ref+':'+relative))


def validate_target(folder, *, source, parity):
    r = read(folder/'qualification.json')
    require(r['utilities_commit'] == source and not r['utilities_status'], 'Target source identity mismatch')
    require(r['script_sha256'] == digest(ROOT/'scripts/build_native_ble_buttons_qualification.py'), 'Frozen builder changed')
    require(r['utilities_base'] == frozen.BASE and r['baseline_system_pin'] == frozen.BASE_SYSTEM and
            r['exact_external_git_pins'] == frozen.PINS, 'Frozen target pins changed')
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
        mode = m['mode']; elf = folder/mode/'ble_buttons.elf'
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
            require(frozen.NATIVE_REQUIRED <= m['source_sha256'].keys(), 'Incomplete native closure')
        for identity, expected in m['source_sha256'].items():
            owner, relative = identity.split(':', 1)
            if mode == 'baseline' and owner in ('utilities', 'system'):
                owner += '-baseline'
            require(digest(safe(folder/'inputs'/owner, relative)) == expected, 'Changed target input '+identity)
    return r


def target_delta(out, source):
    parent = validate_target(out/'parent-target', source=PARENT, parity=True)
    candidate = validate_target(out/'target', source=source, parity=False)
    failure = read(out/'target/failure.json')
    require(failure == {'type': 'ValueError', 'error': PARITY_ERROR, 'qualified': False},
            'Frozen failure was not the expected exact parity refusal')
    require(not (out/'parent-target/failure.json').exists(), 'Parent target unexpectedly failed')
    for name in ('tools', 'toolchain_inputs', 'retained_recipe_sha256', 'native_selectors', 'script_sha256'):
        require(parent[name] == candidate[name], 'Parent/candidate recipe mismatch: '+name)
    pm = {m['mode']: m for m in parent['modules']}; cm = {m['mode']: m for m in candidate['modules']}
    require(pm['baseline']['sha256'] == cm['baseline']['sha256'] == pm['legacy']['sha256'],
            'Parent no longer reproduces frozen selector-off bytes')
    changes = {}
    for mode in ('legacy', 'native'):
        a, b = pm[mode]['source_sha256'], cm[mode]['source_sha256']
        changed = sorted(k for k in set(a)|set(b) if a.get(k) != b.get(k))
        require(changed == ['utilities:Apps/ble_hid_app.inc'], 'Unrelated compiled source change: '+mode)
        require(b[changed[0]] == CONTROLLER, 'Unexpected controller bytes')
        require(pm[mode]['sha256'] != cm[mode]['sha256'], 'Expected Buttons change absent')
        changes[mode] = changed
    return {'frozen_gate_status': 'failed', 'frozen_gate_error': PARITY_ERROR,
            'parent_selector_off_exact': True, 'intentional_changed_inputs': changes,
            'parent_target_sha256': digest(out/'parent-target/qualification.json'),
            'candidate_target_sha256': digest(out/'target/qualification.json')}


def contracts(out):
    inputs = out/'target/inputs'; system = inputs/'system'; util = inputs/'utilities'
    includes = [system/'Apps', util/'Apps', util/'lib/Bluetooth/include', system/'lib/PortableApps/include',
                system/'lib/NativeApps/include', system/'lib/PortableApps/src']
    inc = ['-I'+str(p) for p in includes]
    cc = os.environ.get('CC', 'cc'); results = []
    for pad, paper in ((0, False), (1, False), (0, True), (1, True)):
        flags = ['-DPORTABLE_APP_OWNS_TOUCH_CHROME', '-DPORTABLE_RETURN_APP="springboard.elf"',
                 '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_NOVA_UI', '-DHID_TOUCHPAD='+str(pad)]
        if paper:
            flags += ['-DPORTABLE_PAPER_HID']
        hashes = {}
        for name, path in [('frozen', inputs/'utilities-baseline/Apps/ble_hid_app.inc'),
                           ('parent', out/'parent-target/inputs/utilities/Apps/ble_hid_app.inc'),
                           ('candidate', util/'Apps/ble_hid_app.inc')]:
            label = f'tokens-{pad}-{int(paper)}-{name}'
            run([cc, '-E', '-P', '-std=c11', '-x', 'c', *flags, *inc, path], out, label)
            tokens = '\n'.join(re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\s]',
                                          (out/'runs'/(label+'.log')).read_text()))
            hashes[name] = sha(tokens.encode())
        require(hashes['frozen'] == hashes['parent'], 'Parent selector-off token mismatch')
        require((hashes['parent'] == hashes['candidate']) is bool(pad), 'Incorrect Buttons/Touchpad delta')
        results.append({'touchpad': bool(pad), 'paper': paper, 'token_sha256': hashes})
    for sanitized in (False, True):
        name = 'geometry-'+('sanitized' if sanitized else 'normal'); binary = out/name
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        run([cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags, *inc,
             ROOT/'tests/hid_buttons_native_test.c', '-o', binary], out, name+'-build')
        run([binary], out, name)
    write(out/'contracts.json', results)


def check_host(out, relative, sanitized, asynchronous, temporal, scenarios, source):
    folder = out/relative; r = read(folder/'receipt.json')
    require((r['sanitized'], r['async_rgb565'], r['temporal']) == (sanitized, asynchronous, temporal), 'Wrong host mode')
    require(r['full_feature_profile'] is True and r['full_profile'] is True and r['source_unchanged_during_run'] is True,
            'Host must execute full feature recipe')
    require(not r['counterexamples'] and r['hardware_tested'] is False and r['target_instructions_executed'] is False,
            'Host failed or overstated scope')
    require(r['target_qualification_sha256'] == digest(out/'target/qualification.json'), 'Wrong host target')
    expected_defines = [d for d in read(out/'target/native/command.json') if d.startswith('-D')]
    if not asynchronous:
        expected_defines = [d for d in expected_defines if d != '-DPORTABLE_ASYNC_RGB565']
    require(r['build_defines'] == expected_defines and r['system_git_pin'] == frozen.PINS['system'] and
            r['system_working_tree_allowed'] is False and r['snapshot'] is False, 'Host recipe differs')
    require(r['command'] == read(folder/'command.json') and all(d in r['command'] for d in expected_defines) and
            '-DHID_RENDER_FULL_PROFILE' in r['command'], 'Host compiler command differs')
    sanitizer_flags = {'-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'}
    require(sanitizer_flags <= set(r['command']) if sanitized else
            not any(d.startswith('-fsanitize=') for d in r['command']), 'Host sanitizer flags differ')
    expected = {(w, h, s) for w, h in host.SIZES for s in scenarios}
    actual = [(x['width'], x['height'], x['scene']) for x in r['results']]
    require(len(actual) == len(expected) and set(actual) == expected and all(x['exit_code'] == 0 for x in r['results']),
            'Incomplete or failing host matrix: '+relative)
    require(set(r['required_scenarios']) == set(scenarios), 'Wrong required host scenarios')
    for x in r['results']:
        log = safe(folder, x['log'])
        require(log.stat().st_size > 0 and digest(log) == x['log_sha256'], 'Changed host log')
        require(temporal or x['captures'], 'Missing host captures')
        require(all(name in r['capture_sha256'] for name in x['captures']), 'Unbound host capture')
    for name, value in r['capture_sha256'].items():
        require(digest(safe(folder, name)) == value, 'Changed host frame')
    require(digest(folder/'ble-buttons-native-test') == r['binary_sha256'], 'Changed host binary')
    require(r['capture_sha256'], 'Missing host raster evidence')
    audit = r['native_pixel_audit']; title = audit['title_raster']
    require(len(title) == 5 and title[1] > 0 and title[2] > 0 and title[3] > 0 and audit['icon_rasters'] and
            all(len(v) == 2 and v[0] > 0 for v in audit['icon_rasters'].values()), 'Incomplete native pixel audit')
    require(HOST_INPUTS <= r['compiled_dependency_sha256'].keys(), 'Missing actual-adapter host source binding')
    for identity, value in r['compiled_dependency_sha256'].items():
        owner, relative_path = identity.split(':', 1)
        base = out/'source/utilities' if owner == 'utilities' else out/'target/inputs'/owner
        require(digest(safe(base, relative_path)) == value, 'Changed host source '+identity)
    return len(actual)


def control_matrix():
    scenes = set(host.full_scenes(defaultdict(lambda: (0, 0, 1, 1))))
    rows = []
    for sanitized in (False, True):
        for asynchronous in (False, True):
            mode = ('sanitized' if sanitized else 'normal')+'-'+('async' if asynchronous else 'sync')
            rows.append(('host/'+mode, sanitized, asynchronous, False, scenes, []))
        mode = 'sanitized' if sanitized else 'normal'
        rows.append(('temporal/'+mode, sanitized, True, True, set(host.TEMPORAL_SCENARIOS), ['--temporal']))
        rows.append(('mixed/'+mode, sanitized, True, True, {'temporal-16'}, ['--temporal', '--mixed-contacts']))
    return rows


def required_runs():
    labels = {'parent-frozen-target', 'candidate-frozen-target', 'geometry-normal-build', 'geometry-normal',
              'geometry-sanitized-build', 'geometry-sanitized', 'queue-normal', 'queue-sanitized',
              'hid-apps', 'renderer-color', 'renderer-paper'}
    labels |= {f'tokens-{pad}-{paper}-{kind}' for pad in (0, 1) for paper in (0, 1)
               for kind in ('frozen', 'parent', 'candidate')}
    labels |= {row[0].replace('/', '-') for row in control_matrix()}
    return {'runs/'+label+'.json' for label in labels}


def queue_cases():
    # Parse only literal case declarations in the unchanged captured runner.
    import ast
    tree = ast.parse((ROOT/'scripts/test_hid_buttons_watch_queue.py').read_text())
    cases = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'cases' for t in node.targets) and isinstance(node.value, ast.IfExp):
            cases += ast.literal_eval(node.value.orelse)
        if isinstance(node, ast.AugAssign) and isinstance(node.target, ast.Name) and node.target.id == 'cases':
            cases += ast.literal_eval(node.value)
    require(len(cases) == 50 and len(set(cases)) == 50, 'Captured queue case definitions changed')
    return set(cases)


def validate(out, source, check_inventory=True):
    r = read(out/'candidate-evidence.json')
    require(r['source_commit'] == source and r['captured_source'] == CAPTURED and r['source_parent'] == PARENT,
            'Wrong source evidence')
    require(r['qualification_only'] is True and r['distributable'] is False and r['version'] is None and
            r['hardware_qualified'] is False and r['published'] is False, 'Invalid qualification scope')
    require((out/'NOT_FOR_INSTALLATION.txt').read_text() == NOTICE, 'Missing qualification-only notice')
    require(target_delta(out, source) == r['target_delta'], 'Target delta changed')
    require(digest(out/'source/utilities/Apps/ble_hid_app.inc') == CONTROLLER, 'Wrong source controller')
    for name, value in r['frozen_file_sha256'].items():
        require(name in FROZEN_FILES and digest(safe(out/'source/utilities', name)) == value, 'Frozen source mismatch')
    require(set(r['frozen_file_sha256']) == set(FROZEN_FILES), 'Missing frozen gates')
    counts = {name: check_host(out, name, san, async_, temporal, scenes, source)
              for name, san, async_, temporal, scenes, _ in control_matrix()}
    require(counts == r['host_case_counts'], 'Host count mismatch')
    tokens = read(out/'contracts.json')
    require(len(tokens) == 4 and {(x['touchpad'], x['paper']) for x in tokens} ==
            {(p, q) for p in (False, True) for q in (False, True)}, 'Incomplete token contracts')
    for x in tokens:
        h = x['token_sha256']
        require(h['parent'] == h['frozen'] and (h['parent'] == h['candidate']) is x['touchpad'], 'Bad token contract')
    require(set(r['run_exit_codes']) == required_runs(), 'Missing mandatory control runs')
    for mode in ('normal', 'sanitized'):
        geometry = (out/'runs'/('geometry-'+mode+'.log')).read_text()
        require(all(f'Native Buttons {w}x{h}:' in geometry for w, h in host.SIZES) and
                'Native bounded geometry and controller coverage PASS' in geometry, 'Incomplete geometry controls')
    for relative, code in r['run_exit_codes'].items():
        record = read(safe(out, relative)); log = safe(out, relative.removesuffix('.json')+'.log')
        require(record['exit_code'] == code and digest(log) == record['log_sha256'], 'Run evidence changed')
        require(code == 0 or relative == 'runs/candidate-frozen-target.json' and code == 1, 'Unexpected failed run')
    for mode in ('normal', 'sanitized'):
        queue = read(out/'queue'/mode/'qualification.json')
        require(len(queue['cases']) == 50 and set(queue['cases']) == queue_cases(),
                'Incomplete production touch queue controls')
        require(queue['sanitized'] is (mode == 'sanitized') and queue['hardware_tested'] is False, 'Queue mode mismatch')
    if check_inventory:
        actual = inventory(out)
        require(actual == r['artifact_sha256'], 'Artifact inventory mismatch')
    return r


def inventory(out):
    return {p.relative_to(out).as_posix(): digest(p) for p in sorted(out.rglob('*'))
            if p.is_file() and p.relative_to(out).as_posix() != 'candidate-evidence.json'}


def qualify(args):
    source = source_guard(); out = args.output.resolve()
    require(not out.exists(), 'Output must be new; refusing stale evidence')
    out.mkdir(parents=True); (out/'NOT_FOR_INSTALLATION.txt').write_text(NOTICE)
    repos = {name: getattr(args, name).resolve() for name in ('system', 'runtime', 'bridges')}
    parent = args.parent_utilities.resolve()
    for repo, pin in [(parent, PARENT), (args.watch, WATCH), (args.legacy_system, LEGACY_SYSTEM),
                      *[(repo, frozen.PINS[name]) for name, repo in repos.items()]]:
        require(git(repo, 'rev-parse', 'HEAD').decode().strip() == pin and
                not git(repo, 'status', '--porcelain').strip(), 'Dependency checkout changed: '+str(repo))
    for label, checkout, folder in [('parent-frozen-target', parent, out/'parent-target'),
                                     ('candidate-frozen-target', ROOT, out/'target')]:
        command = [sys.executable, checkout/'scripts/build_native_ble_buttons_qualification.py', '--native-ui',
                   '--xtensa-cc', args.xtensa_cc, '--output-dir', folder]
        for name, repo in repos.items():
            command += ['--'+name, repo]
        code = run(command, out, label, expected=0 if checkout == parent else None)
        if checkout == ROOT:
            require(code == 1, 'Expected frozen parity refusal missing')
        r = read(folder/'qualification.json')
        for m in r['modules']:
            for identity, value in m['source_sha256'].items():
                if identity.startswith('toolchain:'):
                    relative = identity.split(':', 1)[1]; original = args.xtensa_cc.absolute().parents[1]/relative
                    require(digest(original) == value, 'Toolchain header changed')
                    target = safe(folder/'inputs/toolchain', relative); target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(original.read_bytes())
    delta = target_delta(out, source)
    copy_git(ROOT, source, out/'source/utilities', ['Apps', 'lib', 'scripts', 'test', 'tests', 'profiles',
              'LICENSE', '.github/workflows/native-ble-buttons.yml', '.github/workflows/t039-ble-candidate.yml',
              'docs/t039/candidate-qualification'])
    contracts(out)
    for relative, sanitized, asynchronous, temporal, scenes, extra in control_matrix():
        command = [sys.executable, ROOT/'scripts/test_ble_buttons_native.py', '--system-apps', args.system,
                   '--runtime', args.runtime, '--full-target', out/'target', '--output', out/relative, *extra]
        if sanitized: command += ['--sanitize']
        if asynchronous: command += ['--async']
        run(command, out, relative.replace('/', '-'))
    for mode in ('normal', 'sanitized'):
        command = [sys.executable, ROOT/'scripts/test_hid_buttons_watch_queue.py', '--system-apps', args.legacy_system,
                   '--watch', args.watch, '--output', out/'queue'/mode]
        if mode == 'sanitized': command += ['--sanitize']
        run(command, out, 'queue-'+mode)
    for label, script, extra in [('hid-apps', 'test_hid_apps.py', []),
                                  ('renderer-color', 'test_hid_renderer.py', []),
                                  ('renderer-paper', 'test_hid_renderer.py', ['--paper'])]:
        run([sys.executable, ROOT/'scripts'/script, '--system-apps', args.legacy_system, *extra], out, label)
    for name in ('hid-apps', 'hid-renderer', 'hid-paper-renderer'):
        shutil.copytree(ROOT/'build'/name, out/'controls-generated'/name)
    copy_git(args.watch, WATCH, out/'source/watch-touch', ['sdk/driver', 'include', 'drivers/current/twatch_touch'])
    copy_git(args.legacy_system, LEGACY_SYSTEM, out/'source/legacy-system', ['Apps', 'lib/PortableApps', 'lib/NativeApps'])
    # Artifact upload/download materializes symlinks. Freeze their bytes first,
    # without editing any source inputs or the frozen result receipts.
    for p in sorted(out.rglob('*')):
        if p.is_symlink():
            data = p.read_bytes(); p.unlink(); p.write_bytes(data)
    require(source_guard() == source, 'Source changed during qualification')
    for repo in [parent, args.watch, args.legacy_system, *repos.values()]:
        require(not git(repo, 'status', '--porcelain').strip(), 'Dependency source changed during controls')
    counts = {name: check_host(out, name, san, async_, temporal, scenes, source)
              for name, san, async_, temporal, scenes, _ in control_matrix()}
    report = {'schema': 1, 'qualification_only': True, 'distributable': False, 'version': None,
              'hardware_qualified': False, 'published': False, 'source_commit': source,
              'source_tree': git(ROOT, 'rev-parse', 'HEAD^{tree}').decode().strip(),
              'captured_source': CAPTURED, 'source_parent': PARENT, 'target_delta': delta,
              'host_case_counts': counts, 'frozen_file_sha256': {n: digest(ROOT/n) for n in FROZEN_FILES},
              'run_exit_codes': {p.relative_to(out).as_posix(): read(p)['exit_code'] for p in sorted((out/'runs').glob('*.json'))},
              'artifact_sha256': inventory(out)}
    write(out/'candidate-evidence.json', report); validate(out, source)
    print('T039 qualification-only controls PASS; frozen release gate remains FAILED; no distributable identity.', flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='action', required=True)
    q = sub.add_parser('qualify')
    for name in ('parent-utilities', 'system', 'legacy-system', 'runtime', 'bridges', 'watch', 'xtensa-cc', 'output'):
        q.add_argument('--'+name, type=Path, required=True)
    v = sub.add_parser('verify'); v.add_argument('--artifact', type=Path, required=True); v.add_argument('--source-sha', required=True); v.add_argument('--receipt-sha256', required=True)
    args = p.parse_args()
    if args.action == 'qualify': qualify(args)
    else:
        require(source_guard() == args.source_sha, 'Verifier checkout differs from expected source')
        require(digest(args.artifact/'candidate-evidence.json') == args.receipt_sha256, 'Transported evidence receipt changed')
        validate(args.artifact.resolve(), args.source_sha)
        print('Transported T039 evidence matches exact source; qualification only, frozen gate remains failed.')


if __name__ == '__main__':
    main()
