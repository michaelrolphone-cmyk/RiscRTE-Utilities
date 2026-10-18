#!/usr/bin/env python3
"""Qualify local native Spectrum against the retained full-feature Wave recipe.

External sources are read from exact Git objects. The Utilities working source
is snapshotted and checked for concurrent changes. No release inputs are edited.
Native UI is explicit opt-in; this is target structure evidence, not device QA.
"""
import argparse
import hashlib
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
BASE = '12c7440c4c2fd9b6b78e10f11a983947c145b346'
PINS = {
    'system': '947895afdb2ecef59654402cf08fe7ecdf92362f',
    'runtime': '0aab2fd3b03b4190d638e23c79a465afeecf5dfb',
    'watch': '93c87d05d4da54df49240141bf9ca993bc30b462',
    'bridges': '6e82fdcaec0b73ed700d2104cfb4d94f011c19d4',
    'wave': 'f42a6e07133c599a22acfaa8b8a0672ff996dc1b',
}
RECIPE = 'docs/t069/audio_spectrum-command.json'
NATIVE = ['-DPORTABLE_SPECTRUM_NATIVE_UI', '-DPORTABLE_NATIVE_CANVAS']
EXPORTS = {'app_main', 'app_module_init', 'app_module_fini'}
IMPORTS = {'risc_runtime_get_api', 'memcpy', 'memset', 'memcmp', 'strcmp',
           'strlen', 'snprintf', 'malloc', 'calloc', 'free', 'strcpy',
           'memchr', 'strncmp', 'strchr'}
UNITS = ['utilities:Apps/audio_spectrum.c', 'system:lib/PortableApps/src/adapter.c',
         'bridges:apps/clock/portable_navigation.c', 'bridges:apps/clock/portable_sleep.c',
         *['system:lib/PortableApps/src/'+n+'.c' for n in
           ('quick_actions', 'quick_render', 'quick_session', 'quick_radios')]]
REQUIRED = {
    *UNITS, 'utilities:Apps/spectrum_dsp.h', 'utilities:Apps/spectrum_store.h',
    'utilities:Apps/spectrum_signature_app.inc', 'utilities:Apps/spectrum_temporal_app.inc',
    'utilities:Apps/context_fingerprint_app.inc', 'utilities:Apps/context_signal_library.inc',
    'system:lib/PortableApps/src/native_custody_adapter.inc',
    'system:lib/PortableApps/src/text_input_handoff.inc',
    'system:lib/PortableApps/include/PortableAudioSession.h',
    'system:lib/PortableApps/src/contexts_adapter.inc',
    'system:lib/PortableApps/src/broadcast_adapter.inc',
    'runtime:sdk/app/RiscRuntimeV1.h',
}
CAPABILITIES = [
    ('storage.app-data', 1), ('storage.key-value', 2), ('display.output', 1),
    ('input.touch.raw', 1), ('rtc.clock', 2), ('alarm.service', 1),
    ('telemetry.broadcast', 1), ('contexts.service', 1), ('input.power-key', 1),
    ('power.sleep', 1), ('display.power', 1), ('board.battery', 1),
    ('storage.key-value', 1), ('net.wifi', 1), ('bluetooth.hci', 1),
    ('motion.accel', 1), ('audio.input', 1), ('app.catalog', 1),
    ('ui.text-input', 1), ('storage.shared-data', 1), ('storage.key-value.atomic', 1),
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True)+'\n')


def stage(repo, pin, root, selections, identities, owner):
    names = git(repo, 'ls-tree', '-rz', '--name-only', pin, '--', *selections).decode().rstrip('\0').split('\0')
    if not names:
        raise ValueError('No pinned files: '+owner)
    for name in names:
        path = root/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(git(repo, 'show', pin+':'+name))
        identities[path.resolve()] = owner+':'+name


def symbols(text):
    imports, exports = set(), set()
    for line in text.splitlines():
        fields = line.split()
        if len(fields) < 8 or fields[4] not in ('GLOBAL', 'WEAK'):
            continue
        name = fields[7]
        if fields[6] == 'UND':
            if fields[4] == 'WEAK':
                raise ValueError('Undefined weak symbol: '+name)
            imports.add(name)
        else:
            exports.add(name)
    return imports, exports


