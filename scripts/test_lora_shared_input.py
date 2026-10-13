#!/usr/bin/env python3
"""Exercise production LoRa/shared keyboard under deployed Watch feature flags.

Physical services are deterministic fixtures, not a device/RF qualification.
Only LoRa's app source is included verbatim by the fixture for state observation;
all behavior still runs through app_main and production adapter/host providers.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def tap(x, y, gate='app', delay=160, up=30):
    return [(delay, gate, 'down', x, y), (up, gate, 'up', x, y)]


def nav(kind='back', gate='app', delay=200):
    return [(delay, gate, kind, 0, 0)]


def capture(gate='app', after=False, delay=220):
    return [(delay, gate, 'capture', int(after), 0)]


def key(ch, delay=80):
    # Actual compact-color scene-host key geometry, not an app-local keyboard.
    rows = ['QWERTYUIOP', 'ASDFGHJKL', '\x01ZXCVBNM\x02', ', .\x03']
    units = lambda c: 3 if c in '\x01\x02' else 12 if c == ' ' else 4 if c == '\x03' else 2
    for row, chars in enumerate(rows):
        if ch not in chars:
            continue
        xq = 320 + (20 - sum(map(units, chars))) * 104
        for c in chars:
            wq = units(c) * 208
            x, w = (xq + 30) // 10, (wq - 45) // 10
            if c == ch:
                return tap((2 * x + w) // 4, 97 + row * 30, 'text', delay, 25)
            xq += wq
    raise ValueError(ch)


def number(s, initial_length):
    result = key('\x02') * initial_length + key('\x01')
    for ch in s:
        if ch == '-':
            result += tap(35, 127, 'text', 80, 25)
        else:
            result += tap(27 + '1234567890'.index(ch) * 21, 97, 'text', 80, 25)
    return result


def scenarios():
    compose = tap(100, 103) + tap(55, 210)
    rf = tap(175, 203)
    done = key('\x03', 220)
    cancel = nav('back', 'text')
    home = nav('home', 'text')
    edit = tap(55, 210)
    cases = {}
    def add(name, actions, draft='', opens=1, sends=0, **env):
        cases[name] = (actions, {'LORA_EXPECT_DRAFT': draft, 'LORA_EXPECT_LENGTH': str(len(draft)),
                                 'LORA_EXPECT_OPENS': str(opens), 'LORA_EXPECT_SENDS': str(sends),
                                 'LORA_EXPECT_HISTORY': str(sends), **{k: str(v) for k, v in env.items()}})
    add('idle', [], opens=0)
    add('missing-radio', tap(50,203)+capture(), opens=0)
    add('draft-done', compose + key('Q') + done + capture(), 'Q')
    add('draft-cancel', compose + key('Q') + cancel + capture())
    add('draft-header-back', compose + key('Q') + tap(20, 20, 'text') + capture())
    add('draft-home', compose + key('Q') + home)
    add('draft-header-home', compose + key('Q') + tap(190, 20, 'text'))
    add('draft-repeat', compose + key('Q') + done + edit + key('W') + cancel + edit + key('E') + done + capture(), 'QE', opens=3)
    add('send-history', compose + key('Q') + done + tap(175, 210) + tap(20, 20) + tap(100, 150) + tap(100, 55) + capture(), 'Q', sends=1)
    add('send-cancel', compose + key('Q') + done + tap(175, 210) + tap(175, 210) + tap(20, 20) + tap(100, 150) + tap(100, 55) + capture(), 'Q', sends=1)
    add('listen-edit', tap(50, 203) + compose + key('Q') + done + capture(), 'Q')
    add('missing-host', compose + capture(), opens=0)
    add('old-host', compose + capture(), opens=0)
    add('retained', compose + key('Q') + cancel)
    add('retained-listen', tap(50, 203) + compose + key('Q') + cancel)
    add('missing-reopen', compose + key('Q') + done + edit + capture(), 'Q')
    add('old-reopen', compose + key('Q') + done + edit + capture(), 'Q')
    add('loss-open', compose)
    add('loss-poll', compose)
    add('loss-close', compose + key('Q') + cancel)
    add('delayed-done', compose + key('Q') + done + capture(), 'Q', LORA_TEST_DELAY_MS=450)
    add('delayed-back', compose + key('Q') + cancel + capture(), LORA_TEST_DELAY_MS=450)
    add('delayed-home', compose + key('Q') + home, LORA_TEST_DELAY_MS=450)
    rapid = sum((key('Q', 5) for _ in range(24)), [])
    add('rapid-pending', compose + rapid + done + capture(), 'Q' * 24, LORA_TEST_DELAY_MS=300)
    limit = sum((key('Q', 45) for _ in range(243)), [])
    add('limit-240-send', compose + limit + done + tap(175, 210) + capture(), 'Q' * 240, sends=1)
    add('limit-240', compose + limit + capture('text') + done + edit + cancel + capture(after=True), 'Q' * 240, opens=2)
    # Every RF field reaches 11 bytes, rejects a twelfth, and is discarded by Back.
    lengths = [9, 6, 1, 1, 1, 1]
    actions = list(rf)
    for field, length in enumerate(lengths):
        if field == 3:
            actions += tap(55, 210)
        actions += tap(100, 64 + field % 3 * 48) + key('Q') * (13 - length) + cancel
    add('rf-limits', actions + capture(), opens=6)
    actions = rf + tap(100, 64) + key('Q') + done + capture() + tap(100, 210)
    actions += number('916000000', 10) + done + tap(175, 210) + capture(after=True)
    add('rf-recover', actions, opens=2)
    add('rf-invalid-back', rf + tap(100, 64) + key('Q') + done + capture() + nav() + capture(after=True) + tap(100, 64) + cancel, opens=2)
    actions = list(rf)
    for field, (value, length) in enumerate(zip(['916000000', '250000', '8', '6', '-2', '16'], lengths)):
        if field == 3:
            actions += tap(55, 210)
        actions += tap(100, 64 + field % 3 * 48) + number(value, length) + done
    add('rf-all', actions + tap(175, 210) + capture(), opens=6)
    return cases


def run(args):
    system = args.system_apps.resolve()
    output = (args.output or ROOT / 'build/lora-shared-input').resolve()
    recipe_path = args.recipe.resolve()
    recipe = json.loads(recipe_path.read_text())['lora_messages']
    system_flags = [x for x in recipe['defines'] if x.startswith('-D')]
    safety_flags = ['-DPORTABLE_LORA_SHARED_TEXT_INPUT', '-DPORTABLE_TEXT_INPUT_CLIENT',
                    '-DPORTABLE_APP_LAUNCH_GUARD', '-DPORTABLE_CHECKED_STORAGE_RUNTIME',
                    '-DPORTABLE_NATIVE_CUSTODY_FENCE', '-DPORTABLE_ALARM_TERMINAL_RETENTION']
    output.mkdir(parents=True, exist_ok=True)
    sdk = output / 'sdk/include'
    sdk.mkdir(parents=True, exist_ok=True)
    for folder in (args.runtime / 'sdk/app', args.runtime / 'sdk/driver', system / 'lib/PortableApps/include', system / 'sdk/app'):
        for source in folder.glob('*.h'):
            shutil.copyfile(source, sdk / source.name)
    for folder in (ROOT / 'lib/Alarm/include', ROOT / 'lib/Contexts/include'):
        for source in folder.glob('*.h'):
            if not (sdk / source.name).exists():
                shutil.copyfile(source, sdk / source.name)
    shutil.copytree(system / 'lib/PortableApps/time', sdk.parent / 'time', dirs_exist_ok=True)
    # Preserve the deployed alarm profile semantics against the selected SDK.
    alarm_profile = output / 'watch-alarm-profile.h'
    alarm_profile.write_text('#include "AlarmServiceV1.h"\n')
    catalog = output / 'catalog.c'
    catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
    includes = [sdk, ROOT / 'Apps', system / 'Apps', system / 'lib/NativeApps/include',
                system / 'Services/scene_profile', system / 'Services/text_input']
    contexts_header = Path(recipe['defines'][recipe['defines'].index('-include') + 1])
    extra_flags = ['-D' + define for define in args.extra_define]
    flags = system_flags + safety_flags + extra_flags + ['-include', str(contexts_header), '-include', str(alarm_profile)]
    evidence = {'deployed_recipe': str(recipe_path), 'deployed_defines': system_flags,
                'added_safety_defines': safety_flags, 'extra_defines': extra_flags, 'omitted_defines': [],
                'native_toolchain': os.environ.get('CC', 'cc'),
                'omitted_target_options': ['Xtensa architecture/linker options', '-flto (native qualification uses -O1 -g)'],
                'physical_backends': ['display/delayed present', 'ordered raw touch', 'navigation/PMU', 'RTC', 'battery', 'KV', 'LoRa RF', 'alarm', 'BLE control/broadcast', 'Wi-Fi control', 'bounded base Contexts service'],
                'unavailable_optional_backends': ['Contexts fingerprint suffix/background hardware'],
                'leak_sanitizer': 'Unavailable in this executor: fatal ptrace/runtime error even for clean idle; ASan/UBSan run with detect_leaks=0. Fixture asserts zero live grants, frames, subscriptions and allocations on clean exit.',
                'not_claimed': ['hardware deployment', 'actual over-air RF', 'PMU/sleep hardware', 'Runtime ELF-loader integration'],
                'sources': {}, 'results': [], 'frame_hashes': {}}
    for path in [ROOT / 'Apps/lora_messages.c', ROOT / 'Apps/lora_messages_model.h', ROOT / 'test/native_apps/lora_shared_renderer_test.c', system / 'lib/PortableApps/src/adapter.c', system / 'Services/scene_host/host.c', system / 'Services/text_input/host.c', system / 'lib/PortableApps/include/PortableTextInputLongClient.h']:
        evidence['sources'][str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    selected = scenarios()
    if 'PORTABLE_TEXT_INPUT_POLICY' in args.extra_define:
        begin=tap(100,103)+tap(55,210)+key('Q')
        for suffix,end in [('done',key('\x03')+capture()),('cancel',nav('back','text')+capture()),('home',nav('home','text'))]:
            selected['policy-stopped-'+suffix]=(begin+[(65000,'text','idle',0,0)]+end,{'LORA_POLICY_PROBE':'1','LORA_EXPECT_DRAFT':'Q' if suffix=='done' else '','LORA_EXPECT_SENDS':'0','LORA_EXPECT_HISTORY':'0'})
        selected['policy-listen']=(tap(50,203)+begin+[(65000,'text','idle',0,0)]+key('\x03')+capture(),{'LORA_POLICY_PROBE':'1','LORA_POLICY_LISTEN':'1','LORA_EXPECT_DRAFT':'Q','LORA_EXPECT_SENDS':'0','LORA_EXPECT_HISTORY':'0'})
        for name,env in [('policy-unavailable','LORA_POLICY_UNAVAILABLE'),('policy-missing-suffix','LORA_POLICY_MISSING_SUFFIX')]:
            selected[name]=(tap(100,103)+tap(55,210)+capture(),{env:'1','LORA_EXPECT_DRAFT':'','LORA_EXPECT_OPENS':'1','LORA_EXPECT_SENDS':'0'})
        selected['policy-retained']=(begin,{})
        selected['policy-paused-home']=(begin,{'LORA_POLICY_PAUSED_HOME':'1','LORA_EXPECT_DRAFT':''})
    if args.scenario:
        selected = {k: v for k, v in selected.items() if k in args.scenario}
    for sanitized in ([False] if args.normal_only else [False, True]):
        out = output / ('sanitized' if sanitized else 'normal')
        out.mkdir(exist_ok=True)
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                   *['-I' + str(p) for p in includes]]
        if sanitized:
            command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
        objects = []
        for provider, entry in [('scene_host', 'lora_scene_driver_get'), ('text_input', 'lora_text_driver_get'), ('scene_profile', 'lora_profile_driver_get')]:
            obj = out / (provider + '.o')
            source = system / 'Services' / provider / ('profile.c' if provider == 'scene_profile' else 'host.c')
            host_flags = ['-DSCENE_HOST_TELEMETRY=0', '-DSCENE_HOST_WATCH_240=1'] if provider == 'scene_host' else []
            subprocess.run([*command, *host_flags, '-Dt5_driver_get=' + entry, '-c', str(source), '-o', str(obj)], check=True)
            objects.append(obj)
        sources = [ROOT / 'test/native_apps/lora_shared_renderer_test.c', system / 'lib/PortableApps/src/adapter.c', catalog]
        sources += [system / 'lib/PortableApps/src' / name for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')]
        executable = out / 'lora-shared-renderer'
        subprocess.run([*command, *map(str, sources), *map(str, objects), '-Wl,--wrap=malloc', '-Wl,--wrap=free', '-o', str(executable)], check=True)
        (out / 'compile-command.json').write_text(json.dumps(command, indent=2))
        for name, (actions, extra_env) in selected.items():
            folder = out / name
            folder.mkdir(exist_ok=True)
            action_file = folder / 'actions.txt'
            action_file.write_text(''.join(' '.join(map(str, row)) + '\n' for row in actions))
            env = {k: v for k, v in os.environ.items() if not k.startswith('LORA_')}
            env.update(extra_env, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
            result = subprocess.run([str(executable), str(folder), str(action_file), name], env=env, capture_output=True, text=True, timeout=90)
            (folder / 'run.log').write_text(result.stdout + result.stderr)
            evidence['results'].append({'mode': name, 'sanitized': sanitized, 'returncode': result.returncode, 'log': str(folder / 'run.log')})
            (output / 'qualification.json').write_text(json.dumps(evidence, indent=2) + '\n')
            if result.returncode:
                print(result.stdout + result.stderr, file=sys.stderr)
                result.check_returncode()
            if not args.raw_frames:
                from PIL import Image
                for ppm in folder.glob('*.ppm'):
                    with Image.open(ppm) as im:
                        assert im.size == (240, 240)
                        evidence['frame_hashes'][('sanitized/' if sanitized else 'normal/')+name+'/'+ppm.name]=hashlib.sha256(im.tobytes()).hexdigest()
                        if not sanitized and name in ('draft-done', 'rf-recover', 'rf-all', 'send-history', 'send-cancel', 'limit-240', 'rapid-pending', 'missing-host'):
                            im.save(ppm.with_suffix('.png'))
                    ppm.unlink()
            print('LoRa production shared input', 'ASan/UBSan' if sanitized else 'normal', name, 'PASS', flush=True)
        for obj in objects:
            obj.unlink()
    for source,digest in evidence['sources'].items():
        assert hashlib.sha256(Path(source).read_bytes()).hexdigest()==digest, ('source changed during qualification',source)
    if not args.normal_only:
        normal={k[7:]:v for k,v in evidence['frame_hashes'].items() if k.startswith('normal/')}
        sanitized={k[10:]:v for k,v in evidence['frame_hashes'].items() if k.startswith('sanitized/')}
        assert normal==sanitized, 'normal/sanitized pixels differ'
        evidence['normal_sanitized_image_comparisons']=len(normal)
    (output / 'qualification.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('Evidence:', output / 'qualification.json')


def define_argument(value):
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*(?:=[^\r\n]*)?', value):
        raise argparse.ArgumentTypeError('Use NAME or NAME=VALUE without a -D prefix')
    return value


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--system-apps', type=Path, required=True)
    p.add_argument('--runtime', type=Path, default=Path('/workspace/shared/watch-runtime-file-source-20261012'))
    p.add_argument('--recipe', type=Path, default=Path('/workspace/shared/watch-points-components-20261012/recipes/predecessor-clients.json'))
    p.add_argument('--output', type=Path)
    p.add_argument('--scenario', action='append')
    p.add_argument('--normal-only', action='store_true')
    p.add_argument('--raw-frames', action='store_true', help='Keep raw PPM output without optional PNG conversion')
    p.add_argument('--extra-define', action='append', default=[], type=define_argument, metavar='NAME[=VALUE]',
                   help='Additional production selector; repeat once for each define (without -D)')
    run(p.parse_args())
