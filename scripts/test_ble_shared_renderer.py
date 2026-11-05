#!/usr/bin/env python3
"""Replay current Scanner naming through the production scene/text/profile providers."""
import os
from pathlib import Path
import subprocess
import sys
from ble_test_support import stage_sdk

ROOT = Path(__file__).resolve().parents[1]


def tap(delay, x, y, gate='app'):
    return [(delay, gate, 'down', x, y), (40, gate, 'up', x, y)]


def run(system, drivers, paper=False, native=False):
    system, drivers = system.resolve(), drivers.resolve()
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(system / 'scripts'))
    from build_scene_services import PROFILE_FLAGS

    scan = tap(200, 50, 730 if paper else 210)
    detail = scan + tap(500, 100, 140 if paper else 80, 'scan')
    name = detail + tap(250, 100 if paper else 175, 730 if paper else 210)
    key = tap(250, 100 if paper else 50, 400 if paper else 98, 'text')
    save = tap(250, 400 if paper else 200, 690 if paper else 190, 'text')
    cancel = tap(250, 56 if paper else 20, 46 if paper else 20, 'text')
    back = [(250, 'text', 'back', 0, 0)]
    reopen = tap(250, 100 if paper else 175, 730 if paper else 210)
    scenes = {
        'idle': [],
        'scan-stop': scan + tap(800, 50, 730 if paper else 210, 'scan'),
        'nested-back': detail + tap(250, 50 if paper else 20, 40 if paper else 20) * 2,
        'name-save': name + key + save,
        'name-cancel': name + key + cancel,
        'name-navigation-back': name + key + back,
        'name-unavailable': name,
        'name-retained': name + key + cancel,
        'name-back-return': name + key + back + tap(250, 50 if paper else 20, 40 if paper else 20) * 2,
        'name-repeat': name + key + save + reopen + key + cancel + reopen + key + save,
    }
    if paper:
        scenes['enable-scan'] = scan + tap(250, 50, 730) + tap(250, 50, 730)
    root = ROOT / ('build/ble-scanner-native/shared-paper' if paper else 'build/ble-scanner-native/shared') if native else ROOT / ('build/ble-paper/shared' if paper else 'build/ble-renderer/shared')
    for sanitized in (False, True):
        out = root / str(int(sanitized))
        out.mkdir(parents=True, exist_ok=True)
        sdk = stage_sdk(system, out)
        catalog = out / 'catalog.c'
        catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
        flags = ['-DPORTABLE_TEXT_INPUT_CLIENT', '-DPORTABLE_APP_LAUNCH_GUARD',
                 '-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME',
                 '-DPORTABLE_RADIO_SESSION', '-DPORTABLE_ALARM_CLIENT',
                 '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_INPUT_NAVIGATION_LOCAL',
                 '-DPORTABLE_RETURN_APP="springboard.elf"']
        if native:
            flags += ['-DPORTABLE_BLE_SCANNER_NATIVE_UI', '-DPORTABLE_NATIVE_CANVAS']
        if paper:
            flags += ['-DBLE_PAPER_RENDER', '-DPORTABLE_DISPLAY_ROTATION=90']
        else:
            flags += ['-DPORTABLE_FORCE_FULL_FRAMES', '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_QUICK_RADIOS']
        if sanitized:
            flags += ['-fsanitize=' + os.environ.get('BLE_SANITIZERS', 'address,undefined'),
                      '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
            if sys.platform != 'darwin':
                flags += ['-no-pie']
        includes = [ROOT / 'Apps', system / 'Apps', sdk, system / 'lib/NativeApps/include',
                    drivers / 'sdk/driver', system / 'Services/scene_profile', system / 'Services/text_input']
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
                   '-Werror', *flags, *['-I' + str(p) for p in includes]]
        objects = []
        for host, entry in [('scene_host', 'ble_scene_driver_get'), ('text_input', 'ble_text_driver_get'),
                            ('scene_profile', 'ble_profile_driver_get')]:
            obj = out / (host + '.o')
            profile_flags = PROFILE_FLAGS['portrait-monochrome' if paper else 'compact-color'] if host == 'scene_profile' else []
            source = system / 'Services' / host / ('profile.c' if host == 'scene_profile' else 'host.c')
            subprocess.run([*command, *profile_flags, '-Dt5_driver_get=' + entry,
                            '-c', str(source), '-o', str(obj)], check=True)
            objects.append(obj)
        sources = [drivers / 'Drivers/ble_sensors/driver.c',
                   ROOT / 'test/native_apps/ble_paper_renderer_test.c',
                   ROOT / 'Apps/ble_scanner.c', system / 'lib/PortableApps/src/adapter.c', catalog]
        if not paper:
            sources += [system / 'lib/PortableApps/src' / s for s in
                        ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')]
        exe = out / 'ble-shared-renderer'
        subprocess.run([*command, *map(str, sources), *map(str, objects), '-o', str(exe)], check=True)
        for scene, actions in scenes.items():
            folder = out / scene
            folder.mkdir(exist_ok=True)
            for stale in folder.glob('frame-*.ppm'):
                stale.unlink()
            path = folder / 'actions.txt'
            path.write_text(''.join(' '.join(map(str, row)) + '\n' for row in actions))
            env = {k: v for k, v in os.environ.items() if not k.startswith('BLE_RENDER_')}
            env.update(ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
            if scene != 'idle':
                env['BLE_RENDER_SCAN'] = '1'
            if scene.startswith('name-'):
                env['BLE_RENDER_NAME'] = scene[5:]
            if scene in ('nested-back', 'name-back-return'):
                env['BLE_RENDER_BACK'] = '1'
            if scene == 'enable-scan':
                env['BLE_RENDER_ENABLE'] = '1'
            result = subprocess.run([str(exe), str(folder), str(path)], env=env,
                                    capture_output=True, text=True, timeout=60)
            (folder / 'run.log').write_text(result.stdout + result.stderr)
            if result.returncode:
                print(result.stdout + result.stderr, file=sys.stderr)
                result.check_returncode()
            frames = list(folder.glob('frame-*.ppm'))
            assert frames, scene
            width, height = (480, 800) if paper else (240, 240)
            header = f'P6\n{width} {height}\n255\n'.encode()
            for frame in frames:
                raw = frame.read_bytes()
                assert raw.startswith(header) and len(raw) == len(header) + width * height * 3
            print('BLE shared providers', 'paper' if paper else 'compact', sanitized, scene, 'PASS', flush=True)
