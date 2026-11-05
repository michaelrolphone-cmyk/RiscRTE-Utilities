#!/usr/bin/env python3
"""Qualify actual LoRa app/adapter native pixels, actions, custody and legacy.

Rasters are bounded to the last image of each of the eight real app pages.
All attempts retain their logs, SHA-256 receipts, exit status and per-frame hashes.
"""
import argparse
import hashlib
import io
import tarfile
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
SIZES = ((160, 160), (240, 240), (320, 180), (410, 502), (502, 410))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--system-apps', required=True, type=Path)
    p.add_argument('--output', type=Path, default=ROOT / 'build/lora-native')
    p.add_argument('--size', action='append')
    p.add_argument('--scene', action='append')
    p.add_argument('--normal-only', action='store_true')
    p.add_argument('--skip-legacy', action='store_true')
    p.add_argument('--no-rasters', action='store_true')
    p.add_argument('--quick', action='store_true', help='Also compile native QuickActions composition')
    a = p.parse_args()
    system = a.system_apps.resolve()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    attempt = out / time.strftime('run-%Y%m%d-%H%M%S', time.gmtime())
    attempt.mkdir()
    catalog = attempt / 'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    adapter = attempt / 'adapter.c'
    adapter.write_text(f'#include {json.dumps(str(system / "lib/PortableApps/src/adapter.c"))}\n' + '''
#ifdef PORTABLE_LORA_NATIVE_UI
bool ln_diag_poll(unsigned wait,t5_app_input_t*out){return poll(out,wait);}
bool ln_diag_drain(void){return portable_paper_frame_drain();}
bool ln_diag_visible(void){return lora_native_visible;}
uint64_t ln_diag_generation(void){return lora_native_generation;}
unsigned ln_diag_owner(void){return native_token_owner;}
bool ln_diag_failed(void){return failed;}
#endif
''')
    common = ['PORTABLE_NOVA_UI', 'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_RADIO_SESSION', 'PORTABLE_ALARM_CLIENT', 'PORTABLE_FORCE_FULL_FRAMES', 'PORTABLE_INPUT_NAVIGATION', 'PORTABLE_INPUT_NAVIGATION_LOCAL', 'PORTABLE_ASYNC_RGB565', 'PORTABLE_NATIVE_CUSTODY_FENCE', 'LORA_RETURN_APP="springboard.elf"']
    includes = [ROOT / 'Apps', system / 'Apps', system / 'lib/PortableApps/include', ROOT / 'lib/Alarm/include', system / 'lib/NativeApps/include']
    results = []
    env_base = {k: v for k, v in os.environ.items() if not k.startswith('LORA_')}
    env_base.update(ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')

    def record():
        (attempt / 'results.json').write_text(json.dumps(results, indent=2) + '\n')

    def build(sanitized, native=True, base=None):
        flags = [f'-D{x}' for x in common]
        selected_system = system
        selected_adapter = adapter
        if a.quick:
            flags += ['-DPORTABLE_QUICK_ACTIONS', '-DPORTABLE_QUICK_RADIOS', '-DPORTABLE_QUICK_NATIVE']
        if native:
            flags += ['-DPORTABLE_LORA_NATIVE_UI', '-DPORTABLE_NATIVE_CANVAS']
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
        exe = attempt / f'{"native" if native else "legacy"}-{int(sanitized)}{("-base" if base else "")}'
        extra_includes = []
        if base:
            baseline = attempt / 'base-app'
            baseline.mkdir(exist_ok=True)
            (baseline / 'lora_messages.c').write_bytes(subprocess.check_output(['git', 'show', '95f39010ed89d76fc583f1c09a111cfc3cf21f41:Apps/lora_messages.c'], cwd=ROOT))
            extra_includes.append(baseline)
            selected_system = attempt/'base-system'
            if not selected_system.exists():
                selected_system.mkdir()
                archive = subprocess.check_output(['git', 'archive', '4b11af9e44bc7ec71a7a63d14d898b6141c616b4'], cwd=system)
                with tarfile.open(fileobj=io.BytesIO(archive)) as tf:
                    tf.extractall(selected_system, filter='data')
            selected_adapter = selected_system/'lib/PortableApps/src/adapter.c'
        test = ROOT / 'test/native_apps/lora_native_test.c'
        sources = [test, selected_adapter, catalog]
        if a.quick:
            sources += [selected_system/'lib/PortableApps/src'/name for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')]
        selected_includes = [selected_system / i.relative_to(system) if i.is_relative_to(system) else i for i in includes]
        cmd = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags, *['-I' + str(i) for i in extra_includes + selected_includes], *map(str, sources)]
        if native:
            cmd += ['-Wl,--wrap=portable_nova_native_text', '-Wl,--wrap=portable_nova_native_icon']
        cmd += ['-o', str(exe)]
        source_paths = [test, (attempt/'base-app/lora_messages.c') if base else ROOT/'Apps/lora_messages.c', ROOT/'Apps/lora_messages_native_layout.h', ROOT/'Apps/lora_messages_native_view.inc', selected_system/'lib/PortableApps/src/adapter.c']
        if native:
            source_paths += [system/'lib/PortableApps/src/lora_native_input.inc', system/'lib/PortableApps/include/PortableLoRaNative.h']
        source_hashes = {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in source_paths}
        r = subprocess.run(cmd, capture_output=True, text=True)
        assert source_hashes == {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in source_paths}, 'Source changed during compilation; rerun against frozen inputs'
        (attempt/(exe.name+'-sources.json')).write_text(json.dumps(source_hashes, indent=2)+'\n')
        (attempt / (exe.name + '-build.log')).write_text(' '.join(cmd) + '\n' + r.stdout + r.stderr)
        if r.returncode:
            print(r.stdout + r.stderr)
            r.check_returncode()
        return exe

    def run(exe, sanitized, w, h, name, actions, checks, delay=3, legacy=False):
        folder = attempt / ('legacy' if legacy else 'native') / str(int(sanitized)) / f'{w}x{h}' / name
        folder.mkdir(parents=True)
        path = folder / 'actions.txt'
        path.write_text(''.join(f'{n} {x} {y}\n' for n, x, y in actions))
        env = {**env_base, 'LORA_DELAY': str(delay), 'LORA_CAPTURE': '0' if sanitized or a.no_rasters else '1', **{k: str(v) for k, v in checks.items()}}
        start = time.monotonic()
        try:
            r = subprocess.run([str(exe), str(folder), str(path), str(w), str(h)], env=env, capture_output=True, text=True, timeout=90)
            code, log = r.returncode, r.stdout + r.stderr
        except subprocess.TimeoutExpired as e:
            code, log = 124, f'Timeout after 90 seconds\n{e.stdout!r}\n{e.stderr!r}'
        (folder / 'run.log').write_text(log)
        images = []
        for frame in sorted(folder.glob('page-*.ppm')):
            data = frame.read_bytes()
            header = f'P6\n{w} {h}\n255\n'.encode()
            assert data.startswith(header) and len(data) == len(header) + w * h * 3
            assert len(set(data[len(header):])) > (1 if checks.get('LORA_PAPER') else 3), 'Blank raster'
            png = frame.with_suffix('.png')
            def chunk(kind, payload):
                return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))
            rgb = data[len(header):]
            filtered = b''.join(b'\0' + rgb[y*w*3:(y+1)*w*3] for y in range(h))
            compressed = zlib.compress(filtered)
            assert zlib.decompress(compressed) == filtered
            png.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', compressed) + chunk(b'IEND', b''))
            images.append({'name': png.name, 'sha256': hashlib.sha256(png.read_bytes()).hexdigest(), 'bytes': png.stat().st_size, 'ppm_sha256': hashlib.sha256(data).hexdigest(), 'ppm_bytes': len(data)})
            frame.unlink()
        assert len(images) <= 8
        receipt = {'size': [w, h], 'case': name, 'sanitized': sanitized, 'legacy': legacy, 'exit': code, 'seconds': round(time.monotonic() - start, 3), 'checks': checks, 'rasters': images, 'log_sha256': hashlib.sha256(log.encode()).hexdigest()}
        (folder / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        results.append(receipt)
        record()
        if code:
            print(f'FAIL {folder}\n{log}', flush=True)
            return False
        return True

    ok = True
    for san in (False,) if a.normal_only else (False, True):
        exe = build(san)
        native_exe = exe
        ok &= run(exe, san, 1024, 160, "label-boundary", [], {"LORA_LABEL_BOUNDARY": "1"})
        geometry = attempt/f'geometry-{int(san)}'
        geometry_flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if san else []
        geometry_build = subprocess.run([os.environ.get('CC','cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *geometry_flags, '-I'+str(ROOT/'Apps'), str(ROOT/'tests/lora_native_layout_test.c'), '-o', str(geometry)], capture_output=True, text=True)
        (attempt/(geometry.name+'-build.log')).write_text(geometry_build.stdout+geometry_build.stderr)
        geometry_build.check_returncode()
        geometry_run = subprocess.run([str(geometry)], env=env_base, capture_output=True, text=True)
        (attempt/(geometry.name+'.log')).write_text(geometry_run.stdout+geometry_run.stderr)
        geometry_run.check_returncode()
        for w, h in SIZES:
            if a.size and f'{w}x{h}' not in a.size:
                continue
            layout = json.loads(subprocess.check_output([str(exe), '--layout', 'unused', str(w), str(h)], text=True, env=env_base))
            def at(n, key):
                x, y, rw, rh = layout[key]
                return n, x + rw // 2, y + rh // 2
            def seq(*keys):
                return [at(20 + 20 * i, key) for i, key in enumerate(keys)]
            def check(**kw):
                return {f'LORA_{key}': str(value) for key, value in kw.items()}
            profile = {'LORA_PROFILE': '1'}
            send = seq('home0', 'footer0', 'key1', 'key34', 'footer1')
            cases = {
                'idle': ([], check(EXPECT_PAGE=0, EXPECT_CHOICE=0, EXPECT_CONFIGURES=0)),
                'back': (seq('back'), check(EXPECT_LAUNCHES=1)),
                'compose': (seq('home0'), check(EXPECT_PAGE=1)),
                'keyboard': (seq('home0', 'footer0'), check(EXPECT_PAGE=2)),
                'keyboard-edit': (seq('home0', 'footer0', 'key1', 'key2', 'key33', 'key34'), check(EXPECT_PAGE=1, EXPECT_DRAFT='a')),
                'keyboard-page': (seq('home0', 'footer0', 'key32'), check(EXPECT_PAGE=2, EXPECT_KEY_PAGE=0)),
                'keyboard-cancel': (seq('home0', 'footer0', 'key1', 'back'), check(EXPECT_PAGE=1, EXPECT_DRAFT='')),
                'nested-back': (seq('home0', 'footer0', 'back', 'back', 'home1'), check(EXPECT_PAGE=4)),
                'radio-picker': (seq('home3'), check(EXPECT_PAGE=7, EXPECT_CHOICE=0)),
                'radio-save': (seq('home3', 'picker2'), check(EXPECT_PAGE=0, EXPECT_CHOICE=3, EXPECT_WRITES=1, VERIFY_STORED=1)),
                'radio-save-fail': (seq('home3', 'picker2'), check(SAVE_FAIL=1, EXPECT_PAGE=0, EXPECT_CHOICE=3, EXPECT_SAVE=2)),
                'radio-session-only': (seq('home3', 'picker2'), check(NO_MEDIA=1, EXPECT_PAGE=0, EXPECT_CHOICE=3, EXPECT_SAVE=1)),
                'radio-fixed-unavailable': (seq('home3', 'picker0'), check(FIXED=1, EXPECT_PAGE=7, EXPECT_CHOICE=0, EXPECT_WRITES=0)),
                'radio-fixed-select': (seq('home3', 'picker1'), check(FIXED=1, EXPECT_PAGE=0, EXPECT_CHOICE=2, EXPECT_WRITES=1)),
                'radio-picker-back': (seq('home3', 'back'), check(EXPECT_PAGE=0, EXPECT_CHOICE=0)),
                'no-radio': (seq('home3'), check(NO_RADIO=1, EXPECT_PAGE=7, EXPECT_NOTICE='unavailable')),
                'old-radio-provider': (seq('home3'), check(OLD_API=1, EXPECT_PAGE=7, EXPECT_NOTICE='Update driver')),
                'legacy-profile': ([], check(LEGACY=1, EXPECT_PAGE=0, EXPECT_CHOICE=0)),
                'rf': (seq('home3'), {**profile, **check(EXPECT_PAGE=3, EXPECT_GLYPH7=1)}),
                'rf-next': (seq('home3', 'footer0'), {**profile, **check(EXPECT_PAGE=3, EXPECT_RF_PAGE=1)}),
                'rf-previous': (seq('home3', 'footer0', 'footer0'), {**profile, **check(EXPECT_PAGE=3, EXPECT_RF_PAGE=0)}),
                'rf-apply': (seq('home3', 'footer1'), {**profile, **check(EXPECT_PAGE=0, EXPECT_WRITES=1, VERIFY_STORED=1)}),
                'rf-save-fail': (seq('home3', 'footer1'), {**profile, **check(SAVE_FAIL=1, EXPECT_PAGE=0, EXPECT_SAVE=2)}),
                'rf-invalid': (seq('home3', 'footer1'), check(CHOICE=1, EXPECT_PAGE=6, EXPECT_WRITES=0)),
                'rf-error-return': (seq('home3', 'footer1', 'full_footer'), check(CHOICE=1, EXPECT_PAGE=3)),
                'rf-keyboard': (seq('home3', 'rf0'), {**profile, **check(EXPECT_PAGE=2)}),
                'rf-out-of-range': (seq('home3', 'rf0', *(['key33']*9), 'key16', 'key34'), {**profile, **check(EXPECT_PAGE=2, EXPECT_NOTICE='902000000', EXPECT_WRITES=0, EXPECT_FREQUENCY=915000000)}),
                'rf-picker': (seq('home3', 'radio'), {**profile, **check(EXPECT_PAGE=7)}),
                'rf-picker-back': (seq('home3', 'radio', 'back'), {**profile, **check(EXPECT_PAGE=3)}),
                'history-empty': (seq('home1'), check(EXPECT_PAGE=4, EXPECT_COUNT=0)),
                'listen-stop': (seq('home2', 'home2'), {**profile, **check(EXPECT_PAGE=0, EXPECT_LISTENS=1, EXPECT_CONFIGURES=1)}),
                'listen-no-rf': (seq('home2'), check(CHOICE=1, EXPECT_CONFIGURES=0, EXPECT_NOTICE='matching RF')),
                'airplane': (seq('home2'), {**profile, **check(AIRPLANE=1, EXPECT_LISTENS=0, EXPECT_CONFIGURES=0, EXPECT_NOTICE='Airplane')}),
                'send': (send, {**profile, **check(EXPECT_PAGE=1, EXPECT_SENDS=1, EXPECT_COUNT=1, EXPECT_STATUS=1, EXPECT_CLOCK=1, EXPECT_DRAFT='a')}),
                'send-no-rtc': (send, {**profile, **check(NO_RTC=1, EXPECT_SENDS=1, EXPECT_COUNT=1, EXPECT_STATUS=1, EXPECT_CLOCK=0)}),
                'send-failed': (send, {**profile, **check(SEND_FAIL=1, EXPECT_SENDS=1, EXPECT_COUNT=1, EXPECT_STATUS=2, EXPECT_DRAFT='a')}),
                'send-cancel': (send + [at(130, 'footer1')], {**profile, **check(SEND_BUSY=1, EXPECT_SENDS=1, EXPECT_COUNT=1, EXPECT_STATUS=2)}),
                'reentry-send': (send, {**profile, **check(REENTER=1, EXPECT_SENDS=1, EXPECT_COUNT=1)}),
                'reentry-choice': (seq('home3', 'picker2'), check(REENTER=1, EXPECT_PAGE=0, EXPECT_CHOICE=3, EXPECT_WRITES=1)),
                'send-history': (send + [at(130, 'back'), at(150, 'home1'), at(170, 'history0')], {**profile, **check(EXPECT_PAGE=5, EXPECT_SENDS=1, EXPECT_COUNT=1, EXPECT_STATUS=1)}),
                'receive-detail': (seq('home2', 'home2', 'home1', 'history0'), {**profile, **check(RX_COUNT=1, EXPECT_PAGE=5, EXPECT_LISTENS=2, EXPECT_COUNT=1, EXPECT_STATUS=3, EXPECT_CLOCK=1)}),
                'receive-no-rtc': (seq('home2', 'home2', 'home1', 'history0'), {**profile, **check(NO_RTC=1, RX_COUNT=1, EXPECT_PAGE=5, EXPECT_LISTENS=2, EXPECT_COUNT=1, EXPECT_CLOCK=0)}),
                'history-ring': ([at(20, 'home2'), at(1000, 'home2'), at(1020, 'home1'), at(1040, 'footer1')], {**profile, **check(RX_COUNT=20, EXPECT_PAGE=4, EXPECT_LISTENS=21, EXPECT_COUNT=16, EXPECT_HISTORY_PAGE=1)}),
                'previsible': ([at(2, 'home0')], check(FORCE_TOUCH=1, EXPECT_PAGE=0)),
                'held-previsible': ([at(i, 'home0') for i in range(2, 70)], check(FORCE_TOUCH=1, EXPECT_PAGE=0)),
                'compose-first-pixel': ([(20, *layout['home0'][:2])], check(EXPECT_PAGE=1)),
                'compose-last-pixel': ([(20, layout['home0'][0]+layout['home0'][2]-1, layout['home0'][1]+layout['home0'][3]-1)], check(EXPECT_PAGE=1)),
                'compose-right-outside': ([(20, layout['home0'][0]+layout['home0'][2], layout['home0'][1])], check(EXPECT_PAGE=0)),
                'compose-slide-history': ([at(20, 'home0'), at(21, 'home1')], check(EXPECT_PAGE=0)),
                'keyboard-gap': (seq('home0', 'footer0')+[(60, layout['key1'][0]+layout['key1'][2], layout['key1'][1]+2), at(80, 'key34')], check(EXPECT_PAGE=1, EXPECT_DRAFT='')),
                'rf-row-gap': (seq('home3')+[(60, layout['rf0'][0]+2, layout['rf0'][1]+layout['rf0'][3])], {**profile, **check(EXPECT_PAGE=3)}),
                'radio-row-gap': (seq('home3')+[(60, layout['picker0'][0]+2, layout['picker0'][1]+layout['picker0'][3])], check(EXPECT_PAGE=7, EXPECT_CHOICE=0)),
                'footer-gap': ([(20, w // 2, h - 10)], check(EXPECT_PAGE=0, EXPECT_CONFIGURES=0)),
            }
            rf_keys = ['home3']
            for i, value in enumerate(('915000000', '125000', '7', '5', '0', '8')):
                if i == 3:
                    rf_keys.append('footer0')
                rf_keys.append('rf'+str(i%3))
                rf_keys.extend('key'+str(ord(c)-32) for c in value)
                rf_keys.append('key34')
            rf_keys.append('footer1')
            cases['rf-six-values'] = (seq(*rf_keys), check(CHOICE=1, EXPECT_PAGE=0, EXPECT_WRITES=1, VERIFY_STORED=1))
            cases['rf-session-only'] = (seq('home3', 'picker2', *rf_keys), check(NO_MEDIA=1, EXPECT_PAGE=0, EXPECT_SAVE=1, EXPECT_WRITES=0))
            compose_pages = (240 + layout['compose_step'] - 1) // layout['compose_step']
            detail_pages = (255 + layout['detail_step'] - 1) // layout['detail_step']
            cases['compose-all-bytes'] = (seq('home0') + [at(60 + 20*i, 'body') for i in range(compose_pages)], check(LONG_DRAFT=1, EXPECT_PAGE=1, EXPECT_COMPOSE_BYTES=240, EXPECT_COMPOSE_OFFSET=0))
            cases['detail-all-bytes'] = (seq('home2', 'home2', 'home1', 'history0') + [at(120 + 20*i, 'footer1') for i in range(detail_pages)] + [at(120 + 20*detail_pages + 20*i, 'footer0') for i in range(detail_pages)], {**profile, **check(RX_COUNT=1, EXPECT_PAGE=5, EXPECT_LISTENS=2, EXPECT_DETAIL_BYTES=255, EXPECT_DETAIL_OFFSET=0)})
            for name, (actions, checks) in cases.items():
                if a.scene and name not in a.scene:
                    continue
                delay = 120 if 'previsible' in name else 3
                ok &= run(exe, san, w, h, name, actions, checks, delay)
                if name not in ('previsible', 'held-previsible', 'rf-six-values', 'rf-session-only'):
                    ok &= run(exe, san, w, h, name + '-slow', actions, checks, 120)
            if not a.scene or 'temporal' in a.scene:
                for mode in range(13):
                    ok &= run(exe, san, w, h, f'temporal-{mode}', [], check(TEMPORAL=mode), 30)
        if not a.skip_legacy and (not a.scene or 'legacy' in a.scene):
            exe = build(san, False)
            base_exe = build(san, False, base=True)
            legacy_cases = {
                'idle': ([], check(EXPECT_PAGE=0)),
                'compose': ([(20, 100, 100)], check(EXPECT_PAGE=1)),
                'radio-picker': ([(20, 175, 200)], check(EXPECT_PAGE=7)),
                'rf': ([(20, 175, 200)], {**profile, **check(EXPECT_PAGE=3)}),
                'listen-stop': ([(20, 50, 200), (50, 50, 200)], {**profile, **check(EXPECT_LISTENS=1)}),
                'send-history': ([(20, 100, 100), (40, 50, 210), (60, 53, 87), (80, 200, 190), (100, 170, 210), (130, 20, 20), (150, 100, 150), (170, 100, 55)], {**profile, **check(EXPECT_PAGE=5, EXPECT_SENDS=1, EXPECT_COUNT=1)}),
            }
            for name, (actions, checks) in legacy_cases.items():
                candidate_ok = run(exe, san, 240, 240, name, actions, checks, legacy=True)
                baseline_ok = run(base_exe, san, 240, 240, name+'-base', actions, checks, legacy=True)
                ok &= candidate_ok and baseline_ok
                if candidate_ok and baseline_ok:
                    folder = attempt/'legacy'/str(int(san))/'240x240'
                    assert (folder/name/'frame-hashes.tsv').read_bytes() == (folder/(name+'-base')/'frame-hashes.tsv').read_bytes(), f'Feature-off raster/timing drift: {name}'
                    assert (folder/name/'run.log').read_bytes() == (folder/(name+'-base')/'run.log').read_bytes(), f'Feature-off semantic drift: {name}'
        if not a.quick and (not a.scene or 'paper' in a.scene):
            legacy_exe = build(san, False)
            baseline_exe = build(san, False, base=True)
            paper_cases = {
                'idle': ([], check(EXPECT_PAGE=0)),
                'compose': ([(20, 220, 380)], check(EXPECT_PAGE=1)),
                'history': ([(20, 220, 430)], check(EXPECT_PAGE=4)),
                'rf': ([(20, 295, 480)], {**profile, **check(EXPECT_PAGE=3)}),
            }
            for name, (actions, checks) in paper_cases.items():
                checks = {**checks, 'LORA_PAPER': '1'}
                passed = []
                for label, binary in [('native-off', legacy_exe), ('native-on', native_exe), ('baseline', baseline_exe)]:
                    passed.append(run(binary, san, 480, 800, 'paper-'+name+'-'+label, actions, checks, delay=30, legacy=True))
                ok &= all(passed)
                if all(passed):
                    folder = attempt/'legacy'/str(int(san))/'480x800'
                    paths = [folder/('paper-'+name+'-'+label) for label in ('native-off', 'native-on', 'baseline')]
                    assert len({(x/'frame-hashes.tsv').read_bytes() for x in paths}) == 1, f'Paper raster/timing drift: {name}'
                    assert len({(x/'run.log').read_bytes() for x in paths}) == 1, f'Paper semantic drift: {name}'
    sources = [ROOT / 'Apps/lora_messages.c', ROOT / 'Apps/lora_messages_native_layout.h', ROOT / 'Apps/lora_messages_native_view.inc', ROOT / 'test/native_apps/lora_native_test.c', Path(__file__), system / 'lib/PortableApps/src/adapter.c', system / 'lib/PortableApps/src/lora_native_input.inc']
    (attempt / 'source-hashes.json').write_text(json.dumps({str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in sources}, indent=2) + '\n')
    print(f'LoRa native qualification: {sum(r["exit"] == 0 for r in results)}/{len(results)} passed; {attempt}', flush=True)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
