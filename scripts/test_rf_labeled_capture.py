#!/usr/bin/env python3
"""Bounded RF capture/identity regressions with synthetic host IQ, never hardware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--system-apps', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
system, out = args.system_apps.resolve(), args.output.resolve()
out.mkdir(parents=True, exist_ok=True)

def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

# Snapshot all project/include inputs before any compiler runs. Compiler -MMD
# additionally reports each actual transitive project dependency per executable.
inputs = [*ROOT.joinpath('Apps').rglob('*'), *ROOT.joinpath('lib').rglob('*'),
          *system.joinpath('lib/PortableApps/include').rglob('*'),
          *system.joinpath('lib/NativeApps/include').rglob('*'),
          *ROOT.joinpath('test/native_apps').glob('rf_*'), Path(__file__)]
source_hashes = {str(p): sha(p) for p in sorted(set(inputs)) if p.is_file()}
record = {'schema': 2, 'hardware_verified': False, 'full_product_composition': False,
          'utilities_revision': git(ROOT, 'rev-parse', 'HEAD'),
          'system_revision': git(system, 'rev-parse', 'HEAD'),
          'utilities_status': git(ROOT, 'status', '--porcelain'),
          'system_status': git(system, 'status', '--porcelain'),
          'compiler': subprocess.check_output([os.environ.get('CC', 'cc'), '--version'], text=True),
          'sanitizer_environment': {'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'},
          'runs': [], 'sources': source_hashes}

def receipt():
    (out / 'qualification.json').write_text(json.dumps(record, indent=2) + '\n')

for continuous in (False, True):
    for sanitized in (False, True):
        name = ('continuous-' if continuous else 'ordinary-') + ('sanitized' if sanitized else 'normal')
        exe, deps = out / name, out / (name + '.d')
        flags = ['-DPORTABLE_RADIO_CONTINUOUS_CAPTURE'] if continuous else []
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-MMD', '-MF', str(deps), *flags,
                   '-I' + str(system / 'lib/PortableApps/include'), '-I' + str(system / 'lib/NativeApps/include'),
                   '-I' + str(ROOT / 'Apps'), str(ROOT / 'test/native_apps/rf_labeled_capture_test.c'), '-lm', '-o', str(exe)]
        built = subprocess.run(command, capture_output=True, text=True, timeout=120)
        (out / (name + '-build.log')).write_text(built.stdout + built.stderr)
        result = {'name': name, 'command': command, 'build_returncode': built.returncode}
        record['runs'].append(result)
        receipt()
        built.check_returncode()
        paths = deps.read_text().replace('\\\n', '').split(':', 1)[1].split()
        result['dependencies'] = {str(Path(p).resolve()): sha(Path(p)) for p in paths}
        assert all(source_hashes.get(p) == digest for p, digest in result['dependencies'].items()), 'Unrecorded or changed project input'
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120,
                             env={**os.environ, **record['sanitizer_environment']})
        (out / (name + '.log')).write_text(run.stdout + run.stderr)
        result.update(returncode=run.returncode, binary_sha256=sha(exe), log=str(out / (name + '.log')))
        receipt()
        print(name, run.returncode, run.stdout, run.stderr, flush=True)
        run.check_returncode()
record['source_hashes_unchanged'] = all(sha(Path(p)) == digest for p, digest in source_hashes.items())
receipt()
assert record['source_hashes_unchanged'], 'Sources changed during qualification'
print('Four host executions passed; LeakSanitizer disabled. Native UI, Contexts service, target build, and device execution are not qualified.')
