#!/usr/bin/env python3
"""Focused production Spectrum/controller-client safety and opt-out parity.

Host rendering, physical audio, and hardware input delivery are not simulated
by this suite. Restricted executors may require ASAN_OPTIONS=detect_leaks=0
because LSan's thread tracer is unavailable; the runner preserves caller settings.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from shared_input_parity_reference import Reference, compare_tokens, tokenize, source_snapshot, verify_stable

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    try:
        return subprocess.run(command, check=True, timeout=180, **kwargs)
    except subprocess.CalledProcessError as error:
        if error.stdout:
            print(error.stdout, end="")
        if error.stderr:
            print(error.stderr, end="")
        raise


def digest(data):
    return hashlib.sha256(data).hexdigest()


def preprocess(tree, system, source, defines):
    command = [os.environ.get('CC', 'cc'), '-E', '-P', '-std=c11']
    command += ['-D' + define for define in defines]
    command += ['-I' + str(system / 'lib/PortableApps/include'),
                '-I' + str(system / 'lib/NativeApps/include'), 'Apps/' + source]
    data = run(command, cwd=tree, capture_output=True, text=True).stdout
    return tokenize(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps', required=True, type=Path)
    parser.add_argument('--baseline-utilities', type=Path)
    args = parser.parse_args()
    system = args.system_apps.resolve()
    output = ROOT / 'build/spectrum-shared-input-safety'
    output.mkdir(parents=True, exist_ok=True)
    stability = source_snapshot(ROOT)
    fixture = ROOT / 'test/native_apps/spectrum_shared_input_safety_test.c'
    report = {'fixture_sha256': digest(fixture.read_bytes()), 'tests': [], 'parity': []}
    for sanitizers in (False, True):
        target = output / ('spectrum-shared-input-safety' + ('-san' if sanitizers else ''))
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer'] if sanitizers else []
        run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
             *flags, '-I' + str(system / 'lib/PortableApps/include'),
             '-I' + str(system / 'lib/NativeApps/include'), str(fixture), '-lm', '-o', str(target)])
        env = os.environ.copy()
        result = run([str(target)], env=env, capture_output=True, text=True)
        print(result.stdout, end='')
        if result.stderr:
            print(result.stderr, end='')
        report['tests'].append({'mode': ('ASan+UBSan (LSan disabled by caller)' if 'detect_leaks=0' in env.get('ASAN_OPTIONS', '') else 'ASan+UBSan') if sanitizers else 'normal',
                                'passed': True, 'stdout': result.stdout})
    # Real default controller, independent ASCII/hit literals, app_main navigation,
    # capacity, cancellation and no persistence before the dialog's explicit Save.
    local_fixture = ROOT / 'test/native_apps/spectrum_keyboard_test.c'
    for mode, flags in [('normal', []), ('ASan', ['-fsanitize=address', '-fno-omit-frame-pointer', '-no-pie']),
                        ('UBSan', ['-fsanitize=undefined', '-fno-sanitize-recover=all'])]:
        target = output / ('spectrum-local-keyboard-' + mode)
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   *flags, '-I' + str(system / 'lib/PortableApps/include'),
                   '-I' + str(system / 'lib/NativeApps/include'), str(local_fixture), '-lm', '-o', str(target)]
        run(command)
        result = run([str(target)], env=os.environ.copy(), capture_output=True, text=True)
        (output / (target.name + '.log')).write_text(result.stdout + result.stderr)
        report['tests'].append({'mode': mode, 'profile': 'local-keyboard-opt-out', 'passed': True,
                                'command': command, 'stdout': result.stdout})
    local = preprocess(ROOT, system, 'audio_spectrum.c', [])
    assert b'draw_keyboard' in local and b'keyboard_activate' in local
    assert b'portable_text_client_begin' not in local
    shared = preprocess(ROOT, system, 'audio_spectrum.c', ['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT'])
    assert b'draw_keyboard' not in shared and b'keyboard_activate' not in shared
    assert b'portable_text_client_begin' in shared
    report['shared_local_keyboard_absent'] = True
    if args.baseline_utilities:
        reference = Reference(args.baseline_utilities.resolve(), 'spectrum')
        baseline = reference.path
        report['reviewed_evolution'] = reference.evidence
        (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
        for source in ('audio_spectrum.c', 'waterfall.c'):
            for profile in ([], ['PORTABLE_NOVA_UI', 'PORTABLE_ALARM_CLIENT']):
                before = preprocess(baseline, system, source, profile)
                after = preprocess(ROOT, system, source, profile)
                compare_tokens(before, after, output, f'{source} opt-out token mismatch: {profile}')
                report['parity'].append({'source': source, 'defines': profile,
                                        'token_identical': True, 'sha256': digest(after)})
                print(f'{source} opt-out preprocessing token-identical: {profile or ["plain"]}')
        for source,profile in [('audio_spectrum.c',['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT']),('waterfall.c',['PORTABLE_RF_SHARED_TEXT_INPUT','PORTABLE_TEXT_INPUT_POLICY'])]:
            before=preprocess(baseline,system,source,profile)
            after=preprocess(ROOT,system,source,profile)
            compare_tokens(before, after, output, source + ' unselected policy tokens changed')
            report['parity'].append({'source':source,'defines':profile,'token_identical':True,'sha256':digest(after)})
    for source in ('Apps/audio_spectrum.c', 'Apps/context_signal_library.inc'):
        report.setdefault('production_sources', {})[source] = digest((ROOT / source).read_bytes())
    (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    verify_stable(ROOT, stability)
    print('Focused Spectrum shared-input safety passed; evidence:', output / 'evidence.json')


if __name__ == '__main__':
    main()
