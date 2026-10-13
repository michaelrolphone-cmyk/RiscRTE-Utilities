#!/usr/bin/env python3
"""Exercise production Spectrum/shared keyboard under deployed Watch feature flags.

Physical services are deterministic fixtures, not a physical-device qualification.
Only Spectrum's app source is included verbatim by the fixture for state observation;
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


def setup(editor, initial=0):
    return [(100, 'app', 'setup', editor, initial)]


def check(kind=0, gate='app', delay=220):
    return [(delay, gate, 'capture', kind, 0)]


def scenarios():
    cases = {}
    name_taps = [(120, 120), (120, 90), (90, 54), (120, 84)]
    pages = [2, 5, 7, 11]
    done = key('\x03', 220)
    for editor, title in enumerate(['frequency', 'signature', 'temporal', 'shared']):
        edit = tap(*name_taps[editor])
        begin = setup(editor) + edit
        def add(suffix, actions, draft='', opens=1, **env):
            cases[title + '-' + suffix] = (actions, {'SPECTRUM_EXPECT_DRAFT': draft,
                'SPECTRUM_EXPECT_OPENS': str(opens), 'SPECTRUM_EXPECT_KV_WRITES': '0',
                'SPECTRUM_EXPECT_DATA_WRITES': '0', 'SPECTRUM_EXPECT_EDITOR_PAGE': str(pages[editor]),
                **{k: str(v) for k, v in env.items()}})
        add('done', begin + key('Q') + done + check(), 'Q')
        add('cancel', begin + key('Q') + nav('back', 'text') + check())
        add('header-back', begin + key('Q') + tap(20, 20, 'text') + check())
        add('home', begin + key('Q') + nav('home', 'text'))
        add('header-home', begin + key('Q') + tap(190, 20, 'text'))
        add('reopen', begin + key('Q') + done + edit + key('W') + nav('back','text') + edit + key('E') + done + check(), 'QE', opens=3, SPECTRUM_EXPECT_REOPEN=1)
        add('limit', begin + key('Q') * 19 + done + check() + edit + nav('back', 'text'), 'Q' * 16, opens=2)
        add('delayed', begin + key('Q') + done + check(), 'Q', SPECTRUM_TEST_DELAY_MS=450, SPECTRUM_EXPECT_CLOSE_AGAIN=1)
        add('rapid-pending', begin + sum((key('Q', 5) for _ in range(14)), []) + done + check(), 'Q' * 14, SPECTRUM_TEST_DELAY_MS=300, SPECTRUM_EXPECT_CLOSE_AGAIN=1)
        add('no-present', begin + sum((key('Q', 5) for _ in range(14)), []) + done + check(), 'Q' * 14, SPECTRUM_NO_PRESENT=1)
        add('live', tap(120,120) + [(300, 'app', 'idle', 0, 0)] + begin + key('Q') * 8 + done + check() + check(3), 'Q' * 8, SPECTRUM_EXPECT_PROGRESS=1)
        add('live-no-present', tap(120,120)+begin+key('Q')*8+done+check()+check(3), 'Q'*8, SPECTRUM_NO_PRESENT=1, SPECTRUM_EXPECT_PROGRESS=1)
        for ending, terminal in [('done',done),('back',nav('back','text')),('home',nav('home','text'))]:
            actions=tap(120,120)+begin+key('Q')*8+terminal
            if ending!='home': actions+=check()
            add('live-delayed-'+ending, actions, 'Q'*8 if ending=='done' else '', SPECTRUM_TEST_DELAY_MS=3000, SPECTRUM_EXPECT_PROGRESS=1, SPECTRUM_EXPECT_CLOSE_AGAIN=1, SPECTRUM_EXPECT_CLOSE_PROGRESS=1)
        add('mixed-ascii', setup(editor,2)+edit+key('Q')+key('\x02')+key('Q')+done+check(), 'Az 09!?_[]{}|~aQ')
        # Crown/navigation goes through the scene host's actual focus geometry.
        add('crown', begin + [(100,'text','nav',8,0),(100,'text','nav',2,0)] + done + check(), 'W')
        save_at = [(180,185),(65,165),(200,54),(120,135)][editor]
        actions = begin + key('Q') + done + check() + tap(*save_at) + check(1, delay=100)
        env = {}
        if editor==0: env['SPECTRUM_EXPECT_KV_WRITES']=1
        elif editor==2: env['SPECTRUM_EXPECT_DATA_WRITES']=1
        elif editor==1: actions += tap(170,125) # cancel pending capture
        else: actions += tap(120,215) # cancel shared training
        add('explicit-save', actions, 'Q', **env)
    base = setup(0) + tap(120,120)
    for name in ('missing-host','old-host','unavailable-open'):
        cases[name] = (base + check(2), {'SPECTRUM_EXPECT_DRAFT':'','SPECTRUM_EXPECT_OPENS':str(name=='unavailable-open' and 1 or 0),'SPECTRUM_EXPECT_KV_WRITES':'0','SPECTRUM_EXPECT_DATA_WRITES':'0'})
    for name in ('missing-reopen','old-reopen'):
        cases[name]=(base+key('Q')+done+tap(120,120)+check(2),{'SPECTRUM_EXPECT_DRAFT':'Q','SPECTRUM_EXPECT_OPENS':'1'})
    for name in ('retained','loss-close'):
        cases[name]=(tap(120,120)+base+key('Q')+nav('back','text'),{})
    for name in ('loss-open','loss-poll'):
        cases[name]=(tap(120,120)+base,{})
    # Fully touch-driven discovery paths from the normal startup screen.
    monitor = tap(120,120) + [(4500,'app','idle',0,0)] + tap(140,25)
    room_tools = monitor + tap(100,105) + tap(195,20)
    event_tools = monitor + tap(150,222) + tap(195,20)
    begin_ui = [monitor + tap(195,62) + tap(75,108) + tap(120,120),
                room_tools + tap(75,210) + tap(100,90),
                event_tools + tap(120,205),
                monitor + tap(100,105) + tap(120,210) + tap(120,84)]
    for editor, title in enumerate(['frequency','signature','temporal','shared']):
        draft = 'Mains humQ' if editor == 0 else 'Q'
        env = {'SPECTRUM_EDITOR':str(editor),'SPECTRUM_EXPECT_DRAFT':draft,'SPECTRUM_EXPECT_OPENS':'1',
               'SPECTRUM_EXPECT_EDITOR_PAGE':str(pages[editor]),'SPECTRUM_EXPECT_KV_WRITES':'0','SPECTRUM_EXPECT_DATA_WRITES':'0'}
        actions = begin_ui[editor] + key('Q') + done + check()
        if editor == 0:
            actions += tap(180,185) + check(1)
            env.update(SPECTRUM_EXPECT_KV_WRITES='1',SPECTRUM_SAVED_SLOT='0',SPECTRUM_EXPECT_OPENS='2')
            actions+=tap(75,108)+tap(120,120)+nav('back','text')
        elif editor == 2:
            actions += tap(200,54) + check(1)
            env['SPECTRUM_EXPECT_DATA_WRITES']='1'
            env['SPECTRUM_EXPECT_OPENS']='2'
            actions+=tap(175,218)+tap(70,78)+tap(90,54)+nav('back','text')
        else:
            actions += tap(*[(0,0),(65,165),(0,0),(120,135)][editor]) + check(1,delay=100)
            actions += tap(*((170,125) if editor==1 else (120,215)))
        cases['ui-'+title] = (actions,env)
    cases['signature-save-complete']=(setup(1)+tap(120,90)+key('Q')+done+check()+tap(65,165)+[(5000,'app','idle',0,0)]+check(4),
        {'SPECTRUM_EDITOR':'1','SPECTRUM_EXPECT_DRAFT':'Q','SPECTRUM_EXPECT_EDITOR_PAGE':'5','SPECTRUM_EXPECT_OPENS':'1','SPECTRUM_EXPECT_KV_WRITES':'1'})
    cases['shared-save-complete']=(setup(3)+tap(120,84)+key('Q')+done+check()+tap(120,135)+[(32000,'app','idle',0,0)]+tap(120,160)+check(5),
        {'SPECTRUM_EDITOR':'3','SPECTRUM_EXPECT_DRAFT':'Q','SPECTRUM_EXPECT_EDITOR_PAGE':'11','SPECTRUM_EXPECT_OPENS':'1','SPECTRUM_EXPECT_KV_WRITES':'0'})
    a,e=cases['shared-done'];cases['contexts-unavailable']=(list(a),dict(e,SPECTRUM_NO_CONTEXTS='1'))
    cases['missing-once']=(setup(3)+tap(120,84)+check(2)+setup(0)+tap(120,120)+key('Q')+done+check(),
        {'SPECTRUM_EXPECT_DRAFT':'Q','SPECTRUM_EXPECT_OPENS':'1','SPECTRUM_EXPECT_EDITOR_PAGE':'2'})
    cases['non-keyboard-views']=(sum(([(100,'app','scene',i,0)]+check(9) for i in range(18)),[]),{'SPECTRUM_EXPECT_OPENS':'0'})
    return cases


def run(args):
    def source_pin(path):
        try:
            commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=path,text=True,stderr=subprocess.DEVNULL).strip()
            status=subprocess.check_output(['git','status','--porcelain'],cwd=path,text=True,stderr=subprocess.DEVNULL).splitlines()
            return {'path':str(path),'commit':commit,'status':status}
        except subprocess.CalledProcessError:
            return {'path':str(path),'commit':None}
    system = args.system_apps.resolve()
    output = (args.output or ROOT / 'build/spectrum-shared-input').resolve()
    recipe_path = args.recipe.resolve()
    recipe = json.loads(recipe_path.read_text())['audio_spectrum']
    system_flags = [x for x in recipe['defines'] if x.startswith('-D')]
    safety_flags = ['-DPORTABLE_SPECTRUM_SHARED_TEXT_INPUT', '-DPORTABLE_TEXT_INPUT_CLIENT',
                    '-DPORTABLE_APP_LAUNCH_GUARD', '-DPORTABLE_CHECKED_STORAGE_RUNTIME',
                    '-DPORTABLE_NATIVE_CUSTODY_FENCE', '-DPORTABLE_ALARM_TERMINAL_RETENTION', '-DPORTABLE_WATCH_COLOR_ONLY', '-DPORTABLE_WATCH_FROZEN_ICONS']
    if args.native_height:
        safety_flags=[x for x in safety_flags if x!='-DPORTABLE_WATCH_FROZEN_ICONS']
        safety_flags+=['-DPORTABLE_WATCH_NATIVE_HEIGHT','-DSPECTRUM_TEST_WIDTH=410','-DSPECTRUM_TEST_HEIGHT=502']
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
    shutil.copyfile(recipe_path.parent/'build/empty_catalog.c',catalog)
    includes = [sdk, ROOT / 'Apps', system / 'Apps', system / 'lib/NativeApps/include',
                system / 'Services/scene_profile', system / 'Services/text_input']
    contexts_header = Path(recipe['defines'][recipe['defines'].index('-include') + 1])
    extra_flags = ['-D' + define for define in args.extra_define]
    flags = system_flags + safety_flags + extra_flags + ['-include', str(contexts_header), '-include', str(alarm_profile)]
    evidence = {'deployed_recipe': str(recipe_path), 'deployed_defines': system_flags,
                'added_safety_defines': safety_flags, 'extra_defines': extra_flags, 'omitted_defines': [],
                'native_toolchain': os.environ.get('CC', 'cc'),
                'omitted_target_options': ['Xtensa architecture/linker options', '-flto (native qualification uses -O1 -g)'],
                'physical_backends': ['display/delayed present', 'ordered raw touch', 'navigation/PMU', 'RTC', 'battery', 'KV', 'synthetic microphone PCM', 'alarm', 'BLE control/broadcast', 'Wi-Fi control', 'bounded base Contexts service (no background sensors/presets/exports)'],
                'unavailable_optional_backends': ['Contexts fingerprint suffix; app-local shared fingerprint storage remains exercised'],
                'leak_sanitizer': 'Unavailable in this executor: fatal ptrace/runtime error even for clean idle; ASan/UBSan run with detect_leaks=0. Fixture asserts zero live grants, frames, subscriptions and allocations on clean exit.',
                'not_claimed': ['hardware deployment', 'actual microphone/crown hardware', 'PMU/sleep hardware', 'Runtime ELF-loader integration'],
                'catalog_sha256': hashlib.sha256(catalog.read_bytes()).hexdigest(), 'source_pins': {'system':source_pin(system),'runtime':source_pin(args.runtime.resolve()),'legacy_utilities':source_pin(args.baseline_utilities.resolve())},
                'test_script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'source_commit': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(), 'sources': {}, 'results': [], 'setup_note': 'ui-* scenarios navigate entirely via touch. Boundary/fault scenarios seed editor preconditions with production entrypoints. Non-keyboard raster cases seed deterministic display state.'}
    for path in [ROOT / 'Apps/audio_spectrum.c', ROOT / 'Apps/context_signal_library.inc', ROOT / 'Apps/spectrum_signature_app.inc', ROOT / 'Apps/spectrum_temporal_app.inc', ROOT / 'Apps/context_fingerprint_app.inc', ROOT / 'test/native_apps/spectrum_shared_renderer_test.c', system / 'lib/PortableApps/src/adapter.c', system / 'Services/scene_host/host.c', system / 'Services/text_input/host.c', system / 'lib/PortableApps/include/PortableTextInputClient.h']:
        evidence['sources'][str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    selected = scenarios()
    if 'PORTABLE_TEXT_INPUT_POLICY' in args.extra_define:
        begin=setup(0)+tap(120,120)+key('Q')
        for suffix,end in [('done',key('\x03')+check()),('cancel',nav('back','text')+check()),('home',nav('home','text'))]:
            selected['policy-stopped-'+suffix]=(begin+[(65000,'text','idle',0,0)]+end,{'SPECTRUM_POLICY_PROBE':'1','SPECTRUM_POLICY_STOPPED':'1','SPECTRUM_EXPECT_DRAFT':'Q' if suffix=='done' else '','SPECTRUM_EXPECT_KV_WRITES':'0','SPECTRUM_EXPECT_DATA_WRITES':'0'})
        selected['policy-active']=(tap(120,120)+begin+[(65000,'text','idle',0,0)]+key('\x03')+check(),{'SPECTRUM_POLICY_PROBE':'1','SPECTRUM_EXPECT_DRAFT':'Q','SPECTRUM_EXPECT_PROGRESS':'1'})
        selected['policy-unavailable']=(setup(0,1)+tap(120,120)+check(2),{'SPECTRUM_POLICY_UNAVAILABLE':'1','SPECTRUM_EXPECT_DRAFT':'Seed','SPECTRUM_EXPECT_OPENS':'1'})
        selected['policy-retained']=(begin,{})
        selected['policy-missing-suffix']=(setup(0,1)+tap(120,120)+check(2),{'SPECTRUM_POLICY_MISSING_SUFFIX':'1','SPECTRUM_EXPECT_DRAFT':'Seed','SPECTRUM_EXPECT_OPENS':'1'})
        selected['policy-paused-home']=(begin,{'SPECTRUM_POLICY_PAUSED_HOME':'1','SPECTRUM_EXPECT_DRAFT':''})
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
        for provider, entry in [('scene_host', 'spectrum_scene_driver_get'), ('text_input', 'spectrum_text_driver_get'), ('scene_profile', 'spectrum_profile_driver_get')]:
            obj = out / (provider + '.o')
            source = system / 'Services' / provider / ('profile.c' if provider == 'scene_profile' else 'host.c')
            host_flags = ['-DSCENE_HOST_TELEMETRY=0', '-DSCENE_HOST_WATCH_240='+('0' if args.native_height else '1')] if provider == 'scene_host' else []
            subprocess.run([*command, *host_flags, '-Dt5_driver_get=' + entry, '-c', str(source), '-o', str(obj)], check=True)
            objects.append(obj)
        sources = [ROOT / 'test/native_apps/spectrum_shared_renderer_test.c', system / 'lib/PortableApps/src/adapter.c', catalog]
        sources += [system / 'lib/PortableApps/src' / name for name in ('quick_actions.c', 'quick_render.c', 'quick_session.c', 'quick_radios.c')]
        executable = out / 'spectrum-shared-renderer'
        subprocess.run([*command, *map(str, sources), *map(str, objects), '-Wl,--wrap=malloc', '-Wl,--wrap=free', '-lm', '-o', str(executable)], check=True)
        (out / 'compile-command.json').write_text(json.dumps(command, indent=2))
        for name, (actions, extra_env) in selected.items():
            folder = out / name
            folder.mkdir(exist_ok=True)
            for stale in [*folder.glob('*.ppm'),*folder.glob('*.png')]: stale.unlink()
            action_file = folder / 'actions.txt'
            action_file.write_text(''.join(' '.join(map(str, row)) + '\n' for row in actions))
            env = {k: v for k, v in os.environ.items() if not k.startswith('SPECTRUM_')}
            if args.native_height and 0<int(extra_env.get('SPECTRUM_TEST_DELAY_MS','0'))<3000:
                extra_env={**extra_env,'SPECTRUM_TEST_DELAY_MS':'3000'}
            env.update(extra_env, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1')
            result = subprocess.run([str(executable), str(folder), str(action_file), name], env=env, capture_output=True, text=True, timeout=90)
            (folder / 'run.log').write_text(result.stdout + result.stderr)
            evidence['results'].append({'mode': name, 'sanitized': sanitized, 'returncode': result.returncode, 'log': str(folder / 'run.log')})
            (output / 'qualification.json').write_text(json.dumps(evidence, indent=2) + '\n')
            if result.returncode:
                print(result.stdout + result.stderr, file=sys.stderr)
                result.check_returncode()
            from PIL import Image
            for ppm in folder.glob('*.ppm'):
                with Image.open(ppm) as im:
                    assert im.size == ((410,502) if args.native_height else (240,240))
                    im.save(ppm.with_suffix('.png'))
                ppm.unlink()
            print('Spectrum production shared input', 'ASan/UBSan' if sanitized else 'normal', name, 'PASS', flush=True)
        if not args.scenario or 'non-keyboard-views' in args.scenario:
            baseline = out / 'baseline-renderer'
            baseline_flags = [command[0], '-I'+str(args.baseline_utilities.resolve()/'Apps'), *[x for x in command[1:] if x != '-DPORTABLE_SPECTRUM_SHARED_TEXT_INPUT']]
            baseline_source = args.baseline_utilities.resolve() / 'Apps/audio_spectrum.c'
            subprocess.run([*baseline_flags, '-DSPECTRUM_BASELINE', '-DSPECTRUM_APP_SOURCE="'+str(baseline_source)+'"',
                *map(str,sources),*map(str,objects),'-Wl,--wrap=malloc','-Wl,--wrap=free','-lm','-o',str(baseline)],check=True)
            baseline_out=out/'baseline-non-keyboard-views';baseline_out.mkdir(exist_ok=True)
            for stale in [*baseline_out.glob('*.ppm'),*baseline_out.glob('*.png')]: stale.unlink()
            action_file=out/'non-keyboard-views/actions.txt'
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
            result=subprocess.run([str(baseline),str(baseline_out),str(action_file),'non-keyboard-views'],env=env,capture_output=True,text=True,timeout=90)
            (baseline_out/'run.log').write_text(result.stdout+result.stderr)
            if result.returncode: print(result.stdout+result.stderr,file=sys.stderr)
            result.check_returncode()
            comparisons=[]
            for ppm in sorted(baseline_out.glob('capture-*.ppm')):
                from PIL import Image
                current=out/'non-keyboard-views'/ppm.with_suffix('.png').name
                with Image.open(ppm) as im:
                    actual=Image.open(current)
                    assert im.tobytes()==actual.tobytes(), (ppm.name,'non-keyboard pixels changed')
                    im.save(ppm.with_suffix('.png'))
                    comparisons.append({'capture':ppm.name,'sha256':hashlib.sha256(im.tobytes()).hexdigest(),'identical':True})
                ppm.unlink()
            assert len(comparisons)==18
            evidence.setdefault('baseline_pixels',[]).append({'sanitized':sanitized,'baseline_source':str(baseline_source),
                'baseline_sha256':hashlib.sha256(baseline_source.read_bytes()).hexdigest(),'comparisons':comparisons})
        for obj in objects:
            obj.unlink()
    for source,digest in evidence['sources'].items():
        assert hashlib.sha256(Path(source).read_bytes()).hexdigest()==digest, ('source changed during qualification',source)
    if not args.normal_only:
        matches=[]
        from PIL import Image
        for name in selected:
            normal=output/'normal'/name;sanitized=output/'sanitized'/name
            names={x.name for x in normal.glob('*.png')}
            assert names=={x.name for x in sanitized.glob('*.png')}
            for filename in sorted(names):
                with Image.open(normal/filename) as a,Image.open(sanitized/filename) as b:
                    assert a.tobytes()==b.tobytes(), ('normal/sanitized mismatch',name,filename)
                matches.append(name+'/'+filename)
        evidence['normal_sanitized_pixel_identical']=matches
    evidence['non_keyboard_views']=['startup','spectrum','waterfall','monitor','controls','controls-scrolled','frequency-label','signatures','signature-editor','events','event-editor','event-capture','event-examples','shared-rooms','shared-events','shared-editor','shared-capture','monitor-edit']
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
    p.add_argument('--recipe', type=Path, default=Path('/workspace/shared/watch-scroll-regression-20261011/watch-1110-public-source-build/clients.json'))
    p.add_argument('--output', type=Path)
    p.add_argument('--scenario', action='append')
    p.add_argument('--baseline-utilities', type=Path, default=Path('/workspace/shared/lora-shared-input-utilities-published-20261012'))
    p.add_argument('--native-height',action='store_true')
    p.add_argument('--normal-only', action='store_true')
    p.add_argument('--extra-define', action='append', default=[], type=define_argument, metavar='NAME[=VALUE]',
                   help='Additional production selector; repeat once for each define (without -D)')
    run(p.parse_args())
