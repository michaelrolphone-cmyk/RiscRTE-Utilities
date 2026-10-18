#!/usr/bin/env python3
"""Package Countdown from the complete, exact-source dual-app qualification.

The Alarms package remains internal evidence and is never published by this path.
"""
import argparse
import json
from pathlib import Path
import shutil

import build_native_alarms_qualification as recipe
import prepare_native_alarms_release as shared_prepare
import publish_native_alarms as shared_publish
from prepare_native_alarms_release import require, read, checked_path

TAG = 'native-countdown-0.2.0-dev1'
VERSION = '0.2.0'
MANIFEST = 'Apps/native/countdown.json'
PACKAGE = 'countdown-app'
EXTRA_SOURCE = (MANIFEST, 'scripts/prepare_native_countdown_release.py',
                'scripts/publish_native_countdown.py', 'tests/test_native_countdown_release.py',
                '.github/workflows/native-countdown.yml', 'docs/native-countdown/README.md')


def expected_manifest():
    manifest = shared_prepare.expected_manifest()
    manifest.update(id='countdown', version=VERSION, file_name='countdown.elf')
    return manifest


def qualified_record(root, source):
    shared = shared_publish.validate(root, source)
    target = read(root/'target/qualification.json')
    native = shared_prepare.target_modules(target)[('countdown', 'native')]
    package = root/PACKAGE
    return {
        'schema': 1, 'tag': TAG, 'version': VERSION, 'source_commit': source,
        'working_tree_dirty': False, 'external_git_pins': recipe.PINS,
        'required_capabilities': expected_manifest()['requires'],
        'shared_build_record_sha256': recipe.digest((root/'app/alarms-build-record.json').read_bytes()),
        'target_receipt_sha256': shared['target_receipt_sha256'],
        'controls': shared['controls'], 'controls_profile_sha256': shared['controls_profile_sha256'],
        'elf_sha256': native['sha256'], 'elf_size_bytes': native['size_bytes'],
        'command_sha256': native['command_sha256'],
        'manifest_sha256': recipe.digest((package/'countdown.json').read_bytes()),
        'layout_sha256': shared['memory_admissions']['countdown']['layout_sha256'],
        'admission_sha256': recipe.digest((package/'elf-memory-admission.json').read_bytes()),
        'source_files': {**shared['source_files'], **{
            'source/utilities/'+p: recipe.digest((root/'source/utilities'/p).read_bytes()) for p in EXTRA_SOURCE}},
        'hardware_qualified': False, 'firmware_replacement': False,
        'legacy_manifest_unchanged': True, 'alarms_release_unchanged': True,
        'limitations': [
            'No Xtensa execution, physical-device fit or installation-path qualification',
            'All 15 manifest providers and deployment-owned bindings required; preferences namespace 1',
            'Optional app.catalog remains unavailable without a separately reviewed grant',
            'Alarms package is retained as dual-app evidence only; this publisher releases Countdown only'],
    }


def qualify(root):
    root = root.resolve()
    source = recipe.git(recipe.ROOT, 'rev-parse', 'HEAD').decode().strip()
    require(not recipe.git(recipe.ROOT, 'status', '--porcelain').strip(), 'Release requires a clean exact source')
    shared_publish.validate(root, source)
    require(read(recipe.ROOT/MANIFEST) == expected_manifest(), 'Standalone Countdown manifest changed')
    package = root/PACKAGE
    package.mkdir(exist_ok=False)
    shutil.copyfile(root/'target/native/countdown.elf', package/'countdown.elf')
    shutil.copyfile(root/'target/native/countdown-command.json', package/'command.json')
    shutil.copyfile(recipe.ROOT/MANIFEST, package/'countdown.json')
    shutil.copyfile(root/'admission/countdown-elf-layout.json', package/'elf-layout.json')
    admission = read(root/'admission/countdown-elf-memory-admission.json')
    admission['manifest_sha256'] = recipe.digest((package/'countdown.json').read_bytes())
    (package/'elf-memory-admission.json').write_text(json.dumps(admission, indent=2)+'\n')
    for relative in EXTRA_SOURCE:
        content = recipe.git(recipe.ROOT, 'show', source+':'+relative)
        require(content == (recipe.ROOT/relative).read_bytes(), 'Countdown source changed: '+relative)
        dest = root/'source/utilities'/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(content)
    record = qualified_record(root, source)
    (package/'countdown-build-record.json').write_text(json.dumps(record, indent=2)+'\n')
    require(recipe.git(recipe.ROOT, 'rev-parse', 'HEAD').decode().strip() == source and
            not recipe.git(recipe.ROOT, 'status', '--porcelain').strip(), 'Source changed during packaging')
    print('Native Countdown 0.2.0: exact-source standalone package PASS')
    return record


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact', type=Path, required=True)
    qualify(parser.parse_args().artifact)