def elf_memory(data):
    if data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected ELF32 little-endian')
    h = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    if h[1:3] != (3, 94) or h[11] != 40:
        raise ValueError('Expected Xtensa ET_DYN with 40-byte section headers')
    secs = [struct.unpack_from('<10I', data, h[6]+i*40) for i in range(h[12])]
    names = secs[h[13]]
    sections = []
    for s in secs:
        start = names[4]+s[0]
        name = data[start:data.index(b'\0', start)].decode()
        sections.append({'name':name, 'type':s[1], 'flags':s[2], 'address':s[3],
                         'offset':s[4], 'size_bytes':s[5], 'alignment':s[8]})
    alloc = [s for s in sections if s['flags'] & 2]
    return {'elf_bytes':len(data), 'allocated_section_bytes':sum(s['size_bytes'] for s in alloc),
            'executable_section_bytes':sum(s['size_bytes'] for s in alloc if s['flags'] & 4),
            'writable_section_bytes':sum(s['size_bytes'] for s in alloc if s['flags'] & 1),
            'bss_bytes':sum(s['size_bytes'] for s in alloc if s['type'] == 8),
            'sections':sections,
            'scope':'Static ELF sections only; excludes allocator padding, runtime heap, stack, framebuffer and provider allocations.'}


def run(command, folder, name, timeout=240):
    write_json(folder/(name+'-command.json'), list(map(str, command)))
    proc = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, timeout=timeout)
    (folder/(name+'.log')).write_text(proc.stdout)
    if proc.returncode:
        raise RuntimeError(name+' failed ('+str(proc.returncode)+'); see '+str(folder/(name+'.log')))
    return proc.stdout


