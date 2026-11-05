#!/usr/bin/env python3
"""Strict development-only 3D Viewer target qualification, never a release recipe.

Requires a clean, explicitly selected Utilities commit descended from the reviewed
baseline and exact System/Runtime pins. Builds the real Viewer, Canvas adapter and
entry twice with strict warnings, checks the complete compiler dependency closure,
public imports, sole entry export, and the selected Runtime's structural validator.
No target instructions execute here. No firmware, deployment or hardware claim.
"""
import argparse
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BASE = '95f39010ed89d76fc583f1c09a111cfc3cf21f41'
PINS = {'system': 'd66687f7e008ec324e0b5f8f020572c6f3f70968',
        'runtime': '13eca114cb58469cf6004fafc3da84fbd9d29beb'}
PROFILE = 'viewer-native-development-v1'
SCRIPT = 'scripts/build_native_viewer_qualification.py'
CONTRACT_TEST = 'scripts/test_native_viewer_qualification.py'
SOURCES = ('Apps/model_viewer.c', 'lib/CanvasApp/canvas.c', 'lib/CanvasApp/entry.c')
SELECTIONS = {
    'utilities': ['Apps/model_viewer*', 'lib/CanvasApp', SCRIPT, CONTRACT_TEST,
                  'test/native_apps/validate_test.c', 'test/native_apps/stubs/sdkconfig.h'],
    'system': ['lib/PortableApps/include/RiscDisplayOutputV1.h',
               'lib/PortableApps/include/RiscTouchV1.h',
               'lib/PortableApps/include/RiscInputNavigationV1.h'],
    'runtime': ['sdk/app', 'sdk/driver', 'sdk/hardware', 'lib/elf_loader',
                'src/runtime/packages/ElfAdmission.h', 'src/bootstrap/Runtime.cpp'],
}
# Existing public-libc-only scene boundary, with Canvas's malloc/calloc and string
# helpers. No libgcc linkage, private firmware exports, or unresolved weak symbols.
IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'memchr', 'strcmp',
           'strncmp', 'strlen', 'strnlen', 'strchr', 'strcpy', 'snprintf',
           'malloc', 'calloc', 'free', 'strstr', 'strrchr'}
EXPORTS = {'app_main'}
FLAGS = ['-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
         '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib',
         '-nostartfiles', '-shared', '-Wl,--no-relax', '-Wl,--hash-style=sysv',
         '-Wall', '-Wextra', '-Werror', '-DCANVAS_KIND=1']
COMPILER_VERSION = 'xtensa-esp32s3-elf-gcc (crosstool-NG esp-2021r2-patch5) 8.4.0'
TOOL_HASHES = {
    'gcc': '732bebd44b70a21687ffa3ac5da55edc9b555080ad21cfd74503b2288a143d3f',
    'readelf': '25e2332b0a8fede4c285ee6b9f59857458e82af1e8545932b949cbf92ccabc2b',
    'nm': 'af632d7578c77d53fe9767298b0fc0b46763f855df5cab7d40343f51e9b2f0bf',
    'cc1': 'a3ad92e339cb5e29da80ebd2ce152c1eee6b21b0e426dbb3245b953fc2882d4c',
    'collect2': '80832e47f41c72ac62d4f14a09c7aeda39b9cfa6e43bd1f7d98eb83a6fb5d101',
    'as': 'b32b0754d6d295a9164e85f30ce8aef9de2de8ec82ece5614ef8a3705a9c8795',
    'ld': '80f327fd5845e3ce63ebb09c9b00cd0ac88aa61f7d000a9530209ef6b1d5d1a4',
}

