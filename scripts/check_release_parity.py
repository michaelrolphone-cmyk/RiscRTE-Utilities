#!/usr/bin/env python3
"""Compare actual development ELF bytes against an immutable release snapshot.

Only the explicitly validated synchronized cohort is mandatory. Older published
payload differences remain visible, never silently described as full parity.
"""
import hashlib
import json
from check_baseline import ROOT


def compare(expected, actual, data):
    return (actual['version'] == expected['version'] and len(data) == expected['size']
            and hashlib.sha256(data).hexdigest() == expected['sha256'])


def validate_cohort(baseline, evidence, inventory):
    expected_ids = [app['id'] for app in baseline['apps']]
    actual_ids = [app['id'] for app in evidence['apps']]
    inventory_ids = [app['id'] for app in inventory['apps']]
    required = set(baseline['required_byte_parity'])
    for ids in (expected_ids, actual_ids, inventory_ids):
        if len(ids) != len(set(ids)):
            raise ValueError('Duplicate parity inventory/evidence identity')
    if (set(expected_ids) != set(actual_ids) or set(expected_ids) != set(inventory_ids)
            or not required <= set(expected_ids)):
        raise ValueError('Full exact inventory required for release parity comparison')
    for app in evidence['apps']:
        if app['file_name'] != app['id'] + '.elf':
            raise ValueError('Parity evidence filename does not match identity')
    return {app['id']: app for app in evidence['apps']}, required


def main():
    baseline = json.loads((ROOT / 'sdk/release-baseline.json').read_text())
    evidence = json.loads((ROOT / 'dist/apps/build-evidence.json').read_text())
    inventory = json.loads((ROOT / 'utilities-manifest.json').read_text())
    actual, required = validate_cohort(baseline, evidence, inventory)
    rows = []
    failed = []
    for expected in baseline['apps']:
        app = actual[expected['id']]
        data = (ROOT / 'dist/apps' / app['file_name']).read_bytes()
        match = compare(expected, app, data)
        rows.append({'id': app['id'], 'version': app['version'], 'published_version': expected['version'],
                     'byte_parity': match, 'required': app['id'] in required,
                     'built_sha256': hashlib.sha256(data).hexdigest(), 'published_sha256': expected['sha256']})
        if app['id'] in required and not match:
            failed.append(app['id'])
    report = {'reader_source_commit': baseline['reader_source_commit'], 'apps': rows}
    (ROOT / 'dist/apps/release-parity.json').write_text(json.dumps(report, indent=2) + '\n')
    if failed:
        raise ValueError('Required published-byte parity failed: ' + ', '.join(failed))
    print(f"Published-byte parity: {sum(row['byte_parity'] for row in rows)}/{len(rows)}; all {len(required)} required synchronized apps match")


if __name__ == '__main__':
    main()
