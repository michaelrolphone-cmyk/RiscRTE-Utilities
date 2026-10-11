#!/usr/bin/env python3
"""Exercise production Waterfall capture with exact selected build flags/headers.

Provider outcomes, policy and clock are simulated; this never accesses hardware.
The target receipt identifies the installed/source-matched profile, not a device
execution or qualification result. Normal and ASan/UBSan runs are both required.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import signal

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-receipt', type=Path, required=True)
    parser.add_argument('--system-apps', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline-source', type=Path, help='Optional exact receipt source: prove its permanent-stop assertion before testing candidate')
    args = parser.parse_args()
    receipt = args.build_receipt.resolve()
    target = json.loads(receipt.read_text())
    system = args.system_apps.resolve()
    headers = receipt.parent.parent / 'sdk/include'
    assert target['app'] == 'waterfall'
    assert '-DPORTABLE_RADIO_CONTINUOUS_CAPTURE' in target['build_defines']
    assert '-DPORTABLE_RESIDENT_POLICY' in target['build_defines']
    revision = subprocess.check_output(['git', '-C', str(system), 'rev-parse', 'HEAD'], text=True).strip()
    assert revision == target['system_source_revision']
    for name, digest in target['sdk_sha256'].items():
        assert sha(headers / name) == digest, name
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    record = {
        'schema': 1,
        'build_receipt': str(receipt),
        'build_receipt_sha256': sha(receipt),
        'profile_version': target['version'],
        'profile_elf_sha256': target['elf_sha256'],
        'source_revision': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
        'source_dirty': bool(subprocess.check_output(['git', '-C', str(ROOT), 'status', '--porcelain'], text=True).strip()),
        'system_revision': revision,
        'production_source_sha256': {name: sha(ROOT / name) for name in ['Apps/waterfall.c', 'Apps/rf_application.inc', 'test/native_apps/rf_capture_recovery_test.c']},
        'runs': [],
        'hardware_verified': False,
        'originating_hardware_fault_reproduced': False,
    }
    if args.baseline_source:
        baseline = args.baseline_source.resolve()
        baseline_revision = subprocess.check_output(['git', '-C', str(baseline), 'rev-parse', 'HEAD'], text=True).strip()
        assert baseline_revision == target['source_revision']
        assert sha(baseline / 'Apps/rf_application.inc') == target['compiled_dependencies_sha256']['Source/Apps/rf_application.inc']
        probe = out / 'baseline-probe.c'
        probe.write_text((ROOT / 'test/native_apps/rf_capture_stop_probe.c').read_text().replace('../../Apps/waterfall.c', str(baseline / 'Apps/waterfall.c')))
        exe = out / 'baseline-probe'
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-fdata-sections', *target['build_defines'],
                        *['-I' + str(p) for p in [headers, system / 'lib/NativeApps/include', baseline / 'Apps', system / 'Apps']],
                        str(probe), '-Wl,--gc-sections', '-lm', '-o', str(exe)], check=True, timeout=120)
        result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
        (out / 'baseline-probe.log').write_text(result.stdout + result.stderr)
        assert result.returncode == -signal.SIGABRT
        assert 'RF_PROBE_AFTER_FAILURE calls=4 history=3 running=0 requested=0 active=0' in result.stderr
        record['baseline_reproduced'] = {'revision': baseline_revision, 'expected_assertion_returncode': result.returncode, 'log': result.stdout + result.stderr}
        print('Baseline permanent-stop assertion reproduced after three valid bursts', flush=True)
    for source in ['rf_capture_stop_probe.c', 'rf_capture_recovery_test.c', 'x4_idle_waterfall_test.c']:
        for sanitized in [False, True]:
            exe = out / (Path(source).stem + ('-san' if sanitized else ''))
            flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
            command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-fdata-sections', *flags, *target['build_defines'],
                       *['-I' + str(p) for p in [headers, system / 'lib/NativeApps/include', ROOT / 'Apps', system / 'Apps']],
                       str(ROOT / 'test/native_apps' / source), '-Wl,--gc-sections', '-lm', '-o', str(exe)]
            subprocess.run(command, check=True, timeout=120)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120,
                                    env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
            (out / (exe.name + '.log')).write_text(result.stdout + result.stderr)
            print(result.stdout + result.stderr, end='', flush=True)
            result.check_returncode()
            record['runs'].append({'test': source, 'sanitized': sanitized, 'command': command, 'result': result.stdout.strip()})
    (out / 'evidence.json').write_text(json.dumps(record, indent=2) + '\n')


if __name__ == '__main__':
    main()
