#!/usr/bin/env python3
"""Run source-owned host interaction fixtures, with bounded execution."""
import json
import os
import pathlib
import subprocess
from check_baseline import ROOT, check_sdk

check_sdk()
out = ROOT / 'build/host-tests'
out.mkdir(parents=True, exist_ok=True)
for name, app in json.loads((ROOT / 'tests/host-tests.json').read_text()).items():
    fixture = ROOT / f'test/native_apps/{name}_test.c'
    sources = [fixture] if app is None or '#include "../../Apps/' in fixture.read_text() else [ROOT / f'Apps/{app}.c', fixture]
    binary = out / name
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'lib/NativeApps/include'), '-I' + str(ROOT / 'sdk/driver'),
                    *map(str, sources), '-o', str(binary)], check=True, timeout=60)
    subprocess.run([str(binary)], cwd=ROOT, check=True, timeout=30)
    print(name + ': host fixture passed', flush=True)
for fixture in sorted((ROOT / 'tests').glob('*_failures.c')):
    binary = out / fixture.stem
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(ROOT / 'lib/NativeApps/include'), str(fixture), '-o', str(binary)], check=True, timeout=60)
    subprocess.run([str(binary)], check=True, timeout=30)
    print(fixture.stem + ': failure fixture passed', flush=True)
