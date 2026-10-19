#!/usr/bin/env python3
"""Source-pinned LoRa native target qualification; never a product/release builder.

Native UI requires --native-ui. Both existing minimal and shared-input selector-off
compositions are compared byte-for-byte with the exact source baselines. Target
sources are read exclusively from immutable Git objects in clean exact checkouts.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PROFILE = ROOT / 'profiles/native-lora-qualification.json'
# An intentional profile change requires a reviewed recipe change as well.
PROFILE_CONTRACT_SHA256 = 'ba1720ddcd8ced2c1e7b3098604e6d1c98212f6847e59ee0904460fa7dfcdf08'
EXPORTS = {'app_main', 'app_module_init', 'app_module_fini'}
IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp',
           'strlen', 'snprintf', 'malloc', 'free', 'strcpy', 'memchr'}
FLAGS = ['-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
         '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib',
         '-nostartfiles', '-shared', '-Wl,--no-relax', '-Wl,--hash-style=sysv',
         '-Wall', '-Wextra', '-Werror']
UTILITY_SELECTIONS = ['Apps', 'lib/Alarm/include']
SYSTEM_SELECTIONS = ['lib/PortableApps', 'lib/NativeApps/include', 'Apps', 'sdk/app']
RUNTIME_SELECTIONS = ['sdk/app', 'sdk/driver', 'lib/elf_loader']
CATALOG = ('#include "PortableApps.h"\n'
           'const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\n'
           'const unsigned portable_catalog_count=0;\n')
MAP = '{ global: app_main; app_module_init; app_module_fini; local: *; };\n'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def exact_clean(repo, pin, owner):
    if not isinstance(pin, str) or not re.fullmatch('[0-9a-f]{40}', pin):
        raise ValueError(owner + ' requires a complete immutable commit pin')
    head = git(repo, 'rev-parse', 'HEAD').decode().strip()
    if head != pin:
        raise ValueError(owner + ' HEAD must equal exact pinned commit ' + pin + '; got ' + head)
    if git(repo, 'status', '--porcelain', '--untracked-files=all').strip():
        raise ValueError(owner + ' source must be clean (tracked, staged and untracked files)')
    return {'commit': head, 'tree': git(repo, 'rev-parse', 'HEAD^{tree}').decode().strip(),
            'clean': True}


def stage(repo, pin, root, selections, owner, identities):
    archive = git(repo, 'archive', '--format=zip', pin, '--', *selections)
    with zipfile.ZipFile(io.BytesIO(archive)) as bundle:
        for entry in bundle.infolist():
            if entry.is_dir():
                continue
            relative = Path(entry.filename)
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Invalid pinned archive member')
            if ((entry.external_attr >> 16) & 0o170000) == 0o120000:
                raise ValueError('Symlink in pinned source: ' + entry.filename)
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(bundle.read(entry))
            identities[target.resolve()] = owner + ':' + relative.as_posix()


def validate_profile(profile):
    if not isinstance(profile, dict):
        raise ValueError('Qualification profile must be an object')
    canonical = json.dumps(profile, sort_keys=True, separators=(',', ':')).encode()
    if digest(canonical) != PROFILE_CONTRACT_SHA256:
        raise ValueError('Qualification profile violates the exact pinned selectors/source/contract boundary')
    return profile


def symbols(listing):
    imports, exports = set(), set()
    for line in listing.splitlines():
        fields = line.split()
        if len(fields) < 8 or fields[4] not in ('GLOBAL', 'WEAK'):
            continue
        name = fields[7]
        if fields[6] == 'UND':
            if fields[4] == 'WEAK':
                raise ValueError('Undefined weak symbol: ' + name)
            imports.add(name)
        else:
            if fields[3] != 'FUNC' or fields[5] != 'DEFAULT':
                raise ValueError('Unexpected non-function or hidden export: ' + name)
            exports.add(name)
    if imports - IMPORTS or exports != EXPORTS:
        raise ValueError('Unexpected target ABI: ' + repr((imports - IMPORTS, exports)))
    return imports, exports


def elf_layout(data):
    if len(data) < 52 or data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected complete ELF32 little-endian header')
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    if header[1:3] != (3, 94) or header[8] != 52 or header[11] != 40:
        raise ValueError('Expected Xtensa ET_DYN with canonical headers')
    offset, count, index = header[6], header[12], header[13]
    if not count or count > 256 or index >= count or offset + count * 40 > len(data):
        raise ValueError('Invalid ELF section table')
    sections = [struct.unpack_from('<10I', data, offset + i * 40) for i in range(count)]
    names = sections[index]
    if names[1] != 3 or names[4] + names[5] > len(data):
        raise ValueError('Invalid section-name table')
    strings = data[names[4]:names[4] + names[5]]
    result = []
    for section in sections:
        if section[0] >= len(strings) or b'\0' not in strings[section[0]:]:
            raise ValueError('Invalid section name')
        if section[1] != 8 and section[4] + section[5] > len(data):
            raise ValueError('Section exceeds ELF')
        name = strings[section[0]:].split(b'\0', 1)[0].decode('ascii')
        result.append({'name': name, 'type': section[1], 'flags': section[2],
                       'address': section[3], 'offset': section[4],
                       'size_bytes': section[5], 'alignment': section[8]})
    allocated = [s for s in result if s['flags'] & 2]
    return {'elf_bytes': len(data), 'allocated_section_bytes': sum(s['size_bytes'] for s in allocated),
            'executable_section_bytes': sum(s['size_bytes'] for s in allocated if s['flags'] & 4),
            'writable_section_bytes': sum(s['size_bytes'] for s in allocated if s['flags'] & 1),
            'bss_bytes': sum(s['size_bytes'] for s in allocated if s['type'] == 8),
            'sections': result, 'scope': 'Static sections only; no runtime memory or hardware admission.'}


def run(command, output, name):
    command = list(map(str, command))
    write_json(output / (name + '-command.json'), command)
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=240)
    (output / (name + '.log')).write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(name + ' failed; see ' + str(output / (name + '.log')))
    return result.stdout


def check_parity(records):
    by_name = {r['mode']: r for r in records}
    for baseline, current in [('baseline-minimal', 'legacy-minimal'),
                              ('baseline-shared', 'legacy-shared')]:
        if by_name[baseline]['sha256'] != by_name[current]['sha256']:
            raise ValueError('Selector-off target bytes changed: ' + current)


def main(args):
    profile_bytes = PROFILE.read_bytes()
    profile = validate_profile(json.loads(profile_bytes))
    # Fail closed before compiler invocation, including in the default legacy mode.
    system = args.system.resolve()
    runtime = args.runtime.resolve()
    source_records = {'utilities': exact_clean(ROOT, args.utilities_commit, 'Utilities'),
                      'system': exact_clean(system, profile['pins']['system'], 'System'),
                      'runtime': exact_clean(runtime, profile['pins']['runtime'], 'Runtime')}
    base = profile['utilities_base']
    head = git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    # The local task checkout may be shallow exactly at BASE; ancestry checks
    # need no remote lookup and no checkout changes.
    subprocess.run(['git', '-C', str(ROOT), 'merge-base', '--is-ancestor', base, head], check=True)
    out = args.output_dir.resolve()
    if out.exists() and any(out.iterdir()):
        raise ValueError('Output must be empty; refusing stale receipts')
    out.mkdir(parents=True, exist_ok=True)
    write_json(out / 'invocation.json', [sys.executable, *sys.argv])
    (out / 'profile.json').write_bytes(profile_bytes)
    ids = {}
    roots = {name: out / 'inputs' / name for name in
             ('system', 'system-baseline', 'runtime', 'utilities', 'utilities-baseline')}
    stage(system, profile['pins']['system'], roots['system'], SYSTEM_SELECTIONS, 'system', ids)
    stage(system, profile['system_base'], roots['system-baseline'], SYSTEM_SELECTIONS, 'system-baseline', ids)
    stage(runtime, profile['pins']['runtime'], roots['runtime'], RUNTIME_SELECTIONS, 'runtime', ids)
    stage(ROOT, base, roots['utilities-baseline'], UTILITY_SELECTIONS +
          ['test/native_apps/stubs', 'test/native_apps/validate_test.c', 'sdk/firmware-exports.json',
           'scripts/build_lora_apps.py', 'scripts/test_lora_shared_input.py'], 'utilities-baseline', ids)
    stage(ROOT, head, roots['utilities'], UTILITY_SELECTIONS +
          ['scripts/build_native_lora_qualification.py', 'profiles/native-lora-qualification.json'],
          'utilities', ids)
    metadata = {Path(__file__).resolve(): digest(Path(__file__).read_bytes()),
                PROFILE: digest(profile_bytes)}
    for path in ('Apps/lora_messages.json', 'sdk/lora-sources.json', 'utilities-manifest.json'):
        if (ROOT / path).read_bytes() != git(ROOT, 'show', base + ':' + path):
            raise ValueError('Legacy package/release input changed: ' + path)
        metadata[ROOT / path] = digest((ROOT / path).read_bytes())
    reference_path = 'docs/shared-app-crashes/replay/twatch.json'
    reference_bytes = git(system, 'show', profile['system_base'] + ':' + reference_path)
    reference = json.loads(reference_bytes)['lora_messages']
    reference_defines = [item[2:] for item in reference['command'] if item.startswith('-D')]
    selected = set(profile['common_defines'] + profile['shared_input_defines'])
    full_product = {'qualified': False, 'source_commit': profile['system_base'],
                    'source_path': reference_path, 'source_sha256': digest(reference_bytes),
                    'retained_reference': reference,
                    'product_defines_not_selected': [d for d in reference_defines if d not in selected],
                    'qualification_defines_not_in_reference': sorted(selected - set(reference_defines)),
                    'reason': 'Minimal production app/adapter qualification only; product bridges, profile headers, grants and memory admission are not reconstructed.'}
    write_json(out / 'unsupported-full-product.json', full_product)
    generated = out / 'generated'
    generated.mkdir()
    (generated / 'catalog.c').write_text(CATALOG)
    (generated / 'exports.map').write_text(MAP)
    ids[(generated / 'catalog.c').resolve()] = 'qualification:catalog.c'
    generated_hashes = {p.name: digest(p.read_bytes()) for p in generated.iterdir()}
    cc = str(args.xtensa_cc.absolute())
    if Path(cc).name != 'xtensa-esp32s3-elf-gcc':
        raise ValueError('Use the explicit ESP32-S3 target compiler')
    readelf, nm = cc.removesuffix('gcc') + 'readelf', cc.removesuffix('gcc') + 'nm'
    toolchain = {}
    for name, executable in [('cc', cc), ('readelf', readelf), ('nm', nm)]:
        toolchain[name] = {'path': executable, 'sha256': digest(Path(executable).read_bytes()),
                           'version': run([executable, '--version'], out, name + '-version').splitlines()[0]}
    if '8.4.0' not in toolchain['cc']['version'] or '2021r2-patch5' not in toolchain['cc']['version']:
        raise ValueError('Qualification requires official GCC 8.4.0+2021r2-patch5')
    for name, flag in [('libgcc', '-print-libgcc-file-name'), ('cc1', '-print-prog-name=cc1'),
                       ('lto1', '-print-prog-name=lto1'), ('assembler', '-print-prog-name=as'),
                       ('linker', '-print-prog-name=ld')]:
        path = Path(run([cc, flag], out, 'resolve-' + name).strip()).resolve()
        if not path.is_file():
            raise ValueError('Unresolved toolchain input: ' + str(path))
        toolchain[name] = {'path': str(path), 'sha256': digest(path.read_bytes())}
    validator = out / 'validate-elf'
    run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
         '-I' + str(roots['utilities-baseline'] / 'test/native_apps/stubs'),
         '-I' + str(roots['runtime'] / 'lib/elf_loader/include'),
         roots['runtime'] / 'lib/elf_loader/src/esp_elf_validate.c',
         roots['utilities-baseline'] / 'test/native_apps/validate_test.c', '-o', validator], out, 'build-validator')
    runtime_exports = (roots['runtime'] / 'lib/elf_loader/src/esp_elf_symbol.c').read_text()
    modes = [('baseline-minimal', True, False, False), ('legacy-minimal', False, False, False),
             ('baseline-shared', True, True, False), ('legacy-shared', False, True, False)]
    if args.native_ui:
        modes.append(('native-shared', False, True, True))
    records = []
    for mode, baseline, shared, native in modes:
        dest = out / mode
        dest.mkdir()
        app = roots['utilities-baseline' if baseline else 'utilities']
        sysroot = roots['system-baseline' if baseline else 'system']
        source_files = [app / 'Apps/lora_messages.c', sysroot / 'lib/PortableApps/src/adapter.c', generated / 'catalog.c']
        include_dirs = [app / 'Apps', app / 'lib/Alarm/include', sysroot / 'lib/PortableApps/include',
                        sysroot / 'lib/NativeApps/include', sysroot / 'Apps', roots['runtime'] / 'sdk/app', roots['runtime'] / 'sdk/driver']
        defines = profile['common_defines'] + (profile['shared_input_defines'] if shared else [])
        defines += profile['native_selectors'] if native else []
        preprocess = ['-D' + s for s in defines] + ['-I' + str(p) for p in include_dirs]
        elf = dest / 'lora_messages.elf'
        command = [cc, *FLAGS, *preprocess, *source_files, '-Wl,--version-script=' + str(generated / 'exports.map'),
                   '-lgcc', '-o', elf]
        run(command, dest, 'build')
        imports, exports = symbols(run([readelf, '--dyn-syms', '--wide', elf], dest, 'dynamic-symbols'))
        for name in imports - {'risc_runtime_get_api'}:
            if 'ESP_ELFSYM_EXPORT(' + name + ')' not in runtime_exports:
                raise ValueError('Pinned Runtime does not export ' + name)
        closure = {}
        for index, source in enumerate(source_files):
            dependencies = run([cc, '-std=c11', '-M', *preprocess, source], dest, 'dependencies-' + str(index))
            for item in shlex.split(dependencies.replace('\\\n', ' ').split(':', 1)[1]):
                path = Path(item).resolve()
                if {'test', 'tests', 'fixtures'} & set(path.parts):
                    raise ValueError('Host fixture in target closure: ' + str(path))
                identity = ids.get(path)
                if identity is None:
                    tool_root = args.xtensa_cc.absolute().parents[1].resolve()
                    if not path.is_relative_to(tool_root):
                        raise ValueError('Unowned source dependency: ' + str(path))
                    identity = 'toolchain:' + str(path.relative_to(tool_root))
                closure[identity] = digest(path.read_bytes())
        prefix = 'system-baseline:' if baseline else 'system:'
        if shared:
            for needed in ('lib/PortableApps/src/text_input_handoff.inc', 'lib/PortableApps/include/PortableTextInputLongClient.h',
                           'lib/PortableApps/src/native_custody_adapter.inc'):
                if prefix + needed not in closure:
                    raise ValueError('Incomplete shared input closure: ' + needed)
        if native:
            required = profile['native_required_inputs']
            if not set(required) <= closure.keys():
                raise ValueError('Missing native target source closure: ' + repr(set(required) - closure.keys()))
        run([validator, elf], dest, 'loader-validation')
        run([readelf, '--headers', '--sections', '--relocs', '--wide', elf], dest, 'elf-details')
        run([nm, '--size-sort', '--print-size', elf], dest, 'size-symbols')
        data = elf.read_bytes()
        layout = elf_layout(data)
        write_json(dest / 'elf-layout.json', layout)
        write_json(dest / 'source-closure.json', closure)
        records.append({'mode': mode, 'sha256': digest(data), 'size_bytes': len(data), 'defines': defines,
                        'imports': sorted(imports), 'exports': sorted(exports),
                        'command_sha256': digest((dest / 'build-command.json').read_bytes()),
                        'source_closure_sha256': digest((dest / 'source-closure.json').read_bytes()),
                        'structural_loader_admission': 'passed', 'target_instructions_executed': False,
                        'allocated_section_bytes': layout['allocated_section_bytes']})
        print(mode + ': target ELF/imports/exports/loader structure passed', flush=True)
    check_parity(records)
    for path, expected in metadata.items():
        if digest(path.read_bytes()) != expected:
            raise ValueError('Qualification metadata changed during execution: ' + str(path))
    exact_clean(ROOT, args.utilities_commit, 'Utilities')
    exact_clean(system, profile['pins']['system'], 'System')
    exact_clean(runtime, profile['pins']['runtime'], 'Runtime')
    receipt = {'schema': 1, 'profile': profile['profile'], 'scope': profile['scope'],
               'native_ui_requested': args.native_ui, 'utilities_commit': head,
               'utilities_tree': git(ROOT, 'rev-parse', 'HEAD^{tree}').decode().strip(),
               'utilities_source_clean': True, 'utilities_base': base,
               'system_base': profile['system_base'], 'source_pins': source_records,
               'profile_sha256': digest(profile_bytes), 'script_sha256': metadata[Path(__file__).resolve()],
               'generated_inputs': generated_hashes, 'toolchain': toolchain, 'targets': records,
               'display_contract': profile['display_contract'],
               'capability_contract': profile['capability_contract'],
               'host_only_composition': profile['host_only_composition'],
               'unsupported_full_product_sha256': digest((out / 'unsupported-full-product.json').read_bytes()),
               'selector_off_exact_baseline_bytes': True,
               'full_product_qualified': False, 'runtime_executed': False, 'hardware_qualified': False,
               'published': False, 'limitations': profile['limitations']}
    # A successful receipt is created last. Partial attempts retain logs only.
    write_json(out / 'qualification.json', receipt)
    hashes = {str(p.relative_to(out)): digest(p.read_bytes()) for p in sorted(out.rglob('*')) if p.is_file()}
    write_json(out / 'sha256.json', hashes)
    print('Exact selector-off parity passed for minimal and shared-input compositions.', flush=True)
    print('Qualification receipt: ' + str(out / 'qualification.json'), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--utilities-commit', required=True, help='Exact clean Utilities source commit; no candidate or working-tree mode')
    parser.add_argument('--system', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--xtensa-cc', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--native-ui', action='store_true', help='Opt in to the native shared-input target')
    main(parser.parse_args())