# Exact compiler-header closure observed with the same freestanding target flags.
TOOLCHAIN_HEADERS = {
    'lib/gcc/xtensa-esp32s3-elf/8.4.0/include/stdarg.h': 'c13e54e7c95faa3ad5ebe3f024852a755612c26db49eabcf2b15cea48d4aa3c9',
    'lib/gcc/xtensa-esp32s3-elf/8.4.0/include/stdbool.h': 'e623d179a58105d3c9213fd11384dd39cde9f1ecc06c09af9faeaa9cde3d3dde',
    'lib/gcc/xtensa-esp32s3-elf/8.4.0/include/stddef.h': '00d0ea9efd2c42822baefc048d5e99e136e5f3482fbeb610ec512867c6c5f391',
    'lib/gcc/xtensa-esp32s3-elf/8.4.0/include/stdint-gcc.h': '2e51dee23483fb7cec9895dff04fc314372847631f3227aef6fe636e269d8ca7',
    'lib/gcc/xtensa-esp32s3-elf/8.4.0/include/stdint.h': '86a7914ab7d6a18465a6fb9a7b54f945cad604fdbf3ca0d10e4e0254e0037911',
    'xtensa-esp32s3-elf/sys-include/_ansi.h': '5c6403b471ee897c9a6c12e00a58617247804eb3ee72d7baeb7c08616090dd1d',
    'xtensa-esp32s3-elf/sys-include/_newlib_version.h': '93a397574a25b9c143cee7fcb86d8fc9a0c9ba418e4710a30d2d19ba1e4120db',
    'xtensa-esp32s3-elf/sys-include/assert.h': '76d2c321e49e268077d5e6f14e3da71b8f1e28ced650fe3cd403261b3d09923f',
    'xtensa-esp32s3-elf/sys-include/machine/_default_types.h': '5d8f9f7ead7927e4b8b7ed00172e84f2ae82ea7f5f6ba3240ff813a9236771b3',
    'xtensa-esp32s3-elf/sys-include/machine/_types.h': '44544ecbbc10c8aa58ca2da6d0617ffeee5af847af24515ab108c8218d6d92c8',
    'xtensa-esp32s3-elf/sys-include/machine/ieeefp.h': 'd4a06e4410874187a345752942ada750c74c9d80849e1b0d081b98a9c570db4f',
    'xtensa-esp32s3-elf/sys-include/machine/stdlib.h': '43552e520d412993325ba62cb496839cd03fd46b0df67149a5ccbe1b95c45fdd',
    'xtensa-esp32s3-elf/sys-include/machine/types.h': '3be8e785d201f2972db6a96ae1adb4263227623c7e298aded1a662cb8da93ea3',
    'xtensa-esp32s3-elf/sys-include/newlib.h': '3ef7b3e450249c3146c88050f2a30281dadd6ec6592c05d488b6017ae715cac5',
    'xtensa-esp32s3-elf/sys-include/stdio.h': '48c6e22a842695f5ea6147a796da05b8cb3d9422c9d507ac6ccf411fdcac98c9',
    'xtensa-esp32s3-elf/sys-include/stdlib.h': '6ce97f49c423d05b3ef8305591c3e00837027ca10be391e8793109bcec7957d2',
    'xtensa-esp32s3-elf/sys-include/string.h': '3dfccecfe9a38d9cb6d32c371c8269ca5c20e8103ea7f4547c82d38347220634',
    'xtensa-esp32s3-elf/sys-include/sys/_pthreadtypes.h': 'e0924bdccdc1b9983c51436971b690cd58024aa7c56b83f3e076bfc4f51091b6',
    'xtensa-esp32s3-elf/sys-include/sys/_stdint.h': '49f8328943afa2a83ed4a35fcb808ed08b8c9fcb55373c786fb1b1d3a21952d7',
    'xtensa-esp32s3-elf/sys-include/sys/_timespec.h': 'cea8007561c4c9285063cc2f941a284d008e9f044fdfa346572e8bb39f236391',
    'xtensa-esp32s3-elf/sys-include/sys/_types.h': '1bf0428859ddd602938cd25864ba4ebcb7bac003893c1a89d6ff1f11cd04752f',
    'xtensa-esp32s3-elf/sys-include/sys/cdefs.h': 'd8db73988994274b0d28d8abddb8bc555a9687db55d1f54f680e0842e959d296',
    'xtensa-esp32s3-elf/sys-include/sys/config.h': '19e01fbd1d5d99b06baa9575a6c3c99353ac720ca055a68dfe95a69becc6e153',
    'xtensa-esp32s3-elf/sys-include/sys/features.h': 'a0efa8fe696cb885fac7b307793fff00e40bdb1051ecf81306c1a17c1bddef0f',
    'xtensa-esp32s3-elf/sys-include/sys/lock.h': 'f5cad9196ce745967fd2673e085b5c03df7887d4d11d7544aff05ff6679be6b8',
    'xtensa-esp32s3-elf/sys-include/sys/reent.h': '33391d61a83ebbfb122e80514fbf7ed4d770556d8e8d46e029addbbf6077cd79',
    'xtensa-esp32s3-elf/sys-include/sys/sched.h': '48534a2ecc4bbf6287a8ea9dfde9c7660608e25f0812d56c3733da696bd04743',
    'xtensa-esp32s3-elf/sys-include/sys/stdio.h': 'bd987ea6fed5d31d8a4e7d183a7d988e981ecca3cffce76ee482fcb0bf65594b',
    'xtensa-esp32s3-elf/sys-include/sys/string.h': 'b7164ee36ccb0a08bc27c7e381ea417c97d8c3fd3157de62b75d2a6bf7a0a589',
    'xtensa-esp32s3-elf/sys-include/sys/types.h': '0a9beb981196c25b1992ed65eb25c909d6de4047c498392ee24da069e93abf40',
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def environment():
    # Exclude ambient compiler/include/linker injection and inherited sanitizer
    # settings. The host tool path is recorded; every target executable is pinned.
    return {'PATH': '/usr/bin:/bin', 'LANG': 'C', 'LC_ALL': 'C', 'TZ': 'UTC',
            'SOURCE_DATE_EPOCH': '0', 'GIT_NO_LAZY_FETCH': '1',
            'GIT_CONFIG_NOSYSTEM': '1', 'GIT_CONFIG_GLOBAL': '/dev/null',
            'PYTHONDONTWRITEBYTECODE': '1'}


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args],
                                   env=environment(), stderr=subprocess.PIPE, timeout=60)


