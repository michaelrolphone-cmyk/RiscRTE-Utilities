#!/usr/bin/env python3
"""Actual Battery controller + production adapter with modeled hardware.

This reduced UI/status/navigation fixture is separate from the full-feature
15-capability target recipe. --async adds delayed RGB565 ownership checks.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SIZES = ((128, 128), (160, 240), (240, 240), (320, 180), (410, 502),
         (502, 410), (320, 480), (480, 320), (800, 480))
CAPTURE_SIZES = ((240, 240), (410, 502), (128, 128), (320, 180))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dependency_identity(path, system, catalog):
    if path == catalog:
        return 'generated:catalog.c'
    if path.is_relative_to(system):
        return 'system:'+str(path.relative_to(system))
    if path.is_relative_to(ROOT):
        return 'utilities:'+str(path.relative_to(ROOT))
    raise RuntimeError('Unowned host dependency: '+str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--captures', action='store_true')
    parser.add_argument('--async', dest='async_rgb565', action='store_true')
    args = parser.parse_args()
    system, out = args.system_apps.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    catalog = out/'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    flags = ['-D'+name for name in (
        'PORTABLE_ALARM_CLIENT', 'PORTABLE_APP_SLEEP_LOCAL',
        'PORTABLE_INPUT_NAVIGATION', 'PORTABLE_INPUT_NAVIGATION_LOCAL',
        'PORTABLE_NOVA_UI', 'PORTABLE_POWER_STATUS', 'PORTABLE_FORCE_FULL_FRAMES',
        'PORTABLE_NATIVE_CANVAS', 'PORTABLE_BATTERY_NATIVE_UI')]
    flags += ['-DBATTERY_RETURN_APP="springboard.elf"',
              '-DNOVA_APP_SOURCE='+json.dumps(str(ROOT/'Apps/battery.c'))]
    if args.async_rgb565:
        flags += ['-DPORTABLE_ASYNC_RGB565']
    if args.captures:
        flags += ['-DPORTABLE_BLE_BROADCAST', '-DPORTABLE_NATIVE_CUSTODY_FENCE']
    includes = ['-I'+str(path) for path in (
        system/'lib/PortableApps/include', system/'lib/NativeApps/include',
        ROOT/'test/native_apps')]
    sources = [ROOT/'test/native_apps/battery_native_test.c',
               system/'lib/PortableApps/src/adapter.c', catalog]
    cc = os.environ.get('CC', 'cc')
    dependencies = {Path(__file__).resolve()}
    for source in sources:
        raw = subprocess.check_output([cc, '-std=c11', '-MM', *flags, *includes, str(source)], text=True)
        dependencies.update(Path(item).resolve() for item in
                            shlex.split(raw.replace('\\\n', ' ').split(':', 1)[1]))
    before = {path: sha(path) for path in dependencies}
    binary = out/'battery-native-test'
    command = [cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags, *includes]
    if args.sanitize:
        command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-fno-omit-frame-pointer', '-no-pie']
    command += [*map(str, sources), '-Wl,--wrap=portable_nova_native_text', '-o', str(binary)]
    subprocess.run(command, check=True)
    results = []
    sizes = CAPTURE_SIZES if args.captures else SIZES
    cases = range(100, 110) if args.captures else range(33)
    for width, height in sizes:
        for case in cases:
            env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'}
            # Ordinary matrices never inherit an accidental raster-output request.
            env.pop('BATTERY_CAPTURE', None)
            if args.captures:
                env['BATTERY_CAPTURE'] = str(out/f'{width}x{height}-{case}.ppm')
            result = subprocess.run([str(binary), str(out), str(case), str(width), str(height)],
                                    text=True, capture_output=True, timeout=30, env=env)
            if result.returncode:
                raise RuntimeError(f'{width}x{height} case {case}: '+result.stdout+result.stderr)
            results.append({'width': width, 'height': height, 'case': case, 'result': result.stdout.strip()})
    for path, digest in before.items():
        if sha(path) != digest:
            raise RuntimeError('Source changed during host qualification: '+str(path))
    def identity(path):
        return dependency_identity(path, system, catalog)
    receipt = {
        'hardware_tested': False,
        'recipe': 'direct-view capture fixture; BLE reachability unchanged' if args.captures else 'reduced UI/controller host fixture',
        'sanitized': args.sanitize, 'async_rgb565': args.async_rgb565,
        'display_ownership': 'delayed queued/active to complete; immutable framebuffer hash, no reacquire or display release while provider-owned' if args.async_rgb565 else 'synchronous fixture',
        'command': command, 'results': results,
        'source_sha256': {str(path.relative_to(ROOT)): digest for path, digest in sorted(before.items()) if identity(path).startswith('utilities:')},
        'compiled_dependency_sha256': {identity(path): digest for path, digest in sorted(before.items())},
        'source_unchanged_during_run': True,
    }
    if args.captures:
        receipt['capture_sha256'] = {path.name: sha(path) for path in sorted(out.glob('*.ppm'))}
    (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(f'Native Battery: {len(results)} '+('direct-view captures' if args.captures else 'controller/adapter cases')+
          f' PASS; sanitized={args.sanitize}; async={args.async_rgb565}')
    if args.captures:
        print('BLE reachability unchanged; not an end-to-end BLE navigation test')


if __name__ == '__main__':
    main()
