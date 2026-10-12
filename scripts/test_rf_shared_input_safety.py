#!/usr/bin/env python3
"""Focused production RF/shared-client safety, Home routing and opt-out parity.

Host rendering, physical RF, and hardware input delivery are not simulated.
The suite runs separate normal, AddressSanitizer and UndefinedBehaviorSanitizer
executables. Restricted executors may need ASAN_OPTIONS=detect_leaks=0; caller
sanitizer settings are preserved and recorded rather than silently weakened.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    result = subprocess.run(command, timeout=180, capture_output=True, text=True, **kwargs)
    if result.returncode:
        print(result.stdout, end='')
        print(result.stderr, end='')
        result.check_returncode()
    return result


def digest(data):
    return hashlib.sha256(data).hexdigest()


def revision(tree):
    return run(['git', '-C', str(tree), 'rev-parse', 'HEAD']).stdout.strip()


def preprocess(tree, system, source, defines):
    command = [os.environ.get('CC', 'cc'), '-E', '-P', '-std=c11']
    command += ['-D' + define for define in defines]
    command += ['-I' + str(system / 'lib/PortableApps/include'),
                '-I' + str(system / 'lib/NativeApps/include'), 'Apps/' + source]
    data = run(command, cwd=tree).stdout
    # Preserve all literal contents. Formatting around #if/#else branches is
    # irrelevant; string/character literal tokens and numeric tokens are not.
    # Maximal-munch preprocessing numbers and punctuators matter: for example,
    # 1.0 differs from 1 . 0 and ++ differs from + + even after whitespace removal.
    tokens = re.findall(
        r'(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|(?:u|U|L)?\'(?:\\.|[^\'\\])*\'|'
        r'(?:[0-9]|\.[0-9])(?:[eEpP][+-]|[A-Za-z_0-9.])*|[A-Za-z_$][A-Za-z_0-9$]*|'
        r'%:%:|>>=|<<=|\.\.\.|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||'
        r'\*=|/=|%=|\+=|-=|&=|\^=|\|=|##|<:|:>|<%|%>|%:|[^\s]', data)
    return '\n'.join(tokens).encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps', required=True, type=Path)
    parser.add_argument('--baseline-utilities', type=Path,
                        help='Published Spectrum migration; RF opt-out and Audio both modes must match')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/rf-shared-input-safety')
    args = parser.parse_args()
    system = args.system_apps.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    fixture = ROOT / 'test/native_apps/rf_shared_input_safety_test.c'
    source_names = ('Apps/rf_application.inc', 'Apps/context_signal_library.inc',
                    'Apps/audio_spectrum.c', 'Apps/waterfall.c')
    source_hashes = {name: digest((ROOT / name).read_bytes()) for name in source_names}
    report = {'schema': 1, 'hardware_verified': False,
              'fixture_sha256': digest(fixture.read_bytes()),
              'system_revision': revision(system), 'utilities_revision': revision(ROOT),
              'sanitizer_environment': {key: os.environ[key] for key in ('ASAN_OPTIONS', 'UBSAN_OPTIONS') if key in os.environ},
              'tests': [], 'parity': []}
    modes = [('normal', []),
             ('ASan', ['-fsanitize=address', '-fno-omit-frame-pointer', '-no-pie']),
             ('UBSan', ['-fsanitize=undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'])]
    for mode, flags in modes:
        cases = [('watch', fixture, []),
                 ('continuous-resident-home', fixture, ['-DPORTABLE_RADIO_CONTINUOUS_CAPTURE', '-DPORTABLE_RESIDENT_SHELL_CLIENT', '-DRF_TEST_HOME_APP="default.elf"']),
                 ('app-data-scope', ROOT / 'test/native_apps/rf_app_data_scope_test.c', [])]
        for profile, source, defines in cases:
            target = output / ('rf-' + profile + '-' + mode.lower())
            command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                       *flags, *defines, '-I' + str(system / 'lib/PortableApps/include'),
                       '-I' + str(system / 'lib/NativeApps/include'), '-I' + str(ROOT / 'Apps'),
                       str(source), '-lm', '-o', str(target)]
            run(command)
            result = run([str(target)], env=os.environ.copy())
            print(f'{mode} / {profile}: {result.stdout}', end='')
            if result.stderr:
                print(result.stderr, end='')
            (output / (target.name + '.log')).write_text(result.stdout + result.stderr)
            report['tests'].append({'mode': mode, 'profile': profile, 'passed': True,
                                    'command': command, 'stdout': result.stdout})
    shared = preprocess(ROOT, system, 'waterfall.c', ['PORTABLE_RF_SHARED_TEXT_INPUT'])
    assert b'draw_keyboard' not in shared and b'keyboard_activate' not in shared and b'keyboard_before' not in shared
    assert b'portable_text_client_begin' in shared
    report['shared_local_keyboard_absent'] = True
    if args.baseline_utilities:
        baseline = args.baseline_utilities.resolve()
        report['baseline_revision'] = revision(baseline)
        profiles = [[], ['PORTABLE_NOVA_UI', 'PORTABLE_ALARM_CLIENT'],
                    ['PORTABLE_RADIO_CONTINUOUS_CAPTURE', 'PORTABLE_RESIDENT_POLICY',
                     'PORTABLE_CONTEXTS_CLIENT', 'PORTABLE_BLE_BROADCAST',
                     'PORTABLE_HOME_APP="default.elf"', 'PORTABLE_RETURN_APP="springboard.elf"']]
        for source in ('waterfall.c', 'audio_spectrum.c'):
            for profile in profiles:
                before = preprocess(baseline, system, source, profile)
                after = preprocess(ROOT, system, source, profile)
                assert before == after, f'{source} opt-out preprocessor token mismatch: {profile}'
                report['parity'].append({'source': source, 'defines': profile,
                                        'token_identical': True, 'sha256': digest(after)})
                print(f'{source} opt-out preprocessing token-identical: {profile or ["plain"]}')
        for profile in profiles:
            defines = profile + ['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT']
            before = preprocess(baseline, system, 'audio_spectrum.c', defines)
            after = preprocess(ROOT, system, 'audio_spectrum.c', defines)
            assert before == after, f'Audio shared opt-in token mismatch: {defines}'
            report['parity'].append({'source': 'audio_spectrum.c', 'defines': defines,
                                    'token_identical': True, 'sha256': digest(after)})
            print(f'Audio shared opt-in preprocessing token-identical: {profile or ["plain"]}')
        # These are the c12781b2 operation-scope/checkpoint/retained-load paths.
        for name in ('Apps/rf_app_data_scope.h', 'Apps/rf_temporal_app.inc',
                     'Apps/context_fingerprint_app.inc', 'test/native_apps/rf_startup_dirty_checkpoint_test.c',
                     'test/native_apps/rf_startup_contexts_test.c'):
            assert (ROOT / name).read_bytes() == (baseline / name).read_bytes(), name + ' changed'
            report.setdefault('unchanged_storage_sources', {})[name] = digest((ROOT / name).read_bytes())
    assert source_hashes == {name: digest((ROOT / name).read_bytes()) for name in source_names}, 'Production source changed during the test run; rerun for consistent evidence'
    assert report['fixture_sha256'] == digest(fixture.read_bytes()), 'Fixture changed during the test run'
    report['production_sources'] = source_hashes
    (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Focused RF shared-input safety passed; evidence:', output / 'evidence.json')


if __name__ == '__main__':
    main()
