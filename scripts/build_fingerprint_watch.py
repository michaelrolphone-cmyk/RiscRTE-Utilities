#!/usr/bin/env python3
"""Compile the Watch audio/editor integration; does not package or flash firmware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import shutil
from normalize_xtensa_relocations import normalize

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--system-apps', required=True, type=Path)
args = parser.parse_args()
system = args.system_apps.resolve()
cc = os.environ['NATIVE_APP_CC']
compiler = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
if '8.4.0' not in compiler or '2021r2-patch5' not in compiler:
    raise ValueError('Pinned GCC8.4 2021r2-patch5 required')
out = root/'dist/fingerprint-watch'
out.mkdir(parents=True, exist_ok=True)
sdk=out/'sdk'
shutil.copytree(system/'lib/PortableApps/include',sdk,dirs_exist_ok=True)
shutil.copyfile(root/'lib/Contexts/include/ContextsServiceV1.h',sdk/'ContextsServiceV1.h')
shutil.copyfile(root/'lib/Contexts/include/ContextFingerprintService.h',sdk/'ContextFingerprintService.h')
(out/'exports.map').write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
(out/'catalog.c').write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
validator = out/'validate-elf'
subprocess.run(['cc', '-std=c11', '-I'+str(root/'test/native_apps/stubs'),
                '-I'+str(root/'lib/elf_loader/include'), root/'lib/elf_loader/src/esp_elf_validate.c',
                root/'test/native_apps/validate_test.c', '-o', validator], check=True)
for name in ('audio_spectrum', 'contexts'):
    defines = ['PORTABLE_NOVA_UI', 'PORTABLE_ALARM_CLIENT', 'PORTABLE_CONTEXTS_CLIENT',
               'PORTABLE_FORCE_FULL_FRAMES', 'PORTABLE_APP_OWNS_TOUCH_CHROME', 'PORTABLE_QUICK_ACTIONS']
    defines += ['PORTABLE_AUDIO_SESSION', 'PORTABLE_AUDIO_CONTINUOUS_CAPTURE'] if name == 'audio_spectrum' else ['PORTABLE_CONTEXTS_EDITOR']
    includes = (root/'Apps', sdk, system/'lib/PortableApps/include', system/'lib/NativeApps/include')
    target = out/(name+'.elf')
    subprocess.run([cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
        '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles',
        '-shared', '-Wl,--no-relax', '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(out/'exports.map'),
        '-Wall', '-Wextra', '-Werror', *['-D'+s for s in defines], *['-I'+str(s) for s in includes],
        root/'Apps'/(name+'.c'), system/'lib/PortableApps/src/adapter.c', out/'catalog.c',
        system/'lib/NativeApps/src/SingleFloatDivisionCompat.c',
        *[system/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')], '-lgcc', '-o', target], check=True)
    removed = normalize(target)
    symbols = subprocess.check_output([cc.removesuffix('gcc')+'nm', '-D', target], text=True)
    imports = {s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s}
    exports = {s.split()[-1] for s in symbols.splitlines() if len(s.split()) >= 3 and s.split()[-2] in ('T','D','B','R')}
    assert imports <= {'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','strncmp','strcpy','snprintf','malloc','calloc','free','memchr'}
    assert exports == {'app_main','app_module_init','app_module_fini'}
    subprocess.run([validator, target], check=True)
    record = {'compiler':compiler, 'defines':defines, 'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
              'bytes':target.stat().st_size, 'imports':sorted(imports), 'exports':sorted(exports),
              'removed_noop_relocations':removed, 'hardware_verified':False}
    for key, repo in [('utilities', root), ('system', system)]:
        record[key] = {'commit':subprocess.check_output(['git','-C',repo,'rev-parse','HEAD'],text=True).strip(),
                       'dirty':bool(subprocess.check_output(['git','-C',repo,'status','--porcelain'],text=True).strip())}
    (out/(name+'-build.json')).write_text(json.dumps(record, indent=2)+'\n')
    print(name+': ESP32-S3 compilation, exports and runtime loader validation PASS')
