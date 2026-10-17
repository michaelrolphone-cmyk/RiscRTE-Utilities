#!/usr/bin/env python3
"""Actual Alarms and Countdown controllers plus native renderer/adapter.

Reduced controller fixture, separate from full-feature temporal qualification.
No target instruction or hardware execution is claimed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SIZES = ((120, 128), (128, 128), (160, 128), (160, 240), (240, 240),
         (410, 502), (502, 410), (320, 180), (800, 480))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dependency_identity(path, system, catalog):
    if path == catalog:
        return 'generated:catalog.c'
    if path.is_relative_to(system):
        return 'system:' + str(path.relative_to(system))
    if path.is_relative_to(ROOT):
        return 'utilities:' + str(path.relative_to(ROOT))
    raise RuntimeError('Unowned host dependency: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system-apps', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--input-receipt', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--async', dest='async_rgb565', action='store_true')
    parser.add_argument('--captures', action='store_true')
    args = parser.parse_args()
    system, out = args.system_apps.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    target = json.loads(args.input_receipt.read_text()) if args.input_receipt else {}
    catalog = out / 'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    flags = ['-D' + name for name in (
        'PORTABLE_ALARM_CLIENT', 'PORTABLE_APP_SLEEP_LOCAL', 'PORTABLE_INPUT_NAVIGATION',
        'PORTABLE_INPUT_NAVIGATION_LOCAL', 'PORTABLE_NOVA_UI', 'PORTABLE_FORCE_FULL_FRAMES',
        'PORTABLE_NATIVE_CANVAS', 'PORTABLE_ALARMS_NATIVE_UI', 'PORTABLE_APP_OWNS_TOUCH_CHROME')]
    if args.async_rgb565:
        flags += ['-DPORTABLE_ASYNC_RGB565']
    includes = ['-I' + str(path) for path in (
        system / 'lib/PortableApps/include', system / 'lib/NativeApps/include',
        ROOT / 'lib/Alarm/include')]
    sources = [ROOT / 'test/native_apps/alarms_native_test.c',
               system / 'lib/PortableApps/src/adapter.c', catalog]
    cc = os.environ.get('CC', 'cc')
    dependencies = {Path(__file__).resolve()}
    commands, results = [], []
    for app in ('alarms', 'countdown'):
        selected = flags + (['-DTEST_COUNTDOWN'] if app == 'countdown' else [])
        for source in sources:
            raw = subprocess.check_output([cc, '-std=c11', '-MM', *selected, *includes, str(source)], text=True)
            dependencies.update(Path(item).resolve() for item in
                                shlex.split(raw.replace('\\\n', ' ').split(':', 1)[1]))
        before = {path: sha(path) for path in dependencies}
        binary = out / (app + '-native-test')
        command = [cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-Wno-misleading-indentation', *selected, *includes]
        if args.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        '-fno-omit-frame-pointer', '-no-pie']
        command += [*map(str, sources), '-o', str(binary)]
        subprocess.run(command, check=True)
        commands.append(command)
        for width, height in SIZES:
            result = subprocess.run([str(binary), str(width), str(height)], text=True,
                                    capture_output=True, timeout=60,
                                    env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
                                         'UBSAN_OPTIONS': 'halt_on_error=1'})
            log = out / f'{app}-{width}x{height}.log'
            log.write_text(result.stdout + result.stderr)
            results.append({'app': app, 'case': 'geometry-editor-persistence', 'width': width,
                            'height': height, 'exit_code': result.returncode,
                            'log': log.name, 'log_sha256': sha(log)})
            if result.returncode:
                raise RuntimeError(str(log) + ': ' + log.read_text())
        if args.captures:
            from PIL import Image
            for width, height in ((128, 128), (240, 240), (410, 502), (320, 180)):
                for page in ((0, 1, 2, 3) if app == 'alarms' else (0, 1, 2)):
                    subprocess.run([str(binary), str(width), str(height), str(out), str(page)],
                                   check=True, capture_output=True, timeout=60)
                    ppm = out / 'frame.ppm'
                    with Image.open(ppm) as im:
                        im.save(out / f'{app}-{width}x{height}-page{page}.png')
                    ppm.unlink()
        if any(sha(path) != digest for path, digest in before.items()):
            raise RuntimeError('Source changed during native controller qualification')
    target_sources = {}
    for module in target.get('modules', []):
        if module.get('mode') == 'native':
            for key, value in module.get('source_sha256', {}).items():
                if key in target_sources and target_sources[key] != value:
                    raise RuntimeError('Conflicting exact target source: ' + key)
                target_sources[key] = value
    def identity(path):
        result = dependency_identity(path, system, catalog)
        if args.input_receipt and result in (
                'system:lib/PortableApps/include/RiscRuntimeV1.h',
                'system:lib/PortableApps/include/RiscKeyValueAtomicV1.h'):
            alias = 'runtime:sdk/app/' + path.name
            if target_sources.get(alias) != sha(path):
                raise RuntimeError('ABI header differs from exact target: ' + result)
            return alias
        return result
    receipt = {'recipe': 'reduced actual-controller/render fixture', 'hardware_tested': False,
               'target_instructions_executed': False, 'sanitized': args.sanitize,
               'async_rgb565': args.async_rgb565, 'source_unchanged': True,
               'target_receipt_sha256': sha(args.input_receipt) if args.input_receipt else '',
               'external_git_pins': target.get('external_git_pins', {}),
               'required_capabilities': target.get('required_capabilities', []),
               'compiled_dependency_sha256': {identity(path): sha(path)
                                              for path in sorted(dependencies)},
               'commands': commands, 'results': results,
               'capture_sha256': {path.name: sha(path) for path in sorted(out.glob('*.png'))}}
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'Native Alarms/Countdown: {len(results)} controller/render cases PASS; sanitized={args.sanitize}; async={args.async_rgb565}')


if __name__ == '__main__':
    main()
