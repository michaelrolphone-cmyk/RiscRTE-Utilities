#!/usr/bin/env python3
"""Production RF app and Watch/X4 adapters over explicit host peripherals.

Only representative checkpoints produce images. RF is deterministic signed10-bit
complex IQ; Watch touch and Runtime diagnostic implementations are production
sources, physical I2C/USB are their existing host shims. App-data faults in the
renderer are scripted peripheral faults; the separate Runtime fixture uses the
actual AppDataFiles transactional implementation.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--system-apps', type=Path, required=True)
p.add_argument('--x4-system', type=Path)
p.add_argument('--watch', type=Path)
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--scene', action='append')
p.add_argument('--normal-only', action='store_true')
a = p.parse_args()
OUT = ROOT / 'build/rf-application'


def run(args, **kw):
    subprocess.run(list(map(str, args)), check=True, **kw)


def check(key, value, op='eq'):
    return f'check {key} {op} {value}'


def tap(x, y):
    return f'tap {x} {y}'


def scene(*commands):
    return '\n'.join(commands) + '\n'


scenes = {
    'lifecycle': (scene('wait 10', check('running', 1), check('fft', 256), check('last_count', 256), check('peak_hz', 2446250000), check('history', 5, 'ge'), 'snapshot spec',
        'nav 104', check('view', 1), check('running', 1), 'snapshot fall', 'nav 8', check('view', 2), 'snapshot monitor', 'nav 8', check('view', 0),
        'nav 32', check('frozen', 1), check('running', 0), 'remember captures', 'remember history', 'wait 5', 'same captures', 'same history',
        'nav 2', check('running', 1), 'wait 3', 'nav 2', check('running', 0), check('frozen', 0), 'finish'), {}),
    'sidebands': (scene('wait 3', check('peak_hz', 2446250000), 'set signal -1', 'wait 3', check('peak_hz', 2433750000), 'set signal 1', 'wait 3', check('peak_hz', 2446250000), 'finish'), {}),
    'retry': (scene('wait 3', check('running', 0), check('captures', 0), 'set no_radio 0', 'nav 2', check('running', 1), 'wait 2', 'set rf_failure 6', 'wait 2', check('running', 0), check('capture_error', 1), 'nav 2', check('running', 1), 'wait 2', 'set rf_failure 2', 'wait 2', check('running', 0), check('capture_error', 1), 'nav 2', check('running', 1), 'set format_failure 1', 'wait 2', check('running', 0), check('capture_error', 1), 'nav 2', check('running', 1), 'finish'), {'RF_RENDER_NO_RADIO':'1'}),
    'retained-radio': (scene('wait 3', 'set rf_failure 7', 'wait 2', check('running', 0), check('capture_error', 1), check('suspends', 4, 'ge'), 'nav 2', check('running', 1), 'finish'), {}),
}

def key_tap(key):
    return tap(24+(key%8)*27, 86+(key//8)*24) if key<32 else tap(42+(key-32)*72,190)


def type_name(text):
    commands=[]
    page=2
    for ch in text:
        want=(ord(ch)-32)//32
        while page!=want:
            commands.append(key_tap(32))
            page=(page+1)%3
        commands.append(key_tap((ord(ch)-32)%32))
    return commands+[key_tap(34)]


def target_scenes(target):
    result=dict(scenes)
    height=400 if target=='x4' else 240
    max_scroll=624-(height-94)
    def setting(number,x=215):
        offset=min((number-1)*48,max_scroll)
        return [tap(225,26), check('page',1), *(['nav 32']*(number-1)), tap(x,48+(number-1)*48-offset+20)]
    def samples():
        return setting(12,180)+[check('page',4)]
    def events():
        return setting(13,180)+[check('page',6)]
    controls=['wait 3',tap(225,26),'nav 104',check('scroll',48),'nav 1']
    for number,x,name,value in [(1,215,'low',2402500000),(2,100,'high',2477500000),(3,130,'log_frequency',1),(4,190,'log_amplitude',0),(5,190,'show_labels',0),(6,215,'palette',1),(7,215,'gain',3),(8,215,'window',2),(10,215,'threshold',-55)]:
        controls+=setting(number,x)+[check(name,value),'nav 1']
    for size in [512,1024,2048,4096,8192]:
        controls+=setting(9)+['wait 2',check('fft',size),check('last_count',size),'nav 1']
    for size in [4096,2048,1024,512,256]:
        controls+=setting(9,100)+['wait 2',check('fft',size),check('last_count',size),'nav 1']
    controls+=setting(11,180)+[check('pending',0),'nav 1']+samples()+['nav 1',check('page',1),'nav 1']+events()+['nav 1','nav 1','finish']
    result['controls']=(scene(*controls),{})
    receiver=['wait 3',tap(225,26),tap(120,30),check('page',10),'nav 104',check('receiver_scroll',1),'nav 16']
    current=0
    for field,direction,key,value in [(0,1,'lo',2441000000),(1,-1,'rate',16000000),(2,-1,'width',20000000),(3,1,'raw_gain',25),(4,1,'rf_gain',0),(5,1,'bb_gain',0),(6,1,'filter',1),(7,1,'filter',257),(8,1,'dc0',0),(9,1,'dc1',0),(10,1,'dc2',0),(11,1,'dc3',0),(12,1,'iq',0),(13,1,'iq',256),(14,1,'lo',2440000000)]:
        start=min(field,14)
        receiver+=['nav 32']*(start-current)
        current=start
        receiver += [tap(210 if direction>0 else 165,72+(field-start)*44),check(key,value),check('running',0),tap(83,224),check('running',1),'wait 1',check('last_count',256),tap(83,224),check('running',0)]
    receiver += [tap(205,116),check('lo',2440000000),check('running',0)]
    receiver+=['nav 32',tap(205,160),check('running',0),tap(150,224),check('page',1),'nav 1','finish']
    result['receiver']=(scene(*receiver),{})
    label=['wait 3',tap(120,120),check('cursor',1),check('cursor_hz',2440176211),'snapshot cursor',tap(120,61),check('page',2),tap(120,119),check('page',3),'nav 104',check('key_choice',1)]
    # Each printable ASCII key is inserted and deleted through the real keyboard.
    current=2
    for page_no in [2,0,1]:
        while current!=page_no:
            label.append(key_tap(32));current=(current+1)%3
        label.append(check('key_page',page_no))
        for key in range(32 if page_no!=2 else 31):
            label += [key_tap(key),check('name_length',1),check('char',32+page_no*32+key),key_tap(33),check('name_length',0)]
    while current!=2:
        label.append(key_tap(32));current=(current+1)%3
    label += type_name('private-label')+[check('page',2),tap(194,154),tap(166,185),check('labels',1),check('color',7),check('pending',0),tap(140,26),tap(190,62),tap(70,105),check('page',2),tap(120,119),*type_name('x'),tap(176,185),check('labels',1),tap(200,103),check('labels',0),tap(193,211),check('labels',1),'snapshot labels','finish']
    result['labels-keyboard']=(scene(*label),{})
    room=['wait 3']+samples()+[tap(75,207),check('page',5),tap(110,89),*type_name('Room'),tap(60,165),check('signature_goal',64),'wait 8',tap(55,125),check('running',0),'remember signature_frames','remember captures','wait 5','same signature_frames','same captures',tap(55,125),check('running',1),'wait 70',check('signature_goal',0),check('room_frames',64),check('room_kind',1),check('signature_pending',0),tap(195,122),check('signature_mode',2),check('manual_room',0),tap(120,62),check('signature_mode',1),'wait 80',check('room_selected',0),tap(50,62),check('signature_mode',0),tap(70,118),check('page',5),tap(60,165),'wait 70',check('room_frames',128),tap(70,118),tap(60,165),'wait 6',tap(175,125),check('signature_goal',0),check('room_frames',128),'snapshot rooms','finish']
    result['rooms']=(scene(*room),{})
    result['nested-exit']=(scene('wait 3',tap(225,26),tap(120,30),check('page',10),'nav 1',check('page',1),'nav 1',check('page',0),'nav 1','wait 1',check('launches',1),check('running',0),'nav 1'),{'RF_RENDER_LAUNCH_REFUSE':'1','RF_RENDER_EXPECT_LAUNCH':'2'})
    legacy=['wait 3',check('last_count',256)]+setting(9)+[check('fft',256),check('last_count',256),'nav 1','finish']
    result['legacy']=(scene(*legacy),{'RF_RENDER_LEGACY':'1'})
    for kind in (1,2):
        unread=['wait 2',check('load_errors',1)]+setting(7)+[check('gain',0),check('writes',0),'nav 1','set kv_failure 0']+setting(11,180)+[check('load_errors',0),'nav 1']+setting(7)+[check('gain',3),check('pending',0),'nav 1','finish']
        result['storage-unread' if kind==1 else 'storage-corrupt']=(scene(*unread),{'RF_RENDER_KV_FAILURE':str(kind)})
    pending=['wait 3','set kv_failure 3']+setting(7)+[check('gain',3),check('pending',1),'nav 1','set kv_failure 0']+setting(11,180)+[check('gain',3),check('pending',0),'nav 1','finish']
    result['storage-pending']=(scene(*pending),{})
    event_base=['wait 3','set signal 0']+events()+[tap(120,200),check('page',3),*type_name('Activity'),check('page',7)]
    first=event_base+[tap(60,120),check('event_armed',1),'wait 70',check('ambient_ready',1),check('event_wait_quiet',0),'set signal 2','wait 12','set signal 0','wait 8',check('event_ready',1),check('event_flags',0),'raw 20 20',check('event_ready',1),check('page',8),'times','snapshot event-review',tap(60,190),check('examples',1),check('positive',1),check('event_pending',-1),check('neural_active',0)]
    event=first+[tap(175,120),check('event_armed',1),'wait 70','set signal -1','wait 12','set signal 0','wait 8',check('event_ready',1),'times',tap(60,190),check('examples',2),check('negative',1),tap(60,220),check('page',9),'snapshot examples',tap(205,74),check('examples',2),tap(205,74),check('examples',1),check('negative',1),'nav 1',check('page',7),tap(60,120),check('event_armed',1),'nav 1',check('page',8),check('event_armed',1),tap(175,200),check('event_armed',0),check('event_ready',0),check('examples',1),'nav 1',check('page',6),tap(205,76),check('event_labels',1),tap(205,76),check('event_labels',0),'finish']
    result['events']=(scene(*event),{})
    clipped=event_base+[tap(60,120),'wait 70','set signal 2','wait 67',check('event_ready',1),check('event_count',64),check('event_flags',1),'times',tap(120,162),check('event_flags',3),'snapshot whole-event',tap(60,202),check('examples',1),check('positive',1),'finish']
    result['whole-event']=(scene(*clipped),{})
    slow=event_base+[tap(60,120),'wait 70','set slow 900','set signal 2','wait 12','set signal 0','wait 8',check('event_ready',1),'times',tap(60,190),check('examples',1),'nav 1','nav 1','nav 1','nav 32',check('running',0),'remember captures','remember history','wait 8','same captures','same history','finish']
    result['slow-presentation']=(scene(*slow),{})
    for failure in [1,2,3]:
        save=event_base+[tap(60,120),'wait 70','set signal 2','wait 12','set signal 0','wait 8',check('event_ready',1),'set storage_failure '+str(failure),tap(60,190)]
        if failure==1:
            save += [check('event_pending',0),check('examples',1),check('app_writes',2),'set storage_failure 0',tap(50,200),check('event_pending',-1),check('examples',1),check('app_writes',3),'finish']
        elif failure==3:
            save += [check('event_pending',0),check('examples',1),check('app_writes',2),tap(50,200),check('event_pending',-1),check('examples',1),check('app_writes',2),'finish']
        result[{1:'event-full',2:'retained-storage',3:'event-unknown'}[failure]]=(scene(*save),{})
    result['event-storage-absent']=(scene('wait 3',check('event_files_ready',0),check('neural_active',0),*events(),tap(120,200),check('page',6),check('event_labels',0),'nav 1','nav 1','finish'),{'RF_RENDER_NO_STORAGE':'1'})
    neural=['wait 2',check('neural_active',0),check('neural_updates',0),'nav 2',check('running',0),'set poll_ms 1','wait 50',check('neural_state',2),check('neural_epoch',0,'ge'),check('neural_active',0),'remember neural_updates','nav 2',check('running',1),'wait 10','same neural_updates','nav 2',check('running',0),tap(225,26),*sum((['wait 300','nav 16'] for _ in range(9)),[]),check('neural_state',5),check('neural_active',1),check('neural_epoch',256),check('neural_updates',1536),check('app_writes',1),'nav 1',*events(),'snapshot neural-ready','finish']
    result['neural']=(scene(*neural),{'RF_RENDER_NEURAL':'1'})
    frequency=['wait 70',check('ambient_ready',1),tap(137,120),tap(137,61),tap(120,119),*type_name('Carrier'),tap(167,185),check('labels',1),'wait 2',check('label_active',0),'set signal 2','wait 4',check('label_level_valid',1),check('label_active',1),'set signal -1','wait 4',check('label_active',0),'finish']
    result['frequency-labels']=(scene(*frequency),{})
    result['slow-history']=(scene('wait 3','set slow 900','wait 30','equal history captures','equal canonical captures','equal transforms captures','nav 32','remember captures','remember history','wait 6','same captures','same history','snapshot slow-history','finish'),{})
    for mode in range(1,5):
        result['malformed-'+str(mode)]=(scene('wait 3',check('running',0),check('captures',0),check('capture_error',1),'finish'),{'RF_RENDER_MALFORMED':str(mode)})
    for policy in ['airplane','unread']:
        result['radio-policy-'+policy]=(scene('wait 3',check('running',0),check('captures',0),check('radio_acquires',0),'finish'),{'RF_RENDER_POLICY':policy})
    result['sleep-stop']=(scene('wait 3','set jump 61000','wait 1',check('sleep',1),check('running',0),'remember captures','wait 5','same captures','nav 2',check('running',1),'finish'),{'RF_RENDER_SLEEP':'1'})
    result['release-reentry']=(scene('wait 3',check('captures',3,'ge'),check('running',1),'finish'),{'RF_RENDER_RELEASE_RETRY':'1','RF_RENDER_REENTRY':'1'})
    result['trace-dedup']=(scene('wait 100',check('captures',100,'ge'),check('diag_captures',1),check('diag_stages',1),check('diag_total',8,'le'),'finish'),{})
    for traced in [False,True]:
        result['diagnostics-traced' if traced else 'diagnostics']=(scene('wait 3',check('running',0),check('capture_error',1),check('diag_details',1),check('diag_dumps',1),check('diag_stages',3 if traced else 0),'finish'),dict(RF_RENDER_DIAGNOSTICS='1',RF_RENDER_RF_FAILURE='4',**({'RF_RENDER_TRACED':'1'} if traced else {})))
    result['logger-optional']=(scene('wait 3',check('running',1),check('captures',3,'ge'),'finish'),{'RF_RENDER_NO_DIAGNOSTIC':'1'})
    result['keyboard-cancel']=(scene('wait 3',tap(120,120),tap(120,61),tap(120,119),check('page',3),key_tap(1),check('name_length',1),'raw 20 20',check('page',2),check('name_length',0),tap(50,185),check('page',0),check('labels',0),'finish'),{})
    result['touch-exit']=(scene('wait 3','nav 32',check('running',0),tap(200,height-12),check('launches',1),check('running',0),tap(200,height-12)),{'RF_RENDER_LAUNCH_REFUSE':'1','RF_RENDER_EXPECT_LAUNCH':'2'})
    result['logger-optional-legacy']=(scene('wait 3',check('running',1),check('captures',3,'ge'),'finish'),{'RF_RENDER_NO_DIAGNOSTIC':'1','RF_RENDER_DIAGNOSTICS':'1','RF_RENDER_TRACED':'1'})
    result['root-exit']=(scene('wait 3','nav 1'),{'RF_RENDER_EXPECT_LAUNCH':'1'})
    discard=event_base+[tap(60,120),'wait 70','set signal 2','wait 12','set signal 0','wait 8',check('event_ready',1),'set storage_failure 1',tap(60,190),check('event_pending',0),check('examples',1),tap(175,200),check('event_pending',0),tap(175,200),check('event_pending',-1),check('examples',0),check('app_writes',2),'finish']
    result['event-discard']=(scene(*discard),{})
    profile=['wait 3']+samples()+[tap(150,207),check('page',5),tap(110,89),*type_name('Pulse'),tap(60,165),check('room_kind',2),check('room_frames',1),check('signature_pending',0),'finish']
    result['burst-profile']=(scene(*profile),{})
    scale=2 if target=='x4' else 1
    drag=['wait 3',*(f'point {x*scale} {120*scale}' for x in [100,140,180]),'wait 1',check('cursor',1),check('cursor_hz',2461000000,'ge'),check('cursor_hz',2462000000,'le'),tap(225,26),f'point {150*scale} {175*scale}',f'point {150*scale} {75*scale}','wait 1',check('scroll',100),check('gain',0),'snapshot controls-scroll','nav 1','finish']
    result['cursor-controls-drag']=(scene(*drag),{})
    return result


if a.scene:
    unknown=set(a.scene)-set(target_scenes('watch'))-{'runtime-storage'}
    if unknown:
        p.error('Unknown scenes: '+', '.join(sorted(unknown)))
completed_runs=0
for sanitized in (False, True):
    if sanitized and a.normal_only:
        continue
    for target, system in [('watch', a.system_apps), *([('x4', a.x4_system)] if a.x4_system else [])]:
        system = system.resolve()
        out = OUT / target / ('sanitized' if sanitized else 'normal')
        out.mkdir(parents=True, exist_ok=True)
        flags = ['-DPORTABLE_NOVA_UI', '-DPORTABLE_APP_OWNS_TOUCH_CHROME', '-DPORTABLE_RADIO_SESSION', '-DPORTABLE_ALARM_CLIENT', '-DPORTABLE_APP_SLEEP_LOCAL', '-DPORTABLE_FORCE_FULL_FRAMES', '-DPORTABLE_INPUT_NAVIGATION', '-DPORTABLE_INPUT_NAVIGATION_LOCAL', '-DRF_RETURN_APP="springboard.elf"', '-DRF_RENDER_RUNTIME_DIAGNOSTICS']
        san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitized else []
        if target == 'x4':
            flags += ['-DRF_RENDER_PAPER', '-DPORTABLE_DISPLAY_ROTATION=90']
        sources = [ROOT/'test/native_apps/rf_renderer_test.c', system/'lib/PortableApps/src/adapter.c']
        if a.watch and target == 'watch':
            flags += ['-DRF_RENDER_WATCH_TOUCH']
            watch = a.watch.resolve()
            for i, source in enumerate([ROOT/'test/native_apps/hid_watch_touch_backend.c', watch/'drivers/current/twatch_touch/driver.c']):
                obj = out/f'touch-{i}.o'
                run([os.environ.get('CC','cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *san, '-I'+str(watch/'sdk/app'), '-I'+str(watch/'sdk/driver'), '-I'+str(watch/'include'), '-c', source, '-o', obj])
                sources.append(obj)
        runtime = a.runtime.resolve()
        for i, source in enumerate([ROOT/'test/native_apps/rf_renderer_diagnostics.cpp', runtime/'src/ports/esp32s3/SleepDiagnostics.cpp']):
            obj = out/f'diagnostics-{i}.o'
            run([os.environ.get('CXX','c++'), '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *san, '-DRISC_SLEEP_DIAGNOSTICS=1', '-I'+str(runtime/'src'), '-I'+str(runtime/'test/diagnostic_shim'), '-c', source, '-o', obj])
            sources.append(obj)
        catalog = out/'catalog.c'
        catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
        sources.append(catalog)
        exe = out/'rf-renderer'
        run([os.environ.get('CC','cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *san, *flags, *['-I'+str(i) for i in [ROOT/'Apps', ROOT/'lib/Alarm/include', system/'lib/PortableApps/include',system/'lib/NativeApps/include']], *sources, '-lstdc++','-lm','-o',exe])
        for name,(commands,extra_env) in target_scenes(target).items():
            if a.scene and name not in a.scene:
                continue
            folder=out/name
            folder.mkdir(exist_ok=True)
            for stale in folder.glob('*.ppm'):
                stale.unlink()
            command_file=folder/'commands.txt'
            command_file.write_text(commands)
            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', **extra_env)
            if sanitized:
                env['RF_RENDER_NO_IMAGES']='1'
            run([exe,folder,command_file],env=env,timeout=120)
            completed_runs+=1
    if not a.scene or 'runtime-storage' in a.scene:
        runtime=a.runtime.resolve()
        out=OUT/'runtime'/('sanitized' if sanitized else 'normal')
        out.mkdir(parents=True,exist_ok=True)
        exe=out/'rf-runtime-storage'
        san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
        run([os.environ.get('CXX','c++'),'-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*san,'-I'+str(runtime/'src'),'-I'+str(runtime/'sdk/app'),ROOT/'test/native_apps/rf_app_integration_runtime.cpp',runtime/'src/runtime/storage/AppDataFiles.cpp','-Wl,--wrap=read,--wrap=write,--wrap=rename,--wrap=close','-lm','-o',exe])
        with tempfile.TemporaryDirectory(prefix='volume-',dir=out) as volume:
            run([exe,volume],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),timeout=120)
print(f'RF actual controller/adapter regression passed: {completed_runs} scene runs; Runtime storage included unless filtered; host simulation only.')
