"""Utility bindings for the explicitly selected shared System X4 idle builder.
The System helper owns the policy and SDK contract; this module owns utility
versions, exact physical grants, and the final compiled-source receipt.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import subprocess

SYSTEM = '7b175418e3063d9f4c571acf06c366144831b2af'
RUNTIME = 'c77e717571a80bc18009b9c3a98532fee32e6fd2'
VERSIONS = {'battery':'1.1.9', 'calculator':'0.1.16', 'stopwatch':'0.1.16',
            'countdown':'0.1.15', 'alarms':'0.2.11', 'ble_scanner':'0.2.11',
            'ble_touchpad':'0.1.11', 'ble_buttons':'0.1.11', 'waterfall':'0.2.7'}

def options(parser):
    parser.add_argument('--x4-idle-source', type=Path, help='Explicit product reversible Light helper')
    parser.add_argument('--x4-idle-sdk', type=Path, help='Frozen typed product driver SDK')
    parser.add_argument('--x4-idle-runtime-sdk', type=Path, help='Matching Runtime driver SDK')

def selected(args):
    return bool(getattr(args, 'x4_idle_source', None))

def git(root, *args):
    return subprocess.check_output(['git','-C',str(root),*args],text=True).strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def validate(args, parser, system, runtime):
    values = [getattr(args, name, None) for name in ('x4_idle_source','x4_idle_sdk','x4_idle_runtime_sdk')]
    if any(values) and not all(values):
        parser.error('X4 idle requires explicit helper, product SDK and Runtime SDK')
    if not any(values):
        return
    if git(system,'rev-parse','HEAD') != SYSTEM or git(system, 'status','--porcelain','--untracked-files=no'):
        parser.error('X4 idle requires a clean committed shared System checkout')
    if git(runtime, 'rev-parse','HEAD') != RUNTIME or git(runtime,'status','--porcelain','--untracked-files=no'):
        parser.error('X4 idle requires clean Runtime '+RUNTIME)
    if args.x4_idle_runtime_sdk.resolve() != (runtime/'sdk/driver').resolve():
        parser.error('X4 idle Runtime SDK must belong to the selected Runtime checkout')
    if not (system/'scripts/portable_idle_build.py').is_file():
        parser.error('Selected System has no shared portable_idle_build.py')

def configure(args, parser, system, output, base):
    if not selected(args):
        return [], [], base
    path = system/'scripts/portable_idle_build.py'
    spec = importlib.util.spec_from_file_location('system_portable_idle_build',path)
    shared = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(shared)
    # The native utility profile supplies these concrete features below.
    config = argparse.Namespace(**vars(args))
    config.paper_transitions = config.quick_actions = config.quick_radios = config.alarm_client = True
    config.tagged_alarm_utilities = Path(__file__).resolve().parents[1]
    config.time_profile = 'x4-native-time'
    defines, sources = shared.configure(config,parser,system,output,base)
    include = Path(config.x4_idle_receipt['compiled_include_directory'])
    args.x4_idle_receipt = config.x4_idle_receipt
    args.x4_idle_receipt['builder_sha256'] = sha(Path(__file__))
    args.x4_idle_receipt['system_source_revision'] = git(system,'rev-parse','HEAD')
    args.x4_idle_receipt['runtime_source_revision'] = RUNTIME
    args.x4_idle_receipt['final_sdk_sha256'] = {p.name:sha(p) for p in sorted(include.glob('*.h'))}
    return defines+['-DPORTABLE_QUICK_RADIOS'], sources+[system/'lib/PortableApps/src/quick_radios.c'], include

def version(args, name, original):
    return VERSIONS[name] if selected(args) else original

def grants(args, original):
    result = [dict(item) for item in original]
    if selected(args):
        result = [{key:g[key] for key in ('capability','api','instance_id')} for g in result]
        # Runtime selector 0 means the unique authorized provider; the boot
        # grant names its physical instance, shared by scanner and sleep.
        for item in result:
            if item['capability']=='bluetooth.hci':
                item['instance_id']=16
        for name, instance in [('x4.power',17),('storage.volume',9),('net.wifi',15),('bluetooth.hci',16)]:
            item = {'capability':name,'api':1,'instance_id':instance}
            if not any(all(old.get(k)==v for k,v in item.items()) for old in result):
                result.append(item)
    keys = [(g['capability'],g['api'],g['instance_id']) for g in result]
    assert len(keys)==len(set(keys)) and len(keys)<=16
    return result

def record(args, receipt, defines, required_grants):
    if not selected(args):
        return
    receipt['idle_policy'] = args.x4_idle_receipt
    root = Path(__file__).resolve().parents[1]
    receipt['build_source_sha256'] = {name:sha(root/name) for name in ('scripts/native_idle_build.py','scripts/build_native_broadcast.py','scripts/build_native_utc_alarm_apps.py','scripts/build_battery_power.py')}
    receipt['source_dirty'] = bool(git(root,'status','--porcelain'))
    receipt['build_defines'] = defines
    receipt['required_grants'] = required_grants
    receipt['grant_bindings'] = {}
    for g in required_grants:
        receipt['grant_bindings'].setdefault(g['capability'],[]).append(g['instance_id'])
    receipt['sdk_sha256'].update(args.x4_idle_receipt['sdk_sha256'])
    receipt['automatic_idle_light'] = True
    receipt['preferences'] = {'instance':1,'idle_timer_key':'sleep_idle','unused_deep_timer_key':'sleep_deep'}

def dependencies(cc, defines, includes, sources, roots):
    result = {}
    for source in sources:
        dep = subprocess.check_output([cc,'-std=c11','-M',*defines,*includes,source],text=True).replace('\\\n',' ')
        for token in dep.split()[1:]:
            path = Path(token).resolve()
            for label, base in roots:
                if path.is_relative_to(base):
                    result[label+'/'+str(path.relative_to(base))] = sha(path)
                    break
            else:
                if path.is_file():
                    result['External/'+str(path)] = sha(path)
    return result
