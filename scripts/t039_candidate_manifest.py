#!/usr/bin/env python3
"""Exercise unchanged pinned Runtime metadata helpers for an unreleased candidate.

This is not full Runtime app-policy, owner-grant, installation or device admission.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_PIN = '0aab2fd3b03b4190d638e23c79a465afeecf5dfb'
VERSION = '0.1.31'
HARNESS = ROOT/'tests/t039_candidate_manifest_test.cpp'
RUNTIME_SELECTIONS = (
    'src/bootstrap/AppDeclarations.h', 'src/bootstrap/AppRequirementLimits.h',
    'src/bootstrap/Json.h', 'src/bootstrap/Runtime.cpp', 'src/bootstrap/Runtime.h',
    'src/diagnostics/Performance.h', 'src/runtime/packages/PackageIdentity.h',
    'src/runtime/packages/PackageJsonGuard.h', 'lib/ArduinoJson',
    'sdk/app/RiscKeyValueV1.h', 'sdk/app/RiscAppDataV1.h',
    'sdk/driver/RiscBoundKeyValueV1.h', 'sdk/driver/RiscBoundAppDataV1.h',
)
NEGATIVE_BOUNDARIES = {
    **{name: 'identity' for name in ('prerelease-version', 'build-metadata-version', 'incomplete-version',
                                   'overflow-version', 'wrong-type', 'wrong-architecture', 'wrong-entry',
                                   'unsafe-id', 'traversal-artifact', 'absolute-artifact')},
    **{name: 'contract' for name in ('different-candidate-version', 'different-id', 'different-artifact',
                                   'missing-required-row', 'wrong-api-pair', 'wrong-capability-pair')},
    **{name: 'schema' for name in ('missing-requires', 'size-field-not-runtime-schema',
                                 'sha-field-not-runtime-schema', 'unknown-top-level-key')},
    **{name: 'declarations' for name in ('nonarray-requires', 'seventeenth-row', 'duplicate-requirement',
                                       'optional-overflow', 'nonobject-row', 'extra-row-key', 'missing-row-api',
                                       'string-api', 'zero-api', 'negative-api', 'fractional-api', 'boolean-api',
                                       'overflow-api', 'provider-only-capability')},
    **{name: 'json' for name in ('duplicate-json-key', 'escaped-json-key', 'trailing-content', 'embedded-nul')},
    **{name: 'package-size' for name in ('empty-manifest', 'oversized-manifest')},
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], timeout=120,
                                   env={**os.environ, 'GIT_NO_LAZY_FETCH': '1'})


def write(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True)+'\n')


def safe(root, relative):
    require(isinstance(relative, str) and relative and '\\' not in relative and
            not PurePosixPath(relative).is_absolute() and
            all(p not in ('', '.', '..') for p in relative.split('/')), 'Unsafe manifest evidence path')
    path = root.joinpath(*PurePosixPath(relative).parts)
    require(path.resolve().is_relative_to(root.resolve()), 'Manifest evidence escapes output')
    return path


def metadata_gate(runtime_source):
    text = runtime_source.read_text()
    start = text.index('    if (!keys(manifest,{"type","id","version","architecture","file_name","entry","requires"}')
    final = '    if(const char* error=declarations.error(MaxAppRequirements))return fail(error);'
    end = text.index(final, start)+len(final)
    return (text[start:end]+'\n').encode()


def validate_result(parsed):
    require(parsed['manifest_metadata_accepted'] is True and parsed['exact_sixteen_requirements'] is True and
            parsed['candidate_version'] == VERSION and parsed['declaration_limit'] == 16 and
            parsed['production_json_identity_declaration_helpers'] is True and
            parsed['full_runtime_app_policies_executed'] is False and
            parsed['owner_grants_qualified'] is False and parsed['installation_qualified'] is False and
            parsed['target_instructions_executed'] is False, 'Unexpected manifest qualification scope')
    controls = parsed['negative_controls']
    require(len(controls) == parsed['negative_control_count'] == len(NEGATIVE_BOUNDARIES) and
            {x['id']: x['boundary'] for x in controls} == NEGATIVE_BOUNDARIES and
            all(x['rejected'] is True for x in controls), 'Missing or failed manifest negative controls')
    outcomes = parsed['case_outcomes']
    expected = [{'id': 'exact-candidate-manifest', 'accepted': True, 'passed': True}]
    expected += [{'id': x['id'], 'accepted': False, 'passed': True, 'boundary': x['boundary']} for x in controls]
    require(outcomes == expected, 'Manifest case outcomes differ')


def validate_manifest_receipt(output, manifest):
    """Verify transported evidence by relative paths; never use producer paths."""
    output, manifest = Path(output).resolve(), Path(manifest).resolve()
    receipt = json.loads(safe(output, 'receipt.json').read_text())
    require(receipt['schema'] == 1 and receipt['passed'] is True and receipt['candidate_version'] == VERSION and
            receipt['runtime_commit'] == RUNTIME_PIN, 'Wrong manifest receipt identity')
    require(receipt['production_parser_helpers_qualified'] is True and
            all(receipt[k] is False for k in ('full_runtime_app_policies_executed', 'owner_grants_qualified',
                                            'installation_qualified', 'target_instructions_executed')),
            'Manifest receipt overstates scope')
    require(manifest.read_bytes() == safe(output, 'manifest.json').read_bytes() and
            digest(manifest) == receipt['manifest_sha256'] and
            manifest.stat().st_size == receipt['manifest_bytes'] and 0 < receipt['manifest_bytes'] <= 2048,
            'Candidate manifest evidence differs')
    hashes = receipt['runtime_source_sha256']
    require(hashes and all(any(n == selection or n.startswith(selection+'/') for n in hashes)
                          for selection in RUNTIME_SELECTIONS), 'Missing Runtime parser source selection')
    actual = {p.relative_to(output/'runtime').as_posix(): digest(safe(output/'runtime', p.relative_to(output/'runtime').as_posix()))
              for p in (output/'runtime').rglob('*') if p.is_file()}
    require(actual == hashes, 'Runtime parser source inventory differs')
    expected_sources = {'runtime/'+name: value for name, value in hashes.items()}
    expected_sources.update({'manifest-test.cpp': receipt['harness_sha256'],
                             'manifest-helper.py': receipt['helper_sha256'],
                             'app-policies-metadata-gate.txt': receipt['app_policies_metadata_gate']['sha256']})
    require(receipt['source_sha256'] == expected_sources, 'Manifest source closure differs')
    for name, value in receipt['source_sha256'].items():
        require(digest(safe(output, name)) == value, 'Changed manifest source: '+name)
    gate = receipt['app_policies_metadata_gate']
    require(gate['source'] == 'runtime/src/bootstrap/Runtime.cpp' and
            gate['source_sha256'] == hashes['src/bootstrap/Runtime.cpp'] and
            gate['extracted_source'] == 'app-policies-metadata-gate.txt' and
            safe(output, gate['extracted_source']).read_bytes() == metadata_gate(safe(output, gate['source'])) and
            gate['full_app_policies_executed'] is False, 'appPolicies metadata predicate binding differs')
    require(receipt['harness_sha256'] == digest(HARNESS) and
            receipt['helper_sha256'] == digest(Path(__file__)), 'Verifier helper source differs from producer')
    runs = receipt['runs']
    require(len(runs) == 2 and {r['sanitized'] for r in runs} == {False, True}, 'Missing manifest execution modes')
    for run in runs:
        require(type(run['sanitized']) is bool and type(run['returncode']) is int and run['returncode'] == 0,
                'Failed manifest run')
        mode = 'sanitized' if run['sanitized'] else 'normal'
        require(run['mode'] == mode, 'Manifest run mode differs')
        for filename, field in ((mode+'.log', 'log_sha256'), (mode+'-build.log', 'build_log_sha256'),
                                (mode+'-commands.json', 'command_sha256'), (mode+'.d', 'dependency_file_sha256'),
                                ('manifest-test-'+mode, 'binary_sha256')):
            require(digest(safe(output, filename)) == run[field], 'Changed manifest run evidence: '+filename)
        require(json.loads(safe(output, mode+'.log').read_text()) == run['result'], 'Manifest log/result differ')
        validate_result(run['result'])
        commands = json.loads(safe(output, mode+'-commands.json').read_text())
        require(commands == run['commands'] and len(commands) == 2 and
                all(isinstance(c, list) and c and all(isinstance(a, str) for a in c) for c in commands),
                'Manifest commands differ')
        build = commands[0]
        require({'-std=c++17', '-O0', '-g', '-Wall', '-Wextra', '-Werror', '-MMD', '-MF'} <= set(build) and
                any(Path(a).name == 'manifest-test.cpp' for a in build) and
                len(commands[1]) == 2 and Path(commands[1][0]).name == 'manifest-test-'+mode and
                Path(commands[1][1]).name == 'manifest.json', 'Manifest command contract differs')
        sanitizer_flags = {'-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'}
        require(sanitizer_flags <= set(build) if run['sanitized'] else
                not any(a.startswith('-fsanitize=') for a in build), 'Manifest sanitizer flags differ')
        dependencies = run['compiled_dependency_sha256']
        required = {'harness', 'runtime:src/bootstrap/AppDeclarations.h',
                    'runtime:src/runtime/packages/PackageIdentity.h', 'runtime:src/runtime/packages/PackageJsonGuard.h',
                    'runtime:lib/ArduinoJson/src/ArduinoJson.h'}
        require(required <= dependencies.keys(), 'Missing compiled Runtime parser dependency')
        for identity, value in dependencies.items():
            relative = 'manifest-test.cpp' if identity == 'harness' else 'runtime/'+identity.removeprefix('runtime:')
            require((identity == 'harness' or identity.startswith('runtime:')) and
                    receipt['source_sha256'].get(relative) == value and digest(safe(output, relative)) == value,
                    'Changed compiled manifest dependency')
    require(runs[0]['result'] == runs[1]['result'], 'Normal and sanitized manifest controls differ')
    return receipt


def qualify_manifest(runtime, manifest, output):
    """Stage exact Git-object sources, run normal/sanitized checks, return receipt.

    runtime: Runtime repository containing RUNTIME_PIN at HEAD.
    manifest: exact un-stamped candidate sidecar (numeric 0.1.31).
    output: new directory, outside the manifest and all source inputs.
    Writes receipt.json, manifest.json, source closure, commands, logs and binaries.
    """
    runtime, manifest, output = map(lambda p: Path(p).resolve(), (runtime, manifest, output))
    require(git(runtime, 'rev-parse', 'HEAD').decode().strip() == RUNTIME_PIN,
            'Runtime checkout must select the exact candidate pin')
    require(manifest.is_file(), 'Candidate manifest is missing')
    require(not output.exists(), 'Refuse stale manifest evidence: output must be new')
    require(not runtime.is_relative_to(output) and not ROOT.is_relative_to(output) and
            not manifest.is_relative_to(output), 'Output overlaps an input root')
    raw = manifest.read_bytes()
    require(0 < len(raw) <= 2048, 'Candidate manifest must fit the 2048-byte package bound')
    source_files = {str(Path(__file__).resolve()): digest(Path(__file__)), str(HARNESS): digest(HARNESS)}
    output.mkdir(parents=True)
    staged = output/'runtime'; staged.mkdir()
    names = git(runtime, 'ls-tree', '-r', '--name-only', RUNTIME_PIN, '--', *RUNTIME_SELECTIONS).decode().splitlines()
    require(names and all(any(n == selection or n.startswith(selection+'/') for n in names)
                          for selection in RUNTIME_SELECTIONS), 'Incomplete Runtime parser source selection')
    hashes = {}
    for name in names:
        relative = PurePosixPath(name)
        require(not relative.is_absolute() and all(p not in ('', '.', '..') for p in name.split('/')),
                'Unsafe Runtime source name')
        path = staged.joinpath(*relative.parts); path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(git(runtime, 'show', RUNTIME_PIN+':'+name))
        hashes[name] = digest(path)
    harness = output/'manifest-test.cpp'; harness.write_bytes(HARNESS.read_bytes())
    (output/'manifest-helper.py').write_bytes(Path(__file__).read_bytes())
    extracted = output/'app-policies-metadata-gate.txt'
    extracted.write_bytes(metadata_gate(staged/'src/bootstrap/Runtime.cpp'))
    candidate = output/'manifest.json'; candidate.write_bytes(raw)
    includes = ['-I'+str(staged/x) for x in ('src', 'sdk/app', 'sdk/driver', 'lib/ArduinoJson/src')]
    runs = []
    compiler = os.environ.get('CXX', 'c++')
    compiler_version = subprocess.check_output([compiler, '--version'], text=True, timeout=30)
    for sanitized in (False, True):
        mode = 'sanitized' if sanitized else 'normal'
        binary = output/('manifest-test-'+mode); dependencies = output/(mode+'.d')
        # Match the unoptimized production Runtime metadata-test recipe. GCC
        # 14's optimized sanitizer build diagnoses unchanged ArduinoJson 7.4.2
        # iterator internals; do not patch those sources or suppress warnings.
        flags = ['-std=c++17', '-O0', '-g', '-Wall', '-Wextra', '-Werror']
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                      '-fno-omit-frame-pointer', '-no-pie']
        command = [compiler, *flags, *includes, '-MMD', '-MF', str(dependencies), str(harness), '-o', str(binary)]
        commands = [command, [str(binary), str(candidate)]]
        write(output/(mode+'-commands.json'), commands)
        build = subprocess.run(command, cwd=output, text=True, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, timeout=180)
        (output/(mode+'-build.log')).write_text(build.stdout)
        require(build.returncode == 0, 'Manifest helper build failed: '+mode)
        result = subprocess.run(commands[1], cwd=output, text=True, capture_output=True, timeout=30,
                                env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
                                     'UBSAN_OPTIONS': 'halt_on_error=1'})
        (output/(mode+'.log')).write_text(result.stdout+result.stderr)
        require(result.returncode == 0, 'Candidate manifest or negative controls failed: '+mode)
        parsed = json.loads(result.stdout)
        validate_result(parsed)
        consumed = {}
        for name in shlex.split(dependencies.read_text().replace('\\\n', ' ').split(':', 1)[1]):
            path = Path(name).resolve()
            require(path == harness or path.is_relative_to(staged), 'Unbound non-system compiler dependency')
            identity = 'harness' if path == harness else 'runtime:'+path.relative_to(staged).as_posix()
            consumed[identity] = digest(path)
        require('runtime:src/bootstrap/AppDeclarations.h' in consumed and
                'runtime:src/runtime/packages/PackageIdentity.h' in consumed and
                'runtime:src/runtime/packages/PackageJsonGuard.h' in consumed and
                'runtime:lib/ArduinoJson/src/ArduinoJson.h' in consumed,
                'Production parser dependency missing')
        runs.append({'mode': mode, 'sanitized': sanitized, 'commands': commands, 'returncode': result.returncode,
                     'result': parsed, 'compiled_dependency_sha256': consumed,
                     'dependency_file_sha256': digest(dependencies),
                     'binary_sha256': digest(binary), 'log_sha256': digest(output/(mode+'.log')),
                     'build_log_sha256': digest(output/(mode+'-build.log')),
                     'command_sha256': digest(output/(mode+'-commands.json'))})
    require(runs[0]['result'] == runs[1]['result'], 'Normal and sanitized manifest controls differ')
    require(manifest.read_bytes() == raw and all(digest(Path(p)) == value for p, value in source_files.items()),
            'Manifest qualification source changed during execution')
    require(all(digest(staged/name) == value for name, value in hashes.items()), 'Staged Runtime source changed')
    source_hashes = {'runtime/'+name: value for name, value in hashes.items()}
    source_hashes.update({name: digest(output/name) for name in
                         ('manifest-test.cpp', 'manifest-helper.py', 'app-policies-metadata-gate.txt')})
    receipt = {'schema': 1, 'passed': True, 'candidate_version': VERSION, 'runtime_commit': RUNTIME_PIN,
               'runtime_source_sha256': hashes, 'manifest_sha256': digest(candidate),
               'source_sha256': source_hashes,
               'manifest_bytes': len(raw), 'harness_sha256': digest(harness),
               'helper_sha256': source_files[str(Path(__file__).resolve())],
               'app_policies_metadata_gate': {'source': 'runtime/src/bootstrap/Runtime.cpp',
                   'source_sha256': hashes['src/bootstrap/Runtime.cpp'],
                   'extracted_source': extracted.name, 'sha256': digest(extracted),
                   'full_app_policies_executed': False},
               'compiler_version': compiler_version, 'runs': runs,
               'production_parser_helpers_qualified': True,
               'full_runtime_app_policies_executed': False,
               'owner_grants_qualified': False, 'installation_qualified': False,
               'target_instructions_executed': False,
               'scope': 'Pinned production JSON, package identity and app declaration helpers, with the appPolicies top-level metadata predicate and exact candidate contract; no owner grants, provider binding, full appPolicies, ELF execution, installation or device fit.'}
    write(output/'receipt.json', receipt)
    return validate_manifest_receipt(output, manifest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'manifest', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    qualify_manifest(args.runtime, args.manifest, args.output)
    print('Candidate 0.1.31 metadata helpers PASS in normal and sanitized modes; installation remains unqualified.')


if __name__ == '__main__':
    main()