def run(command, directory, label):
    command = list(map(str, command))
    write_json(directory / (label + '-command.json'), command)
    result = subprocess.run(command, env=environment(), cwd=directory,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=240)
    (directory / (label + '.log')).write_text(result.stdout)
    if result.returncode:
        raise ValueError(f'{label} failed ({result.returncode}): {directory / (label + ".log")}')
    return result.stdout


def verify_repository(repo, pin, selections):
    if not re.fullmatch('[0-9a-f]{40}', pin):
        raise ValueError('Expected a full lowercase 40-character commit identity')
    if git(repo, 'rev-parse', 'HEAD').decode().strip() != pin:
        raise ValueError(f'Unsupported repository HEAD: {repo}; expected {pin}')
    status = git(repo, 'status', '--porcelain=v1', '--untracked-files=all',
                 '--ignored', '--', *selections)
    if status:
        raise ValueError(f'Dirty, ignored, or untracked relevant input in {repo}: {status.decode()}')
    # ls-tree uses literal prefixes rather than status's wildcard pathspecs.
    entries = git(repo, 'ls-tree', '-rz', pin)
    files = {}
    for record in entries.rstrip(b'\0').split(b'\0'):
        if not record:
            continue
        meta, raw_name = record.split(b'\t', 1)
        mode, kind, _ = meta.decode().split()
        name = raw_name.decode()
        if not any(fnmatch.fnmatchcase(name, pattern) or name.startswith(pattern.rstrip('/') + '/')
                   for pattern in selections):
            continue
        path = repo / name
        if kind != 'blob' or mode not in ('100644', '100755') or path.is_symlink():
            raise ValueError('Unsupported source object or symlink: ' + name)
        # Compare bytes even when assume-unchanged/skip-worktree hides a change.
        data = git(repo, 'show', pin + ':' + name)
        if not path.is_file() or path.read_bytes() != data:
            raise ValueError('Relevant source does not match pinned Git bytes: ' + name)
        if any(parent.is_symlink() for parent in path.parents if parent != repo.parent):
            raise ValueError('Symlinked source parent: ' + name)
        files[name] = data
    if not files:
        raise ValueError('No selected pinned source files: ' + str(repo))
    return files


