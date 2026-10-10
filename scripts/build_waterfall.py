#!/usr/bin/env python3
"""Build the complete RF app against an exact Watch or paper adapter source."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

from app_manifest import validate_manifest
from normalize_xtensa_relocations import normalize

ROOT = Path(__file__).resolve().parents[1]
PINS = {
    'watch': 'fcdb5b0a54a11a407bf68c6e35ac2548471cd9f1',
    'paper': 'ffbac0fdb117de62e6fce19baa4dc090f5f44404',
}


def git(path, *args):
    return subprocess.check_output(['git', *args], cwd=path, text=True).strip()


def run(args):
    subprocess.run(list(map(str, args)), check=True)


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--system-apps', type=Path, required=True)
p.add_argument('--presentation', choices=PINS, default='watch')
p.add_argument('--system-source', help='Explicit immutable integration adapter SHA')
p.add_argument('--output', type=Path)
p.add_argument('--storage-instance', type=int, default=0)
p.add_argument('--app-data-instance', type=int, default=3)
p.add_argument('--home-app', help='Explicit physical Home destination, e.g. default.elf')
p.add_argument('--quick-actions', action='store_true', help='Shared paper QuickActions; no radio control selection')
a = p.parse_args()
system = a.system_apps.resolve()
pin = a.system_source or PINS[a.presentation]
if len(pin) != 40 or any(c not in '0123456789abcdef' for c in pin):
    raise ValueError('An immutable 40-character System Apps SHA is required')
if git(system, 'rev-parse', 'HEAD') != pin or git(system, 'status', '--porcelain', '--untracked-files=no'):
    raise ValueError('Clean exact System Apps source required: ' + pin)
if not 0 <= a.storage_instance < 2**32 or not 0 <= a.app_data_instance < 2**32:
    raise ValueError('Storage instances must fit uint32')
if (a.home_app or a.quick_actions) and a.presentation != 'paper':
    p.error('Home and QuickActions here use the exact paper adapter; Watch uses its product builder')
validate_manifest(ROOT / 'Apps/waterfall.c', 'waterfall.elf')
core = Path(os.environ.get('PLATFORMIO_CORE_DIR', str(Path.home() / '.platformio')))
cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(core / 'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
out = (a.output or ROOT / ('dist/waterfall-' + a.presentation)).resolve()
out.mkdir(parents=True, exist_ok=True)
quick = None
quick_sources = []
quick_flags = []
if a.home_app or a.quick_actions:
    if 'PORTABLE_APP_LAUNCH_GUARD' not in (system / 'lib/PortableApps/src/adapter.c').read_text():
        p.error('Home/QuickActions requires the shared app pre-launch guard')
    spec = importlib.util.spec_from_file_location('rf_portable_quick_build', system / 'scripts/portable_quick_build.py')
    quick = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(quick)
    a.alarm_client = True
    a.quick_radios = False
    quick_flags, quick_sources = quick.configure(a, p, system, out)
    quick_flags.append('-DPORTABLE_APP_LAUNCH_GUARD')
(out / 'catalog.c').write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
(out / 'exports.map').write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
defines = ['PORTABLE_FORCE_FULL_FRAMES', 'PORTABLE_APP_OWNS_TOUCH_CHROME',
           'PORTABLE_RADIO_SESSION', 'PORTABLE_ALARM_CLIENT', 'PORTABLE_NOVA_UI',
           'RF_RETURN_APP="springboard.elf"',
           'RF_STORAGE_INSTANCE=' + str(a.storage_instance),
           'RF_APP_DATA_INSTANCE=' + str(a.app_data_instance)]
if a.presentation == 'paper':
    defines += ['PORTABLE_DISPLAY_ROTATION=90', 'PORTABLE_INPUT_NAVIGATION']
elf = out / 'waterfall.elf'
run([cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
     '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib',
     '-nostartfiles', '-shared', '-Wl,--no-relax', '-Wl,--hash-style=sysv',
     '-Wl,--version-script=' + str(out / 'exports.map'), '-Wall', '-Wextra', '-Werror',
     *['-D' + d for d in defines], *quick_flags,
     *['-I' + str(i) for i in [ROOT / 'Apps', system / 'lib/PortableApps/include', system / 'lib/NativeApps/include']],
     ROOT / 'Apps/waterfall.c', system / 'lib/PortableApps/src/adapter.c', out / 'catalog.c',
     system / 'lib/NativeApps/src/SingleFloatDivisionCompat.c', *quick_sources, '-lgcc', '-o', elf])
normalize(elf)
syms = subprocess.check_output([cc.removesuffix('gcc') + 'nm', '-D', str(elf)], text=True)
imports = {s.split()[-1] for s in syms.splitlines() if ' U ' in ' ' + s}
exports = {s.split()[-1] for s in syms.splitlines() if len(s.split()) >= 3 and s.split()[-2] in ('T', 'D', 'B', 'R')}
allowed = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp', 'strlen', 'snprintf', 'malloc', 'calloc', 'free', 'strcpy', 'memchr'}
if imports - allowed or exports != {'app_main', 'app_module_init', 'app_module_fini'}:
    raise ValueError('Waterfall ABI differs: ' + repr((imports, exports)))
validator = out / 'validate-elf'
run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
     '-I' + str(system / 'test/native_apps/stubs'), '-I' + str(system / 'lib/elf_loader/include'),
     system / 'lib/elf_loader/src/esp_elf_validate.c', system / 'test/native_apps/validate_test.c', '-o', validator])
run([validator, elf])
m = json.loads((ROOT / 'Apps/waterfall.json').read_text())
manifest = dict(type='application', id='waterfall', version=m['version'], architecture='xtensa-esp32s3',
                file_name='waterfall.elf', entry='app_main',
                requires=[dict(capability=q['capability'], api=int(q['api'][2:])) for q in m['requires']])
# API2 owns private RF records; API1 independently reads shared radio policy.
# Paper additionally binds physical navigation. Watch's product builder links
# its local crown client instead of requiring the generic navigation provider.
manifest['requires'] += [dict(capability='storage.key-value', api=1), dict(capability='board.battery', api=1)]
if a.presentation == 'paper':
    manifest['requires'].append(dict(capability='input.navigation', api=1))
if quick:
    quick.requirements(a, manifest['requires'])
(out / 'waterfall.json').write_text(json.dumps(manifest, indent=2) + '\n')
record = dict(schema=1, source_revision=git(ROOT, 'rev-parse', 'HEAD'),
              source_dirty=bool(git(ROOT, 'status', '--porcelain')), system_source=pin,
              presentation=a.presentation, defines=defines + [d.removeprefix('-D') for d in quick_flags],
              home_app=a.home_app, quick_actions=a.quick_actions, quick_radios=False,
              sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
              bytes=elf.stat().st_size, imports=sorted(imports), exports=sorted(exports), target_validation='passed',
              storage=dict(key_value_api=2, key_value_instance=a.storage_instance, app_data_instance=a.app_data_instance,
                           legacy_max_bytes=129004, fingerprint_max_bytes=60000, quota_bytes=131072, combined_full_capacity=False),
              memory='Bulk state uses module BSS placed in PSRAM by the supported Runtime ELF loader; optional bulk allocator only, never internal-heap fallback.')
(out / 'build-record.json').write_text(json.dumps(record, indent=2) + '\n')
for name in ['LICENSE-FontAwesome.txt', 'LICENSE-Orbitron.txt', 'LICENSE-Rajdhani.txt', 'SOURCES.json']:
    (out / name).write_bytes((system / 'lib/PortableApps/fonts' / name).read_bytes())
(out / 'LICENSE-Utilities.txt').write_bytes((ROOT / 'LICENSE').read_bytes())
print('Built Waterfall', m['version'], a.presentation, record['bytes'], record['sha256'])
