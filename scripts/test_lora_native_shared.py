#!/usr/bin/env python3
"""Qualify native LoRa with the actual adapter and shared scene/text providers.

Physical display, ordered touch, navigation, radio and service dependencies are
fixtures. No RF, device deployment, ELF-loader or product qualification is claimed.
The unchanged legacy runner supplies its established behavior scenarios; app taps
are retargeted using the production native geometry, while host keyboard taps use
the production host's separate compact/portrait geometry.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from test_lora_shared_input import scenarios, define_argument, tap, key, capture, number

ROOT = Path(__file__).resolve().parents[1]
SIZES = ((160, 160), (240, 240), (320, 180), (410, 502), (502, 410))
BASE_DEFINES = ('PORTABLE_FORCE_FULL_FRAMES', 'PORTABLE_ALARM_CLIENT',
                'PORTABLE_RADIO_SESSION', 'PORTABLE_NOVA_UI',
                'PORTABLE_APP_OWNS_TOUCH_CHROME')
SAFETY_DEFINES = ('PORTABLE_LORA_SHARED_TEXT_INPUT', 'PORTABLE_TEXT_INPUT_CLIENT',
                  'PORTABLE_APP_LAUNCH_GUARD', 'PORTABLE_CHECKED_STORAGE_RUNTIME',
                  'PORTABLE_NATIVE_CUSTODY_FENCE', 'PORTABLE_ALARM_TERMINAL_RETENTION')
NATIVE_DEFINES = ('PORTABLE_LORA_NATIVE_UI', 'PORTABLE_NATIVE_CANVAS')
HARNESS_DEFINES = ('PORTABLE_INPUT_NAVIGATION', 'PORTABLE_INPUT_NAVIGATION_LOCAL', 'PORTABLE_ASYNC_RGB565')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def native_actions(actions, layout, width, height):
    targets = {(100, 103): 'home0', (100, 150): 'home1',
               (50, 203): 'home2', (175, 203): 'home3',
               (55, 210): 'footer0', (175, 210): 'footer1',
               (100, 210): 'full_footer', (20, 20): 'back', (100, 90): 'body',
               (100, 55): 'history0', (100, 64): 'rf0',
               (100, 112): 'rf1', (100, 160): 'rf2'}
    result = []
    for delay, gate, kind, x, y in actions:
        if kind in ('down', 'up'):
            if gate == 'app':
                name = targets[(x, y)]
                left, top, w, h = layout[name]
                x, y = left + w // 2, top + h // 2
            elif gate == 'text':
                # The requested size matrix uses the compact scene keyboard.
                # Header geometry is profile-owned and not scaled with height.
                assert height < width * 3 // 2
                if (x, y) == (20, 20):
                    x, y = 42, 19
                elif (x, y) == (190, 20):
                    x, y = width - 42, 19
                else:
                    x, y = x * width // 240, y * height // 240
            assert 0 <= x < width and 0 <= y < height
        result.append((delay, gate, kind, x, y))
    return result


def run(args):
    system = args.system_apps.resolve()
    output = (args.output or ROOT / 'build/lora-native-shared').resolve()
    output.mkdir(parents=True, exist_ok=True)
    sdk = output / 'sdk/include'
    sdk.mkdir(parents=True, exist_ok=True)
    folders = []
    if args.runtime:
        folders += [args.runtime / 'sdk/app', args.runtime / 'sdk/driver']
    folders += [system / 'lib/PortableApps/include', system / 'sdk/app']
    for folder in folders:
        for source in folder.glob('*.h'):
            shutil.copyfile(source, sdk / source.name)
    for folder in (ROOT / 'lib/Alarm/include', ROOT / 'lib/Contexts/include'):
        for source in folder.glob('*.h'):
            if not (sdk / source.name).exists():
                shutil.copyfile(source, sdk / source.name)
    shutil.copytree(system / 'lib/PortableApps/time', sdk.parent / 'time', dirs_exist_ok=True)
    alarm_profile = output / 'alarm-profile.h'
    alarm_profile.write_text('#include "AlarmServiceV1.h"\n')
    catalog = output / 'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    flags = ['-D' + x for x in BASE_DEFINES + HARNESS_DEFINES]
    forced_includes = []
    if args.recipe:
        recipe = json.loads(args.recipe.read_text())['lora_messages']['defines']
        flags = [x for x in recipe if x.startswith('-D')]
        for i, value in enumerate(recipe):
            if value == '-include':
                forced_includes += ['-include', recipe[i + 1]]
    flags += ['-D' + x for x in SAFETY_DEFINES + NATIVE_DEFINES]
    flags += ['-D' + x for x in args.extra_define]
    flags += forced_includes + ['-include', str(alarm_profile)]
    includes = [sdk, ROOT / 'Apps', system / 'Apps', system / 'lib/NativeApps/include',
                system / 'Services/scene_profile', system / 'Services/text_input']
    source_paths = [ROOT / 'Apps/lora_messages.c', ROOT / 'Apps/lora_messages_model.h',
                    ROOT / 'Apps/lora_messages_native_layout.h', ROOT / 'Apps/lora_messages_native_view.inc',
                    ROOT / 'test/native_apps/lora_native_shared_test.c', Path(__file__),
                    ROOT / 'test/native_apps/lora_shared_renderer_test.c',
                    ROOT / 'scripts/test_lora_shared_input.py',
                    system / 'lib/PortableApps/src/adapter.c',
                    system / 'lib/PortableApps/include/PortableLoRaNative.h', system / 'lib/PortableApps/src/lora_native_input.inc',
                    system / 'Services/scene_host/host.c', system / 'Services/scene_host/keyboard.inc',
                    system / 'Services/text_input/host.c', system / 'Services/scene_profile/profile.c']
    evidence = {'scope': 'Actual app/adapter/shared hosts with deterministic physical dependencies',
                'recipe': str(args.recipe.resolve()) if args.recipe else 'minimal explicit composition',
                'defines': flags, 'harness_features': list(HARNESS_DEFINES) if not args.recipe else [], 'host_defines': ['-DSCENE_HOST_TELEMETRY=0', '-DSCENE_HOST_WATCH_240=0'],
                'host_profile_change': 'Generic production scene host admits the native dimension matrix; legacy run retains SCENE_HOST_WATCH_240=1.',
                'native_canvas_owner': 11, 'compiler': os.environ.get('CC', 'cc'),
                'not_claimed': ['hardware/RF qualification', 'Runtime ELF loader', 'product pins/recipes or publication'],
                'leak_sanitizer': 'ASan/UBSan use detect_leaks=0; clean exits assert zero grants, leases, subscriptions, pending tokens and wrapped allocations.',
                'sources': {str(p): sha(p) for p in source_paths}, 'results': []}
    receipt = output / 'qualification.json'
    receipt.write_text(json.dumps(evidence, indent=2) + '\n')
    selected = scenarios()
    compose = tap(100, 103) + tap(55, 210)
    for retry in ('missing-retry', 'old-retry'):
        selected[retry] = (compose + capture() + tap(55, 210) + key('Q') + key('\x03', 220) + capture(),
                           {'LORA_EXPECT_DRAFT': 'Q', 'LORA_EXPECT_LENGTH': '1', 'LORA_EXPECT_OPENS': '1',
                            'LORA_EXPECT_SENDS': '0', 'LORA_EXPECT_HISTORY': '0'})
    selected['review-240'] = (compose + key('Q', 45) * 243 + key('\x03', 220),
                              {'LORA_EXPECT_DRAFT': 'Q' * 240, 'LORA_EXPECT_LENGTH': '240',
                               'LORA_EXPECT_OPENS': '1', 'LORA_EXPECT_SENDS': '0', 'LORA_EXPECT_HISTORY': '0'})
    actions = tap(175, 203)
    for field, (invalid, valid, length) in enumerate(zip(
            ['0', '333', '1', '2', '99', '1'],
            ['916000000', '250000', '8', '6', '-2', '16'], [9, 6, 1, 1, 1, 1])):
        if field == 3:
            actions += tap(55, 210)
        actions += tap(100, 64 + field % 3 * 48) + number(invalid, length) + key('\x03', 220) + capture()
        actions += tap(100, 210) + number(valid, len(invalid)) + key('\x03', 220)
    selected['rf-recover-all'] = (actions + tap(175, 210) + capture(after=True),
                                   {'LORA_EXPECT_DRAFT': '', 'LORA_EXPECT_LENGTH': '0',
                                    'LORA_EXPECT_OPENS': '12', 'LORA_EXPECT_SENDS': '0', 'LORA_EXPECT_HISTORY': '0'})
    selected['header-layout-cancel'] = (tap(100, 103) + [(160, 'app', 'down', 20, 20),
         (100, 'app', 'back-held', 0, 0), (200, 'app', 'up', 20, 20)] + capture(),
         {'LORA_EXPECT_DRAFT': '', 'LORA_EXPECT_LENGTH': '0', 'LORA_EXPECT_OPENS': '0',
          'LORA_EXPECT_SENDS': '0', 'LORA_EXPECT_HISTORY': '0'})
    selected['header-slide-cancel'] = (tap(100, 103) + [(160, 'app', 'down', 20, 20),
         (100, 'app', 'up', 100, 90)] + capture(),
         {'LORA_EXPECT_DRAFT': '', 'LORA_EXPECT_LENGTH': '0', 'LORA_EXPECT_OPENS': '0',
          'LORA_EXPECT_SENDS': '0', 'LORA_EXPECT_HISTORY': '0'})
    slow_actions, slow_env = selected['draft-repeat']
    selected['slow-display'] = ([(max(delay, 800) if kind in ('down', 'capture') and gate == 'app' else delay,
                                 gate, kind, x, y) for delay, gate, kind, x, y in slow_actions],
                                {**slow_env, 'LORA_TEST_DELAY_MS': '450', 'LORA_TEST_APP_DELAY_MS': '450'})
    selected['exit-display-failure'] = (tap(100, 103, delay=800) + capture(delay=800),
                                      {'LORA_TEST_APP_DELAY_MS': '450'})
    if args.scenario:
        unknown = set(args.scenario) - set(selected)
        if unknown:
            raise ValueError('Unknown scenarios: ' + ', '.join(sorted(unknown)))
        selected = {k: v for k, v in selected.items() if k in args.scenario}
    sizes = [s for s in SIZES if not args.size or f'{s[0]}x{s[1]}' in args.size]
    assert sizes
    modes = [False] if args.normal_only else [True] if args.sanitize_only else [False, True]
    for sanitized in modes:
        out = output / ('sanitized' if sanitized else 'normal')
        out.mkdir(exist_ok=True)
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   *flags, *['-I' + str(p) for p in includes]]
        if sanitized:
            command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
        commands, objects = [], []
        compile_log = out / 'compile.log'
        with compile_log.open('w') as log:
            for provider, entry in [('scene_host', 'lora_scene_driver_get'), ('text_input', 'lora_text_driver_get'), ('scene_profile', 'lora_profile_driver_get')]:
                obj = out / (provider + '.o')
                source = system / 'Services' / provider / ('profile.c' if provider == 'scene_profile' else 'host.c')
                extra = evidence['host_defines'] if provider == 'scene_host' else []
                cmd = [*command, *extra, '-Dt5_driver_get=' + entry, '-c', str(source), '-o', str(obj)]
                commands.append(cmd)
                (out / 'compile-command.json').write_text(json.dumps(commands, indent=2) + '\n')
                subprocess.run(cmd, check=True, stdout=log, stderr=subprocess.STDOUT)
                objects.append(obj)
            sources = [ROOT / 'test/native_apps/lora_native_shared_test.c', system / 'lib/PortableApps/src/adapter.c', catalog]
            sources += [system / 'lib/PortableApps/src' / name for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')]
            executable = out / 'lora-native-shared'
            cmd = [*command, *map(str, sources), *map(str, objects), '-Wl,--wrap=malloc', '-Wl,--wrap=free', '-Wl,--wrap=portable_nova_native_text', '-o', str(executable)]
            commands.append(cmd)
            (out / 'compile-command.json').write_text(json.dumps(commands, indent=2) + '\n')
            subprocess.run(cmd, check=True, stdout=log, stderr=subprocess.STDOUT)
        for width, height in sizes:
            base_env = {k: v for k, v in os.environ.items() if not k.startswith('LORA_')}
            base_env.update(ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
            layout = json.loads(subprocess.check_output([str(executable), '--layout', str(width), str(height)], env=base_env, text=True))
            for name, (original_actions, extra_env) in selected.items():
                folder = out / f'{width}x{height}' / name
                folder.mkdir(parents=True, exist_ok=True)
                if name == 'review-240':
                    page_count = (240 + layout['compose_step'] - 1) // layout['compose_step']
                    original_actions = original_actions + capture()
                    for _ in range(page_count):
                        original_actions += tap(100, 90) + capture()
                actions = native_actions(original_actions, layout, width, height)
                action_file = folder / 'actions.txt'
                action_file.write_text(''.join(' '.join(map(str, row)) + '\n' for row in actions))
                env = {**base_env, **extra_env}
                result = subprocess.run([str(executable), str(folder), str(action_file), name, str(width), str(height)], env=env, capture_output=True, text=True, timeout=120)
                log = folder / 'run.log'
                log.write_text(result.stdout + result.stderr)
                record = {'size': [width, height], 'mode': name, 'sanitized': sanitized,
                          'returncode': result.returncode, 'log': str(log), 'log_sha256': sha(log),
                          'actions_sha256': sha(action_file), 'rasters': {}}
                from PIL import Image
                for ppm in folder.glob('*.ppm'):
                    with Image.open(ppm) as im:
                        assert im.size == (width, height)
                        keep = result.returncode or (not sanitized and name in ('draft-done', 'rf-recover', 'rf-all', 'send-history', 'limit-240', 'review-240', 'rapid-pending', 'missing-host'))
                        if keep:
                            png = ppm.with_suffix('.png')
                            im.save(png)
                            record['rasters'][str(png)] = sha(png)
                    ppm.unlink()
                evidence['results'].append(record)
                receipt.write_text(json.dumps(evidence, indent=2) + '\n')
                if result.returncode:
                    print(result.stdout + result.stderr, file=sys.stderr)
                    result.check_returncode()
                print('Native LoRa shared input', f'{width}x{height}', 'ASan/UBSan' if sanitized else 'normal', name, 'PASS', flush=True)
        for obj in objects:
            obj.unlink()
    evidence['all_passed'] = all(r['returncode'] == 0 for r in evidence['results'])
    evidence['source_hashes_stable'] = all(sha(Path(path)) == digest for path, digest in evidence['sources'].items())
    receipt.write_text(json.dumps(evidence, indent=2) + '\n')
    print('Evidence:', receipt)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--system-apps', type=Path, required=True)
    p.add_argument('--runtime', type=Path)
    p.add_argument('--recipe', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--size', action='append', choices=[f'{w}x{h}' for w, h in SIZES])
    p.add_argument('--scenario', action='append')
    modes = p.add_mutually_exclusive_group()
    modes.add_argument('--normal-only', action='store_true')
    modes.add_argument('--sanitize-only', action='store_true')
    p.add_argument('--extra-define', action='append', default=[], type=define_argument)
    run(p.parse_args())