def stage(files, destination, owner, identities):
    for name, data in files.items():
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        identities[path.resolve()] = owner + ':' + name


def stage_sdk(roots, destination, identities):
    destination.mkdir(parents=True)
    selected = {}

    def choose(path, require_identical=False):
        previous = selected.get(path.name)
        if previous is not None and require_identical and previous.read_bytes() != path.read_bytes():
            raise ValueError('Conflicting canonical SDK header: ' + path.name)
        selected[path.name] = path

    for part in ('app', 'driver', 'hardware'):
        for path in sorted((roots['runtime'] / 'sdk' / part).glob('*.h')):
            choose(path, require_identical=True)
    for name in ('RiscDisplayOutputV1.h', 'RiscTouchV1.h', 'RiscInputNavigationV1.h'):
        choose(roots['system'] / 'lib/PortableApps/include' / name, require_identical=True)
    for path in sorted((roots['utilities'] / 'lib/CanvasApp/include').glob('*.h')):
        if not path.name.startswith('Risc') or path.name not in selected:
            choose(path)
    for name in ('RiscStorageVolumeV1.h', 'RiscAppDataV1.h', 'T5FileOpenApi.h'):
        choose(roots['runtime'] / 'sdk/app' / name)
    for name, path in selected.items():
        target = destination / name
        target.write_bytes(path.read_bytes())
        identities[target.resolve()] = identities[path.resolve()]
    return destination


def verify_tool_digest(name, path):
    if not path.is_file() or sha(path.read_bytes()) != TOOL_HASHES[name]:
        raise ValueError('Unsupported target tool identity: ' + name)


def tools_for(cc, out):
    cc = cc.absolute()
    if cc.name != 'xtensa-esp32s3-elf-gcc':
        raise ValueError('Expected explicit xtensa-esp32s3-elf-gcc path')
    paths = {'gcc': cc, 'readelf': cc.with_name('xtensa-esp32s3-elf-readelf'),
             'nm': cc.with_name('xtensa-esp32s3-elf-nm')}
    for name, path in paths.items():
        verify_tool_digest(name, path)
    if run([cc, '--version'], out, 'target-compiler-version').splitlines()[0] != COMPILER_VERSION:
        raise ValueError('Unsupported target compiler version')
    if run([cc, '-dumpmachine'], out, 'target-machine').strip() != 'xtensa-esp32s3-elf':
        raise ValueError('Unsupported target machine')
    for name in ('cc1', 'collect2', 'as', 'ld'):
        path = Path(run([cc, '-print-prog-name=' + name], out, 'resolve-' + name).strip()).resolve()
        verify_tool_digest(name, path)
        paths[name] = path
    return paths


def symbols(listing):
    imports, exports = set(), set()
    for line in listing.splitlines():
        fields = line.split()
        if len(fields) < 8 or fields[4] not in ('GLOBAL', 'WEAK', 'UNIQUE'):
            continue
        name = fields[7]
        if fields[4] != 'GLOBAL' or fields[5] != 'DEFAULT':
            raise ValueError('Unsupported symbol binding/visibility: ' + line.strip())
        (imports if fields[6] == 'UND' else exports).add(name)
    if imports - IMPORTS or exports != EXPORTS or 'risc_runtime_get_api' not in imports:
        raise ValueError(f'Unexpected module ABI: imports={sorted(imports)}, exports={sorted(exports)}')
    return imports, exports


