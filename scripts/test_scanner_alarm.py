#!/usr/bin/env python3
"""Published alarm-service lifecycle helper with strict endpoint doubles."""
import argparse, hashlib, json, os, shlex, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--system', type=Path, required=True)
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
from scanner_support import sdk
include=sdk(a.runtime,a.system,out)
source = ROOT/'test/scanner_alarm/test.c'
flags = ['-std=c11', '-O0', '-g0', '-pipe', '-Wall', '-Wextra', '-Werror',
         '-I'+str(ROOT/'Apps'),
         '-I'+str(include)]
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
paths.extend([Path(__file__).resolve(),ROOT/'scripts/scanner_support.py'])
inputs = {str(v):hashlib.sha256(v.read_bytes()).hexdigest() for v in paths}
run([cc, *flags, str(source), '-o', str(out/'test')], 'build')
cases=['legacy','refresh','refresh-reentry','absent','dirty','reserve','defer','blocked-stop','cue','generation','uncertain','unknown','bad-status','reentry','identity','ack','close-pending','close-output']
cases += ["close-ready","close-loading","close-shared-ready","close-stale-owned","close-generation","close-cue","close-blocked","close-continuation","close-busy","close-bad-status","close-uncertain","close-unknown-status","close-reentry"]
for case in cases: run([str(out/'test'),case],case)
count=len(cases);result=f'{count} alarm cases PASS\n'
assert inputs == {str(v):hashlib.sha256(v.read_bytes()).hexdigest() for v in paths}
(out/'receipt.json').write_text(json.dumps(dict(passed=True, cases=count,
    sanitized=a.sanitize, inputs=inputs, commands=commands,
    binary_sha256=hashlib.sha256((out/'test').read_bytes()).hexdigest(),
    scope='Actual Scanner alarm helper with strict provider endpoint doubles. '
          'No provider rendering, Runtime, target, physical UI or radio claim.'), indent=2)+'\n')
print(result, end='')
