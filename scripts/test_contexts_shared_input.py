#!/usr/bin/env python3
"""Contexts real scene/text hosts, ownership faults and display-independent input.

Host-only evidence. No target build, device operation or artifact delivery.
"""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
import shutil
import sys
from pathlib import Path
from build_contexts_scene import stage

ROOT = Path(__file__).resolve().parents[1]
PROFILES = ('watch', 'paper')
LEGACY = ('normal','storage','retained','no-input','event-no-input','delayed','save-failure','exit-home','exit-busy','exit-keyboard','exit-learning','exit-saving')
EDIT = ('done','cancel','header-back','home','header-home','native-home','limit','empty','delayed','frozen','frozen-home','frozen-back','hardware')
FAULT = ('missing','old-host','open-unavailable','open-busy','open-retained','open-token','acquire-dirty',
         'poll-retained','poll-error','poll-revision','poll-flags','poll-state','poll-unterminated','poll-control',
         'close-retained','close-error','release-fail','release-dirty','resume-fault',
         'loss-acquire','loss-open','loss-poll','loss-close','loss-release','loss-attention','loss-resume','loss-post-resume',
         'home-close','home-release','home-resume','home-post-resume')
CHECKPOINT = ('checkpoint-idle','checkpoint-presenting','checkpoint-input-busy','checkpoint-inhibit',
              'checkpoint-missing','checkpoint-enable-unavailable','checkpoint-enable-retained','checkpoint-enable-invalid',
              'checkpoint-snapshot-retained','checkpoint-snapshot-flags','checkpoint-snapshot-size','checkpoint-snapshot-paused',
              'checkpoint-pause-retained','checkpoint-pause-invalid','checkpoint-pause-again','checkpoint-activity-race','checkpoint-touch-race',
              'checkpoint-resume-retained','checkpoint-resume-invalid','checkpoint-policy-exit','checkpoint-policy-busy',
              'checkpoint-policy-retained','checkpoint-policy-size','checkpoint-policy-denied','checkpoint-policy-config','checkpoint-policy-redraw','checkpoint-paused-touch','checkpoint-policy-touch','checkpoint-policy-usb','checkpoint-resume-touch','checkpoint-policy-nav-home','checkpoint-policy-nav-back',
              'loss-checkpoint-enable','loss-checkpoint-snapshot','loss-checkpoint-pause','loss-checkpoint-policy','loss-checkpoint-resume',
              'home-checkpoint-pause','home-checkpoint-policy','home-checkpoint-resume',
              'home-checkpoint-enable','home-checkpoint-snapshot','home-checkpoint-poll',
              'checkpoint-poll-exit','checkpoint-poll-retained','checkpoint-poll-size','loss-checkpoint-poll',
              'checkpoint-layer','checkpoint-caps','checkpoint-held-usb',
              'checkpoint-backend-fingerprint-retained','checkpoint-backend-rules-retained',
              'checkpoint-no-marker','checkpoint-unknown-poll-flags','checkpoint-sleep-admission','checkpoint-continuity-violation',
              'loss-checkpoint-backend-pause','loss-checkpoint-backend-release','loss-checkpoint-backend-reacquire',
              'home-checkpoint-backend-pause','home-checkpoint-backend-release','home-checkpoint-backend-reacquire')