def dependency_closure(listing, identities, toolchain):
    flattened = listing.replace('\\\n', ' ')
    if ':' not in flattened:
        raise ValueError('Malformed compiler dependency output')
    closure, paths = {}, {}
    for token in shlex.split(flattened.split(':', 1)[1]):
        path = Path(token).resolve()
        if {'test', 'tests', 'fixtures'} & set(path.parts):
            raise ValueError('Host fixture leaked into target closure: ' + str(path))
        identity = identities.get(path)
        if identity is None:
            if not path.is_relative_to(toolchain):
                raise ValueError('Unowned compiler dependency: ' + str(path))
            relative = path.relative_to(toolchain).as_posix()
            if relative not in TOOLCHAIN_HEADERS or sha(path.read_bytes()) != TOOLCHAIN_HEADERS[relative]:
                raise ValueError('Unsupported compiler header identity: ' + relative)
            identity = 'toolchain:' + relative
        digest = sha(path.read_bytes())
        if identity in closure and closure[identity] != digest:
            raise ValueError('Conflicting dependency identity: ' + identity)
        closure[identity] = digest
        paths[path] = digest
    return closure, paths


def elf_memory(data):
    if len(data) < 52 or data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected ELF32 little-endian target image')
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    if header[1:3] != (3, 94) or header[11] != 40:
        raise ValueError('Expected Xtensa ET_DYN image')
    sections = [struct.unpack_from('<10I', data, header[6] + index * 40)
                for index in range(header[12])]
    allocated = [s for s in sections if s[2] & 2]
    return {'elf_bytes': len(data), 'allocated_section_bytes': sum(s[5] for s in allocated),
            'executable_section_bytes': sum(s[5] for s in allocated if s[2] & 4),
            'writable_section_bytes': sum(s[5] for s in allocated if s[2] & 1),
            'bss_bytes': sum(s[5] for s in allocated if s[1] == 8),
            'scope': 'Static ELF sections; excludes heap, stack, framebuffer and provider allocations.'}


def verify_import_evidence(imports, runtime):
    loader = (runtime / 'lib/elf_loader/src/esp_elf_symbol.c').read_text()
    admission = (runtime / 'src/runtime/packages/ElfAdmission.h').read_text()
    bootstrap = (runtime / 'src/bootstrap/Runtime.cpp').read_text()
    for name in imports:
        if '"' + name + '"' not in admission:
            raise ValueError('Missing Runtime admission evidence: ' + name)
        if name == 'risc_runtime_get_api':
            if not re.search(r'\{"risc_runtime_get_api",\s*reinterpret_cast<const void\*>\(&risc_runtime_get_api\)\}', bootstrap):
                raise ValueError('Missing Runtime bootstrap export evidence')
        elif 'ESP_ELFSYM_EXPORT(' + name + ')' not in loader:
            raise ValueError('Missing Runtime public-libc export evidence: ' + name)


def verify_repetition(first_bytes, first_closure, data, closure):
    if data != first_bytes or closure != first_closure:
        raise ValueError('Independent-path repeated target build is not byte/closure deterministic')


