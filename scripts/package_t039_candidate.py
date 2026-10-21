#!/usr/bin/env python3
"""Build an unreleased T039 candidate, never publish or change frozen gates."""
import argparse
from pathlib import Path
import shutil
import re
import subprocess
import sys
from types import SimpleNamespace
import zipfile

import qualify_t039_ble_candidate as evidence
import prepare_native_ble_buttons_release as admission
from package_integrity import stamp_app_manifest
from t039_candidate_manifest import qualify_manifest, validate_manifest_receipt

ROOT = Path(__file__).resolve().parents[1]
QUALIFIED_SOURCE = '9a7a508d0a43572684c7feb64958eb31ee1fd583'
VERSION = '0.1.31'
MANIFEST = 'Apps/candidates/t039/ble_buttons.json'
RECORD = 'candidate-package.json'
ARCHIVE = 'ble_buttons-0.1.31-candidate.zip'
ADDITIONS = {MANIFEST, 'scripts/package_t039_candidate.py',
             'scripts/t039_candidate_manifest.py', 'tests/t039_candidate_manifest_test.cpp',
             'tests/test_t039_candidate_package.py', '.github/workflows/t039-ble-package.yml',
             'docs/t039/candidate-package/README.md'}
NOTICE = ('UNRELEASED SOFTWARE CANDIDATE 0.1.31. NOT APPROVED FOR INSTALLATION.\n'
          'The historical 0.1.30-dev1 frozen gate remains failed for this controller repair.\n'
          'No tag, release, product pin, firmware, provider grant or device action is authorized by this artifact.\n'
          'Host parser and structural admission are not installation, target execution, hardware or RF acceptance.\n')
require, read, write, digest, git, safe = evidence.require, evidence.read, evidence.write, evidence.digest, evidence.git, evidence.safe


def expected_manifest():
    result = admission.expected_manifest()
    result['version'] = VERSION
    return result


def source_guard(expected=None):
    source = git(ROOT, 'rev-parse', 'HEAD').decode().strip()
    require(expected is None or source == expected, 'Verifier source commit mismatch')
    require(not git(ROOT, 'status', '--porcelain').strip(), 'Candidate source must be clean')
    changes = git(ROOT, 'diff', '--name-status', QUALIFIED_SOURCE, source).decode().splitlines()
    require(changes and all(row.startswith('A\t') and row[2:] in ADDITIONS for row in changes),
            'Only the seven additive candidate files may differ from PR94')
    require({row[2:] for row in changes} == ADDITIONS, 'Incomplete candidate source capture')
    require(read(ROOT/MANIFEST) == expected_manifest(), 'Candidate identity or sixteen requirements changed')
    require(digest(ROOT/'Apps/ble_hid_app.inc') == evidence.CONTROLLER, 'Captured controller changed')
    return source


def inventory(root):
    result = {}
    for p in sorted(root.rglob('*')):
        require(not p.is_symlink(), 'Candidate artifact must not contain symlinks')
        if p.is_file() and p.relative_to(root).as_posix() != RECORD:
            result[p.relative_to(root).as_posix()] = digest(p)
    return result


def source_copy(root, repo, pin, owner, relative):
    content = git(repo, 'show', pin+':'+relative)
    path = safe(root/'source', owner+'/'+relative)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return evidence.sha(content)


def verify_correspondence(root):
    target = read(root/'evidence/target/qualification.json')
    native = admission.native_module(target)
    for module in target['modules']:
        if module['mode'] == 'baseline':
            continue
        for name, value in module['source_sha256'].items():
            if name.startswith('utilities:'):
                relative = name.split(':', 1)[1]
                require(evidence.sha(git(ROOT, 'show', 'HEAD:'+relative)) == value,
                        'Candidate compiled source differs: '+relative)
    return native


def package_files(root):
    return {'ble_buttons.elf': root/'app/ble_buttons.elf',
            'ble_buttons.json': root/'app/ble_buttons.json',
            'CANDIDATE_ONLY.txt': root/'CANDIDATE_ONLY.txt',
            **{p.relative_to(root).as_posix(): p for p in sorted((root/'licenses').rglob('*')) if p.is_file()}}


def create_archive(root):
    with zipfile.ZipFile(root/ARCHIVE, 'w', compression=zipfile.ZIP_STORED) as z:
        for name, path in package_files(root).items():
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.external_attr = 0o100644 << 16
            z.writestr(info, path.read_bytes())


def check_archive(root):
    with zipfile.ZipFile(root/ARCHIVE) as z:
        require(z.namelist() == list(package_files(root)), 'Archive entries changed or duplicated')
        for name, path in package_files(root).items():
            require(z.read(name) == path.read_bytes(), 'Archive payload differs')


