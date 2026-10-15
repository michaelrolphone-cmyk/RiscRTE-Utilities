#!/usr/bin/env python3
"""Qualify real RF app_main and shared text/scene hosts on Watch and X4 profiles.

RF IQ, display, input, storage and native services are deterministic peripherals.
This is native host qualification, not physical hardware or Runtime ELF loading.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
VIEWS = ['startup', 'spectrum', 'waterfall', 'monitor', 'controls', 'controls-scrolled',
         'frequency-label', 'signatures', 'signature-editor', 'events', 'event-editor',
         'event-capture', 'event-examples', 'shared-rooms', 'shared-events',
         'shared-editor', 'shared-capture', 'monitor-edit', 'receiver']


def tap(x, y, gate='app', delay=160, up=30):
    return [(delay, gate, 'down', x, y), (up, gate, 'up', x, y)]


def nav(kind='back', gate='app', delay=200):
    return [(delay, gate, kind, 0, 0)]


def setup(editor, initial=0):
    return [(100, 'app', 'setup', editor, initial)]


def check(kind=0, gate='app', delay=220):
    return [(delay, gate, 'capture', kind, 0)]


def key(ch, target, delay=80):
    # Production shared-host topology and geometry; the app has no key layout.
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
                return tap((2*x+w)//(2 if target == 'x4' else 4),
                           412 + row*92 if target == 'x4' else 97 + row*30,
                           'text', delay, 25)
            xq += wq
    raise ValueError(ch)


def scenarios(target, controls_height=None):
    cases = {}
    name_taps = [(120, 120), (120, 90), (90, 54), (120, 84)]
    pages = [2, 5, 7, 12]
    K = lambda c, delay=80: key(c, target, delay)
    done = K('\x03', 220)
    back_header = tap(56,46,'text') if target == 'x4' else tap(20,20,'text')
    home_header = tap(424,46,'text') if target == 'x4' else tap(190,20,'text')
    home_target = 'default.elf' if target == 'x4' else 'springboard.elf'
    home_env = {'RF_EXPECT_LAUNCHES':0 if target=='x4' else 1,'RF_EXPECT_LAUNCH_TARGET':home_target,'RF_EXPECT_HOME_EXIT':1}
    for editor, title in enumerate(['label', 'signature', 'temporal', 'context-signal']):
        edit = tap(*name_taps[editor])
        begin = setup(editor) + edit
        def add(suffix, actions, draft='', opens=1, **env):
            cases[title + '-' + suffix] = (actions, {'RF_EXPECT_DRAFT': draft,
                'RF_EXPECT_OPENS': str(opens), 'RF_EXPECT_KV_WRITES': '0',
                'RF_EXPECT_DATA_WRITES': '0', 'RF_EXPECT_EDITOR_PAGE': str(pages[editor]),
                **{k: str(v) for k, v in env.items()}})
        add('done', begin + K('Q') + done + check(), 'Q')
        add('cancel', begin + K('Q') + nav('back','text') + check())
        add('header-back', begin + K('Q') + back_header + check())
        add('home', begin + K('Q') + nav('home','text'), **home_env)
        add('header-home', begin + K('Q') + home_header, **home_env)
        if target=='watch':
            add('home-retry', begin + K('Q') + nav('home','text') + check(6) + nav('home'),
                opens=1, **dict(home_env,RF_EXPECT_LAUNCHES=2,RF_LAUNCH_REFUSE=1))
        add('home-seeded-draft', setup(editor,1) + edit + K('Q') + nav('home','text'), **home_env)
        add('home-accepted-draft', begin + K('Q') + done + edit + K('W') + nav('home','text'), opens=2, **home_env)
        add('cancel-seed', setup(editor,1) + edit + K('Q') + nav('back','text') + check() + edit + K('W') + nav('back','text') + check(), 'Seed', opens=2)
        add('reopen', begin + K('Q') + done + edit + K('W') + nav('back','text') + edit + K('E') + done + check(), 'QE', opens=3, RF_EXPECT_REOPEN=1)
        add('limit', begin + K('Q')*19 + done + check() + edit + nav('back','text'), 'Q'*16, opens=2)
        add('delayed', begin + K('Q') + done + check(), 'Q', RF_TEST_DELAY_MS=450, RF_EXPECT_CLOSE_AGAIN=1)
        add('rapid-pending', begin + sum((K('Q',5) for _ in range(14)),[]) + done + check(), 'Q'*14, RF_TEST_DELAY_MS=300, RF_EXPECT_CLOSE_AGAIN=1)
        add('no-present', begin + sum((K('Q',5) for _ in range(14)),[]) + done + check(), 'Q'*14, RF_NO_PRESENT=1)
        add('live', begin + K('Q')*8 + done + check() + check(3), 'Q'*8, RF_EXPECT_PROGRESS=1)
        add('live-no-present', begin + K('Q')*8 + done + check() + check(3), 'Q'*8, RF_NO_PRESENT=1, RF_EXPECT_PROGRESS=1)
        for ending, terminal in [('done',done),('back',nav('back','text')),('home',nav('home','text'))]:
            actions = begin + K('Q')*8 + terminal
            if ending != 'home': actions += check()
            add('live-delayed-'+ending, actions, 'Q'*8 if ending == 'done' else '',
                RF_TEST_DELAY_MS=3000, RF_EXPECT_PROGRESS=1, RF_EXPECT_CLOSE_AGAIN=1,
                RF_EXPECT_CLOSE_PROGRESS=1, **(home_env if ending == 'home' else {}))
        add('awake', begin + [(65000,'text','idle',0,0)] + K('Q') + done + check() + check(3), 'Q', RF_EXPECT_PROGRESS=1, RF_EXPECT_AWAKE=1)
        add('mixed-ascii', setup(editor,2) + edit + K('Q') + K('\x02') + K('Q') + done + check(), 'Az 09!?_[]{}|~aQ')
        add('navigation', begin + [(100,'text','nav',8,0),(100,'text','nav',2,0)] + done + check(), 'W')
        # Done is draft-only. Explicit Save/Train remains the sole commit action.
        save_at = [(180,185),(65,165),(200,54),(120,135)][editor]
        actions = begin + K('Q') + done + check() + tap(*save_at) + check(1,delay=100)
        env = {}
        if editor == 0: env['RF_EXPECT_KV_WRITES'] = 1
        elif editor == 2: env['RF_EXPECT_DATA_WRITES'] = 1
        elif editor == 1: actions += tap(170,125)
        else: actions += tap(120,215)
        add('explicit-save', actions, 'Q', **env)
    base = setup(0) + tap(120,120)
    for name in ('missing-host','old-host','unavailable-open'):
        cases[name] = (base + check(2), {'RF_EXPECT_DRAFT':'','RF_EXPECT_OPENS':str(int(name=='unavailable-open')),'RF_EXPECT_KV_WRITES':'0','RF_EXPECT_DATA_WRITES':'0'})
    for name in ('missing-reopen','old-reopen'):
        cases[name] = (base + K('Q') + done + tap(120,120) + check(2), {'RF_EXPECT_DRAFT':'Q','RF_EXPECT_OPENS':'1'})
    for name in ('retained','loss-close'):
        cases[name] = (base + K('Q') + nav('back','text'), {})
    for name in ('loss-open','loss-poll'):
        cases[name] = (base,{})
    # Discovery workflows reach every editor from normal live startup through UI.
    def settings(number):
        height=controls_height or (400 if target=='x4' else 240)
        offset=min((number-1)*48,max(0,624-(height-94)))
        return tap(225,26) + [(100,'app','nav',32,0)]*(number-1) + tap(180,48+(number-1)*48-offset+20)
    room=settings(12)
    events=settings(13)
    begin_ui=[tap(120,120)+tap(120,61)+tap(120,120),
              room+tap(195,20)+tap(75,207)+tap(120,90),
              events+tap(195,20)+tap(120,200),
              room+tap(120,210)+tap(120,84)]
    for editor,title in enumerate(['label','signature','temporal','context-signal']):
        cases['ui-'+title]=(begin_ui[editor]+K('Q')+done+check(),
            {'RF_EDITOR':str(editor),'RF_EXPECT_DRAFT':'Q','RF_EXPECT_OPENS':'1',
             'RF_EXPECT_EDITOR_PAGE':str(pages[editor]),'RF_EXPECT_KV_WRITES':'0','RF_EXPECT_DATA_WRITES':'0'})
    cases['signature-save-complete'] = (setup(1)+tap(120,90)+K('Q')+done+check()+tap(65,165)+[(10000,'app','idle',0,0)]+check(4),
        {'RF_EDITOR':'1','RF_EXPECT_DRAFT':'Q','RF_EXPECT_EDITOR_PAGE':'5','RF_EXPECT_OPENS':'1','RF_EXPECT_KV_WRITES':'1'})
    cases['context-signal-save-complete'] = (setup(3)+tap(120,84)+K('Q')+done+check()+tap(120,135)+[(32000,'app','idle',0,0)]+tap(120,160)+check(5),
        {'RF_EDITOR':'3','RF_EXPECT_DRAFT':'Q','RF_EXPECT_EDITOR_PAGE':'12','RF_EXPECT_OPENS':'1','RF_EXPECT_KV_WRITES':'0'})
    for editor,title in enumerate(['label','signature','temporal','context-signal']):
        save_at=[(180,185),(65,165),(200,54),(120,135)][editor]
        actions=setup(editor)+tap(*name_taps[editor])+K('Q')+done+tap(*save_at)
        env={'RF_EDITOR':str(editor),'RF_EXPECT_OPENS':'3',**{k:str(v) for k,v in home_env.items()}}
        if editor==0:
            actions+=check(1)+tap(140,26)+tap(190,62)+tap(70,105)
            env.update(RF_EXPECT_KV_WRITES='1',RF_EXPECT_DATA_WRITES='0',RF_EXPECT_SAVED_LABEL='Q')
        elif editor==1:
            actions += [(10000,'app','idle',0,0)]+check(4)+tap(70,120)
            env.update(RF_EXPECT_KV_WRITES='1',RF_EXPECT_SAVED_SIGNATURE='Q')
        elif editor==2:
            actions += check(1)
            env.update(RF_EXPECT_KV_WRITES='0',RF_EXPECT_DATA_WRITES='1',RF_EXPECT_SAVED_TEMPORAL='Q')
        else:
            actions += [(32000,'app','idle',0,0)]+tap(120,160)+check(5)+tap(120,105)
            env.update(RF_EXPECT_KV_WRITES='0',RF_EXPECT_SAVED_CONTEXT='Q')
        actions+=check(10)+tap(*name_taps[editor])+K('W')+done+tap(*name_taps[editor])+K('E')+nav('home','text')
        cases[title+'-saved-home']=(actions,dict(env,RF_SAVED_CHECK_NAME='Q',RF_EXPECT_PRESERVE_SAVED='1'))
    # In-progress work is cancelled by the explicit Home action, never committed.
    for editor,title in [(1,'signature'),(3,'context-signal')]:
        actions=setup(editor)+tap(*name_taps[editor])+K('Q')+done+tap(65,165) if editor==1 else setup(editor)+tap(*name_taps[editor])+K('Q')+done+tap(120,135)
        cases[title+'-capture-home']=(actions+[(600,'app','idle',0,0)]+check(1)+nav('home'),
            {'RF_EDITOR':str(editor),'RF_EXPECT_OPENS':'1','RF_EXPECT_KV_WRITES':'0','RF_EXPECT_DATA_WRITES':'0','RF_SAVED_CHECK_NAME':'Q',**{k:str(v) for k,v in home_env.items()}})
    cases['temporal-capture-home']=(setup(2)+tap(90,54)+K('Q')+done+tap(60,120)+[(600,'app','idle',0,0)]+check(8)+check(10)+nav('home'),
        {'RF_EDITOR':'2','RF_EXPECT_OPENS':'1','RF_EXPECT_KV_WRITES':'0','RF_EXPECT_DATA_WRITES':'1','RF_EXPECT_SAVED_TEMPORAL':'Q','RF_EXPECT_PRESERVE_SAVED':'1',**{k:str(v) for k,v in home_env.items()}})
    # A failed commit remains authoritative; Home may not retry it during exit.
    # Watch also exercises a refused Home launch followed by ordinary Back.
    for editor,title in enumerate(['label','signature','temporal','context-signal']):
        actions=setup(editor)+tap(*name_taps[editor])+K('Q')+done+tap(*[(180,185),(65,165),(200,54),(120,135)][editor])
        if editor==0:actions+=check(1)+tap(140,26)+tap(190,62)+tap(70,105)
        elif editor==1:actions+=[(10000,'app','idle',0,0)]+check(4)+tap(70,120)
        elif editor==2:actions+=check(1)
        else:actions+=[(32000,'app','idle',0,0)]+tap(120,160)+check(5)+tap(120,105)
        actions+=tap(*name_taps[editor])+K('W')+done
        fault=1 if editor in (0,1) else 2
        if editor in (0,2):actions+=[(100,'app','fault',fault,1)]+tap(*[(180,185),(0,0),(200,54)][editor])
        elif editor==1:actions+=[(100,'app','fault',fault,1)]+tap(65,165)+[(10000,'app','idle',0,0)]
        else:actions+=tap(120,135)+[(32000,'app','idle',0,0),(100,'app','fault',fault,1)]+tap(120,160)
        actions+=check(11)+check(10)+nav('home')
        env={'RF_EDITOR':str(editor),'RF_EXPECT_OPENS':'2','RF_SAVED_CHECK_NAME':'Q','RF_EXPECT_PRESERVE_SAVED':'1','RF_EXPECT_NO_STORAGE_READS':'1',
             ['RF_EXPECT_SAVED_LABEL','RF_EXPECT_SAVED_SIGNATURE','RF_EXPECT_SAVED_TEMPORAL','RF_EXPECT_SAVED_CONTEXT'][editor]:'Q'}
        if target=='watch':
            actions+=check(6)+check(12)+nav('back')
            env.update(RF_LAUNCH_REFUSE='1',RF_EXPECT_LAUNCHES='2',RF_EXPECT_LAUNCH_TARGET=home_target)
        else:env.update({k:str(v) for k,v in home_env.items()})
        cases[title+'-pending-home'+('-refused-back' if target=='watch' else '')]=(actions,env)
    a,e=cases['context-signal-done'];cases['contexts-unavailable']=(list(a),dict(e,RF_NO_CONTEXTS='1'))
    cases['capture-stop-restart']=(tap(120,120)+[(500,'app','idle',0,0)]+tap(75,225)+check(13)+[(500,'app','idle',0,0)]+check(14)+tap(75,225)+[(500,'app','idle',0,0)]+check(15)+nav('home'),{})
    cases['non-keyboard-views']=(sum(([(100,'app','scene',i,0)]+check(9) for i in range(len(VIEWS))),[]),{'RF_EXPECT_OPENS':'0'})
    return cases


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def pin(path):
    return {'path':str(path), 'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=path,text=True).strip(),
            'status':subprocess.check_output(['git','status','--porcelain'],cwd=path,text=True).splitlines()}


def run(args):
    system=args.system_apps.resolve(); runtime=args.runtime.resolve()
    output=(args.output or ROOT/'build/rf-shared-input').resolve();output.mkdir(parents=True,exist_ok=True)
    evidence={'schema':'rf.shared-input.qualification.v1','source_pins':{'utilities':pin(ROOT),'system':pin(system),'runtime':pin(runtime),'baseline':None if args.skip_baseline else pin(args.baseline_utilities.resolve())},
        'not_claimed':['physical hardware','RF/crown/touch/PMU hardware','Runtime ELF loader integration','Runtime storage implementation'],
        'physical_backends':['deterministic signed 10-bit coherent complex IQ','ordered raw touch','navigation','RGB565/mono1 display with stride guards','KV and app-data','bounded base Contexts','alarm','resident checkpoint','RTC/realtime','BLE/Wi-Fi'],
        'leak_sanitizer':'detect_leaks=0 for executor ptrace/runtime limit; explicit malloc/free, grants, subscriptions and display lease accounting',
        'sources':{str(p):digest(p) for p in [Path(__file__),ROOT/'test/rf_shared_input/renderer.c',*sorted(ROOT.glob('Apps/rf*')),ROOT/'Apps/waterfall.c',ROOT/'Apps/context_signal_library.inc',ROOT/'Apps/context_fingerprint_app.inc'] if p.is_file()},
        'profiles':{},'results':[],'baseline_pixels':[],'normal_sanitized_pixel_identical':[],'non_keyboard_views':VIEWS}
    for target in args.target or ['watch','x4']:
        runtime=(args.runtime if target=='watch' else args.x4_runtime).resolve()
        host_system=(args.x4_host_system if target=='x4' else system).resolve()
        receipt=(args.watch_recipe if target=='watch' else args.x4_recipe).resolve()
        record=json.loads(receipt.read_text());recipe=record.get('waterfall',record)
        deployed=[x for x in recipe.get('defines',recipe['command']) if x.startswith('-D')]
        added=['-DPORTABLE_RF_SHARED_TEXT_INPUT','-DPORTABLE_TEXT_INPUT_CLIENT','-DPORTABLE_APP_HOME_GUARD']
        if target=='watch':
            added+=['-DPORTABLE_HOME_APP="springboard.elf"','-DPORTABLE_CHECKED_STORAGE_RUNTIME','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_ALARM_TERMINAL_RETENTION','-DPORTABLE_WATCH_COLOR_ONLY','-DPORTABLE_WATCH_FROZEN_ICONS']
        else: added+=['-DPORTABLE_CHECKED_STORAGE_RUNTIME','-DRF_TEST_PAPER']
        if args.native_height and target=="watch":
            added=[x for x in added if x!="-DPORTABLE_WATCH_FROZEN_ICONS"]
            added += ["-DPORTABLE_WATCH_NATIVE_HEIGHT","-DRF_TEST_NATIVE_HEIGHT","-DRF_TEST_WIDTH=410u","-DRF_TEST_HEIGHT=502u"]
        added=[flag for flag in added if flag not in deployed]
        telemetry_include=None
        if target=='x4':
            # Full deployed scene host includes exact generated telemetry glyphs,
            # even though RF text entry does not invoke telemetry components.
            stage=output/target/'host-fonts';font_dir=stage/'Services/scene_host/fonts'
            font_dir.mkdir(parents=True,exist_ok=True);(stage/'scripts').mkdir(exist_ok=True)
            generator=host_system/'scripts/generate_telemetry_fonts.py'
            shutil.copyfile(generator,stage/'scripts'/generator.name)
            for font in ['Orbitron-700.ttf','Orbitron-900.ttf','Rajdhani-700.ttf']:
                shutil.copyfile(host_system/'Services/scene_host/fonts'/font,font_dir/font)
            subprocess.run([sys.executable,str(stage/'scripts'/generator.name)],check=True)
            expected='2d7501e42d6223ebbc191817715431182c435fea944a1a02166a0748a577eaf1'
            assert digest(font_dir/'telemetry.inc')==expected,'Published full-host typography mismatch'
            evidence['sources'][str(generator)]=digest(generator)
            evidence['sources'][str(font_dir/'telemetry.inc')]=expected
            telemetry_include=font_dir.parent
        sdk=output/target/'sdk/include';sdk.mkdir(parents=True,exist_ok=True)
        for folder in [runtime/'sdk/app',runtime/'sdk/driver',system/'lib/PortableApps/include',system/'sdk/app',ROOT/'lib/Alarm/include',ROOT/'lib/Contexts/include']:
            for source in folder.glob('*.h'):
                if folder in [ROOT/'lib/Alarm/include',ROOT/'lib/Contexts/include'] and (sdk/source.name).exists():continue
                dest=sdk/source.name
                dest.unlink(missing_ok=True);dest.symlink_to(source.resolve())
        time_alias=sdk.parent/'time'
        if not time_alias.exists():time_alias.symlink_to(system/'lib/PortableApps/time',target_is_directory=True)
        extra=[]
        if target=='watch':
            defines=recipe['defines']; contexts=Path(defines[defines.index('-include')+1])
            if record.get('schema'):
                extra=[token for i,x in enumerate(defines) if x=='-include' for token in ['-include',defines[i+1]]]
                for i,x in enumerate(extra):
                    if x=='-include':evidence['sources'][extra[i+1]]=digest(extra[i+1])
            else:
                alarm=output/target/'alarm-profile.h';alarm.write_text('#include "AlarmServiceV1.h"\n')
                presentation=output/target/'rf-presentation.h';presentation.write_text('#include "'+str(system/'Apps/PaperPresentation.h')+'"\n#define RF_PRESENTATION_H\n')
                extra=['-include',str(contexts),'-include',str(alarm),'-include',str(presentation)]
        catalog=output/target/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
        flags=deployed+added+['-D'+d for d in args.extra_define]+extra
        includes=[sdk,ROOT/'Apps',system/'Apps',system/'lib/NativeApps/include',system/'Services/scene_profile',system/'Services/text_input']
        evidence['profiles'][target]={'receipt':str(receipt),'runtime':pin(runtime),'host_system':pin(host_system),'sha256':digest(receipt),'deployed_defines':deployed,'added_defines':added,'omitted_defines':[],
            'omitted_target_options':['Xtensa machine/linker options','-flto; native tests use -O1 -g'],'raster':[480,800] if target=='x4' else ([410,502] if args.native_height else [240,240]),
            'scene_profile':{'display_rotation':270 if target=='x4' else 0,'touch_rotation':0,'paper':target=='x4','source':str(host_system/'Services/scene_profile/profile.c')}}
        selected=scenarios(target, (502*240+409)//410 if target=='watch' and args.native_height and '-DPORTABLE_RF_NATIVE_CANVAS' in flags else None)
        if args.scenario:
            unknown=set(args.scenario)-set(selected)
            if unknown:raise ValueError('Unknown scenario: '+', '.join(unknown))
            selected={k:v for k,v in selected.items() if k in args.scenario}
        for sanitized in [False] if args.normal_only else [False,True]:
            out=output/target/('sanitized' if sanitized else 'normal');out.mkdir(exist_ok=True)
            command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(p) for p in includes]]
            if sanitized:command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
            objects=[];commands=[]
            for provider,entry in [('scene_host','rf_scene_driver_get'),('text_input','rf_text_driver_get'),('scene_profile','rf_profile_driver_get')]:
                obj=out/(provider+'.o');source=host_system/'Services'/provider/('profile.c' if provider=='scene_profile' else 'host.c')
                host_flags=['-DSCENE_HOST_TELEMETRY=0'] if provider=='scene_host' and target=='watch' else []
                if target=='watch' and provider=='scene_host':host_flags+=['-DSCENE_HOST_WATCH_240='+('0' if args.native_height else '1')]
                if target=='watch' and provider=='scene_profile':host_flags+=['-DSCENE_PROFILE_ID="scene-profile-compact-color"']
                if target=='x4' and provider=='scene_profile':host_flags+=['-DSCENE_PROFILE_PAPER=1','-DSCENE_DISPLAY_ROTATION=270','-DSCENE_PROFILE_ID="scene-profile-portrait-monochrome"']
                provider_command=[command[0],*(['-I'+str(telemetry_include)] if telemetry_include else []),'-I'+str(host_system/'lib/PortableApps/include'),'-I'+str(host_system/'Services/scene_profile'),*command[1:]]
                cmd=[*provider_command,*host_flags,'-Dt5_driver_get='+entry,'-c',str(source),'-o',str(obj)];subprocess.run(cmd,check=True);commands.append(cmd);objects.append(obj)
                evidence['sources'][str(source)]=digest(source)
            sources=[ROOT/'test/rf_shared_input/renderer.c',system/'lib/PortableApps/src/adapter.c',catalog]
            names=['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c'] if target=='watch' else ['PortableNativeTimeSource.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
            sources += [system/'lib/PortableApps/src'/name for name in names]
            executable=out/'rf-shared-renderer'
            cmd=[*command,*map(str,sources),*map(str,objects),'-Wl,--wrap=malloc','-Wl,--wrap=free','-lm','-o',str(executable)];subprocess.run(cmd,check=True);commands.append(cmd)
            (out/'compile-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
            for name,(actions,env) in selected.items():
                folder=out/name;folder.mkdir(exist_ok=True)
                for old in [*folder.glob('*.ppm'),*folder.glob('*.png')]:old.unlink()
                action_file=folder/'actions.txt';action_file.write_text(''.join(' '.join(map(str,row))+'\n' for row in actions))
                if args.native_height and 0<int(env.get('RF_TEST_DELAY_MS','0'))<3000:env={**env,'RF_TEST_DELAY_MS':'3000'}
                environ={k:v for k,v in os.environ.items() if not k.startswith('RF_')};environ.update(env,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
                result=subprocess.run([str(executable),str(folder),str(action_file),name],env=environ,capture_output=True,text=True,timeout=120)
                (folder/'run.log').write_text(result.stdout+result.stderr)
                evidence['results'].append({'target':target,'mode':name,'sanitized':sanitized,'returncode':result.returncode,'log':str(folder/'run.log')})
                (output/'qualification.json').write_text(json.dumps(evidence,indent=2)+'\n')
                if result.returncode:
                    print(result.stdout+result.stderr,file=sys.stderr)
                    if not args.keep_going: result.check_returncode()
                    continue
                from PIL import Image
                for ppm in folder.glob('*.ppm'):
                    with Image.open(ppm) as im:
                        assert im.size==tuple(evidence['profiles'][target]['raster']);im.save(ppm.with_suffix('.png'))
                    ppm.unlink()
                print('RF shared input',target,'ASan/UBSan' if sanitized else 'normal',name,'PASS',flush=True)
            if 'non-keyboard-views' in selected and not args.skip_baseline:
                for label,baseline_root in [('published',args.baseline_utilities),('packaged',args.watch_baseline if target=='watch' else args.x4_baseline)]:
                    baseline=out/(label+'-baseline-renderer');baseline_source=baseline_root.resolve()/'Apps/waterfall.c'
                    baseline_flags=[command[0],'-I'+str(baseline_root.resolve()/'Apps'),*[x for x in command[1:] if x not in ['-DPORTABLE_RF_SHARED_TEXT_INPUT','-DPORTABLE_APP_HOME_GUARD']]]
                    cmd=[*baseline_flags,'-DRF_BASELINE','-DRF_APP_SOURCE="'+str(baseline_source)+'"',*map(str,sources),*map(str,objects),'-Wl,--wrap=malloc','-Wl,--wrap=free','-lm','-o',str(baseline)]
                    subprocess.run(cmd,check=True);folder=out/(label+'-baseline-non-keyboard-views');folder.mkdir(exist_ok=True)
                    result=subprocess.run([str(baseline),str(folder),str(out/'non-keyboard-views/actions.txt'),'non-keyboard-views'],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),capture_output=True,text=True,timeout=120)
                    (folder/'run.log').write_text(result.stdout+result.stderr)
                    if result.returncode:print(result.stdout+result.stderr,file=sys.stderr)
                    result.check_returncode();comparisons=[]
                    from PIL import Image
                    for ppm in sorted(folder.glob('capture-*.ppm')):
                        with Image.open(ppm) as before,Image.open(out/'non-keyboard-views'/ppm.with_suffix('.png').name) as after:
                            assert before.tobytes()==after.tobytes(),(target,sanitized,ppm.name,'non-keyboard pixels changed')
                            before.save(ppm.with_suffix('.png'));comparisons.append({'capture':ppm.stem,'sha256':hashlib.sha256(before.tobytes()).hexdigest(),'identical':True})
                        ppm.unlink()
                    assert len(comparisons)==len(VIEWS)
                    evidence['baseline_pixels'].append({'target':target,'sanitized':sanitized,'baseline':label,'source_pin':pin(baseline_root.resolve()),'source':str(baseline_source),'comparisons':comparisons})
                    (output/'qualification.json').write_text(json.dumps(evidence,indent=2)+'\n')
        if not args.normal_only:
            from PIL import Image
            for name in selected:
                if any(r['target']==target and r['mode']==name and r['returncode'] for r in evidence['results']):continue
                normal=output/target/'normal'/name;sanitized=output/target/'sanitized'/name
                names={p.name for p in normal.glob('*.png')};assert names=={p.name for p in sanitized.glob('*.png')}
                for filename in sorted(names):
                    with Image.open(normal/filename) as a,Image.open(sanitized/filename) as b:assert a.tobytes()==b.tobytes(),(target,name,filename)
                    evidence['normal_sanitized_pixel_identical'].append(target+'/'+name+'/'+filename)
    for source,expected in evidence['sources'].items():assert digest(source)==expected,('source changed during qualification',source)
    (output/'qualification.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print('Evidence:',output/'qualification.json')
    failed=[r for r in evidence['results'] if r['returncode']]
    if failed: raise SystemExit(f'{len(failed)} shared-input scene runs failed; see qualification.json')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--system-apps',type=Path,required=True)
    p.add_argument('--runtime',type=Path,default=Path('/workspace/shared/watch-runtime-file-source-20261012'))
    p.add_argument('--x4-runtime',type=Path,default=Path('/workspace/shared/first-attempt-time-alarm-runtime-28-20261012'))
    p.add_argument('--watch-recipe',type=Path,default=Path('/workspace/shared/rf-shared-input-watch-target-preflight4-20261012/waterfall-build-record.json'))
    p.add_argument('--x4-recipe',type=Path,default=Path('/workspace/shared/rf-shared-input-x4-target-preflight3-20261012/waterfall-build-record.json'))
    p.add_argument('--baseline-utilities',type=Path,default=Path('/workspace/shared/spectrum-shared-input-utilities-published-20261012'))
    p.add_argument('--watch-baseline',type=Path,default=Path('/workspace/shared/watch-scroll-regression-20261011/utilities'))
    p.add_argument('--x4-baseline',type=Path,default=Path('/workspace/shared/rf-shared-input-x4-baseline-utilities-20261012'))
    p.add_argument('--x4-host-system',type=Path,default=Path('/workspace/shared/rf-shared-input-x4-baseline-system-20261012'))
    p.add_argument('--output',type=Path)
    p.add_argument('--target',choices=['watch','x4'],action='append')
    p.add_argument('--scenario',action='append')
    p.add_argument('--extra-define',action='append',default=[])
    p.add_argument('--native-height',action='store_true')
    p.add_argument('--normal-only',action='store_true')
    p.add_argument('--keep-going',action='store_true',help='Record every failure and exit nonzero after completing other scenes')
    p.add_argument('--skip-baseline',action='store_true',help='Intentional presentation changes; retain normal/sanitized pixel equality')
    run(p.parse_args())
