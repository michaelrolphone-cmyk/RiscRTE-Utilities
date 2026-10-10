#!/usr/bin/env python3
"""Test the real intent-driven Alarms client and domain service with fault fixtures."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from scene_support import ROOT, sdk, time_sources

CONTROL_CASES = ('arm-cancel', 'tomorrow', 'gap', 'fold', 'stale', 'corrupt',
                 'unknown-commit', 'unwritten-retry', 'io-confirmed', 'expired',
                 'backward', 'bad-command', 'volume', 'dismiss', 'blocked',
                 'get-context', 'put-context', 'service-retained', 'copied-retained',
                 'clock-context', 'bad-zone')
APP_CASES = ('edit-restore', 'save-commit', 'save-unknown', 'checkpoint-unknown',
             'checkpoint-failure', 'stale-event', 'corrupt-checkpoint', 'headless',
             'read-retained', 'write-retained', 'apply-retained', 'snapshot-retained',
             'close-retained','acquire-context','step-context','yield-context')


def run(runtime: Path, system: Path, output: Path, sanitize: bool) -> int:
    include = sdk(runtime, system, output/'sdk')
    compiler = os.environ.get('CC', 'cc')
    flags = ['-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', '-I'+str(include)]
    if sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    env = dict(os.environ)
    env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
    for profile, define in (('raw', 'PORTABLE_RTC_UTC8_DENVER'), ('utc', 'ALARM_NATIVE_UTC')):
        executable = output/f'control-{profile}'
        subprocess.run([compiler, *flags, '-D'+define,
                        str(ROOT/'Services/alarm_control/control.c'),
                        str(ROOT/'test/scene/control_test.c'), *time_sources(system),
                        '-o', str(executable)], check=True)
        for case in CONTROL_CASES:
            subprocess.run([str(executable), case], env=env, check=True)
    executable = output/'app-test'
    subprocess.run([compiler, *flags, str(ROOT/'Apps/alarms_scene.c'),
                    str(ROOT/'test/scene/app_test.c'), '-o', str(executable)], check=True)
    for case in APP_CASES:
        subprocess.run([str(executable), case], env=env, check=True)
    return len(CONTROL_CASES)*2+len(APP_CASES)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--system-apps', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='risc-alarms-scene-') as temporary:
        count = run(args.runtime.resolve(), args.system_apps.resolve(), Path(temporary), args.sanitize)
    print(f'{count} Alarms/domain executions passed; hardware remains untested.')
