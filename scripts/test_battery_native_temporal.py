#!/usr/bin/env python3
"""Full-feature Battery temporal controls, bound to the target's source receipt.

Modeled providers do not execute target instructions or qualify real hardware.
Original and selector-off modes deliberately assert the measured old failures.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('battery_recipe', ROOT/'scripts/build_native_battery_qualification.py')
recipe = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(recipe)
ORIGINAL_SYSTEM = '9d2c55b32e88c0b12b1fa16200f399164767f5d1'
ORIGINAL_UTILITIES = '48b983154e295238f87ba5050ebdab9773c5eec7'
ORIGINAL_CASES = (0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13)
SIZES = ((240, 240), (410, 502), (128, 128), (320, 180))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def dependency_owner(path, root, roots, baseline, original_app=None):
    """Explicit staged owners take precedence over a containing CI checkout."""
    if original_app is not None and path.is_relative_to(original_app):
        return 'utilities-original', str(path.relative_to(original_app))
    if path.is_relative_to(baseline):
        return 'utilities-baseline', str(path.relative_to(baseline))
    matches = [(owner, str(path.relative_to(base))) for owner, base in roots.items() if path.is_relative_to(base)]
    if len(matches) == 1:
        return matches[0]
    if matches:
        raise ValueError('Ambiguous compiled dependency: '+str(path))
    if path.is_relative_to(root):
        return 'utilities', str(path.relative_to(root))
    raise ValueError('Unowned compiled dependency: '+str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-receipt', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    for owner in recipe.PINS:
        parser.add_argument('--'+owner.replace('_', '-'), type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--snapshot', action='store_true')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--overlays', action='store_true')
    mode.add_argument('--original', action='store_true')
    mode.add_argument('--selector-off', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    target = json.loads(args.input_receipt.read_text())
    if target['external_git_pins'] != recipe.PINS:
        raise ValueError('Target receipt pins differ from selected recipe')
    if target['required_capabilities'] != json.loads((ROOT/recipe.PROFILE_PATH).read_text())['requires']:
        raise ValueError('Target receipt lacks the complete capability contract')
    native_record = next(record for record in target['modules'] if record['mode'] == 'native')
    inputs = args.input_receipt.resolve().parent/'inputs'
    repos = {owner: getattr(args, owner).resolve() for owner in recipe.PINS}
    pins = dict(recipe.PINS)
    cases = tuple(range(25,33)) if args.overlays else ORIGINAL_CASES if args.original or args.selector_off else (*ORIGINAL_CASES, *range(14, 25))
    with tempfile.TemporaryDirectory(prefix='battery-temporal-', dir=out) as temporary:
        temporary = Path(temporary)
        roots = {owner: inputs/owner for owner in recipe.PINS}
        app = ROOT
        if args.original:
            pins['system'] = ORIGINAL_SYSTEM
            roots['system'] = temporary/'system'
            recipe.stage(repos['system'], ORIGINAL_SYSTEM, roots['system'],
                         ['lib/PortableApps', 'lib/NativeApps/include', 'Apps/alarm_writer.h',
                          'Apps/SpringboardPresentation.h', 'Apps/PaperPresentation.h', 'Apps/PaperFrame.h'], {}, 'system')
            app = temporary/'utilities'
            recipe.stage(ROOT, ORIGINAL_UTILITIES, app, ['Apps/'+name for name in (
                'battery.c', 'battery_power.inc', 'battery_native_view.inc', 'battery_native_layout.h',
                'alarm_writer.h', 'utility_frame.h', 'utility_preference_atomic.h')], {}, 'utilities')
            for name in ('RiscRuntimeV1.h', 'RiscKeyValueAtomicV1.h'):
                (roots['system']/'lib/PortableApps/include'/name).write_bytes((roots['runtime']/'sdk/app'/name).read_bytes())
        baseline = inputs/'utilities-baseline'
        includes = [roots['system']/'lib/PortableApps/include', roots['system']/'lib/NativeApps/include',
                    app/'Apps', baseline/'Apps', baseline/'lib/Alarm/include', baseline/'lib/Contexts/include',
                    roots['profile_source']/'profiles/shared-editor-inputs', roots['runtime']/'sdk/app',
                    roots['bridges']/'sdk/app', roots['bridges']/'sdk/driver', roots['bridges']/'include']
        selected = recipe.define_arguments(not args.selector_off)
        if args.original:
            selected = [flag for flag in selected if flag not in ['-D'+name for name in recipe.OVERLAYS]]
        flags = selected+['-I'+str(path) for path in includes]
        if args.original or args.selector_off:
            flags += ['-DBATTERY_EXPECT_ORIGINAL=1']
        if args.snapshot:
            flags += ['-DPORTABLE_RASTER_SNAPSHOT']
        for name, path in {'PERIPHERALS': ROOT/'test/native_apps/battery_native_peripherals.h',
                           'ATOMIC': ROOT/'tests/utility_atomic_transport.h', 'APP': app/'Apps/battery.c',
                           'ADAPTER': roots['system']/'lib/PortableApps/src/adapter.c'}.items():
            flags += ['-DBATTERY_DIAG_'+name+'='+json.dumps(str(path))]
        for item in recipe.FORCED:
            owner, relative = item.split(':', 1)
            flags += ['-include', str(roots[owner]/relative)]
        sources = [ROOT/'test/native_apps/battery_native_temporal_test.c',
                   ROOT/'test/native_apps/battery_native_temporal_adapter.c',
                   *[roots['bridges']/'apps/clock'/name for name in ('portable_navigation.c', 'portable_sleep.c')],
                   *[roots['system']/'lib/PortableApps/src'/name for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')]]
        paths, closure = {}, {}
        cc = os.environ.get('CC', 'cc')
        for source in sources:
            raw = subprocess.check_output([cc, '-std=gnu11', '-MM', *flags, str(source)], text=True)
            for name in shlex.split(raw.replace('\\\n', ' ').split(':', 1)[1]):
                path = Path(name).resolve()
                data = path.read_bytes()
                owner, relative = dependency_owner(path, ROOT, roots, baseline, app if args.original else None)
                if owner == 'system' and relative in ('lib/PortableApps/include/RiscRuntimeV1.h', 'lib/PortableApps/include/RiscKeyValueAtomicV1.h'):
                    owner, relative = 'runtime', 'sdk/app/'+path.name
                identity = owner+':'+relative
                if owner == 'utilities-original':
                    expected = recipe.git(ROOT, 'show', ORIGINAL_UTILITIES+':'+relative)
                    identity = 'utilities:'+relative
                elif owner == 'utilities-baseline':
                    identity = 'utilities:'+relative
                    expected = recipe.git(ROOT, 'show', recipe.BASE+':'+relative)
                elif owner in repos:
                    expected = recipe.git(repos[owner], 'show', pins[owner]+':'+relative)
                else:
                    expected = data
                    if not (args.original or args.selector_off) and identity in native_record['source_sha256']:
                        if digest(data) != native_record['source_sha256'][identity]:
                            raise ValueError('Controller differs from target: '+identity)
                if data != expected:
                    raise ValueError('Compiled external dependency differs from pin: '+identity)
                paths[path] = digest(data)
                closure[identity] = digest(data)
        if not recipe.REQUIRED_CLOSURE <= closure.keys():
            raise ValueError('Full production feature closure absent: '+repr(recipe.REQUIRED_CLOSURE-closure.keys()))
        binary = temporary/'probe'
        command = [cc, '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-function', '-Wno-unused-variable', *flags]
        if args.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
        command += [*map(str, sources), '-Wl,--wrap=portable_app_alarm_sleep', '-o', str(binary)]
        (out/'command.json').write_text(json.dumps(command, indent=2)+'\n')
        with (out/'build.log').open('w') as log:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
        results = []
        for case in cases:
            for width, height in SIZES if case >= 14 else ((240, 240),):
                result = subprocess.run([str(binary), str(case), str(width), str(height)], text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60,
                    env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
                logname = f'case{case}-{width}x{height}.log'
                (out/logname).write_text(result.stdout)
                results.append({'case': case, 'width': width, 'height': height, 'exit_code': result.returncode,
                                'log': logname, 'log_sha256': digest(result.stdout.encode())})
        for path, expected in paths.items():
            if digest(path.read_bytes()) != expected:
                raise ValueError('Source changed during temporal qualification: '+str(path))
        receipt = {'schema': 1, 'native_overlay_control': args.overlays, 'original_failure_control': args.original, 'selector_off': args.selector_off,
                   'sanitized': args.sanitize, 'snapshot': args.snapshot, 'external_git_pins': pins,
                   'utilities_original': ORIGINAL_UTILITIES if args.original else None,
                   'target_receipt_sha256': digest(args.input_receipt.read_bytes()),
                   'required_capabilities': target['required_capabilities'], 'build_defines': selected,
                   'source_unchanged': True, 'compiled_dependency_sha256': dict(sorted(closure.items())),
                   'binary_sha256': digest(binary.read_bytes()), 'results': results,
                   'hardware_tested': False, 'target_instructions_executed': False,
                   'sanitizer_limit': 'LeakSanitizer unsupported under ptrace; ASan/UBSan, explicit custody counters and immutable provider image checks remain enabled.'}
        (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
        if any(result['exit_code'] for result in results):
            raise RuntimeError('Temporal controls failed; inspect receipt and case logs')
        print(f'Battery temporal: {len(results)} PASS; original={args.original}; selector_off={args.selector_off}; sanitized={args.sanitize}; snapshot={args.snapshot}')


if __name__ == '__main__':
    main()