def main(args):
    out = args.output_dir.resolve()
    if out.exists() and any(out.iterdir()):
        raise ValueError('Output must be empty; refusing stale receipts')
    out.mkdir(parents=True, exist_ok=True)
    write_json(out/'invocation.json', [sys.executable, *sys.argv])
    source_commit = git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    source_tree = git(ROOT, 'rev-parse', 'HEAD^{tree}').decode().strip()
    source_status = git(ROOT, 'status', '--porcelain').decode().splitlines()
    # Inspect only the needed parent chain; older unrelated history may be absent
    # from this deliberately object-pinned checkout.
    pending, seen = [source_commit], set()
    while pending and BASE not in seen:
        commit = pending.pop()
        if commit in seen:
            continue
        seen.add(commit)
        if commit != BASE:
            pending += re.findall(r'^parent ([0-9a-f]{40})$', git(ROOT,'cat-file','-p',commit).decode(), re.M)
    if BASE not in seen:
        raise ValueError('Utilities must descend from exact baseline')
    repos = {key:getattr(args,key).resolve() for key in PINS}
    roots = {key:out/'inputs'/key for key in PINS}
    ids = {}
    selections = {
        'system':['lib/PortableApps', 'lib/NativeApps', 'Apps', 'sdk/app', 'scripts/native_toolchain.py'],
        'runtime':['sdk/app', 'sdk/driver', 'lib/elf_loader'],
        'watch':['apps/clock/portable_navigation.c', 'apps/clock/portable_sleep.c', 'apps/clock/watch_sleep.h', 'apps/clock/watch_alarm_sleep.h', 'apps/clock/watch_motion_client.h', 'apps/clock/PortableSleepPolicy.h', 'apps/clock/WatchRuntimeSleep.h', 'apps/clock/faces/ContextsFaces.h', 'apps/clock/faces/names.h', 'sdk/app', 'sdk/driver', 'include'],
        'bridges':['apps/clock', 'sdk/app', 'sdk/driver', 'include'],
        'wave':[RECIPE, 'profiles/shared-editor-sources.json', 'profiles/shared-editor-inputs', 'profiles/app-source-plan.json'],
    }
    for key in PINS:
        stage(repos[key], PINS[key], roots[key], selections[key], ids, key)
    baseline = out/'inputs/utilities-baseline'
    stage(ROOT, BASE, baseline, ['Apps', 'lib/Alarm/include', 'lib/Contexts/include',
          'lib/NativeApps/src', 'lib/ContextFingerprint', 'lib/VoiceActivity', 'test/native_apps/stubs', 'test/native_apps/validate_test.c'], ids, 'utilities-baseline')
    current = out/'inputs/utilities'
    snapshots = {}
    for selection in ('Apps', 'lib/Alarm/include', 'lib/Contexts/include', 'lib/NativeApps/src', 'lib/ContextFingerprint', 'lib/VoiceActivity'):
        for original in sorted((ROOT/selection).rglob('*')):
            if not original.is_file():
                continue
            relative = original.relative_to(ROOT)
            data = original.read_bytes()
            path = current/relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            ids[path.resolve()] = 'utilities:'+str(relative)
            snapshots[relative.as_posix()] = sha(data)
    metadata_hash = sha(Path(__file__).read_bytes())
    native_manifest = None
    native_manifest_bytes = None
    legacy_manifest = ROOT/'Apps/audio_spectrum.json'
    if legacy_manifest.read_bytes() != git(ROOT,'show',BASE+':Apps/audio_spectrum.json'):
        raise ValueError('Legacy Spectrum manifest changed')
    if args.native_manifest:
        native_manifest_bytes = args.native_manifest.read_bytes()
        native_manifest = json.loads(native_manifest_bytes)
        expected = [{'capability':c,'api':v} for c,v in CAPABILITIES]
        if native_manifest.get('requires') != expected:
            raise ValueError('Native manifest must preserve exact twenty-one capability pairs')
        if native_manifest.get('version') == json.loads(legacy_manifest.read_bytes()).get('version'):
            raise ValueError('Native manifest needs distinct version identity')
        (out/'native-manifest.json').write_bytes(native_manifest_bytes)

    recipe = json.loads((roots['wave']/RECIPE).read_bytes())
    flags = [arg for arg in recipe[1:] if arg.startswith('-') and not arg.startswith(('-I','-Wl,--version-script='))
             and arg not in ('-include','-o','-lgcc','-Wl,--undefined=__divsf3')]
    if any(arg in flags for arg in NATIVE):
        raise ValueError('Native selectors already present in retained recipe')
    # Preserve all feature selectors and compiler/linker flags. libgcc and its
    # forced helper are temporarily omitted only from the undefined-import probe.
    if '-DPORTABLE_WATCH_NATIVE_HEIGHT' not in flags or '-DPORTABLE_NOVA_UI' not in flags or '-DPORTABLE_SPECTRUM_SHARED_TEXT_INPUT' not in flags:
        raise ValueError('Full-feature Wave selector contract is incomplete')
    cc = str(args.xtensa_cc.absolute())
    if Path(cc).name != 'xtensa-esp32s3-elf-gcc':
        raise ValueError('Use the explicit ESP32-S3 target wrapper')
    readelf = cc.removesuffix('gcc')+'readelf'
    nm = cc.removesuffix('gcc')+'nm'
    tools = {}
    for name, path in [('cc',cc),('readelf',readelf),('nm',nm)]:
        tools[name] = {'path':path, 'sha256':sha(Path(path).read_bytes()),
                       'version':run([path,'--version'],out,name+'-version').splitlines()[0]}
    if '8.4.0' not in tools['cc']['version'] or '2021r2-patch5' not in tools['cc']['version']:
        raise ValueError('Retained Spectrum recipe requires official GCC8.4.0+2021r2-patch5; GCC15 changes uint32_t format diagnostics')
    toolchain_inputs = {}
    for name, flag in [('libgcc','-print-libgcc-file-name'),('cc1','-print-prog-name=cc1'),
                       ('lto1','-print-prog-name=lto1'),('assembler','-print-prog-name=as'),
                       ('linker','-print-prog-name=ld')]:
        path = Path(run([cc,flag],out,'resolve-'+name).strip()).resolve()
        if not path.is_file():
            raise ValueError('Unresolved toolchain input: '+str(path))
        toolchain_inputs[name] = {'path':str(path),'sha256':sha(path.read_bytes())}
    # Match the retained recipe's aggregate SDK precedence, from pinned blobs.
    sdk = out/'sdk/include';sdk.mkdir(parents=True)
    for owner, directory in [('system','lib/PortableApps/include'),('system','sdk/app'),
                             ('runtime','sdk/driver'),('runtime','sdk/app')]:
        for path in (roots[owner]/directory).glob('*.h'):
            target = sdk/path.name;target.write_bytes(path.read_bytes());ids[target.resolve()] = ids[path.resolve()]
    for directory in ('lib/Alarm/include','lib/Contexts/include'):
        for path in (current/directory).glob('*.h'):
            target = sdk/path.name
            if not target.exists():
                target.write_bytes(path.read_bytes());ids[target.resolve()] = ids[path.resolve()]
    for path in (roots['system']/'lib/PortableApps/time').rglob('*'):
        if path.is_file():
            target = sdk.parent/'time'/path.relative_to(roots['system']/'lib/PortableApps/time')
            target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(path.read_bytes());ids[target.resolve()] = ids[path.resolve()]
    # Quoted includes in PortableApps headers must use the selected Runtime ABI.
    for path in (roots['runtime']/'sdk/app').glob('*.h'):
        target = roots['system']/'lib/PortableApps/include'/path.name
        if target.exists():
            target.write_bytes(path.read_bytes());ids[target.resolve()] = ids[path.resolve()]
    # #pragma-once ABI headers copied into an aggregate SDK must preserve one
    # physical identity when a bridge header also includes them by quote.
    # Coalesce identical copies only; differing bytes are never hidden.
    canonical = {}
    for directory in [sdk, roots['system']/'lib/PortableApps/include',
                      roots['bridges']/'sdk/app', roots['bridges']/'sdk/driver',
                      roots['runtime']/'sdk/app', roots['runtime']/'sdk/driver']:
        for path in directory.glob('*.h'):
            identity = (path.name, sha(path.read_bytes()))
            if identity in canonical:
                selected = canonical[identity]
                path.unlink();path.symlink_to(selected)
            else:
                canonical[identity] = path.resolve()
    profile = roots['wave']/'profiles/shared-editor-inputs'
    mapping = profile/'audio_spectrum.map'
    validator = out/'validate-elf'
    run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
         '-I'+str(baseline/'test/native_apps/stubs'),'-I'+str(roots['runtime']/'lib/elf_loader/include'),
         str(roots['runtime']/'lib/elf_loader/src/esp_elf_validate.c'),
         str(baseline/'test/native_apps/validate_test.c'),'-o',str(validator)],out,'build-validator')
    records = []
    consumed_current = set()
    for mode in ['baseline','legacy']+(['native'] if args.native_ui else []):
        dest = out/mode;dest.mkdir()
        app = baseline if mode=='baseline' else current
        mode_roots = {**roots,'utilities':app}
        sources = [mode_roots[key]/rel for key,rel in (item.split(':',1) for item in UNITS)]
        includes = [sdk, roots['system']/'lib/PortableApps/include', roots['system']/'lib/NativeApps/include',
                    roots['system']/'Apps', app/'Apps',app/'lib/Alarm/include',roots['bridges']/'sdk/app',
                    roots['bridges']/'sdk/driver',roots['bridges']/'include',app/'lib/Contexts/include',profile,
                    roots['runtime']/'sdk/app']
        prep = [arg for arg in flags if arg.startswith('-D')]+(NATIVE if mode=='native' else [])
        prep += ['-I'+str(path) for path in includes]
        prep += ['-include',str(roots['bridges']/'apps/clock/faces/ContextsFaces.h'),
                 '-include',str(profile/'watch-alarm-profile.h')]
        compile_flags = [arg for arg in flags if not arg.startswith('-D')]
        elf = dest/'audio_spectrum.elf'
        command = [cc,*compile_flags,*prep,*map(str,sources),'-Wl,--version-script='+str(mapping),'-o',str(elf)]
        run(command,dest,'initial-build')
        initial_symbols = run([readelf,'--dyn-syms','--wide',str(elf)],dest,'initial-symbols')
        imports, exports = symbols(initial_symbols)
        # The exact retained libgcc route is admitted by the pinned Runtime.
        # Keep the unlinked-helper probe for ABI evidence; do not introduce
        # source helpers unless the retained linkage actually fails admission.
        helpers = []
        command = [cc,*compile_flags,*prep,*map(str,sources),
                   '-Wl,--version-script='+str(mapping),'-lgcc','-o',str(elf)]
        run(command,dest,'retained-recipe-build')
        write_json(dest/'command.json',command)
        listing = run([readelf,'--dyn-syms','--wide',str(elf)],dest,'dynamic-symbols')
        imports,exports = symbols(listing)
        if imports-IMPORTS or exports != EXPORTS:
            raise ValueError(mode+' unexpected ABI: '+repr((imports-IMPORTS,exports)))
        runtime_symbols = (roots['runtime']/'lib/elf_loader/src/esp_elf_symbol.c').read_text()
        for symbol in imports-{'risc_runtime_get_api'}:
            if 'ESP_ELFSYM_EXPORT('+symbol+')' not in runtime_symbols:
                raise ValueError('No selected Runtime public-libc export evidence: '+symbol)
        closure = {}
        for index, source in enumerate(sources):
            dependency = run([cc,'-std=c11','-M',*prep,str(source)],dest,'dependencies-'+str(index))
            for token in shlex.split(dependency.replace('\\\n',' ').split(':',1)[1]):
                path = Path(token).resolve()
                if {'test','tests','fixtures'} & set(path.parts):
                    raise ValueError('Host fixture leaked into target closure: '+str(path))
                identity = ids.get(path)
                if identity is None:
                    toolchain_root = args.xtensa_cc.absolute().parents[1]
                    if path.is_relative_to(toolchain_root):
                        identity = 'toolchain:'+str(path.relative_to(toolchain_root))
                    else:
                        raise ValueError('Unowned target input: '+str(path))
                if identity.startswith('utilities-baseline:'):
                    identity = identity.replace('utilities-baseline:','utilities:',1)
                elif identity.startswith('utilities:'):
                    consumed_current.add(identity.split(':',1)[1])
                closure[identity] = sha(path.read_bytes())
        if not REQUIRED <= closure.keys():
            raise ValueError(mode+' incomplete full-feature closure: '+repr(REQUIRED-closure.keys()))
        if mode=='native':
            for name in ('spectrum_native_layout.h','spectrum_native_view.inc','spectrum_native_subpages.inc'):
                if 'utilities:Apps/'+name not in closure:
                    raise ValueError('Missing native source: '+name)
        run([str(validator),str(elf)],dest,'loader-validation')
        run([readelf,'--headers','--sections','--relocs','--wide',str(elf)],dest,'elf-details')
        run([nm,'--size-sort','--print-size',str(elf)],dest,'size-symbols')
        data = elf.read_bytes()
        memory = elf_memory(data)
        write_json(dest/'memory.json',memory)
        write_json(dest/'source-closure.json',closure)
        records.append({'mode':mode,'sha256':sha(data),'size_bytes':len(data),'memory':memory,'imports':sorted(imports),
                        'exports':sorted(exports),'helpers_selected_from_actual_imports':helpers,
                        'initial_imports':sorted(symbols(initial_symbols)[0]),
                        'source_closure_sha256':sha((dest/'source-closure.json').read_bytes()),
                        'source_sha256':closure,'command_sha256':sha((dest/'command.json').read_bytes()),
                        'structural_loader_admission':'passed','target_instructions_executed':False})
        print(mode+': target ELF, imports, exports and pinned Runtime loader admission passed',flush=True)
    exact_legacy = (out/'baseline/audio_spectrum.elf').read_bytes()==(out/'legacy/audio_spectrum.elf').read_bytes()
    retained = None
    if args.retained_archive:
        archive = args.retained_archive.resolve()
        with zipfile.ZipFile(archive) as z:
            elf = z.read('payload/Apps/audio_spectrum/audio_spectrum.elf')
            manifest = z.read('payload/Apps/audio_spectrum/manifest.json')
        expected = [{'capability':c,'api':v} for c,v in CAPABILITIES]
        if json.loads(manifest)['requires'] != expected:
            raise ValueError('Frozen .63 capability list differs')
        dest = out/'retained-dot63';dest.mkdir()
        (dest/'audio_spectrum.elf').write_bytes(elf);(dest/'manifest.json').write_bytes(manifest)
        run([str(validator),str(dest/'audio_spectrum.elf')],dest,'loader-validation')
        listing = run([readelf,'--dyn-syms','--wide',str(dest/'audio_spectrum.elf')],dest,'dynamic-symbols')
        retained = {'archive':str(archive),'archive_sha256':sha(archive.read_bytes()),'elf_sha256':sha(elf),
                    'manifest_sha256':sha(manifest),'memory':elf_memory(elf),
                    'imports':sorted(symbols(listing)[0]),'exports':sorted(symbols(listing)[1]),
                    'requires':json.loads(manifest)['requires'],'structural_loader_admission':'passed',
                    'comparison':'Historical reference only: different sources and compiler; no byte-reproduction claim.'}
        write_json(dest/'comparison.json',retained)
    for relative in consumed_current:
        if sha((ROOT/relative).read_bytes()) != snapshots[relative]:
            raise ValueError('Utilities source changed during target build: '+relative)
    if sha(Path(__file__).read_bytes())!=metadata_hash or git(ROOT,'rev-parse','HEAD').decode().strip()!=source_commit:
        raise ValueError('Qualification script or Utilities HEAD changed during build')
    if args.native_manifest and args.native_manifest.read_bytes()!=native_manifest_bytes:
        raise ValueError('Native manifest changed during qualification')
    receipt = {'schema':1,'utilities_base':BASE,'utilities_commit':source_commit,'utilities_head_tree':source_tree,
        'utilities_status':source_status,'exact_external_git_pins':PINS,'external_working_trees_used':False,
        'script_sha256':metadata_hash,'tools':tools,'toolchain_inputs':toolchain_inputs,'retained_recipe_path':RECIPE,
        'retained_recipe_sha256':sha((roots['wave']/RECIPE).read_bytes()),
        'retained_flags_removed':['-Wl,--undefined=__divsf3'],
        'bridge_selection':'Retained Wave profiles/shared-editor-sources.json pins generic bridge6e82fdca; raw Watch93c portable_sleep is incompatible with the generic SDK and is not selected.',
        'recipe_adjustment':'Retained libgcc linkage and all feature selectors; omit redundant forced __divsf3 after an observed baseline linker abort; later aborts also occurred without it, so removal is not a proven remedy. Actual undefined-import probe confirms __divsf3 is required and final libgcc resolves it. Source roots and aggregate SDK use recorded immutable pins. Two native selectors added only in native mode.',
        'native_manifest':native_manifest,'native_manifest_sha256':sha(native_manifest_bytes) if native_manifest_bytes else None,
        'legacy_manifest_sha256':sha(legacy_manifest.read_bytes()),'legacy_manifest_exact_base':True,
        'native_opt_in':args.native_ui,'native_selectors':NATIVE,'modules':records,
        'selector_off_exact_baseline_bytes':exact_legacy,'retained_dot63':retained,
        'capability_contract':[{'capability':c,'api':v} for c,v in CAPABILITIES],
        'capability_contract_scope':'Frozen .63 deployment requirements retained as comparison; no new manifest or binding is published.',
        'native_display_contract':{'width':410,'height':502,'selection':'PORTABLE_NATIVE_CANVAS + PORTABLE_WATCH_NATIVE_HEIGHT; runtime-provided dimensions, no new fixed-resolution compile define',
            'hardware_exercised':False,'behavioral_host_tests':'Separate qualification required; target admission does not execute application behavior.'},
        'historical_reproduction':False,'hardware_qualified':False,'firmware_assembled':False,'published':False}
    write_json(out/'qualification.json',receipt)
    if not exact_legacy:
        raise ValueError('Selector-off ELF differs from exact Utilities baseline; inspect retained receipt')
    print('Selector-off exact baseline bytes passed; target qualification receipt written',flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name,pin in PINS.items():
        p.add_argument('--'+name,type=Path,required=True,help='Repository containing exact '+pin)
    p.add_argument('--xtensa-cc',type=Path,required=True)
    p.add_argument('--output-dir',type=Path,required=True)
    p.add_argument('--retained-archive',type=Path)
    p.add_argument('--native-ui',action='store_true')
    p.add_argument('--native-manifest',type=Path,help='Optional separate versioned native manifest; never writes release inputs')
    a=p.parse_args()
    try:
        main(a)
    except Exception as exc:
        if a.output_dir.exists():
            write_json(a.output_dir/'failure.json',{'type':type(exc).__name__,'error':str(exc),'qualified':False})
        raise
