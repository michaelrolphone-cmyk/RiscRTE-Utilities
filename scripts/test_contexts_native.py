#!/usr/bin/env python3
"""Qualify actual Contexts scene/text hosts at capability-selected color sizes."""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path
from test_contexts_shared_input import build, LEGACY, EDIT, FAULT, CHECKPOINT

SIZES = ('240x240', '410x502', '160x160', '320x180', '502x410')


def run(args):
    args.output.mkdir(parents=True, exist_ok=True)
    evidence = {'physical_testing': 'not performed', 'sanitized': args.sanitize,
                'scenarios': [], 'frame_storage_sha256': {}}
    with tempfile.TemporaryDirectory(prefix='contexts-native-') as temp:
        for shared in (False, True):
            work = Path(temp) / str(int(shared))
            work.mkdir()
            exe = build(args.runtime, args.system_apps, work, 'watch', shared,
                        args.sanitize, native=True)
            cases = list(LEGACY)
            if shared:
                cases += [f'text-{reason}-{mode}' for reason in range(5) for mode in EDIT]
                cases += [f'text-2-{mode}' for mode in FAULT + CHECKPOINT]
                cases += [f'text-{reason}-{mode}' for reason in (0, 1, 3, 4)
                          for mode in ('checkpoint-idle', 'home-checkpoint-resume')]
                cases += ['text-2-retry', 'text-2-reopen', 'text-2-prior-edit-cancel',
                          'text-0-learn-save']
                cases += [f'text-{reason}-explicit-save' for reason in (2, 3, 4)]
            for dimensions in SIZES:
                for mode in cases:
                    key = f'{dimensions}-{int(shared)}-{mode}'
                    out = args.output / key
                    out.mkdir(parents=True, exist_ok=True)
                    result = subprocess.run(
                        [str(exe), 'watch', str(out), mode], capture_output=True,
                        text=True, timeout=60, env={**os.environ,
                            'CONTEXTS_NATIVE_SIZE': dimensions,
                            'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1',
                            'UBSAN_OPTIONS': 'halt_on_error=1'})
                    (out / 'run.log').write_text(result.stdout + result.stderr)
                    print(dimensions, shared, mode, result.returncode, flush=True)
                    if result.returncode:
                        print(result.stdout + result.stderr, flush=True)
                    result.check_returncode()
                    evidence['scenarios'].append(key)
                    evidence['frame_storage_sha256'][key] = {
                        file.name: hashlib.sha256(file.read_bytes()).hexdigest()
                        for file in out.iterdir() if file.suffix in ('.ppm', '.ctx')}
    # Existing app/controller output remains equal with shared input selected.
    for dimensions in SIZES:
        for mode in LEGACY:
            legacy = evidence['frame_storage_sha256'][f'{dimensions}-0-{mode}']
            shared = evidence['frame_storage_sha256'][f'{dimensions}-1-{mode}']
            for name, digest in legacy.items():
                if not name.endswith('-keyboard.ppm'):
                    assert shared.get(name) == digest, (dimensions, mode, name)
    (args.output / 'results.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(f'{len(evidence["scenarios"])} native Contexts scenarios PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'system-apps', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    run(parser.parse_args())
