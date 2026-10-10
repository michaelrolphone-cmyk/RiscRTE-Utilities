#!/usr/bin/env python3
"""Real app loops: list/row state is independent of display availability.

RF reuses the selected receipt's flags and production shared adapter; scanner
and Audio use transport doubles. Run normal + ASan/UBSan. No hardware exercised.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--system-apps', type=Path, required=True)
p.add_argument('--sdk-include', type=Path, required=True)
p.add_argument('--rf-build-receipt', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
system, sdk, out = a.system_apps.resolve(), a.sdk_include.resolve(), a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
record = []
env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'}
includes = [ROOT/'Apps', ROOT/'lib/Bluetooth/include', system/'lib/PortableApps/include', system/'lib/NativeApps/include', system/'Apps', sdk]
base = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror']

def run(name, command, arguments=(), extra=None):
    folder = out/name
    folder.mkdir(exist_ok=True)
    exe = folder/'test'
    subprocess.run([*command, '-lm', '-o', str(exe)], check=True, timeout=120)
    result = subprocess.run([str(exe), *map(str, arguments)], capture_output=True, text=True, timeout=120, env={**env, **(extra or {})})
    (folder/'run.log').write_text(result.stdout+result.stderr)
    print(name, result.stdout if not result.returncode else result.stdout+result.stderr, end='', flush=True)
    result.check_returncode()
    record.append({'name': name, 'compile_command': command, 'result': result.stdout.strip()})

r = json.loads(a.rf_build_receipt.read_text())
cmd = r['compile_command']
old_system = str(Path(next(x for x in cmd if x.endswith('/lib/PortableApps/src/adapter.c'))).parents[3])
old_root = str(Path(next(x for x in cmd if x.endswith('/Apps/waterfall.c'))).parents[1])
rf_sources = [x.replace(old_system, str(system)).replace(old_root, str(ROOT)) for x in cmd if x.endswith('.c') and not x.endswith('/Apps/waterfall.c')]
serial = out/'serial.c'
serial.write_text('void rf_serial_start(void){} void rf_serial_line(const char*s){(void)s;} void rf_serial_finish(int a,int b){(void)a;(void)b;}\n')
for sanitized in (False, True):
    san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
    suffix = '-san' if sanitized else ''
    for name, source in [('spectrum', 'test/native_apps/utility_monitor_state_test.c'), ('scanner', 'tests/utility_scanner_state_test.c')]:
        run(name+suffix, [*base, *san, *['-I'+str(i) for i in includes], str(ROOT/source)])
    for paper in (False, True):
        flags = [x for x in r['build_defines'] if paper or x != '-DPORTABLE_DISPLAY_ROTATION=90']
        flags += ['-DRF_RENDER_RESIDENT', '-DRF_RENDER_ORDERED_INPUT', '-DRF_RENDER_MONITOR_STATE']
        if paper:
            flags += ['-DRF_RENDER_PAPER']
        for withheld in (False, True):
            name = 'rf-'+('paper' if paper else 'lcd')+'-'+('withheld' if withheld else 'ready')+suffix
            folder = out/name
            folder.mkdir(exist_ok=True)
            actions = folder/'actions.txt'
            scene = 'wait 4\nnav 32\nwait 4\nset poll_ms 20\nset frame_delay '+('10000' if withheld else '0')+'\nnav 8\nwait 2\nremember presents\nnav 8\nlabels 8\nwait 2\ntap 190 60\n'
            scene += 'tap 210 214\n'*6
            scene += 'check list_scroll eq 6\ncheck labels eq 8\ntap 201 94\ncheck label6 eq 0\ncheck undo_slot eq 6\ncheck list_scroll eq 6\ntap 201 94\ncheck label7 eq 0\ncheck undo_slot eq 7\ncheck list_scroll eq 4\ntap 100 94\ncheck page eq 2\ncheck edit_slot eq 4\nnav 1\nnav 1\nset jump 5000\nwait 2\nlabels 8\nwait 2\n'
            scene += 'tap 215 224\n'*5
            scene += 'check list_scroll eq 5\nlabels 5\nwait 2\ncheck list_scroll eq 2\nlabels 0\nwait 2\ncheck list_scroll eq 0\nlabels 8\nwait 2\ncheck list_scroll eq 0\n'
            if withheld:
                scene += 'same presents\ncheck frame_pending eq 1\ncheck busy_checks ge 20\n'
            scene += 'release_frame\nwait 4\ncheck frame_pending eq 0\ncheck last_page eq 0\ncheck last_list_scroll eq 0\nnav 1\n'
            scene = ''.join(line+'\n'+('wait 2\n' if line.startswith(('tap ', 'nav ')) else '') for line in scene.splitlines())
            actions.write_text(scene)
            run(name, [*base, *san, *flags, *['-I'+str(i) for i in includes], str(ROOT/'test/native_apps/rf_renderer_test.c'), str(serial), *rf_sources], [folder, actions], {'RF_RENDER_ASYNC': '1', 'RF_RENDER_NO_IMAGES': '1', 'RF_RENDER_EXPECT_LAUNCH': '1'})
(out/'evidence.json').write_text(json.dumps({'hardware_verified': False, 'rf_build_receipt': str(a.rf_build_receipt.resolve()), 'runs': record}, indent=2)+'\n')
