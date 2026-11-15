#!/usr/bin/env python3
"""Focused copied Scanner content/intent tests using exact public scene APIs."""
import argparse, hashlib, json, os, shlex, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--system', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
source = ROOT/'test/scanner_presenter/presenter_test.c'
flags = ['-std=c11', '-O0', '-g0', '-pipe', '-Wall', '-Wextra', '-Werror',
         '-I'+str(ROOT/'Apps'), '-I'+str(ROOT/'lib/Bluetooth/include'),
         '-I'+str(a.system.resolve()/'sdk/app')]
if a.sanitize:
    flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
              '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
cc = os.environ.get('CC', 'cc')
commands = []
def run(command, name):
    commands.append(command)
    (out/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
    result = subprocess.run(command, text=True, capture_output=True,
                            env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0'})
    (out/(name+'.log')).write_text(result.stdout+result.stderr)
    result.check_returncode()
    return result.stdout
raw = run([cc, *flags, '-MM', str(source)], 'dependencies')
paths = [Path(v).resolve() for v in shlex.split(raw.replace('\\\n',' ').split(':',1)[1])]
paths.append(Path(__file__).resolve())
inputs = {str(v):hashlib.sha256(v.read_bytes()).hexdigest() for v in paths}
run([cc, *flags, str(source), '-o', str(out/'test')], 'build')
result = run([str(out/'test')], 'run')
assert result == '33 presenter cases PASS\n', result
assert inputs == {str(v):hashlib.sha256(v.read_bytes()).hexdigest() for v in paths}
(out/'receipt.json').write_text(json.dumps(dict(passed=True, cases=33,
    sanitized=a.sanitize, inputs=inputs, commands=commands,
    binary_sha256=hashlib.sha256((out/'test').read_bytes()).hexdigest(),
    scope='Pure copied component documents and committed-intent validation. '
          'No provider rendering, Runtime, target, physical UI or radio claim.'), indent=2)+'\n')
print(result, end='')
