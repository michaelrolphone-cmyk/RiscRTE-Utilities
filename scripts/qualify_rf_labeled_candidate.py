#!/usr/bin/env python3
"""Bounded RF host qualification, reconstructed from the pinned PR93 contracts.

This is not recovery of the absent six-file candidate payload. The frozen codec,
default hash, product pins, token gate and target ELF gate remain unchanged.
The current composition and its exact neural comparison overlay are separately
admitted. Both System cohorts are explicit and never pooled. Historical nondefault CFP
provenance is unresolved. Synthetic callbacks are not physical acceptance.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import difflib
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BASE = '3565ac151765705457b34b7d0ac4bbcc5125c753'
PREDECESSOR = '6b2047500905f8f45087cb49989103a926fc5c63'
FROZEN = '82d17d0d902d427938af95aae0e976c8f78041ed'
SYSTEMS = {'historical-host': 'f1db70fcc3ba9df2a3551a711a9d3b0b714a5f8a',
           'ci-composition': 'f3731ab02127dea86a308472e3780068c5a0a1cf'}
AUDIO_OVERLAY = {'Apps/audio_spectrum.c': {'commit': '390b8ee8580db7d7b3e37b99c38cb10378d6d413', 'sha256': '974ba8acf763df90999140711bea6edff00765bbcd46e5221003fd1babe971d3'}, 'Apps/spectrum_temporal_app.inc': {'commit': '390b8ee8580db7d7b3e37b99c38cb10378d6d413', 'sha256': '42853d31cba4a26976c6b0f0f5f5e21471d96acba5db860ae688e25466804d39'}}
ADDITIONS = sorted(['profiles/rf-labeled-candidate-v1.json', 'scripts/qualify_rf_labeled_candidate.py',
    'test/native_apps/rf_candidate_optout_test.c', 'test/native_apps/rf_candidate_audio_differential_test.c',
    'tests/test_rf_labeled_candidate_qualification.py', '.github/workflows/rf-labeled-candidate.yml'])
EVIDENCE_ARCHIVE = 'docs/evidence/rf-labeled-candidate-20261022.tar.gz'
GUARD_FILES = sorted(['Apps/context_fingerprint_app.inc', 'Apps/context_signal_library.inc',
    'Apps/rf_application.inc', 'Apps/rf_controls.inc', 'Apps/rf_signature_app.inc', 'Apps/rf_temporal_app.inc'])
PROFILES = {'plain': [], 'nova-alarm': ['PORTABLE_NOVA_UI', 'PORTABLE_ALARM_CLIENT'],
    'resident-contexts': ['PORTABLE_RADIO_CONTINUOUS_CAPTURE', 'PORTABLE_RESIDENT_POLICY',
    'PORTABLE_CONTEXTS_CLIENT', 'PORTABLE_BLE_BROADCAST', 'PORTABLE_HOME_APP="default.elf"',
    'PORTABLE_RETURN_APP="springboard.elf"']}
COUNTS = {'rf-labeled': 4, 'rf-optout': 4, 'rf-safety': 9, 'rf-models': 6, 'audio-behavior': 8, 'audio-code-data': 12}
CONTRACT = {'schema': 1, 'base': BASE, 'guard_source': '055f86ea32df0f10ca96c5aa48cbb64199eadcba',
    'predecessor': PREDECESSOR, 'frozen': FROZEN, 'systems': SYSTEMS, 'additions': ADDITIONS,
    'optional_evidence_archive': EVIDENCE_ARCHIVE, 'production_delta': GUARD_FILES,
    'default_identity_hash': 2931434081, 'distributable': False, 'hardware_verified': False,
    'target_elf_qualified': False, 'historical_nondefault_cfp_provenance': 'unresolved',
    'profiles': PROFILES, 'payload_origin': 'bounded reconstruction; not the absent original six-file payload'}


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    return digest(Path(path).read_bytes())


def valid_hash(value):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value) is not None


def call(command, cwd=None, env=None, timeout=240, combined=False):
    p = subprocess.run([str(x) for x in command], cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout)
    require(p.returncode == 0, f'{command!r}: exit {p.returncode}\n{p.stdout}\n{p.stderr}')
    return p.stdout + p.stderr if combined else p.stdout


def git(tree, *args):
    return call(['git', '-C', tree, *args]).strip()


def expected_names():
    runs = {f'{stage}/{continuous}-{sanitized}' for stage in ('rf-labeled', 'rf-optout')
            for continuous in (0, 1) for sanitized in (0, 1)}
    runs |= {f'rf-safety/{profile}-{mode}' for profile in ('watch', 'continuous-resident-home', 'app-data')
             for mode in ('normal', 'asan', 'ubsan')}
    runs |= {f'rf-models/rf_{model}_model_test{sanitized}' for model in ('dsp', 'room_store', 'temporal')
             for sanitized in (0, 1)}
    runs |= {f'audio-behavior/{shared}-{sanitized}-{side}' for shared in (False, True)
             for sanitized in (False, True) for side in ('predecessor', 'candidate')}
    runs |= {f'audio-code/{profile}-{shared}-{side}.o' for profile in PROFILES for shared in (False, True)
             for side in ('predecessor', 'candidate')}
    pairs = {f'behavior-{shared}-{sanitized}' for shared in (False, True) for sanitized in (False, True)}
    pairs |= {f'code-data-{profile}-{shared}' for profile in PROFILES for shared in (False, True)}
    tokens = {f'{transition}-{source}-{profile}-{shared}'
              for transition in ('frozen-to-predecessor', 'predecessor-to-candidate')
              for source, shared in (('waterfall.c', False), ('audio_spectrum.c', False), ('audio_spectrum.c', True))
              for profile in PROFILES}
    return runs, pairs, tokens


def validate_admission(profile, facts):
    require(profile == CONTRACT, 'qualification contract changed')
    require(facts['candidate'] == facts['expected_candidate'] and
            re.fullmatch('[0-9a-f]{40}', facts['candidate']), 'full exact candidate revision required')
    composition = facts.get('composition')
    if composition:
        require(composition.get('source') == facts['candidate'] and 'signal-capture' in composition.get('partitions', []),
                'composition does not bind the exact signal candidate')
    else:
        require(facts['base_ancestor'], 'PR93 is not an ancestor')
    require(facts['predecessor'] == PREDECESSOR and facts['frozen'] == FROZEN, 'comparison pin mismatch')
    require(facts['cohort'] in SYSTEMS and facts['system'] == SYSTEMS[facts['cohort']], 'System cohort mismatch')
    require(set(facts['dirty']) == {'candidate', 'predecessor', 'frozen', 'system'} and not any(facts['dirty'].values()), 'dirty or missing input tree')
    if not composition:
        require({p: state for p, state in facts['delta'].items() if p != EVIDENCE_ARCHIVE} ==
                {p: 'A' for p in ADDITIONS}, 'only six qualification source additions admitted')
        require(facts['delta'].get(EVIDENCE_ARCHIVE, 'A') == 'A', 'evidence must be additive')
    require(sorted(facts['production_delta']) == GUARD_FILES, 'unexpected PR93 production delta')


def elf_sections(data):
    require(data[:6] == b'\x7fELF\x02\x01' and len(data) >= 64, 'expected ELF64 little-endian host object')
    offset = struct.unpack_from('<Q', data, 40)[0]
    size, count, names = struct.unpack_from('<HHH', data, 58)
    require(size == 64 and count > 0 and names < count and offset + count * size <= len(data), 'invalid section table')
    rows = [struct.unpack_from('<IIQQQQIIQQ', data, offset + i * size) for i in range(count)]
    strings = rows[names]
    require(strings[4] + strings[5] <= len(data), 'invalid section names')
    table = data[strings[4]:strings[4] + strings[5]]
    result = {}
    for row in rows:
        require(row[0] < len(table) and (row[1] == 8 or row[4] + row[5] <= len(data)), 'invalid section range')
        name = table[row[0]:].split(b'\0', 1)[0].decode()
        require(name not in result, 'duplicate section name')
        result[name] = {'type': row[1], 'flags': row[2], 'size': row[5],
            'bytes': b'' if row[1] == 8 else data[row[4]:row[4] + row[5]]}
    return result


def compare_sections(before, after):
    """No code/data/relocation difference is normalized or waived."""
    require(before.keys() == after.keys(), 'Audio section inventory mismatch')
    changes = []
    for name in before:
        a, b = before[name], after[name]
        if a == b:
            continue
        require(not (a['flags'] & 2 or b['flags'] & 2) and a['type'] not in (4, 9) and b['type'] not in (4, 9),
                'Audio allocated code/data or relocation mismatch: ' + name)
        require(name == '.strtab' and a['type'] == b['type'] == 3 and a['flags'] == b['flags'],
                'unaccounted Audio metadata difference: ' + name)
        a_names, b_names = set(a['bytes'].split(b'\0')), set(b['bytes'].split(b'\0'))
        removed, added = sorted(a_names - b_names), sorted(b_names - a_names)
        require(len(removed) == len(added) and removed and
                all(re.fullmatch(rb'CSWTCH\.[0-9]+', s) for s in removed + added),
                'only compiler-generated switch-table numbering may differ')
        changes.append({'section': name, 'before_only': [s.decode() for s in removed],
                        'after_only': [s.decode() for s in added]})
    return changes


def validate_result(record):
    require(all(record[k] is False for k in ('distributable', 'hardware_verified', 'target_elf_qualified')), 'unsupported acceptance claim')
    require(record['source_hashes_unchanged'] is True and record.get('inputs') and
            all(valid_hash(h) for h in record['inputs'].values()), 'missing or changed input evidence')
    require(record['default_identity_hash'] == 2931434081, 'default hash changed')
    if record.get('admission', {}).get('composition'):
        require(record.get('audio_comparison_baseline') == {'base': PREDECESSOR, 'overlay': AUDIO_OVERLAY},
                'RF comparison omitted or substituted the approved neural overlay')
    require(record['historical_nondefault_cfp_provenance'] == 'unresolved', 'historical provenance overclaim')
    require(record['counts'] == COUNTS, 'missing matrix row count')
    require(set(record['stages']) == set(COUNTS) | {'manifest-tests'} and
            all(x['status'] == 'passed' for x in record['stages'].values()), 'failed or missing stage')
    names, pairs, tokens = expected_names()
    require(len(record['runs']) == len(names) and {r['name'] for r in record['runs']} == names, 'run inventory mismatch')
    require(all(r['status'] == 'passed' and valid_hash(r['binary_sha256']) and r['dependencies'] and
                all(valid_hash(h) for h in r['dependencies'].values()) for r in record['runs']), 'invalid execution evidence')
    require(all(r.get('selector_verified_off', False) == r['name'].startswith('rf-optout/') for r in record['runs']), 'genuine optout evidence missing or misplaced')
    require(len(record['audio_pairs']) == len(pairs) and {p['name'] for p in record['audio_pairs']} == pairs, 'Audio pair inventory mismatch')
    by_name = {r['name']: r for r in record['runs']}
    for pair in record['audio_pairs']:
        require(valid_hash(pair['before']) and pair['before'] == pair['after'], 'Audio equivalence failed')
        if pair['name'].startswith('behavior-'):
            key = pair['name'].removeprefix('behavior-')
            a, b = [digest(by_name[f'audio-behavior/{key}-{side}']['stdout'].encode()) for side in ('predecessor', 'candidate')]
        else:
            key = pair['name'].removeprefix('code-data-')
            a, b = [by_name[f'audio-code/{key}-{side}.o']['normalized_sha256'] for side in ('predecessor', 'candidate')]
            require(pair['allocated_sections_identical'] is True and pair['relocations_identical'] is True and
                    pair['raw_object_status'] in ('passed', 'failed'), 'missing strict Audio section evidence')
        require(pair['before'] == a and pair['after'] == b, 'Audio pair detached from execution evidence')
    require(len(record['token_comparisons']) == len(tokens) and {r['name'] for r in record['token_comparisons']} == tokens, 'token inventory mismatch')
    require(all(valid_hash(r['before']) and valid_hash(r['after']) and
                r['status'] == ('passed' if r['before'] == r['after'] else 'failed') for r in record['token_comparisons']), 'invalid token evidence')
    frozen = record['frozen_contract']
    require(frozen['status'] == 'failed' and frozen['waived'] is False and frozen['target_elf_gate'] == 'not-run', 'frozen incompatibility omitted or waived')
    by_token = {r['name']: r for r in record['token_comparisons']}
    for row in frozen['token_checks']:
        key = row['name'].removeprefix('frozen-to-candidate')
        before = by_token['frozen-to-predecessor' + key]['before']
        after = by_token['predecessor-to-candidate' + key]['after']
        require(row['status'] == ('passed' if before == after else 'failed'), 'final frozen check detached from token evidence')
    storage_names = {'Apps/rf_app_data_scope.h', 'Apps/rf_temporal_app.inc', 'Apps/context_fingerprint_app.inc',
                     'test/native_apps/rf_startup_dirty_checkpoint_test.c', 'test/native_apps/rf_startup_contexts_test.c'}
    require(set(frozen['unchanged_storage_checks']) == storage_names and
            all(v in ('passed', 'failed') for v in frozen['unchanged_storage_checks'].values()), 'missing frozen byte checks')
    require(any(r['status'] == 'failed' for r in frozen['token_checks']) or
            'failed' in frozen['unchanged_storage_checks'].values(), 'frozen failure not supported')
    expected_frozen = {n.replace('frozen-to-predecessor', 'frozen-to-candidate') for n in tokens if n.startswith('frozen-to-predecessor')}
    require(len(frozen['token_checks']) == 9 and {r['name'] for r in frozen['token_checks']} == expected_frozen and
            all(r['status'] in ('passed', 'failed') for r in frozen['token_checks']), 'missing final frozen comparisons')


def snapshot(tree):
    return {str(tree / p): sha(tree / p) for p in git(tree, 'ls-files').splitlines() if (tree / p).is_file()}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for option in ('system-apps', 'predecessor', 'frozen', 'output'):
        ap.add_argument('--' + option, type=Path, required=True)
    ap.add_argument('--cohort', choices=SYSTEMS, required=True)
    ap.add_argument('--candidate-revision', required=True)
    ap.add_argument('--jobs', type=int, default=2, choices=range(1, 5))
    args = ap.parse_args()
    system, predecessor, frozen, out = [p.resolve() for p in (args.system_apps, args.predecessor, args.frozen, args.output)]
    out.mkdir(parents=True, exist_ok=True)
    trees = {'candidate': ROOT, 'predecessor': predecessor, 'frozen': frozen, 'system': system}
    facts = {k: git(t, 'rev-parse', 'HEAD') for k, t in trees.items()}
    facts.update(expected_candidate=args.candidate_revision, cohort=args.cohort,
        dirty={k: git(t, 'status', '--porcelain', '--untracked-files=normal') for k, t in trees.items()},
        base_ancestor=subprocess.run(['git', '-C', str(ROOT), 'merge-base', '--is-ancestor', BASE, 'HEAD']).returncode == 0,
        delta={line.split('\t')[1]: line.split('\t')[0] for line in git(ROOT, 'diff', '--name-status', BASE, 'HEAD').splitlines()},
        production_delta=git(ROOT, 'diff', '--name-only', PREDECESSOR, BASE, '--', 'Apps', 'lib').splitlines())
    import consolidated_source_guard as composition
    if composition.available(ROOT):
        facts['composition'] = composition.validate(ROOT)
    (out / 'admission.json').write_text(json.dumps(facts, indent=2) + '\n')
    validate_admission(json.loads((ROOT / 'profiles/rf-labeled-candidate-v1.json').read_text()), facts)
    inputs = {}
    for tree in trees.values(): inputs.update(snapshot(tree))
    cc = str(Path(shutil.which(os.environ.get('CC', 'cc'))).resolve())
    os.environ['CC'] = cc
    compiler = {'path': cc, 'sha256': sha(cc), 'version': call([cc, '--version']), 'target': call([cc, '-dumpmachine']).strip()}
    inputs[cc] = sha(cc)
    for tool in ('cc1', 'as', 'ld'):
        value = call([cc, '-print-prog-name=' + tool]).strip()
        path = Path(shutil.which(value) or value).resolve()
        compiler[tool] = {'path': str(path), 'sha256': sha(path)}
        inputs[str(path)] = sha(path)
    objcopy = Path(shutil.which('objcopy')).resolve()
    compiler['objcopy'] = {'path': str(objcopy), 'sha256': sha(objcopy), 'version': call([objcopy, '--version'])}
    inputs[str(objcopy)] = sha(objcopy)
    record = {k: CONTRACT[k] for k in ('distributable', 'hardware_verified', 'target_elf_qualified', 'historical_nondefault_cfp_provenance', 'default_identity_hash', 'payload_origin')}
    record.update(schema=1, admission=facts, compiler=compiler, stages={}, counts={}, runs=[], audio_pairs=[], token_comparisons=[],
        frozen_contract={'status': 'not-run'}, source_hashes_unchanged=False, scoped_pass=False,
        sanitizer_environment={'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
    def receipt():
        (out / 'qualification.json').write_text(json.dumps(record, indent=2) + '\n')
    receipt()
    includes = ['-I' + str(system / 'lib/PortableApps/include'), '-I' + str(system / 'lib/NativeApps/include')]
    san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
    def build(spec):
        name, source, tree, defines, flags, execute = spec
        target = out / name
        target.parent.mkdir(parents=True, exist_ok=True)
        dep = Path(str(target) + '.d')
        command = [cc, '-std=c11', '-O1' if execute else '-O2', '-g0', '-Wall', '-Wextra', '-Werror', '-fno-ident', '-frandom-seed=rf-candidate',
            '-ffile-prefix-map=' + str(tree) + '=/src', '-ffile-prefix-map=' + str(system) + '=/system',
            '-MD', '-MF', str(dep), *flags, *['-D' + d for d in defines], *includes,
            '-I' + str(tree / 'Apps'), '-I' + str(tree / 'test/native_apps'), str(source)]
        command += ['-lm', '-o', str(target)] if execute else ['-c', '-o', str(target)]
        item = {'name': name, 'command': command, 'status': 'failed'}
        try:
            item['build_log'] = call(command, cwd=tree, combined=True)
            Path(str(target) + '.build.log').write_text(item['build_log'])
            paths = dep.read_text().replace('\\\n', '').split(':', 1)[1].split()
            item['dependencies'] = {str(Path(p).resolve()): sha(p) for p in paths}
            item['binary_sha256'] = sha(target)
            if name.startswith(('rf-optout/', 'audio-behavior/')):
                macros = call([cc, '-E', '-dM', *includes, '-I' + str(tree / 'test/native_apps'), *['-D' + d for d in defines], source])
                if name.startswith('rf-optout/'):
                    require('#define PORTABLE_RF_SHARED_TEXT_INPUT' not in macros, 'optout fixture forced on')
                    item['selector_verified_off'] = True
                else:
                    require(('#define PORTABLE_SPECTRUM_SHARED_TEXT_INPUT' in macros) == ('PORTABLE_SPECTRUM_SHARED_TEXT_INPUT' in defines), 'Audio selector differs from matrix')
            if execute:
                item['stdout'] = call([target], env={**os.environ, **record['sanitizer_environment']})
                Path(str(target) + '.log').write_text(item['stdout'])
            else:
                normalized = Path(str(target) + '.normalized')
                item['normalization_command'] = [str(objcopy), '--strip-unneeded', str(target), str(normalized)]
                call(item['normalization_command'])
                item['normalized_sha256'] = sha(normalized)
            item['status'] = 'passed'
        except Exception as exc:
            item['error'] = str(exc)
            Path(str(target) + '.failure.log').write_text(str(exc))
        return item
    def matrix(name, specs, compare=None):
        errors = []
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for future in as_completed([pool.submit(build, spec) for spec in specs]):
                item = future.result()
                for path, value in item.get('dependencies', {}).items():
                    if path in inputs and inputs[path] != value:
                        errors.append('input changed before consumption: ' + path)
                    inputs.setdefault(path, value)
                record['runs'].append(item)
                if item['status'] != 'passed': errors.append(item.get('error', item['name']))
                receipt()
        record['counts'][name] = len(specs)
        if not errors and compare:
            try: compare()
            except Exception as exc: errors.append(str(exc))
        record['stages'][name] = {'status': 'failed' if errors else 'passed', 'errors': errors}
        receipt()
        print(name, record['stages'][name]['status'], flush=True)
    # Isolate RF invariance from the independently approved neural guard. The
    # real predecessor remains untouched and is still compared against frozen
    # and candidate tokens below. Every synthetic baseline input is recorded.
    audio_predecessor = predecessor
    if facts.get('composition'):
        audio_predecessor = out / 'audio-predecessor-with-approved-neural-guard'
        for path in git(predecessor, 'ls-files').splitlines():
            source = predecessor / path
            if not source.is_file():
                continue
            target = audio_predecessor / path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        for path, identity in AUDIO_OVERLAY.items():
            approved = subprocess.check_output(['git','-C',str(ROOT),'show',identity['commit']+':'+path])
            require(digest(approved) == identity['sha256'] and sha(ROOT/path) == identity['sha256'],
                    'approved neural overlay changed: '+path)
            (audio_predecessor/path).write_bytes(approved)
        record['audio_comparison_baseline'] = {'base': PREDECESSOR, 'overlay': AUDIO_OVERLAY}
        for path in audio_predecessor.rglob('*'):
            if path.is_file(): inputs[str(path)] = sha(path)
    native = ROOT / 'test/native_apps'
    for stage, fixture in [('rf-labeled', 'rf_labeled_capture_test.c'), ('rf-optout', 'rf_candidate_optout_test.c')]:
        matrix(stage, [(f'{stage}/{c}-{s}', native / fixture, ROOT, ['PORTABLE_RADIO_CONTINUOUS_CAPTURE'] if c else [], san if s else [], True)
                      for c in (0, 1) for s in (0, 1)])
    safety = []
    for mode, flags in [('normal', []), ('asan', ['-fsanitize=address', '-fno-omit-frame-pointer', '-no-pie']), ('ubsan', ['-fsanitize=undefined', '-fno-sanitize-recover=all'])]:
        for profile, fixture, defines in [('watch', 'rf_shared_input_safety_test.c', []),
            ('continuous-resident-home', 'rf_shared_input_safety_test.c', ['PORTABLE_RADIO_CONTINUOUS_CAPTURE', 'PORTABLE_RESIDENT_SHELL_CLIENT', 'RF_TEST_HOME_APP="default.elf"']),
            ('app-data', 'rf_app_data_scope_test.c', [])]:
            safety.append((f'rf-safety/{profile}-{mode}', native / fixture, ROOT, defines, flags, True))
    matrix('rf-safety', safety)
    matrix('rf-models', [(f'rf-models/{fixture.stem}{s}', fixture, ROOT, [], san if s else [], True)
        for fixture in sorted(native.glob('rf_*_model_test.c')) for s in (0, 1)])
    audio = []
    for side, tree in [('predecessor', audio_predecessor), ('candidate', ROOT)]:
        overlay = out / 'audio-fixtures' / side / 'rf_candidate_audio_differential_test.c'
        overlay.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(native / overlay.name, overlay)
        inputs[str(overlay)] = sha(overlay)
        audio += [(f'audio-behavior/{shared}-{sanitized}-{side}', overlay, tree,
                   ['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT'] if shared else [], san if sanitized else [], True)
                  for shared in (False, True) for sanitized in (False, True)]
    def audio_behavior():
        runs = {r['name']: r for r in record['runs']}
        for shared in (False, True):
            for sanitized in (False, True):
                a, b = [runs[f'audio-behavior/{shared}-{sanitized}-{side}']['stdout'] for side in ('predecessor', 'candidate')]
                record['audio_pairs'].append({'name': f'behavior-{shared}-{sanitized}', 'before': digest(a.encode()), 'after': digest(b.encode())})
                require(a == b, 'Audio observable mismatch')
    matrix('audio-behavior', audio, audio_behavior)
    def audio_code():
        runs = {r['name']: r for r in record['runs']}
        for profile in PROFILES:
            for shared in (False, True):
                a, b = [runs[f'audio-code/{profile}-{shared}-{side}.o'] for side in ('predecessor', 'candidate')]
                x, y = [(out / r['name']).read_bytes() for r in (a, b)]
                changes = compare_sections(elf_sections(x), elf_sections(y))
                pair = {'name': f'code-data-{profile}-{shared}', 'before': a['normalized_sha256'], 'after': b['normalized_sha256'],
                    'raw_object_status': 'passed' if x == y else 'failed', 'raw_metadata_changes': changes,
                    'allocated_sections_identical': True, 'relocations_identical': True}
                record['audio_pairs'].append(pair)
                require(pair['before'] == pair['after'], 'Audio normalized object mismatch')
    matrix('audio-code-data', [(f'audio-code/{profile}-{shared}-{side}.o', tree / 'Apps/audio_spectrum.c', tree,
        defines + (['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT'] if shared else []), [], False)
        for profile, defines in PROFILES.items() for shared in (False, True)
        for side, tree in [('predecessor', audio_predecessor), ('candidate', ROOT)]], audio_code)
    try:
        spec = importlib.util.spec_from_file_location('frozen_runner', ROOT / 'scripts/test_rf_shared_input_safety.py')
        runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runner)
        for transition, before, after in [('frozen-to-predecessor', frozen, predecessor), ('predecessor-to-candidate', predecessor, ROOT)]:
            for source, shared in [('waterfall.c', False), ('audio_spectrum.c', False), ('audio_spectrum.c', True)]:
                for profile, defines in PROFILES.items():
                    ds = defines + (['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT'] if shared else [])
                    a, b = [runner.preprocess(t, system, source, ds) for t in (before, after)]
                    name = f'{transition}-{source}-{profile}-{shared}'
                    (out / (name + '.before.tokens')).write_bytes(a)
                    (out / (name + '.after.tokens')).write_bytes(b)
                    (out / (name + '.diff')).write_text(''.join(difflib.unified_diff(a.decode().splitlines(True), b.decode().splitlines(True))))
                    record['token_comparisons'].append({'name': name, 'status': 'passed' if a == b else 'failed', 'before': digest(a), 'after': digest(b)})
        checks = []
        for row in record['token_comparisons'][:9]:
            name = row['name']
            a = (out / (name + '.before.tokens')).read_bytes()
            b = (out / (name.replace('frozen-to-predecessor', 'predecessor-to-candidate') + '.after.tokens')).read_bytes()
            checks.append({'name': name.replace('frozen-to-predecessor', 'frozen-to-candidate'), 'status': 'passed' if a == b else 'failed'})
        storage_names = ['Apps/rf_app_data_scope.h', 'Apps/rf_temporal_app.inc', 'Apps/context_fingerprint_app.inc',
                         'test/native_apps/rf_startup_dirty_checkpoint_test.c', 'test/native_apps/rf_startup_contexts_test.c']
        storage = {p: 'passed' if sha(frozen / p) == sha(ROOT / p) else 'failed' for p in storage_names}
        record['frozen_contract'] = {'status': 'failed' if any(x['status'] == 'failed' for x in checks) or 'failed' in storage.values() else 'passed',
            'waived': False, 'token_checks': checks, 'unchanged_storage_checks': storage, 'target_elf_gate': 'not-run'}
    except Exception as exc:
        record['frozen_contract'] = {'status': 'not-run', 'error': str(exc)}
    receipt()
    try:
        log = call([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-v'], cwd=ROOT, timeout=900, combined=True)
        (out / 'manifest-tests.log').write_text(log)
        record['stages']['manifest-tests'] = {'status': 'passed'}
    except Exception as exc:
        record['stages']['manifest-tests'] = {'status': 'failed', 'error': str(exc)}
        (out / 'manifest-tests.log').write_text(str(exc))
    record['inputs'] = inputs
    record['source_hashes_unchanged'] = all(Path(p).is_file() and sha(p) == h for p, h in inputs.items())
    try:
        validate_result(record)
        record['scoped_pass'] = True
    except Exception as exc:
        record['failure'] = str(exc)
    receipt()
    print(json.dumps({'scoped_pass': record['scoped_pass'], 'frozen_contract': record['frozen_contract']['status'], 'distributable': False, 'receipt': str(out / 'qualification.json')}), flush=True)
    return 0 if record['scoped_pass'] else 1


if __name__ == '__main__':
    sys.exit(main())
