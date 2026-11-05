#!/usr/bin/env python3
"""Full-feature actual Alarms/Countdown temporal controls.

A release run requires an exact target receipt. --unbound-diagnostic builds the
same host controls from immutable input Git objects, but produces explicitly
unbound evidence which cannot pass the release gate. No target instructions or
hardware are executed. Original means the unchanged Utilities baseline editor,
with the current pinned shared adapter, not historical .59 firmware.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('alarms_recipe', ROOT/'scripts/build_native_alarms_qualification.py')
recipe = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(recipe)
PROFILE = ROOT/'profiles/native-alarms-controls.json'
STAGED_PATHS = {
    'system': ['lib/PortableApps', 'lib/NativeApps/include', 'Apps/alarm_writer.h',
               'Apps/SpringboardPresentation.h', 'Apps/PaperPresentation.h', 'Apps/PaperFrame.h'],
    'bridges': ['apps/clock/portable_navigation.c', 'apps/clock/portable_sleep.c',
                'apps/clock/watch_sleep.h', 'apps/clock/watch_alarm_sleep.h',
                'apps/clock/watch_motion_client.h', 'apps/clock/faces/ContextsFaces.h',
                'apps/clock/faces/names.h', 'sdk/app', 'sdk/driver', 'include'],
    'runtime': ['sdk/app'],
    'profile_source': ['profiles/shared-editor-inputs/watch-alarm-profile.h',
                       'profiles/shared-editor-inputs/AlarmServicePinnedV1.h'],
}
BASELINE_PATHS = ['Apps/'+n for n in ('alarms.c', 'countdown.c', 'alarm_app_shared.h',
    'alarm_nova.inc', 'daily_draw.h', 'alarms_paper.inc', 'countdown_paper.inc',
    'alarm_writer.h', 'utility_frame.h', 'utility_preference_atomic.h')]+['lib/Alarm/include', 'lib/Contexts/include']


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    bind = parser.add_mutually_exclusive_group(required=True)
    bind.add_argument('--input-receipt', type=Path)
    bind.add_argument('--unbound-diagnostic', action='store_true')
    for owner in recipe.PINS:
        parser.add_argument('--'+owner.replace('_', '-'), type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=('temporal','snapshot','selector-off','original'), required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists() and any(out.iterdir()):
        raise ValueError('Use an empty output directory; stale evidence is refused')
    out.mkdir(parents=True, exist_ok=True)
    qualification_files = [Path(__file__).resolve(), PROFILE, ROOT/'scripts/build_native_alarms_qualification.py']
    qualification_hashes = {str(p.relative_to(ROOT)): recipe.digest(p.read_bytes()) for p in qualification_files}
    contract = next(c for c in read(PROFILE)['temporal'] if c['mode'] == args.mode)
    native = args.mode in ('temporal','snapshot')
    target = read(args.input_receipt) if args.input_receipt else None
    capabilities = read(ROOT/recipe.PROFILE_PATH)['requires']
    if target is not None and (target['external_git_pins'] != recipe.PINS or target['required_capabilities'] != capabilities):
        raise ValueError('Target receipt differs from complete pinned capability contract')
    target_hash = recipe.digest(args.input_receipt.read_bytes()) if target else ''
    repos = {owner:getattr(args, owner).resolve() for owner in recipe.PINS}
    results, snapshots, commands, binaries = [], {}, {}, {}
    closure = {'utilities:'+relative:digest for relative,digest in qualification_hashes.items()}
    with tempfile.TemporaryDirectory(prefix='alarms-temporal-', dir=out) as temporary:
        temporary = Path(temporary)
        identities = {}
        supplied = args.input_receipt.resolve().parent/'inputs' if target else None
        if supplied and supplied.is_dir():
            roots={owner:supplied/owner for owner in recipe.PINS}
            baseline=supplied/'utilities-baseline'
            for owner,root in {**roots,'utilities-baseline':baseline}.items():
                for path in root.rglob('*'):
                    if path.is_file():identities[path.resolve()]=(owner,str(path.relative_to(root)))
        else:
            roots = {owner:temporary/owner for owner in recipe.PINS}
            for owner, paths in STAGED_PATHS.items():
                recipe.stage(repos[owner], recipe.PINS[owner], roots[owner], paths, identities, owner)
            baseline = temporary/'utilities-baseline'
            recipe.stage(ROOT, recipe.BASE, baseline, BASELINE_PATHS, identities, 'utilities-baseline')
            for name in ('RiscRuntimeV1.h','RiscKeyValueAtomicV1.h'):
                path=roots['system']/'lib/PortableApps/include'/name
                path.write_bytes((roots['runtime']/'sdk/app'/name).read_bytes())
        app_root = baseline if args.mode == 'original' else ROOT
        for name in ('RiscRuntimeV1.h','RiscKeyValueAtomicV1.h'):
            path = roots['system']/'lib/PortableApps/include'/name
            identities[path.resolve()] = ('runtime','sdk/app/'+name)
        includes = [roots['system']/'lib/PortableApps/include', roots['system']/'lib/NativeApps/include',
                    app_root/'Apps', baseline/'Apps', baseline/'lib/Alarm/include', baseline/'lib/Contexts/include',
                    roots['profile_source']/'profiles/shared-editor-inputs', roots['runtime']/'sdk/app',
                    roots['bridges']/'sdk/app', roots['bridges']/'sdk/driver', roots['bridges']/'include']
        base_flags = recipe.define_arguments(native)+['-I'+str(path) for path in includes]
        if not native:
            base_flags += ['-DALARMS_EXPECT_ORIGINAL=1']
        if args.mode == 'snapshot':
            base_flags += ['-DPORTABLE_RASTER_SNAPSHOT']
        for name,path in {'PERIPHERALS':ROOT/'test/native_apps/alarms_native_temporal_peripherals.h',
                          'ATOMIC':ROOT/'tests/utility_atomic_transport.h',
                          'ADAPTER':roots['system']/'lib/PortableApps/src/adapter.c'}.items():
            base_flags += ['-DALARMS_DIAG_'+name+'='+json.dumps(str(path))]
        for item in recipe.FORCED:
            owner,relative=item.split(':',1)
            base_flags += ['-include',str(roots[owner]/relative)]
        sources = [ROOT/'test/native_apps/alarms_native_temporal_test.c',
                   ROOT/'test/native_apps/alarms_native_temporal_adapter.c',
                   *[roots['bridges']/'apps/clock'/n for n in ('portable_navigation.c','portable_sleep.c')],
                   *[roots['system']/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]]
        cc = os.environ.get('CC','cc')
        for app in recipe.APPLICATIONS:
            flags=base_flags+['-DALARMS_DIAG_APP='+json.dumps(str(app_root/'Apps'/(app+'.c')))]
            app_closure = {}
            for source in sources:
                raw=subprocess.check_output([cc,'-std=gnu11','-MM',*flags,str(source)],text=True)
                for token in shlex.split(raw.replace('\\\n',' ').split(':',1)[1]):
                    path=Path(token).resolve(); data=path.read_bytes()
                    if path in identities:
                        owner,relative=identities[path]
                    elif path.is_relative_to(ROOT):
                        owner,relative='utilities',str(path.relative_to(ROOT))
                    else:
                        raise ValueError('Unowned compiled dependency: '+str(path))
                    if owner=='utilities-baseline':
                        expected=recipe.git(ROOT,'show',recipe.BASE+':'+relative);owner='utilities'
                    elif owner in repos:
                        expected=recipe.git(repos[owner],'show',recipe.PINS[owner]+':'+relative)
                    else:
                        expected=data
                    if data!=expected:
                        raise ValueError('Compiled external dependency differs from pin: '+owner+':'+relative)
                    identity=owner+':'+relative; digest=recipe.digest(data)
                    if identity in closure and closure[identity]!=digest:
                        raise ValueError('Conflicting compiled source: '+identity)
                    snapshots[path]=digest;closure[identity]=app_closure[identity]=digest
            required=recipe.required_closure(app)|(recipe.NATIVE_CLOSURE if native else set())
            if not required<=app_closure.keys():
                raise ValueError('Production feature closure missing: '+repr(required-app_closure.keys()))
            if target and args.mode!='original':
                matches=[r for r in target['modules'] if r['app']==app and r['mode']==('native' if native else 'legacy')]
                if len(matches)!=1:
                    raise ValueError('Missing/duplicate exact target module: '+app)
                expected=matches[0]['source_sha256']
                if not expected.keys()<=app_closure.keys():
                    raise ValueError('Target source absent from host: '+repr(expected.keys()-app_closure.keys()))
                for identity,digest in expected.items():
                    if app_closure[identity]!=digest:
                        raise ValueError('Compiled source differs from target: '+identity)
            binary=temporary/(app+'-probe')
            command=[cc,'-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',
                     '-Wno-unused-variable','-Wno-misleading-indentation',*flags]
            if args.sanitize:
                command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
            command += [*map(str,sources),'-Wl,--wrap=portable_app_alarm_sleep','-o',str(binary)]
            commands[app]=command
            (out/(app+'-command.json')).write_text(json.dumps(command,indent=2)+'\n')
            with (out/(app+'-build.log')).open('w') as log:
                subprocess.run(command,check=True,stdout=log,stderr=subprocess.STDOUT)
            binaries[app]=recipe.digest(binary.read_bytes())
            for case in contract['cases']:
                for width,height in contract['sizes']:
                    p=subprocess.run([str(binary),str(case),str(width),str(height)],text=True,
                        stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60,
                        env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
                    logname=f'{app}-case{case}-{width}x{height}.log'
                    (out/logname).write_text(p.stdout)
                    results.append({'app':app,'case':case,'width':width,'height':height,'exit_code':p.returncode,
                                    'log':logname,'log_sha256':recipe.digest(p.stdout.encode())})
            print(app+': '+str(sum(x['exit_code']==0 for x in results if x['app']==app))+' controls passed',flush=True)
        for path,expected in snapshots.items():
            if recipe.digest(path.read_bytes())!=expected:
                raise ValueError('Source changed during qualification: '+str(path))
    if target and recipe.digest(args.input_receipt.read_bytes())!=target_hash:
        raise ValueError('Target receipt changed during qualification')
    if args.mode=='original' and target:
        expected=contract.get('source_sha256',{})
        if not expected or not expected.keys()<=closure.keys() or any(closure[k]!=v for k,v in expected.items()):
            raise ValueError('Original control differs from exact source contract')
    for relative, expected in qualification_hashes.items():
        if recipe.digest((ROOT/relative).read_bytes()) != expected:
            raise ValueError('Qualification source changed during run: '+relative)
    receipt={'schema':1,'mode':args.mode,'sanitized':args.sanitize,'async_rgb565':True,
             'snapshot':args.mode=='snapshot','source_unchanged':True,
             'target_receipt_sha256':target_hash,'unbound_diagnostic':not bool(target),
             'external_git_pins':recipe.PINS,'required_capabilities':capabilities,
             'utilities_original':recipe.BASE if args.mode=='original' else None,
             'build_defines':recipe.define_arguments(native),
             'qualification_source_sha256':qualification_hashes,
             'compiled_dependency_sha256':dict(sorted(closure.items())),
             'binary_sha256':binaries,'commands':commands,'results':results,
             'hardware_tested':False,'target_instructions_executed':False,
             'limitations':['Modeled providers; actual controller, adapter, Quick, navigation and sleep bridge code.',
                            'Original is the pre-native Utilities editor with current external pins, not historical .59 firmware.',
                            'LeakSanitizer disabled under ptrace; ASan/UBSan and explicit custody checks remain enabled.']}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    if any(r['exit_code'] for r in results):
        raise RuntimeError('Temporal controls failed; inspect receipt and case logs')
    print(f'Alarms/Countdown {args.mode}: {len(results)} PASS; sanitized={args.sanitize}; bound={bool(target)}')


if __name__=='__main__':
    main()
