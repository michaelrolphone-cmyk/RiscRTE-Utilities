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
    # Keep literal contents intact; compare C preprocessing tokens independent
    # of formatting-only line breaks introduced around conditional branches.
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\s]', data)
    return '\n'.join(tokens).encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps', required=True, type=Path)
    parser.add_argument('--baseline-utilities', type=Path)
    args = parser.parse_args()
    system = args.system_apps.resolve()
    output = ROOT / 'build/spectrum-shared-input-safety'
    output.mkdir(parents=True, exist_ok=True)
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
    shared = preprocess(ROOT, system, 'audio_spectrum.c', ['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT'])
    assert b'draw_keyboard' not in shared and b'keyboard_activate' not in shared
    assert b'portable_text_client_begin' in shared
    report['shared_local_keyboard_absent'] = True
    if args.baseline_utilities:
        baseline = args.baseline_utilities.resolve()
        for source in ('audio_spectrum.c', 'waterfall.c'):
            for profile in ([], ['PORTABLE_NOVA_UI', 'PORTABLE_ALARM_CLIENT']):
                before = preprocess(baseline, system, source, profile)
                after = preprocess(ROOT, system, source, profile)
                assert before == after, f'{source} opt-out preprocessor token mismatch: {profile}'
                report['parity'].append({'source': source, 'defines': profile,
                                        'token_identical': True, 'sha256': digest(after)})
                print(f'{source} opt-out preprocessing token-identical: {profile or ["plain"]}')
        for source,profile in [('audio_spectrum.c',['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT']),('waterfall.c',['PORTABLE_RF_SHARED_TEXT_INPUT','PORTABLE_TEXT_INPUT_POLICY'])]:
            before=preprocess(baseline,system,source,profile)
            after=preprocess(ROOT,system,source,profile)
            assert before==after, (source,'unselected policy tokens changed')
            report['parity'].append({'source':source,'defines':profile,'token_identical':True,'sha256':digest(after)})
    for source in ('Apps/audio_spectrum.c', 'Apps/context_signal_library.inc'):
        report.setdefault('production_sources', {})[source] = digest((ROOT / source).read_bytes())
    (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Focused Spectrum shared-input safety passed; evidence:', output / 'evidence.json')


if __name__ == '__main__':
    main()
