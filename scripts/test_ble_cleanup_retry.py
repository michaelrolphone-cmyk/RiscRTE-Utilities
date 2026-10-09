#!/usr/bin/env python3
"""Actual scanner/adapter/sensor-provider cleanup over a faulting HCI boundary."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SCENES = {
    'stop': [(3, 50, 210), (40, 50, 210)],
    'rescan': [(3, 50, 210), (40, 50, 210), (60, 50, 210), (100, 50, 210)],
    'back': [(3, 50, 210), (40, 20, 20)],
    'name': [(3, 50, 210), (30, 100, 80), (50, 175, 210)],
    'exit': [(3, 50, 210)],
    'quick-radio': [(3, 50, 210), (30, 120, 5), (31, 120, 60),
                    (32, 120, 140), (70, 190, 140), (90, 120, 222)],
    'idle-sleep': [(3, 50, 210)],
}


def replace(source, old, new):
    assert source.count(old) == 1, old
    return source.replace(old, new)


def fixture(out):
    source = (ROOT / 'test/native_apps/ble_cleanup_renderer_test.c').read_text()
    source = replace(source, 'static void save_frame(void)',
                     '#include "ble_cleanup_probe.h"\nstatic void save_frame(void)')
    names = re.findall(r'static [^\n{]+ ((?:fake_|radio_|wifi_)\w+)\([^\n]*?\)\{', source)
    for name in names:
        if name in ('fake_diag', 'fake_yield', 'fake_release', 'radio_close'):
            continue
        source, count = re.subn(r'(static [^\n{]+ ' + name + r'\([^\n]*?\)\{)',
                                r'\1probe_io();', source)
        assert count == 1, name
    source = replace(source, 'static void fake_yield(uint32_t n){',
                     'static void fake_yield(uint32_t n){probe_yield(n);')
    source = replace(source, 'static int32_t radio_close(void*c,uint64_t token){',
                     'static int32_t radio_close(void*c,uint64_t token){if(probe_refuse(1))return -1;')
    source = replace(source, 'static bool fake_release(risc_runtime_capability_v1*g){',
                     'static bool fake_release(risc_runtime_capability_v1*g){'
                     'if(g->api==sensor_driver->capability){if(probe_refuse(2))return false;}'
                     'else probe_io();')
    source = replace(source, 'save_frame();return true;',
                     '(void)save_frame;for(unsigned y=0;y<240;y++)for(unsigned x=240;x<244;x++)'
                     'assert(pixels[y*244+x]==0xa5a5);return true;')
    source = replace(source, 'assert(!"Unexpected hardware sleep in audit");return 0;',
                     'probe_io();assert(getenv("BLE_PROBE_SLEEP")&&!radio_owned);'
                     'probe_sleeps++;stop_poll=polls;return 0;')
    source = replace(source, 'polls++;return true;',
                     'polls++;if(getenv("BLE_PROBE_SLEEP")&&polls==30)ticks+=70000;return true;')
    source = replace(source, 'assert(app_module_init()==0);app_main();app_module_fini();',
                     'probe_init();if(setjmp(probe_escape)){probe_finish(true);return 0;}'
                     'assert(app_module_init()==0);app_main();app_module_fini();probe_finish(false);')
    source = replace(source, 'radio_sends==5&&radio_closes==radio_claims',
                     'radio_sends==5*radio_claims&&radio_closes==radio_claims')
    source = replace(source, 'if(getenv("BLE_RENDER_BACK"))assert(',
                     'if(getenv("BLE_PROBE_RESCAN"))assert(radio_claims==2);\n'
                     'if(getenv("BLE_RENDER_BACK"))assert(')
    (out / 'fixture.c').write_text(source)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--system-apps', required=True, type=Path)
    p.add_argument('--drivers', required=True, type=Path)
    p.add_argument('--presentation-sdk', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--app-source', type=Path, default=ROOT / 'Apps/ble_scanner.c')
    p.add_argument('--scene', choices=SCENES)
    p.add_argument('--normal-only', action='store_true')
    a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    system = a.system_apps.resolve(); drivers = a.drivers.resolve()
    fixture(out)
    (out / 'catalog.c').write_text('#include "PortableApps.h"\n'
        'const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\n'
        'const unsigned portable_catalog_count=0;\n')
    flags = ['-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME',
        '-DPORTABLE_RADIO_SESSION', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_FORCE_FULL_FRAMES',
        '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_INPUT_NAVIGATION_LOCAL',
        '-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_QUICK_RADIOS', '-DPORTABLE_APP_SLEEP_LOCAL',
        '-DPORTABLE_RETURN_APP="springboard.elf"']
    includes = [a.presentation_sdk.resolve(), ROOT / 'Apps', ROOT / 'lib/Bluetooth/include', ROOT / 'test/native_apps',
                system / 'lib/PortableApps/include', system / 'lib/NativeApps/include', drivers / 'sdk/driver']
    sources = [drivers / 'Drivers/ble_sensors/driver.c', out / 'fixture.c', a.app_source,
               system / 'lib/PortableApps/src/adapter.c', out / 'catalog.c']
    sources += [system / 'lib/PortableApps/src' / s for s in
                ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')]
    results = []
    for san in ([False] if a.normal_only else [False, True]):
        exe = out / ('probe-san' if san else 'probe')
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                   *flags, *['-I' + str(i) for i in includes]]
        if san:
            command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
        command += [*map(str, sources), '-o', str(exe)]
        subprocess.run(command, check=True)
        (out / (exe.name + '-command.json')).write_text(json.dumps(command, indent=2) + '\n')
        for scene, actions in SCENES.items():
            if a.scene and scene != a.scene:
                continue
            action_file = out / (scene + '-actions.txt')
            action_file.write_text(''.join(f'{n} {x} {y}\n' for n, x, y in actions))
            for operation in ('close', 'grant'):
                for failures in (0, 1, 3, -1):
                    label = f'{scene}-{operation}-{failures}-san{int(san)}'
                    env = {k: v for k, v in os.environ.items() if not k.startswith(('BLE_RENDER_', 'BLE_PROBE_'))}
                    env.update(ASAN_OPTIONS='detect_leaks=0', BLE_PROBE_OPERATION=operation,
                               BLE_PROBE_FAILURES=str(failures), BLE_RENDER_SCAN='1')
                    if scene == 'idle-sleep': env['BLE_PROBE_SLEEP'] = '1'
                    if scene == 'rescan': env['BLE_PROBE_RESCAN'] = '1'
                    proc = subprocess.run([str(exe), str(out), str(action_file)], env=env,
                                          text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
                    (out / (label + '.log')).write_text(proc.stdout)
                    assert proc.returncode == 0, label + '\n' + proc.stdout
                    results.append({'case': label, 'passed': True, 'log': label + '.log'})
                    print(label + ': PASS', flush=True)
    (out / 'results.json').write_text(json.dumps({'schema': 1, 'results': results,
        'physical_qualification': False, 'production_sources': list(map(str, sources))}, indent=2) + '\n')


if __name__ == '__main__': main()
