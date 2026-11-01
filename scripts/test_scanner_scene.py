#!/usr/bin/env python3
"""Actual production Scanner root endpoint suite with public sensor, scene and text types."""
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
source = ROOT/'test/scanner_scene/root_test.c'
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
cases=['attention-quiet','attention-alarm','attention-text','attention-short','attention-long','attention-unknown','attention-dirty-busy','alarm-editor-due','alarm-editor-presenting','alarm-reserve','alarm-close-pending','alarm-text','alarm-exact-ack','alarm-return','alarm-uncertain','no-ui','close-cut','close-continuation','text-again','text-accept','text-cancel','text-home','text-malformed','text-unknown-open','text-clean-absent','handoff-pref','stale-event','sequence-regression','identity-close','selection-changed','selection-moved','name-changed','alias-identity','save-retires','cancel-pending','handoff-pending','detail-moved','detail-gone','text-clean-again','text-dirty-again','loop-close','loop-pref']
for case in cases: run([str(out/'test'),case],case)
count=len(cases);result=f'{count} root cases PASS\n'
assert inputs == {str(v):hashlib.sha256(v.read_bytes()).hexdigest() for v in paths}
(out/'receipt.json').write_text(json.dumps(dict(passed=True, cases=count,
    sanitized=a.sanitize, inputs=inputs, commands=commands,
    binary_sha256=hashlib.sha256((out/'test').read_bytes()).hexdigest(),
    scope='Actual production root with strict provider endpoint doubles. '
          'No provider rendering, Runtime, target, physical UI or radio claim.'), indent=2)+'\n')
print(result, end='')