def verify_manifest_receipt(root):
    return validate_manifest_receipt(root/'manifest-admission', root/'app/ble_buttons.json')


def validate(root, source, receipt_sha256):
    source_guard(source)
    require(digest(root/RECORD) == receipt_sha256, 'Transported candidate receipt changed')
    record = read(root/RECORD)
    require(record['schema'] == 1 and record['version'] == VERSION and record['status'] == 'unreleased-software-candidate',
            'Candidate status changed')
    require(record['source_commit'] == source and
            record['source_tree'] == git(ROOT, 'rev-parse', 'HEAD^{tree}').decode().strip(), 'Candidate source identity differs')
    require(record['target_source_commit'] == QUALIFIED_SOURCE, 'Target source changed')
    require(all(record[k] is False for k in ('published', 'hardware_qualified', 'installation_qualified',
                    'firmware_assembled', 'product_pins_changed', 'target_instructions_executed')), 'Candidate scope overstated')
    require(record['frozen_gate_status'] == 'failed' and record['frozen_gate_error'] == evidence.PARITY_ERROR,
            'Frozen parity refusal was not preserved')
    require((root/'CANDIDATE_ONLY.txt').read_text() == NOTICE, 'Candidate notice missing')
    require(digest(root/'evidence/candidate-evidence.json') == record['qualification_receipt_sha256'],
            'Qualification receipt changed')
    q = evidence.validate(root/'evidence', QUALIFIED_SOURCE)
    require(q['host_case_counts'] == record['host_case_counts'], 'Host case counts changed')
    require(record['external_git_pins'] == evidence.frozen.PINS and
            record['watch_touch_pin'] == evidence.WATCH and record['legacy_system_pin'] == evidence.LEGACY_SYSTEM,
            'Candidate cohort changed')
    native = verify_correspondence(root)
    app = root/'app'
    require(read(app/'ble_buttons.json') == expected_manifest() and
            (app/'ble_buttons.json').read_bytes() == (ROOT/MANIFEST).read_bytes(), 'Candidate manifest differs')
    require(digest(app/'ble_buttons.elf') == native['sha256'] == record['elf_sha256'] and
            (app/'ble_buttons.elf').stat().st_size == native['size_bytes'] == record['elf_bytes'], 'Candidate ELF differs')
    require(read(app/'integrity.json') == stamp_app_manifest(expected_manifest(), app/'ble_buttons.elf'),
            'Detached integrity sidecar differs')
    require(digest(app/'ble_buttons.json') == record['manifest_sha256'], 'Candidate manifest digest differs')
    require((root/'target/native-manifest.json').read_bytes() == (app/'ble_buttons.json').read_bytes() and
            (root/'target/native/ble_buttons.elf').read_bytes() == (app/'ble_buttons.elf').read_bytes(),
            'Admission candidate inputs differ')
    adm = admission.validate_admission(root, native)
    require(adm['budgets'] == record['memory_budgets'], 'Candidate memory evidence differs')
    verify_manifest_receipt(root)
    for name, value in record['candidate_source_sha256'].items():
        require(name in ADDITIONS and digest(root/'source/candidate'/name) == value == digest(ROOT/name),
                'Candidate source capture changed')
    require(set(record['candidate_source_sha256']) == ADDITIONS, 'Missing candidate source')
    require(record['artifact_sha256'] == inventory(root), 'Candidate artifact inventory differs')
    require(record['licenses_sha256'] and all(digest(safe(root, p)) == h for p, h in record['licenses_sha256'].items()), 'License evidence differs')
    check_archive(root)
    return record


