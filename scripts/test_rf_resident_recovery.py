#!/usr/bin/env python3
"""Real RF app + shared adapter with simulated resident Runtime/peripheral I/O."""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-receipt', type=Path, required=True)
    parser.add_argument('--system-apps', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    receipt = json.loads(args.build_receipt.read_text())
    command = receipt['compile_command']
    original_app = next(Path(x) for x in command if x.endswith('/Apps/waterfall.c'))
    original_root = str(original_app.parent.parent)
    system = str(args.system_apps.resolve())
    assert subprocess.check_output(['git', '-C', system, 'rev-parse', 'HEAD'], text=True).strip() == receipt['system_source_revision']
    flags = [x.replace(original_root, str(ROOT)) for x in command if x.startswith(('-D', '-I'))]
    assert '-DPORTABLE_RESIDENT_POLICY' in flags and '-DPORTABLE_RADIO_CONTINUOUS_CAPTURE' in flags
    original_system = str(next(Path(x).parents[3] for x in command if x.endswith('/lib/PortableApps/src/adapter.c')))
    flags = [x.replace(original_system, system) for x in flags]
    sources = [x.replace(original_system, system) for x in command if x.endswith('.c') and x != str(original_app)]
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    stubs = out / 'serial-stubs.c'
    stubs.write_text('void rf_serial_start(void){}\nvoid rf_serial_line(const char*s){(void)s;}\nvoid rf_serial_finish(int a,int b){(void)a;(void)b;}\n')
    prefix = '''wait 4
check running eq 1
remember suspends
remember resident_polls
remember resident_policies
set jump 61000
wait 3
check running eq 1
same suspends
same resident_polls
same resident_policies
set poll_ms 1
set rf_failure 4
set jump 1000
wait 1
check retry eq 1
check capture_active eq 1
remember captures
wait 50
same captures
same resident_polls
same resident_policies
check retry eq 1
'''
    recovery = prefix + '''wait 250
check retry eq 0
check capture_error eq 0
check running eq 1
check capture_active eq 1
same suspends
same resident_polls
same resident_policies
nav 1
'''
    cancel = '''wait 5
check running eq 0
check capture_requested eq 0
check capture_active eq 0
check retry eq 0
remember captures
set jump 61000
wait 5
same captures
check resident_policies ge 1
nav 2
wait 5
check running eq 1
check capture_requested eq 1
check retry eq 0
nav 1
'''
    scenes = {
        'recover-reentry': (recovery, {'RF_RENDER_REENTRY': '1', 'RF_RENDER_EXPECT_LAUNCH': '2'}),
        'stop-retry': (prefix + 'nav 2\n' + cancel, {}),
        'freeze-retry': (prefix + 'nav 32\n' + cancel, {}),
        'back-retry': (prefix + 'nav 1\n', {}),
    }
    record = {'schema': 1, 'build_receipt': str(args.build_receipt.resolve()), 'hardware_verified': False,
              'runtime_dispatch': 'simulated; real app and shared adapter', 'runs': []}
    for sanitized in [False, True]:
        mode = 'sanitized' if sanitized else 'normal'
        exe = out / ('rf-resident-' + mode)
        san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        compile_command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *san, *flags,
                           '-DRF_RENDER_PAPER', '-DRF_RENDER_RESIDENT', str(ROOT / 'test/native_apps/rf_renderer_test.c'), str(stubs), *sources, '-lm', '-o', str(exe)]
        subprocess.run(compile_command, check=True, timeout=120)
        for name, (script, extra) in scenes.items():
            dest = out / (mode + '-' + name)
            dest.mkdir(exist_ok=True)
            actions = dest / 'actions.txt'
            actions.write_text(script)
            result = subprocess.run([str(exe), str(dest), str(actions)], capture_output=True, text=True, timeout=60,
                                    env={**os.environ, 'RF_RENDER_NO_IMAGES': '1', 'RF_RENDER_EXPECT_LAUNCH': '1',
                                         'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1', **extra})
            (dest / 'run.log').write_text(result.stdout + result.stderr)
            if result.returncode:
                print(result.stdout + result.stderr)
            result.check_returncode()
            print(mode, name, result.stdout.strip(), flush=True)
            record['runs'].append({'sanitized': sanitized, 'scene': name, 'result': result.stdout.strip(), 'compile_command': compile_command})
    (out / 'evidence.json').write_text(json.dumps(record, indent=2) + '\n')


if __name__ == '__main__':
    main()