def qualify(args):
    out = args.output_dir.resolve()
    repos = {'utilities': ROOT.resolve(), 'system': args.system.resolve(), 'runtime': args.runtime.resolve()}
    if out == Path('/') or any(out.is_relative_to(repo) or repo.is_relative_to(out) for repo in repos.values()):
        raise ValueError('Output must be separate from every input repository')
    if out.exists() and any(out.iterdir()):
        raise ValueError('Output must be empty; refusing stale receipts')
    out.mkdir(parents=True, exist_ok=True)
    pins = {'utilities': args.utilities_commit, **PINS}
    files = {owner: verify_repository(repo, pins[owner], SELECTIONS[owner])
             for owner, repo in repos.items()}
    git(ROOT, 'merge-base', '--is-ancestor', BASE, pins['utilities'])
    if SCRIPT not in files['utilities'] or CONTRACT_TEST not in files['utilities']:
        raise ValueError('Qualifier and focused contract tests must be committed with the source')
    if files['utilities'][SCRIPT] != Path(__file__).read_bytes():
        raise ValueError('Running qualifier differs from selected committed script')
    manifest_bytes = files['utilities']['Apps/model_viewer.json']
    if manifest_bytes != git(ROOT, 'show', BASE + ':Apps/model_viewer.json'):
        raise ValueError('Product Viewer manifest changed; unsupported development profile')
    manifest = json.loads(manifest_bytes)
    if manifest['id'] != 'model_viewer' or manifest['entry'] != 'app_main' or manifest['architecture'] != 'xtensa-esp32s3':
        raise ValueError('Unsupported Viewer manifest identity')
    write_json(out / 'invocation.json', vars_to_json(args))
    # Fail if compiler selectors ever grow warning suppressions or external libs.
    if not {'-Wall', '-Wextra', '-Werror'} <= set(FLAGS) or any(flag.startswith(('-Wno', '-l')) for flag in FLAGS):
        raise ValueError('Unsupported strict compiler flags')
    paths = tools_for(args.xtensa_cc, out)
    toolchain = args.xtensa_cc.absolute().parents[1].resolve()
    for relative, expected in TOOLCHAIN_HEADERS.items():
        path = toolchain / relative
        if not path.is_file() or sha(path.read_bytes()) != expected:
            raise ValueError('Unsupported compiler header identity: ' + relative)
    identities = {}
    roots = {owner: out / 'inputs' / owner for owner in repos}
    for owner in repos:
        stage(files[owner], roots[owner], owner, identities)
    # Compile the selected Runtime's real structural validator. The fixture is
    # host-only; dependency checks forbid it from entering the target closure.
    validator = out / 'validate-elf'
    host_cc = Path(shutil.which('cc', path=environment()['PATH'])).resolve()
    host_version = run([host_cc, '--version'], out, 'host-compiler-version').splitlines()[0]
    run([host_cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
         '-I' + str(roots['utilities'] / 'test/native_apps/stubs'),
         '-I' + str(roots['runtime'] / 'lib/elf_loader/include'),
         roots['runtime'] / 'lib/elf_loader/src/esp_elf_validate.c',
         roots['utilities'] / 'test/native_apps/validate_test.c', '-o', validator],
        out, 'build-runtime-validator')
    records, original_dependencies = [], {}
    first_bytes = None
    first_closure = None
    for repetition in (1, 2):
        directory = out / ('run-' + str(repetition))
        directory.mkdir()
        local = {**roots, 'utilities': directory / 'inputs/utilities'}
        stage({name: data for name, data in files['utilities'].items()
               if name.startswith(('Apps/', 'lib/CanvasApp/'))}, local['utilities'], 'utilities', identities)
        include = stage_sdk(local, directory / 'include', identities)
        sources = [local['utilities'] / name for name in SOURCES]
        mapping = directory / 'exports.map'
        mapping.write_text('{ global: app_main; local: *; };\n')
        elf = directory / 'model_viewer.elf'
        command = [paths['gcc'], *FLAGS, '-I' + str(include),
                   '-Wl,--version-script=' + str(mapping), *sources, '-o', elf]
        compile_output = run(command, directory, 'target-build')
        if compile_output.strip():
            raise ValueError('Strict target build emitted unexpected diagnostics')
        listing = run([paths['readelf'], '--dyn-syms', '--wide', elf], directory, 'dynamic-symbols')
        imports, exports = symbols(listing)
        verify_import_evidence(imports, roots['runtime'])
        closure = {}
        for index, source in enumerate(sources):
            listing = run([paths['gcc'], *FLAGS, '-I' + str(include), '-M', source], directory, 'dependencies-' + str(index))
            values, observed = dependency_closure(listing, identities, toolchain)
            for name, digest in values.items():
                if name in closure and closure[name] != digest:
                    raise ValueError('Source changed while computing closure: ' + name)
                closure[name] = digest
            original_dependencies.update(observed)
        required = {'utilities:' + name for name in SOURCES} | {
            'utilities:lib/CanvasApp/include/CanvasApp.h', 'runtime:sdk/app/RiscRuntimeV1.h',
            'runtime:sdk/app/RiscAppDataV1.h', 'runtime:sdk/app/RiscStorageVolumeV1.h',
            'runtime:sdk/app/T5FileOpenApi.h', 'system:lib/PortableApps/include/RiscDisplayOutputV1.h'}
        required |= {'toolchain:' + name for name in TOOLCHAIN_HEADERS}
        if not required <= closure.keys():
            raise ValueError('Incomplete real Viewer source closure: ' + str(sorted(required - closure.keys())))
        write_json(directory / 'source-closure.json', closure)
        run([validator, elf], directory, 'runtime-loader-validation')
        run([paths['readelf'], '--headers', '--sections', '--relocs', '--wide', elf], directory, 'elf-details')
        run([paths['nm'], '--size-sort', '--print-size', elf], directory, 'size-symbols')
        data = elf.read_bytes()
        if repetition == 1:
            first_bytes, first_closure = data, closure
        else:
            verify_repetition(first_bytes, first_closure, data, closure)
        records.append({'run': repetition, 'sha256': sha(data), 'size_bytes': len(data),
                        'imports': sorted(imports), 'exports': sorted(exports),
                        'source_closure': closure, 'memory': elf_memory(data),
                        'structural_loader_validation': 'passed', 'target_instructions_executed': False})
    for path, digest in original_dependencies.items():
        if sha(path.read_bytes()) != digest:
            raise ValueError('Compiler input changed during qualification: ' + str(path))
    for name, path in paths.items():
        verify_tool_digest(name, path)
    for owner, repo in repos.items():
        if verify_repository(repo, pins[owner], SELECTIONS[owner]) != files[owner]:
            raise ValueError('Source changed during qualification: ' + owner)
    provenance = {owner + ':' + name: sha(data)
                  for owner in files for name, data in files[owner].items()}
    receipt = {'schema': 1, 'profile': PROFILE, 'qualified': True,
               'qualification_scope': 'Development-only target structure and deterministic build; no target execution.',
               'exact_git_pins': pins, 'utilities_baseline': BASE,
               'relevant_working_trees_clean': True, 'source_stability_verified': True,
               'product_manifest_unchanged': True, 'product_manifest_sha256': sha(manifest_bytes),
               'capabilities': manifest['requires'], 'target_flags': FLAGS,
               'warning_suppressions': [], 'linker_extra_libraries': [],
               'tools': {name: {'path': str(path), 'sha256': sha(path.read_bytes())} for name, path in paths.items()},
               'compiler_version': COMPILER_VERSION, 'toolchain_header_sha256': TOOLCHAIN_HEADERS,
               'host_compiler': {'path': str(host_cc), 'version': host_version, 'sha256': sha(host_cc.read_bytes())},
               'pinned_input_sha256': provenance, 'runs': records,
               'independent_path_repeated_builds': 2, 'byte_deterministic': True,
               'hardware_tested': False, 'firmware_assembled': False, 'published': False,
               'production_recipe_qualification': False, 'target_instructions_executed': False}
    write_json(out / 'qualification.json', receipt)
    print('PASS: real Viewer/Canvas/entry, strict GCC8, public ABI, Runtime structural loader, and two identical target builds')
    print('Development evidence only; no target execution, firmware assembly, publication or hardware qualification.')
    return receipt


def vars_to_json(args):
    return {key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--system', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--xtensa-cc', type=Path, required=True)
    parser.add_argument('--utilities-commit', required=True, help='Exact clean source commit descended from ' + BASE)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    try:
        qualify(args)
    except Exception as error:
        # Do not overwrite a success/stale directory when admission itself failed.
        out = args.output_dir.resolve()
        inputs = [ROOT.resolve(), args.system.resolve(), args.runtime.resolve()]
        safe = not any(out.is_relative_to(repo) or repo.is_relative_to(out) for repo in inputs)
        if safe and out.is_dir() and (out / 'invocation.json').exists() and not (out / 'qualification.json').exists() and not (out / 'failure.json').exists():
            write_json(out / 'failure.json', {'qualified': False, 'profile': PROFILE,
                                            'error_type': type(error).__name__, 'error': str(error)})
        raise


if __name__ == '__main__':
    main()
