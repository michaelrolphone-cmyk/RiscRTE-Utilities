#!/usr/bin/env python3
"""Check current Scanner host/target agreement without changing product profiles."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def check(build, host):
    profile_path = ROOT / 'Apps/x4-ble-resident-profile.json'
    profile = json.loads(profile_path.read_text())
    target = json.loads((build / 'ble_scanner/x4-native-app.json').read_text())
    witness = json.loads((host / 'summary.json').read_text())
    for field in ('build_defines', 'requires', 'required_grants'):
        assert target[field] == witness[field] == profile[field], ('shared Scanner profile drift', field)
    assert witness['profile_sha256'] == hashlib.sha256(profile_path.read_bytes()).hexdigest()
    for name, expected in target['compiled_dependencies_sha256'].items():
        assert witness['compiled_dependencies_sha256'].get(name) == expected, ('target/host input drift', name)
    versions = json.loads((ROOT / 'Apps/x4-resident-versions.json').read_text())
    assert target['version'] == witness['selected_x4_version'] == versions['ble_scanner']
    assert target['source_revision'] == witness['source_revision']
    assert target['system_source_revision'] == witness['system_revision']
    assert target['runtime_source_revision'] == witness['runtime_revision']
    assert not target['system_dirty'] and not witness['system_dirty']
    assert len(witness['results']) == 48 and all(row['passed'] for row in witness['results'])
    assert [row['sanitized'] for row in witness['model_results']] == [False, True]
    assert all(row['passed'] for row in witness['model_results'])
    assert target['quick_render_definitions'] == 0
    assert target['target_validation'] == 'passed'
    assert not target['installable'] and not target['hardware_verified'] and not witness['hardware_verified']
    assert not witness['target_elf_compiled']
    assert hashlib.sha256((build / 'ble_scanner/ble_scanner.elf').read_bytes()).hexdigest() == target['elf_sha256']
    print('Current Scanner target and source-only host witness agree; delivered flags/grants unchanged')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--host', type=Path, required=True)
    args = p.parse_args()
    check(args.build, args.host)
