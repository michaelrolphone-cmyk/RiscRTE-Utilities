#!/usr/bin/env python3
"""Qualify production Scanner/shared hosts with exact delivered X4 profile flags.

Without --system-apps, verify and replay a matching target receipt. Source-only
recovery uses --system-apps and --runtime to remap the same compilation inputs,
stage current headers over the archived SDK, and hash every host dependency.
--source-only needs no archived artifact: it stages a fresh SDK from explicitly
pinned clean source checkouts and uses the same delivered profile and fixture.
No mode builds a target ELF or changes the profile's flags or grants.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path), *args], text=True).strip()


def tap(delay, x, y, gate='app'):
    return [(delay, gate, 'down', x, y), (40, gate, 'up', x, y)]


def nav(delay, kind, gate='app'):
    return [(delay, gate, kind, 0, 0)]


def scenarios():
    scan = tap(200, 50, 730)
    detail = scan + tap(500, 100, 140, 'scan')
    name = detail + tap(250, 100, 730)
    key = tap(250, 100, 400, 'text')
    save = tap(250, 400, 690, 'text')
    cancel = tap(250, 56, 46, 'text')
    back = nav(250, 'back', 'text')
    scenes = {
        'idle': [],
        'scan-stop': scan + tap(800, 50, 730, 'scan'),
        'nested-back': detail + tap(250, 50, 40) + tap(250, 50, 40),
        'enable-scan': scan + tap(250, 50, 730) + tap(250, 50, 730),
        'name-save': name + key + save,
        'name-cancel': name + key + cancel,
        'name-navigation-back': name + key + back,
        'name-unavailable': name,
        'name-retained': name + key + cancel,
        'name-back-return': name + key + back + tap(250, 50, 40) + tap(250, 50, 40),
        'name-repeat': name + key + save + tap(250, 100, 730) + key + cancel + tap(250, 100, 730) + key + save,
        'home': scan + nav(800, 'home'),
        'name-navigation-home': name + key + nav(250, 'home', 'text') + nav(250, 'home'),
        'resident-controls': [(200, 'app', 'down', 100, 20),
                              (40, 'app', 'move', 100, 60),
                              (40, 'app', 'move', 100, 100),
                              (40, 'app', 'up', 100, 100)],
    }
    for boundary in ('acquire', 'open', 'poll', 'close', 'release', 'kv-acquire',
                     'kv-get', 'kv-put', 'kv-release', 'acquire-fail'):
        scenes['name-loss-' + boundary] = name + key + save
    return scenes


def exact(path, revision):
    if not revision or not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('Source-only qualification requires a full explicit revision: ' + str(path))
    if git(path, 'rev-parse', 'HEAD') != revision or git(path, 'status', '--porcelain', '--untracked-files=no'):
        raise ValueError('Clean exact source required: ' + str(path) + ' @ ' + revision)
    return dict(revision=revision, tree=git(path, 'rev-parse', 'HEAD^{tree}'))


def source_recipe(a, profile, sdk):
    roots = [('System', a.system_apps, a.system_revision), ('Runtime', a.runtime, a.runtime_revision),
             ('Drivers', a.drivers, a.drivers_revision), ('Reader', a.reader, a.reader_revision),
             ('X4', a.x4, a.x4_revision)]
    pins = {label: exact(path, revision) for label, path, revision in roots}
    if sdk.exists():
        shutil.rmtree(sdk)
    sdk.mkdir(parents=True, exist_ok=True)
    system = a.system_apps
    for folder in (system / 'lib/PortableApps/include', a.runtime / 'sdk/app', ROOT / 'lib/Alarm/include'):
        for source in folder.glob('*.h'):
            shutil.copyfile(source, sdk / source.name)
    shutil.copyfile(ROOT / 'lib/Bluetooth/include/RiscBluetoothSensorsV1.h', sdk / 'RiscBluetoothSensorsV1.h')
    # These are the exact display contracts used by the delivered shared host.
    # Never substitute the older portable display header or copy provider code.
    display = {
        'RiscDisplayOutputV1.h': (a.reader / 'sdk/driver', '99b682ba34f07e7868dec8e78dfb816c067fdc184e647033bc6ed8d489b9c9a2'),
        'RiscDisplayOutputPowerV1.h': (a.reader / 'sdk/driver', 'e9c7d3bd3ddf9bce128503aafb5669734db09424149f45b3a6f735cd5a7c8667'),
        'RiscDisplayOutputMetricsV1.h': (a.x4 / 'minimal/interfaces', '5be17adbae2ec26a855894e0d6443ac29572002c21efe53b2a913a5a0f0c642d'),
        'RiscDisplayOutputSnapshotV1.h': (a.x4 / 'minimal/interfaces', 'b835fd1a3600551d54efb2806cbe8dd3ecc512a538572dc853aaf11f4e2490b6'),
    }
    for name, (folder, expected) in display.items():
        assert digest(folder / name) == expected, ('shared-host display contract drift', name)
        shutil.copyfile(folder / name, sdk / name)
    for name in ('PaperPresentation.h', 'PaperFrame.h'):
        (sdk / name).unlink(missing_ok=True)
    shutil.copytree(system / 'lib/PortableApps/time', sdk.parent / 'time', dirs_exist_ok=True)
    catalog = a.output / 'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    sources = [ROOT / 'Apps/ble_scanner.c', system / 'lib/PortableApps/src/adapter.c',
               *[system / 'lib/PortableApps/src' / name for name in (
                   'PortableNativeTimeSource.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
                   'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c')], catalog]
    includes = [sdk, system / 'lib/NativeApps/include', ROOT / 'lib/NativeApps/include',
                ROOT / 'lib/Bluetooth/include', ROOT / 'lib/Contexts/include', ROOT / 'Apps', system / 'Apps']
    return sources, profile['build_defines'], ['-I' + str(path) for path in includes], pins


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, help='Matching or archived target receipt directory')
    p.add_argument('--source-only', action='store_true', help='Qualify exact source checkouts without any target artifact')
    p.add_argument('--drivers', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--system-apps', type=Path, help='Current System source')
    p.add_argument('--runtime', type=Path, help='Current Runtime SDK; required with --system-apps')
    p.add_argument('--reader', type=Path, help='Pinned Reader display base/power SDK for --source-only')
    p.add_argument('--x4', type=Path, help='Pinned X4 display metrics/snapshot SDK for --source-only')
    for name in ('system', 'runtime', 'drivers', 'reader', 'x4'):
        p.add_argument('--' + name + '-revision', help='Exact clean source revision for --source-only')
    a = p.parse_args()
    if bool(a.system_apps) != bool(a.runtime):
        p.error('--system-apps and --runtime must be supplied together')
    if a.source_only == bool(a.build):
        p.error('Supply exactly one of --source-only or --build')
    if not a.source_only and any((a.reader, a.x4, a.system_revision, a.runtime_revision, a.drivers_revision, a.reader_revision, a.x4_revision)):
        p.error('Exact source selection options require --source-only')
    if a.source_only and not all((a.system_apps, a.runtime, a.reader, a.x4)):
        p.error('--source-only requires --system-apps, --runtime, --reader and --x4')
    for name in ('build', 'drivers', 'output', 'system_apps', 'runtime', 'reader', 'x4'):
        value = getattr(a, name)
        if value:
            setattr(a, name, value.resolve())
    a.output.mkdir(parents=True, exist_ok=True)
    profile = json.loads((ROOT / 'Apps/x4-ble-resident-profile.json').read_text())
    receipt = receipt_path = None
    source_pins = {}
    if a.source_only:
        system = a.system_apps
        sdk = a.output / 'sdk/include'
        sources, defines, include_flags, source_pins = source_recipe(a, profile, sdk)
    else:
        receipt_path = a.build / 'ble_scanner/x4-native-app.json'
        receipt = json.loads(receipt_path.read_text())
        original = receipt['compile_command']
        old_system = Path(next(x for x in original if x.endswith('/lib/PortableApps/src/adapter.c'))).parents[3]
        old_source = Path(next(x for x in original if x.endswith('/Apps/ble_scanner.c'))).parents[1]
        old_build = Path(next(x for x in original if x.endswith('/ble_scanner/catalog.c'))).parents[1]
        system = a.system_apps if a.system_apps else old_system
        sdk = a.build / 'sdk/include'
        if a.system_apps:
            for name, expected in receipt['sdk_sha256'].items():
                assert digest(sdk / name) == expected, ('archived SDK drift', name)
            for field in ('build_defines', 'requires', 'required_grants'):
                assert profile[field] == receipt[field], ('delivered profile drift', field)
            sdk = a.output / 'sdk/include'
            sdk.mkdir(parents=True, exist_ok=True)
            for folder in (a.build / 'sdk/include', system / 'lib/PortableApps/include',
                           a.runtime / 'sdk/app', ROOT / 'lib/Alarm/include'):
                for source in folder.glob('*.h'):
                    shutil.copyfile(source, sdk / source.name)
            shutil.copyfile(ROOT / 'lib/Bluetooth/include/RiscBluetoothSensorsV1.h', sdk / 'RiscBluetoothSensorsV1.h')
            for name in ('RiscDisplayOutputV1.h', 'RiscDisplayOutputPowerV1.h',
                         'RiscDisplayOutputMetricsV1.h', 'RiscDisplayOutputSnapshotV1.h'):
                shutil.copyfile(a.build / 'sdk/include' / name, sdk / name)
            for name in ('PaperPresentation.h', 'PaperFrame.h'):
                (sdk / name).unlink(missing_ok=True)
            shutil.copytree(system / 'lib/PortableApps/time', sdk.parent / 'time', dirs_exist_ok=True)
        else:
            for name, expected in receipt['compiled_dependencies_sha256'].items():
                label, relative = name.split('/', 1)
                base = {'Source': ROOT, 'System': system, 'CompiledSDK': sdk}[label]
                assert digest(base / relative) == expected, name

        def remap(path):
            path = Path(path)
            for old, new in ((old_build / 'sdk/include', sdk), (old_source, ROOT),
                             (old_system, system), (old_build, a.build)):
                if path.is_relative_to(old):
                    return new / path.relative_to(old)
            raise ValueError('Unmapped receipt compilation input: ' + str(path))

        sources = [remap(x) for x in original if x.endswith('.c')]
        defines = [x for x in original if x.startswith('-D')]
        assert defines == receipt['build_defines']
        include_flags = ['-I' + str(remap(x[2:])) for x in original if x.startswith('-I')]
    flags = defines + include_flags + ['-DBLE_PAPER_RENDER', '-DBLE_RESIDENT_RENDER']
    flags += ['-I' + str(x) for x in (system / 'Services/scene_profile', system / 'Services/text_input',
                                     system / 'sdk/app', a.drivers / 'sdk/driver')]
    # Import only profile data without writing into the read-only System source.
    sys.dont_write_bytecode = True
    sys.path.insert(0, str(system / 'scripts'))
    from build_scene_services import PROFILE_FLAGS

    results, commands, dependencies, model_results = [], [], {}, []
    roots = [('CompiledSDK', sdk), ('Source', ROOT), ('System', system),
             ('Drivers', a.drivers), ('Qualification', a.output)]
    if a.build:
        roots.append(('BaselineBuild', a.build))

    def compile_source(command, source, output):
        depfile = output.with_suffix('.d')
        cmd = [*command, '-MMD', '-MF', str(depfile), '-c', str(source), '-o', str(output)]
        commands.append(cmd)
        subprocess.run(cmd, check=True)
        for name in shlex.split(depfile.read_text().replace('\\\n', ' ').split(':', 1)[1]):
            path = Path(name).resolve()
            for label, base in roots:
                if path.is_relative_to(base):
                    key = label + '/' + str(path.relative_to(base))
                    current = digest(path)
                    assert dependencies.get(key, current) == current, ('input changed during qualification', key)
                    dependencies[key] = current
                    break
            else:
                raise ValueError('Unrecorded host dependency: ' + str(path))

    for sanitized in (False, True):
        out = a.output / str(int(sanitized))
        out.mkdir(exist_ok=True)
        extra = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *extra, *flags]
        if a.source_only:
            # Current logical controller test: compare LCD and paper actions
            # with every draw ready versus withheld, without substituting app code.
            model_obj = out / 'scanner-state.o'
            model_command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                             '-Wall', '-Wextra', '-Werror', *extra, *include_flags]
            compile_source(model_command, ROOT / 'tests/utility_scanner_state_test.c', model_obj)
            model_exe = out / 'scanner-state'
            link_model = [*model_command, str(model_obj), '-o', str(model_exe)]
            commands.append(link_model)
            subprocess.run(link_model, check=True)
            model_run = subprocess.run([str(model_exe)], check=True, capture_output=True, text=True,
                                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'), timeout=60)
            model_log = out / 'scanner-state.log'
            model_log.write_text(model_run.stdout + model_run.stderr)
            print('Scanner state', sanitized, model_run.stdout.strip(), flush=True)
            model_results.append(dict(sanitized=sanitized, passed=True, run_log_sha256=digest(model_log)))
        objects = []
        for host, entry in [('scene_host', 'ble_scene_driver_get'), ('text_input', 'ble_text_driver_get'),
                            ('scene_profile', 'ble_profile_driver_get')]:
            obj = out / (host + '.o')
            objects.append(obj)
            compile_source([*command, *(PROFILE_FLAGS['portrait-monochrome'] if host == 'scene_profile' else []),
                            '-Dt5_driver_get=' + entry],
                           system / 'Services' / host / ('profile.c' if host == 'scene_profile' else 'host.c'), obj)
        for index, source in enumerate([a.drivers / 'Drivers/ble_sensors/driver.c',
                                        ROOT / 'test/native_apps/ble_paper_renderer_test.c', *sources]):
            obj = out / ('input-' + str(index) + '.o')
            objects.append(obj)
            compile_source(command, source, obj)
        exe = out / 'ble-renderer'
        link = [*command, *map(str, objects), '-o', str(exe)]
        commands.append(link)
        subprocess.run(link, check=True)
        for scene, actions in scenarios().items():
            folder = out / scene
            folder.mkdir(exist_ok=True)
            for old_frame in folder.glob('frame-*.ppm'):
                old_frame.unlink()
            path = folder / 'actions.txt'
            path.write_text(''.join(' '.join(map(str, row)) + '\n' for row in actions))
            env = {k: v for k, v in os.environ.items() if not k.startswith('BLE_RENDER_')}
            env.update(ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
            if scene not in ('idle', 'resident-controls'):
                env['BLE_RENDER_SCAN'] = '1'
            if scene in ('home', 'name-navigation-home'):
                env['BLE_RENDER_HOME'] = '1'
            if scene == 'resident-controls':
                env['BLE_RENDER_CONTROLS'] = '1'
            if scene in ('nested-back', 'name-back-return'):
                env['BLE_RENDER_BACK'] = '1'
            if scene == 'enable-scan':
                env['BLE_RENDER_ENABLE'] = '1'
            if scene.startswith('name-'):
                env['BLE_RENDER_NAME'] = scene[5:]
            run = subprocess.run([str(exe), str(folder), str(path)], env=env, capture_output=True, text=True, timeout=60)
            (folder / 'run.log').write_text(run.stdout + run.stderr)
            if run.returncode:
                print('X4 resident', sanitized, scene, 'FAIL:', folder / 'run.log', flush=True)
                results.append(dict(sanitized=sanitized, scenario=scene, passed=False, returncode=run.returncode,
                                    actions_sha256=digest(path), run_log_sha256=digest(folder / 'run.log')))
                continue
            frames = list(folder.glob('frame-*.ppm'))
            assert frames
            header = b'P6\n480 800\n255\n'
            for frame in frames:
                data = frame.read_bytes()
                assert data.startswith(header) and len(data) == len(header) + 480 * 800 * 3
            print('X4 resident', sanitized, scene, 'PASS', len(frames), 'frames', flush=True)
            results.append(dict(sanitized=sanitized, scenario=scene, frames=len(frames), passed=True,
                                actions_sha256=digest(path), run_log_sha256=digest(folder / 'run.log')))
    if a.source_only:
        for label, path, revision in [('System', a.system_apps, a.system_revision),
                                      ('Runtime', a.runtime, a.runtime_revision),
                                      ('Drivers', a.drivers, a.drivers_revision),
                                      ('Reader', a.reader, a.reader_revision), ('X4', a.x4, a.x4_revision)]:
            assert exact(path, revision) == source_pins[label], ('source changed during qualification', label)
    record = dict(schema=3, scope='Exact-source host qualification without target artifact' if a.source_only else
                  ('Source-only host qualification' if a.system_apps else 'Matching target-receipt host qualification'),
                  source_pins=source_pins, profile_sha256=digest(ROOT / 'Apps/x4-ble-resident-profile.json'),
                  source_revision=git(ROOT, 'rev-parse', 'HEAD'), source_dirty=bool(git(ROOT, 'status', '--porcelain')),
                  source_manifest_version=json.loads((ROOT / 'Apps/ble_scanner.json').read_text())['version'],
                  selected_x4_version=json.loads((ROOT / 'Apps/x4-resident-versions.json').read_text())['ble_scanner'],
                  baseline_target_version=receipt['version'] if receipt else None,
                  system_revision=git(system, 'rev-parse', 'HEAD'),
                  system_dirty=bool(git(system, 'status', '--porcelain', '--untracked-files=no')),
                  runtime_revision=git(a.runtime, 'rev-parse', 'HEAD') if a.runtime else receipt['runtime_source_revision'],
                  driver_revision=git(a.drivers, 'rev-parse', 'HEAD'),
                  baseline_target_elf_sha256=receipt['elf_sha256'] if receipt else None,
                  baseline_target_receipt_sha256=digest(receipt_path) if receipt else None,
                  baseline_source_revision=receipt['source_revision'] if receipt else None,
                  baseline_system_revision=receipt['system_source_revision'] if receipt else None,
                  build_defines=defines, requires=(receipt or profile)['requires'], required_grants=(receipt or profile)['required_grants'],
                  physical_panel=[800, 480], logical_viewport=[480, 800], profile_flags=PROFILE_FLAGS['portrait-monochrome'],
                  host_commands=commands, compiled_dependencies_sha256=dependencies, runner_sha256=digest(__file__),
                  results=results, model_results=model_results, native_time_callback_checked_after_clean_app_return=True,
                  compiler=subprocess.check_output([os.environ.get('CC', 'cc'), '--version'], text=True).splitlines()[0],
                  sanitizers=['address', 'undefined'], asan_options='detect_leaks=0', ubsan_options='halt_on_error=1',
                  target_elf_compiled=False, hardware_verified=False)
    (a.output / 'summary.json').write_text(json.dumps(record, indent=2) + '\n')
    if any(not result['passed'] for result in results):
        raise SystemExit('Scanner host qualification failed; inspect summary.json and scenario run.log files')


if __name__ == '__main__':
    main()
