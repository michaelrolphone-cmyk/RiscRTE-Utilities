#!/usr/bin/env python3
"""Exact feature-off Stopwatch tokens, plus current-source paper compatibility.

This host-only test does not change historical build pins or select a product
profile. It records the actual current dependency revisions and file hashes.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
BASELINE = '2d0e03a58d1d78cec10033dfa384368281156a46'
TIME_SOURCES = ('PortableNativeTimeSource.c', 'PortableRealtimeClient.c',
                'PortableTimeZone.c', 'PortableTimeZoneCatalog.c',
                'PortableTimeZonePreference.c')
NATIVE = ['PORTABLE_STOPWATCH_NATIVE_UI', 'PORTABLE_NATIVE_CANVAS',
          'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_NATIVE_CUSTODY_FENCE']
UTC = ['PORTABLE_STOPWATCH_NATIVE_UTC', 'PORTABLE_NATIVE_TIME_TOOLBAR',
       'PORTABLE_NATIVE_CUSTODY_FENCE', 'PORTABLE_APP_LAUNCH_GUARD',
       'ALARM_SERVICE_TAGGED_V2']


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def provenance(repo, directories):
    return {'path': str(repo), 'commit': git(repo, 'rev-parse', 'HEAD'),
            'status': git(repo, 'status', '--short'),
            'sha256': {str(p.relative_to(repo)): sha(p)
                       for directory in directories for p in sorted((repo / directory).rglob('*'))
                       if p.is_file() and p.suffix in ('.c', '.h', '.inc', '.py')}}


def stage(system, runtime, destination, utc):
    headers = destination / 'include'
    shutil.copytree(system / 'lib/PortableApps/include', headers, dirs_exist_ok=True)
    shutil.copytree(system / 'lib/PortableApps/time', destination / 'time', dirs_exist_ok=True)
    for name in ('RiscRuntimeV1.h', 'RiscRealtimeV1.h'):
        shutil.copyfile(runtime / 'sdk/app' / name, headers / name)
    if utc:
        for name in ('AlarmServiceV1.h', 'AlarmServiceV2.h'):
            shutil.copyfile(ROOT / 'lib/Alarm/include' / name, headers / name)
    return headers


def tokens(source, defines, headers, system):
    data = subprocess.check_output([
        os.environ.get('CC', 'cc'), '-std=c11', '-E', '-P',
        *('-D' + flag for flag in defines),
        *('-I' + str(path) for path in (headers, system / 'lib/NativeApps/include', system / 'Apps')),
        str(source)], text=True)
    return '\n'.join(re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_0-9]+|[^\s]''', data))


def compare_tokens(system, runtime, out):
    baseline = out / 'baseline'
    baseline.mkdir(exist_ok=True)
    archive = subprocess.check_output(['git', '-C', str(ROOT), 'archive', BASELINE, 'Apps'])
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        tar.extractall(baseline, filter='data')
    headers = stage(system, runtime, out / 'token-headers', True)
    profiles = [
        ('raw', []),
        ('nova', ['PORTABLE_NOVA_UI']),
        ('native-canvas', ['PORTABLE_NOVA_UI', 'PORTABLE_NATIVE_CANVAS']),
        ('native-height', ['PORTABLE_NOVA_UI', 'PORTABLE_WATCH_NATIVE_HEIGHT']),
        ('utc-raw', UTC),
        ('utc-nova', [*UTC, 'PORTABLE_NOVA_UI']),
        ('paper-rtc', ['PORTABLE_NOVA_UI', 'PORTABLE_PAPER_UTILITIES', 'PORTABLE_PAPER_PREFERENCES']),
        ('paper-utc', [*UTC, 'PORTABLE_NOVA_UI', 'PORTABLE_PAPER_UTILITIES', 'PORTABLE_PAPER_PREFERENCES']),
    ]
    rows = []
    for profile, defines in profiles:
        for alarm in (False, True):
            for snapshot in (False, True):
                for unpadded in (False, True):
                    flags = defines + (['PORTABLE_ALARM_CLIENT'] if alarm else [])
                    flags += ['PORTABLE_RASTER_SNAPSHOT'] if snapshot else []
                    flags += ['PORTABLE_UNPADDED_HOURS'] if unpadded else []
                    before = tokens(baseline / 'Apps/stopwatch.c', flags, headers, system)
                    after = tokens(ROOT / 'Apps/stopwatch.c', flags, headers, system)
                    assert before == after, (profile, alarm, snapshot, unpadded)
                    assert not re.search(r'\bstopwatch_native_(?:active|select|render|input|mapping)\b|\bportable_stopwatch_native_', after)
                    rows.append({'profile': profile, 'alarm': alarm, 'snapshot': snapshot,
                                 'unpadded_hours': unpadded, 'identical': True,
                                 'defines': flags, 'token_sha256': hashlib.sha256(before.encode()).hexdigest()})
    print(f'PASS: {len(rows)} exact feature-off raw/NOVA/UTC/paper token comparisons', flush=True)
    return rows


def run_controllers(system, runtime, out, normal_only, lifecycle=False):
    catalog = out / 'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    rows = []
    for utc in ([True] if lifecycle else [False, True]):
        headers = stage(system, runtime, out / ('utc' if utc else 'rtc'), utc)
        fixture = ROOT / 'test/native_apps' / ('native_utility_controller_test.c' if utc else 'paper_utility_test.c')
        sources = [fixture, *(system / 'lib/PortableApps/src' / name for name in
                   ('adapter.c', 'quick_actions.c', 'quick_render.c', 'quick_session.c')),
                   catalog]
        if utc:
            sources += [system / 'lib/PortableApps/src' / name for name in TIME_SOURCES]
        for selected in (False, True):
            # Snapshot coverage is paired selected/off on paper so both frame
            # paths establish the same unchanged controller outcomes.
            for snapshot in (False, True):
                for sanitized in ([False] if normal_only else [False, True]):
                    label = f'{"utc" if utc else "rtc"}-{int(selected)}-{int(snapshot)}-{int(sanitized)}'
                    if lifecycle:
                        label += '-lifecycle'
                    binary = out / label
                    defines = ['PORTABLE_NOVA_UI', 'PORTABLE_PAPER_UTILITIES',
                               'PORTABLE_ALARM_CLIENT', 'PORTABLE_DISPLAY_ROTATION=90',
                               'PORTABLE_INPUT_NAVIGATION', 'PORTABLE_HOME_APP="default.elf"',
                               'PORTABLE_QUICK_ACTIONS', 'TEST_STOPWATCH_ORDERED_TOUCH',
                               'NOVA_APP_ID=2', 'NOVA_APP_SOURCE=' + json.dumps(str(ROOT / 'Apps/stopwatch.c'))]
                    defines += UTC if utc else ['PORTABLE_RTC_WALL_TIME']
                    defines += NATIVE if selected else []
                    defines += ['PORTABLE_RASTER_SNAPSHOT'] if snapshot else []
                    if lifecycle:
                        defines += ['TEST_STOPWATCH_EXTRA_LIFECYCLE', 'PORTABLE_APP_SLEEP_LOCAL',
                                    'PORTABLE_CROWN_SLEEP_LOCAL']
                    sanitize = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                                '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
                    cmd = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-no-pie', '-Wall', '-Wextra',
                           '-Werror', '-Wno-missing-field-initializers', *sanitize,
                           *('-D' + flag for flag in defines),
                           *('-I' + str(path) for path in (headers, system / 'lib/NativeApps/include', system / 'Apps',
                                                         ROOT / 'test/native_apps')),
                           *map(str, sources)]
                    if utc:
                        cmd += ['-Wl,--wrap=free']
                    cmd += ['-o', str(binary)]
                    subprocess.run(cmd, check=True, timeout=120)
                    # Selected native UI must stay inactive on MONO1 paper.
                    # Off UTC additionally replays the existing RGB controller.
                    for paper in ([1] if selected or snapshot or not utc or lifecycle else [0, 1]):
                        for case in (range(25, 30) if lifecycle else range(25 if utc else 12)):
                            if utc and case == 20 and not paper:
                                continue
                            logfile = out / f'{label}-{paper}-{case}.log'
                            case_dir = out / f'{label}-{paper}-{case}'
                            case_dir.mkdir(exist_ok=True)
                            argv = [str(binary)] + ([] if utc else [str(case_dir)]) + [str(case), str(paper)]
                            result = subprocess.run(argv, capture_output=True, text=True, timeout=30,
                                      env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1',
                                           'UBSAN_OPTIONS': 'halt_on_error=1'})
                            logfile.write_text(result.stdout + result.stderr)
                            if result.returncode:
                                raise RuntimeError(f'{label} paper={paper} case={case}: {result.returncode}\n{result.stdout}{result.stderr}')
                            rows.append({'utc': utc, 'native_ui_selected': selected,
                                         'paper': bool(paper), 'snapshot': snapshot,
                                         'lifecycle': lifecycle,
                                         'sanitized': sanitized, 'case': case, 'passed': True,
                                         'defines': defines, 'log': str(logfile.relative_to(out))})
                    print(f'PASS: {label} existing Stopwatch controller cases', flush=True)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--preprocess-only', action='store_true')
    parser.add_argument('--normal-only', action='store_true')
    parser.add_argument('--lifecycle-only', action='store_true')
    args = parser.parse_args()
    system, runtime = args.system_apps.resolve(), args.runtime.resolve()
    out = ROOT / 'build/stopwatch-native-optout'
    out.mkdir(parents=True, exist_ok=True)
    receipt = {'baseline': BASELINE, 'hardware_tested': False, 'publication': 'local only',
               'compiler': subprocess.check_output([os.environ.get('CC', 'cc'), '--version'], text=True).splitlines()[0],
               'scope': 'Exact-base application token parity; existing host controller fixtures on explicitly current dependencies',
               'preprocess_only': args.preprocess_only, 'normal_only': args.normal_only,
               'lifecycle_only': args.lifecycle_only,
               'fixture_adaptations': [
                   'Test-only ordered contact/move/up and physical-button edge stream with matching snapshots',
                   '20 ms hardware poll time preserves original poll-indexed scenario duration',
                   'Explicit Back ends bounded scenarios; runtime health remains healthy',
                   'Canonical live grant/release suffix and required retention callback',
                   'Cases 25-29: alarm retention, sleep retention, sleep wake, sleep refusal, alarm cue interruption'],
               'utilities': provenance(ROOT, ('Apps', 'test/native_apps', 'scripts', 'lib/Alarm/include')),
               'system': provenance(system, ('lib/PortableApps', 'lib/NativeApps/include', 'Apps')),
               'runtime': provenance(runtime, ('sdk/app',)), 'complete': False}
    receipt['token_comparisons'] = compare_tokens(system, runtime, out)
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    receipt['controller_runs'] = []
    if not args.preprocess_only:
        if not args.lifecycle_only:
            receipt['controller_runs'] += run_controllers(system, runtime, out, args.normal_only)
        receipt['controller_runs'] += run_controllers(system, runtime, out, args.normal_only, lifecycle=True)
    changed = []
    for label, repo in (('utilities', ROOT), ('system', system), ('runtime', runtime)):
        changed += [label + '/' + name for name, expected in receipt[label]['sha256'].items()
                    if not (label == 'utilities' and name.startswith('scripts/')
                            and name != 'scripts/test_stopwatch_native_optout.py')
                    if not (repo / name).is_file() or sha(repo / name) != expected]
    receipt['source_stability_scope'] = 'All recorded C/header/include inputs plus this runner; unrelated build/test scripts are not executed or guarded'
    receipt['sources_unchanged_during_run'] = not changed
    if changed:
        receipt['changed_during_run'] = changed
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        raise RuntimeError('Sources changed during verification; rerun: ' + ', '.join(changed))
    receipt['complete'] = True
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'PASS: {len(receipt["token_comparisons"])} token comparisons; {len(receipt["controller_runs"])} current-source controller scenarios')


if __name__ == '__main__':
    main()