def build(args):
    source = source_guard(); out = args.output.resolve(); original = args.evidence.resolve()
    require(not out.exists(), 'Output must be new; refusing stale candidate data')
    require(digest(original/'candidate-evidence.json') == args.evidence_sha256,
            'Incoming qualification receipt does not match independently supplied digest')
    evidence.validate(original, QUALIFIED_SOURCE)
    for name in ('system', 'runtime', 'bridges'):
        repo = getattr(args, name)
        require(git(repo, 'rev-parse', 'HEAD').decode().strip() == evidence.frozen.PINS[name] and
                not git(repo, 'status', '--porcelain').strip(), 'Admission dependency changed: '+name)
    out.mkdir(parents=True)
    (out/'CANDIDATE_ONLY.txt').write_text(NOTICE)
    shutil.copytree(original, out/'evidence')
    native = verify_correspondence(out)
    app = out/'app'; app.mkdir()
    shutil.copyfile(out/'evidence/target/native/ble_buttons.elf', app/'ble_buttons.elf')
    shutil.copyfile(ROOT/MANIFEST, app/'ble_buttons.json')
    write(app/'integrity.json', stamp_app_manifest(expected_manifest(), app/'ble_buttons.elf'))
    # The app manifest must remain accepted by the production parser: integrity
    # fields are detached, never inserted as unrecognized top-level app keys.
    (out/'target/native').mkdir(parents=True)
    shutil.copyfile(app/'ble_buttons.elf', out/'target/native/ble_buttons.elf')
    shutil.copyfile(app/'ble_buttons.json', out/'target/native-manifest.json')
    adm = admission.stage_admission(SimpleNamespace(artifact=out, system=args.system, runtime=args.runtime))
    for relative in adm['runtime_source_sha256']:
        source_copy(out, args.runtime, evidence.frozen.PINS['runtime'], 'runtime', relative)
    source_copy(out, args.system, evidence.frozen.PINS['system'], 'system', 'scripts/report_native_settings_memory.py')
    qualify_manifest(args.runtime, app/'ble_buttons.json', out/'manifest-admission')
    candidate_sources = {name: source_copy(out, ROOT, source, 'candidate', name) for name in sorted(ADDITIONS)}
    licenses = {}
    for owner, repo, pin in [('utilities', ROOT, source),
                             *[(name, getattr(args, name), pin) for name, pin in evidence.frozen.PINS.items()]]:
        for relative in git(repo, 'ls-tree', '-r', '--name-only', pin).decode().splitlines():
            if re.search(r'(^|/)(LICENSE[^/]*|COPYING[^/]*|NOTICE[^/]*|PATENTS[^/]*|AUTHORS[^/]*)$', relative, re.I):
                content = git(repo, 'show', pin+':'+relative)
                dest = safe(out/'licenses', owner+'/'+relative)
                dest.parent.mkdir(parents=True, exist_ok=True); dest.write_bytes(content)
                licenses[dest.relative_to(out).as_posix()] = digest(dest)
    target = read(out/'evidence/target/qualification.json')
    toolchain = Path(target['tools']['cc']['path']).resolve().parent.parent
    require(digest(Path(target['tools']['cc']['path'])) == target['tools']['cc']['sha256'], 'Compiler changed before notice capture')
    for notice in sorted((toolchain/'share/licenses').rglob('*')):
        if notice.is_file():
            dest = out/'licenses/toolchain'/notice.relative_to(toolchain)
            dest.parent.mkdir(parents=True, exist_ok=True); dest.write_bytes(notice.read_bytes())
            licenses[dest.relative_to(out).as_posix()] = digest(dest)
    require(licenses and any(p.startswith('licenses/toolchain/') for p in licenses), 'Missing source/compiler notices')
    create_archive(out)
    q = read(out/'evidence/candidate-evidence.json')
    record = {'schema': 1, 'version': VERSION, 'status': 'unreleased-software-candidate',
              'source_commit': source, 'source_tree': git(ROOT, 'rev-parse', 'HEAD^{tree}').decode().strip(),
              'target_source_commit': QUALIFIED_SOURCE, 'qualification_receipt_sha256': args.evidence_sha256,
              'external_git_pins': evidence.frozen.PINS, 'watch_touch_pin': evidence.WATCH,
              'legacy_system_pin': evidence.LEGACY_SYSTEM, 'host_case_counts': q['host_case_counts'],
              'candidate_source_sha256': candidate_sources, 'licenses_sha256': licenses, 'elf_sha256': native['sha256'],
              'elf_bytes': native['size_bytes'], 'manifest_sha256': digest(app/'ble_buttons.json'),
              'memory_budgets': adm['budgets'], 'frozen_gate_status': 'failed',
              'frozen_gate_error': evidence.PARITY_ERROR, 'published': False, 'hardware_qualified': False,
              'installation_qualified': False, 'firmware_assembled': False, 'product_pins_changed': False,
              'target_instructions_executed': False, 'artifact_sha256': inventory(out)}
    write(out/RECORD, record)
    require(source_guard() == source, 'Candidate source changed during packaging')
    validate(out, source, digest(out/RECORD))
    print('Candidate 0.1.31 software package PASS; unreleased, installation/hardware unqualified; frozen gate remains FAILED.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='action', required=True)
    b = commands.add_parser('build')
    for name in ('evidence', 'system', 'runtime', 'bridges', 'output'):
        b.add_argument('--'+name, type=Path, required=True)
    b.add_argument('--evidence-sha256', required=True)
    v = commands.add_parser('verify')
    v.add_argument('--artifact', type=Path, required=True)
    v.add_argument('--source-sha', required=True)
    v.add_argument('--receipt-sha256', required=True)
    args = parser.parse_args()
    if args.action == 'build':
        build(args)
    else:
        validate(args.artifact.resolve(), args.source_sha, args.receipt_sha256)
        print('Transported candidate exact source/ELF/manifest/evidence match; no publication or hardware acceptance.')


if __name__ == '__main__':
    main()
