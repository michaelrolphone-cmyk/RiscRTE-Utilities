#!/usr/bin/env python3
"""Local full-feature Alarms qualification. Native UI requires --native-ui.

External inputs are read from immutable Git objects, never their working trees.
No production manifest, package, install catalog or firmware is generated.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PROFILE_PATH = 'profiles/native-alarms-qualification.json'
BASELINE_MANIFEST_SHA256 = '0977a98cbf3c799ca13a89a9c6adce473b8f7538738c9148b019bcda03c02011'
BASE = '4fdcad2495c33fe63f5f6faca9b11ced3c739a5b'
PINS = {
    'system': '02f65827d524ec7b1bd6a5d0576a9d731cc589d5',
    'bridges': '6e82fdcaec0b73ed700d2104cfb4d94f011c19d4',
    'runtime': '48229bd1593a2e24e95bf4537b495ee615990fd3',
    'profile_source': 'b9862e1379813a0914d4926ef59a3feb6fcf656a',
}
CAPABILITIES = (('display.output', 1), ('input.touch.raw', 1), ('rtc.clock', 2), ('storage.key-value', 1), ('alarm.service', 1), ('telemetry.broadcast', 1), ('contexts.service', 1), ('input.power-key', 1), ('power.sleep', 1), ('display.power', 1), ('board.battery', 1), ('net.wifi', 1), ('bluetooth.hci', 1), ('motion.accel', 1), ('storage.key-value.atomic', 1))
NATIVE = ['PORTABLE_ALARMS_NATIVE_UI', 'PORTABLE_NATIVE_CANVAS', 'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_QUICK_NATIVE', 'PORTABLE_ALARM_NATIVE']
APPLICATIONS = ('alarms', 'countdown')
BASELINE_MANIFESTS = {'alarms': '0977a98cbf3c799ca13a89a9c6adce473b8f7538738c9148b019bcda03c02011', 'countdown': '7f3571ae3429bae2baf5eaf43c79e75a64048d872a1f340b8fa121ed9f63d148'}
EXPORTS = {'app_main', 'app_module_init', 'app_module_fini'}
IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp',
           'strlen', 'snprintf', 'malloc', 'calloc', 'free', 'strcpy',
           'memchr', 'strncmp'}
COMMON = [
    'PORTABLE_LOW_BATTERY', 'PORTABLE_BLE_BROADCAST', 'PORTABLE_TOUCH_ROTATION=0',
    'PORTABLE_RTC_UTC8_DENVER', 'PORTABLE_FORCE_FULL_FRAMES',
    'PORTABLE_INPUT_NAVIGATION', 'PORTABLE_INPUT_NAVIGATION_LOCAL',
    'PORTABLE_APP_SLEEP_LOCAL', 'PORTABLE_ALARM_CLIENT', 'PORTABLE_QUICK_ACTIONS',
    'PORTABLE_QUICK_RADIOS', 'PORTABLE_MOTION_WAKE', 'WATCH_ALARM_SLEEP_RESUME',
    'PORTABLE_NOVA_UI', 'PORTABLE_POWER_STATUS', 'ALARM_RETURN_APP="springboard.elf"',
    'PORTABLE_CONTEXTS_CLIENT', 'PORTABLE_NAVIGATION_RETENTION',
    'PORTABLE_ASYNC_RGB565', 'PORTABLE_WATCH_NATIVE_HEIGHT',
    'PORTABLE_RUNTIME_APP_CATALOG', 'PORTABLE_CHECKED_STORAGE_RUNTIME',
    'PORTABLE_NATIVE_CUSTODY_FENCE', 'PORTABLE_UTILITIES_ATOMIC_PREFERENCES',
    'PORTABLE_BROADCAST_RESULT_ONLY', 'PORTABLE_QUICK_ATOMIC_PREFERENCES',
    'PORTABLE_HOME_AUTOMATIC_PREFERENCES', 'PORTABLE_ALARM_TERMINAL_RETENTION',
]
SOURCES = ['utilities:Apps/alarms.c', 'system:lib/PortableApps/src/adapter.c',
           'bridges:apps/clock/portable_navigation.c', 'bridges:apps/clock/portable_sleep.c',
           *['system:lib/PortableApps/src/'+name+'.c' for name in
             ('quick_actions', 'quick_render', 'quick_session', 'quick_radios')]]
FORCED = ['system:lib/PortableApps/include/PortableNativeCustody.h',
          'bridges:apps/clock/faces/ContextsFaces.h',
          'profile_source:profiles/shared-editor-inputs/watch-alarm-profile.h']
REQUIRED_CLOSURE = {
    'utilities:Apps/alarm_app_shared.h', 'utilities:Apps/alarm_nova.inc',
    'utilities:Apps/alarm_writer.h', 'utilities:Apps/daily_draw.h', 'utilities:Apps/utility_frame.h',
    'system:lib/PortableApps/src/broadcast_adapter.inc',
    'system:lib/PortableApps/src/contexts_adapter.inc',
    'system:lib/PortableApps/src/quick_atomic.inc',
    'system:lib/PortableApps/src/native_custody_adapter.inc',
    'runtime:sdk/app/RiscRuntimeV1.h', 'runtime:sdk/app/RiscKeyValueAtomicV1.h',
    *SOURCES,
}
FLAGS = ['-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
         '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib',
         '-nostartfiles', '-shared', '-Wl,--gc-sections', '-Wl,--no-relax',
         '-Wl,--hash-style=sysv', '-ffunction-sections', '-fdata-sections',
         '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation']


NATIVE_CLOSURE = {
    'utilities:Apps/alarms_native_layout.h', 'utilities:Apps/alarms_native_view.inc',
    'system:lib/PortableApps/include/PortableAlarmsNative.h',
    'system:lib/PortableApps/src/alarms_native_input.inc',
    'system:lib/PortableApps/src/alarm.inc', 'system:lib/PortableApps/src/alarm_native.inc',
    'system:lib/PortableApps/src/quick_native_render.inc',
}


def app_sources(app_name):
    if app_name not in APPLICATIONS:
        raise ValueError('Unknown application')
    return [SOURCES[0].replace('alarms.c', app_name+'.c'), *SOURCES[1:]]


def required_closure(app_name):
    return (REQUIRED_CLOSURE - {SOURCES[0]}) | set(app_sources(app_name))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def validate_profile(profile, baseline_bytes):
    if profile['applications'] != list(APPLICATIONS):
        raise ValueError('Both Alarms and Countdown controls are required')
    if profile['pins'] != PINS or profile['utilities_base'] != BASE:
        raise ValueError('Immutable qualification pins changed')
    if profile['default'] != 'legacy' or profile['native_selectors'] != NATIVE:
        raise ValueError('Native UI must remain an explicit opt-in')
    if profile['common_defines'] != COMMON:
        raise ValueError('Complete feature selectors changed')
    if profile['translation_units'] != SOURCES or profile['forced_headers'] != FORCED:
        raise ValueError('Complete feature source selection changed')
    if (digest(baseline_bytes) != BASELINE_MANIFEST_SHA256 or
            profile['baseline_manifest']['sha256'] != BASELINE_MANIFEST_SHA256):
        raise ValueError('Exact .59 manifest bytes changed')
    expected = [{'capability': name, 'api': api} for name, api in CAPABILITIES]
    if profile['requires'] != expected or json.loads(baseline_bytes)['requires'] != expected:
        raise ValueError('The exact fifteen capability types/versions must be preserved')
    lifecycle = profile['lifecycle']
    if (not lifecycle['automatic_idle'] or not lifecycle['manual_sleep'] or
            lifecycle['manual_only_suppression'] or not lifecycle['shared_idle_correction_included']):
        raise ValueError('Inherited idle behavior must remain selected and disclosed')
    if profile['historical_reproduction'] or profile['hardware_qualified'] or profile['published']:
        raise ValueError('Local qualification cannot declare release or hardware parity')


def stage(repo, pin, destination, paths, identities, owner):
    """Copy only selected Git blobs. Local checkout state is irrelevant."""
    listing = git(repo, 'ls-tree', '-r', '--name-only', pin, '--', *paths).decode().splitlines()
    if not listing:
        raise ValueError('Missing immutable inputs: '+owner)
    for relative in listing:
        target = destination/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(git(repo, 'show', pin+':'+relative))
        identities[target.resolve()] = (owner, relative)


def symbol_contract(text):
    """Reject undefined weak symbols as well as undeclared strong imports."""
    imports, exports = set(), set()
    for line in text.splitlines():
        fields = line.split()
        if len(fields) < 2:
            continue
        kind, name = fields[-2:]
        if kind in ('w', 'v'):
            raise ValueError('Undefined weak import is not admitted: '+name)
        if kind == 'U':
            imports.add(name)
        elif kind in ('T', 'D', 'B', 'R', 'W', 'V', 'A', 'S', 'G'):
            exports.add(name)
    if not imports <= IMPORTS or exports != EXPORTS:
        raise ValueError('Unexpected imports/exports: '+repr((imports-IMPORTS, exports)))
    return sorted(imports), sorted(exports)


def elf_contract(data):
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected ELF32 little-endian Xtensa ET_DYN')


def define_arguments(native=False):
    return ['-D'+name for name in COMMON+(NATIVE if native else [])]


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    for name, pin in PINS.items():
        parser.add_argument('--'+name.replace('_', '-'), type=Path, required=True,
                            help='Git object repository containing '+pin+' (working tree unused)')
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--native-ui', action='store_true', help='Also qualify the explicit native Alarms UI')
    parser.add_argument('--xtensa-cc', help='Existing ESP32-S3 compiler; no tool download')
    return parser


def build(args):
    profile_bytes = (ROOT/PROFILE_PATH).read_bytes()
    profile = json.loads(profile_bytes)
    manifest_path = profile['baseline_manifest']['path']
    baseline_bytes = (ROOT/manifest_path).read_bytes()
    validate_profile(profile, baseline_bytes)
    for app_name, expected in BASELINE_MANIFESTS.items():
        item = profile['baseline_manifests'][app_name]
        content = (ROOT/item['path']).read_bytes()
        if digest(content) != expected or item['sha256'] != expected or json.loads(content)['requires'] != profile['requires']:
            raise ValueError('Exact '+app_name+' baseline manifest changed')
    repos = {owner: getattr(args, owner).resolve() for owner in PINS}
    for owner, repo in repos.items():
        git(repo, 'cat-file', '-e', PINS[owner]+'^{commit}')
    git(ROOT, 'cat-file', '-e', BASE+'^{commit}')
    source_commit = git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    if subprocess.run(['git', '-C', str(ROOT), 'merge-base', '--is-ancestor', BASE,
                       source_commit], capture_output=True).returncode:
        raise ValueError('Utilities source does not descend from the exact baseline')
    out = args.output_dir.resolve()
    if out.exists() and any(out.iterdir()):
        raise ValueError('Use an empty output directory; stale qualification is refused')
    out.mkdir(parents=True, exist_ok=True)
    identities = {}
    roots = {owner: out/'inputs'/owner for owner in PINS}
    stage(repos['system'], PINS['system'], roots['system'],
          ['lib/PortableApps', 'lib/NativeApps/include', 'Apps/alarm_writer.h', 'Apps/SpringboardPresentation.h',
           'Apps/PaperPresentation.h', 'Apps/PaperFrame.h',
           'scripts/native_toolchain.py', 'LICENSE'], identities, 'system')
    stage(repos['bridges'], PINS['bridges'], roots['bridges'],
          ['apps/clock/portable_navigation.c', 'apps/clock/portable_sleep.c',
           'apps/clock/watch_sleep.h', 'apps/clock/watch_alarm_sleep.h',
           'apps/clock/watch_motion_client.h', 'apps/clock/faces/ContextsFaces.h',
           'apps/clock/faces/names.h', 'sdk/app', 'sdk/driver', 'include', 'LICENSE'],
          identities, 'bridges')
    stage(repos['runtime'], PINS['runtime'], roots['runtime'],
          ['sdk/app', 'LICENSE'], identities, 'runtime')
    stage(repos['profile_source'], PINS['profile_source'], roots['profile_source'],
          ['profiles/shared-editor-inputs/watch-alarm-profile.h',
           'profiles/shared-editor-inputs/AlarmServicePinnedV1.h', 'LICENSE'],
          identities, 'profile_source')
    baseline = out/'inputs'/'utilities-baseline'
    stage(ROOT, BASE, baseline, ['Apps/alarms.c', 'Apps/countdown.c', 'Apps/alarm_app_shared.h',
          'Apps/alarm_nova.inc', 'Apps/daily_draw.h', 'Apps/alarms_paper.inc', 'Apps/countdown_paper.inc',
          'Apps/alarm_writer.h',
          'Apps/utility_frame.h', 'Apps/utility_preference_atomic.h',
          'lib/Alarm/include', 'lib/Contexts/include', 'lib/elf_loader',
          'test/native_apps/stubs', 'test/native_apps/validate_test.c', 'LICENSE'],
          identities, 'utilities-baseline')
    # Quoted includes in System headers must resolve the selected Runtime ABI.
    include = roots['system']/'lib/PortableApps/include'
    for name in ('RiscRuntimeV1.h', 'RiscKeyValueAtomicV1.h'):
        selected = roots['runtime']/'sdk/app'/name
        (include/name).write_bytes(selected.read_bytes())
        identities[(include/name).resolve()] = ('runtime', 'sdk/app/'+name)
    toolchain_path = roots['system']/'scripts/native_toolchain.py'
    spec = importlib.util.spec_from_file_location('alarms_pinned_toolchain', toolchain_path)
    toolchain = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(toolchain)
    cc = toolchain.compiler_path(args.xtensa_cc)
    mapping = out/'exports.map'
    mapping.write_text('{ global: '+'; '.join(sorted(EXPORTS))+'; local: *; };\n')
    validator = out/'validate-elf'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
        '-I'+str(baseline/'test/native_apps/stubs'), '-I'+str(baseline/'lib/elf_loader/include'),
        str(baseline/'lib/elf_loader/src/esp_elf_validate.c'),
        str(baseline/'test/native_apps/validate_test.c'), '-o', str(validator)], check=True)
    metadata = {item['path']: digest((ROOT/item['path']).read_bytes()) for item in profile['baseline_manifests'].values()}
    metadata.update({PROFILE_PATH: digest(profile_bytes), manifest_path: digest(baseline_bytes),
                'scripts/build_native_alarms_qualification.py': digest(Path(__file__).read_bytes())})
    records, snapshots = [], {}
    for app_name in APPLICATIONS:
        for mode in ['baseline', 'legacy']+(['native'] if args.native_ui else []):
            app_root = baseline if mode == 'baseline' else ROOT
            mode_roots = {**roots, 'utilities': app_root}
            sources = [mode_roots[owner]/relative for owner, relative in
                       (item.split(':', 1) for item in app_sources(app_name))]
            incs = [include, roots['system']/'lib/NativeApps/include', app_root/'Apps',
                    baseline/'Apps', baseline/'lib/Alarm/include', baseline/'lib/Contexts/include',
                    roots['profile_source']/'profiles/shared-editor-inputs',
                    roots['runtime']/'sdk/app', roots['bridges']/'sdk/app',
                    roots['bridges']/'sdk/driver', roots['bridges']/'include']
            prep = define_arguments(mode == 'native')+['-I'+str(path) for path in incs]
            for item in FORCED:
                owner, relative = item.split(':', 1)
                prep += ['-include', str(roots[owner]/relative)]
            closure = {}
            for source in sources:
                dependencies = subprocess.check_output([cc, '-std=c11', '-MM', *prep, str(source)], text=True)
                for token in shlex.split(dependencies.replace('\\\n', ' ').split(':', 1)[1]):
                    path = Path(token).resolve()
                    if {'test', 'tests', 'fixtures'} & set(path.parts):
                        raise ValueError('Host fixture leaked into target: '+str(path))
                    if path in identities:
                        owner, relative = identities[path]
                    elif path.is_relative_to(ROOT):
                        owner, relative = 'utilities', str(path.relative_to(ROOT))
                    else:
                        raise ValueError('Unowned target input: '+str(path))
                    data = path.read_bytes()
                    if owner == 'utilities-baseline':
                        original = git(ROOT, 'show', BASE+':'+relative)
                        owner = 'utilities'
                    elif owner in repos:
                        original = git(repos[owner], 'show', PINS[owner]+':'+relative)
                    else:
                        original = data
                    if data != original:
                        raise ValueError('Staged input differs from immutable Git object: '+str(path))
                    closure[owner+':'+relative] = digest(data)
                    if path in snapshots and snapshots[path] != digest(data):
                        raise ValueError('Source changed across profile builds: '+str(path))
                    snapshots[path] = digest(data)
            if not required_closure(app_name) <= closure.keys():
                raise ValueError('Meaningful feature source closure absent: '+repr(required_closure(app_name)-closure.keys()))
            if mode == 'native' and not NATIVE_CLOSURE <= closure.keys():
                raise ValueError('Native Alarms UI/overlay closure absent: '+repr(NATIVE_CLOSURE-closure.keys()))
            dest = out/mode
            dest.mkdir(exist_ok=True)
            elf = dest/(app_name+'.elf')
            command = [cc, *FLAGS, '-Wl,--version-script='+str(mapping), *prep,
                       *map(str, sources), '-lgcc', '-o', str(elf)]
            (dest/(app_name+'-command.json')).write_text(json.dumps(command, indent=2)+'\n')
            subprocess.run(command, check=True, timeout=180)
            data = elf.read_bytes()
            elf_contract(data)
            nm = cc.removesuffix('gcc')+'nm'
            symbols = subprocess.check_output([nm, '-D', str(elf)], text=True)
            imports, exports = symbol_contract(symbols)
            subprocess.run([str(validator), str(elf)], check=True)
            (dest/(app_name+'-dynamic-symbols.txt')).write_text(symbols)
            records.append({'app': app_name, 'mode': mode, 'native_ui': mode == 'native', 'sha256': digest(data),
                'size_bytes': len(data), 'imports': imports, 'exports': exports,
                'command_sha256': digest((dest/(app_name+'-command.json')).read_bytes()),
                'build_defines': define_arguments(mode == 'native'), 'source_sha256': closure,
                'structural_loader_admission': 'passed', 'target_instructions_executed': False})
            print(app_name+' '+mode+': target ELF/import/export/loader admission passed', flush=True)
    for app_name in APPLICATIONS:
        if (out/'baseline'/(app_name+'.elf')).read_bytes() != (out/'legacy'/(app_name+'.elf')).read_bytes():
            raise ValueError('Native-selector-off '+app_name+' ELF changed from exact Utilities baseline')
    for path, expected in snapshots.items():
        if digest(path.read_bytes()) != expected:
            raise ValueError('Source changed during qualification: '+str(path))
    for path, expected in metadata.items():
        if digest((ROOT/path).read_bytes()) != expected:
            raise ValueError('Recipe metadata changed during qualification: '+path)
    if git(ROOT, 'rev-parse', 'HEAD').decode().strip() != source_commit:
        raise ValueError('Utilities commit changed during qualification')
    receipt = {'schema': 1, 'purpose': profile['purpose'], 'profile': profile['profile'],
        'utilities_commit': source_commit, 'utilities_base': BASE,
        'working_tree_dirty': bool(git(ROOT, 'status', '--porcelain').strip()),
        'external_git_pins': PINS, 'external_working_trees_used': False,
        'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
        'compiler_sha256': digest(Path(cc).read_bytes()), 'recipe_sha256': metadata,
        'required_capabilities': profile['requires'],
        'optional_capability_requests': profile['optional_capability_requests'],
        'physical_bindings': 'deployment-owned; atomic and regular preferences namespace 1',
        'lifecycle': profile['lifecycle'], 'legacy_exact_baseline_bytes': True, 'legacy_applications': list(APPLICATIONS),
        'modules': records, 'historical_dot59_elf_reproduction': False,
        'hardware_qualified': False, 'firmware_assembled': False, 'published': False}
    (out/'qualification.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('Exact selector-off baseline bytes passed; local qualification receipt written')
    return receipt


if __name__ == '__main__':
    build(argument_parser().parse_args())