def build(runtime, system, tmp, profile, shared, sanitized, baseline=None):
    runtime,system = runtime.resolve(),system.resolve()
    include = stage(runtime, system, tmp / 'sdk')
    flags = ['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(include),'-I'+str(system/'test/scene'),'-I'+str(system/'Services/text_input')]
    if sanitized:
        flags += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    if profile == 'paper':
        flags += ['-DCONTEXTS_ALARM_API=2','-DCONTEXTS_SETTING_MASK=0x274u','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO']
    if profile == 'watch':
        flags += ['-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=1']
    else:
        generated = tmp / 'font-generation'
        font_dir = generated / 'Services/scene_host/fonts'
        font_dir.mkdir(parents=True)
        (generated/'scripts').mkdir()
        for source in (system/'Services/scene_host/fonts').glob('*.ttf'):
            shutil.copyfile(source,font_dir/source.name)
        generator = generated/'scripts/generate_telemetry_fonts.py'
        shutil.copyfile(system/'scripts/generate_telemetry_fonts.py',generator)
        subprocess.run([sys.executable,str(generator)],check=True)
        assert hashlib.sha256((font_dir/'telemetry.inc').read_bytes()).hexdigest() == '2d7501e42d6223ebbc191817715431182c435fea944a1a02166a0748a577eaf1'
        flags += ['-I'+str(generated/'Services/scene_host')]
    if not shared and baseline:
        flags += ['-DCONTEXTS_APP_SOURCE="'+str(baseline.resolve()/'Apps/contexts.c')+'"']
    cc = os.environ.get('CC','cc')
    objects = []
    if shared:
        obj = tmp / 'text.o'
        subprocess.run([cc,*flags,'-Dt5_driver_get=contexts_text_driver_get','-c',str(system/'Services/text_input/host.c'),'-o',str(obj)],check=True)
        objects += [str(obj)]
        flags += ['-DPORTABLE_CONTEXTS_SHARED_TEXT_INPUT']
    exe = tmp / 'app'
    subprocess.run([cc,*flags,'-DSCENE_HOST_SOURCE="'+str(system/'Services/scene_host/host.c')+'"',str(ROOT/'test/contexts/host_probe.c'),str(ROOT/'test/contexts/app_test.c'),*objects,'-o',str(exe)],check=True)
    return exe


def compact(frames, keep):
    result = {}
    for file in frames.iterdir():
        if file.suffix not in ('.ppm','.ctx'):
            continue
        raw = file.read_bytes()
        if file.name == 'paper-learning.ppm':
            header,body = raw.split(b'255\n',1)
            assert header == b'P6\n480 800\n'
            pixel = lambda x,y: body[(y*480+x)*3:(y*480+x+1)*3]
            assert pixel(331,236) == bytes((0,0,0)) and pixel(149,236) == bytes((255,255,255))
            assert pixel(240,145) == bytes((0,0,0))
        result[file.name] = hashlib.sha256(raw).hexdigest()
        if file.suffix == '.ppm' and file.stem in keep:
            from PIL import Image
            Image.open(file).save(file.with_suffix('.png'))
        file.unlink()
    return result


def run(args):
    out = args.output.resolve()
    out.mkdir(parents=True,exist_ok=True)
    def pin(path):
        if not path:
            return None
        return {'commit':subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip(),
                'tree':subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD^{tree}'],text=True).strip(),
                'working_tree_status':subprocess.check_output(['git','-C',str(path),'status','--porcelain'],text=True).splitlines()}
    if args.baseline_utilities:
        assert (ROOT/'Apps/contexts_controller.inc').read_bytes() == (args.baseline_utilities/'Apps/contexts_controller.inc').read_bytes(), 'Contexts controller changed'
    files = ['Apps/contexts.c','Apps/contexts_scene_view.inc','Apps/contexts_controller.inc','Apps/contexts_text_input.inc',
             'scripts/test_contexts_shared_input.py','test/contexts/app_test.c','test/contexts/host_probe.c',
             'test/contexts/shared_input_fixture.h','test/contexts/shared_input_fixture.inc']
    evidence = {'physical_testing':'not performed','scenarios':[],'parity':{},
                'sources':{'utilities':pin(ROOT),'system':pin(args.system_apps),'watch_runtime':pin(args.runtime),
                           'x4_runtime':pin(args.x4_runtime),'baseline_utilities':pin(args.baseline_utilities)},
                'source_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in files},
                'sanitizer_options':os.environ.get('ASAN_OPTIONS','default')}
    for sanitized in ((False,True) if args.sanitize else (False,)):
        flavor = 'sanitized' if sanitized else 'normal'
        for profile in PROFILES:
            parity = {}
            for shared in ((True,) if args.only else (False,True)):
                kind = 'shared' if shared else 'legacy'
                with tempfile.TemporaryDirectory(prefix='contexts-shared-test-') as directory:
                    tmp = Path(directory)
                    runtime = args.x4_runtime if profile == 'paper' and args.x4_runtime else args.runtime
                    exe = build(runtime,args.system_apps,tmp,profile,shared,sanitized,args.baseline_utilities)
                    frames = out / flavor / profile / kind
                    frames.mkdir(parents=True,exist_ok=True)
                    cases = list(LEGACY) if not args.only else []
                    if shared:
                        cases += [f'text-{reason}-{mode}' for reason in range(5) for mode in EDIT]
                        cases += [f'text-2-{mode}' for mode in FAULT+CHECKPOINT]
                        cases += [f'text-{reason}-{mode}' for reason in (0,1,3,4) for mode in ('checkpoint-idle','home-checkpoint-resume')]
                        cases += ['text-2-retry','text-2-reopen','text-2-prior-edit-cancel','text-0-learn-save']
                        cases += [f'text-{reason}-explicit-save' for reason in (2,3,4)]
                        if args.only:
                            cases = args.only
                    env = dict(os.environ)
                    for case in cases:
                        result = subprocess.run([str(exe),profile,str(frames),case],timeout=30,env=env,text=True,capture_output=True)
                        if result.returncode:
                            print(result.stderr,file=sys.stderr,flush=True)
                            result.check_returncode()
                        print(result.stdout,end='',flush=True)
                        evidence['scenarios'].append({'flavor':flavor,'profile':profile,'implementation':kind,'case':case,'result':'PASS'})
                    parity[kind] = compact(frames,{profile+'-home',profile+'-keyboard',profile+'-learning'})
            if not args.only:
                matching = []
                for file, digest in parity['legacy'].items():
                    if file.endswith('-keyboard.ppm'):
                        continue
                    assert parity['shared'].get(file) == digest, f'Non-keyboard frame/storage parity failure: {flavor}/{profile}/{file}'
                    matching.append(file)
                evidence['parity'][flavor+'/'+profile] = {'matched':matching,'sha256':{name:parity['shared'][name] for name in matching}}
                print(f'{flavor}/{profile}: {len(matching)} non-keyboard frame/storage hashes identical',flush=True)
    (out/'results.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(f'{len(evidence["scenarios"])} Contexts scenarios PASS')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('runtime','system-apps','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--baseline-utilities',type=Path,help='Compile the exact earlier Contexts Apps sources for parity')
    parser.add_argument('--x4-runtime',type=Path,help='Stage Paper headers from the selected X4 source')
    parser.add_argument('--sanitize',action='store_true',help='Run both ordinary and ASan/UBSan suites')
    parser.add_argument('--only',nargs='+',help='Run selected shared scenarios while developing tests')
    run(parser.parse_args())
