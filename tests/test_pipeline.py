import json
import hashlib
import pathlib
import sys
import tempfile
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'scripts'))
from app_manifest import validate_manifest
from build_all_apps import validate_inventory
from check_baseline import ROOT, audit, check_sdk, classify, git_blob
from check_release_parity import compare, validate_cohort
from native_app_symbols import validate_imports
from package_integrity import stamp_app_manifest


class PipelineTests(unittest.TestCase):
    def test_sdk_snapshot_and_inventory(self):
        check_sdk()
        rows = audit()['files']
        self.assertEqual(len(rows), 6)
        self.assertTrue(all(row['state'] == 'unchanged' for row in rows))

    def test_current_reader_release_inputs_and_provenance(self):
        inventory = json.loads((ROOT / 'utilities-manifest.json').read_text())
        drift = json.loads((ROOT / 'docs/source-drift.json').read_text())
        releases = json.loads((ROOT / 'sdk/release-baseline.json').read_text())
        self.assertEqual(drift['reader_commit'],
                         '3722a3f44a3294ba5e8adab830807a2523df3b03')
        self.assertTrue(all(app['upstream_commit'] == drift['reader_commit']
                            for app in inventory['apps']))
        self.assertEqual({app['id']: app['version'] for app in inventory['apps']},
                         {'gps': '1.0.1', 'lora': '1.0.1', 'battery': '1.0.2'})
        states = {row['path']: row['state'] for row in drift['files']}
        for path in ('Apps/gps.json', 'Apps/lora.c', 'Apps/lora.json', 'Apps/battery.json'):
            self.assertEqual(states[path], 'converged')
        self.assertEqual(states['Apps/gps.c'], 'unchanged')
        self.assertEqual(states['Apps/battery.c'], 'unchanged')
        versions = {item['id']: item['version'] for item in inventory['apps']}
        for app in releases['apps']:
            self.assertEqual(app['version'], versions[app['id']])
            self.assertEqual(app['sha256'], app['package_asset']['payload_sha256'])
        profile = json.loads((ROOT / 'sdk/baseline.json').read_text())
        self.assertTrue(any(row.get('profile') == 'lora-1.0.1'
                            for row in profile['files']))

    def test_three_way_conflict_preservation(self):
        for base, local, upstream, state in [('a','a','a','unchanged'), ('a','b','b','converged'),
                                            ('a','a','b','upstream-only'), ('a','b','a','external-only'),
                                            ('a','b','c','conflict')]:
            self.assertEqual(classify(base, local, upstream), state)

    def test_git_blob_identity(self):
        self.assertEqual(git_blob(b''), 'e69de29bb2d1d6434b8b29ae775ad8c2e48c5391')

    def test_all_app_manifests(self):
        for app in json.loads((ROOT / 'utilities-manifest.json').read_text())['apps']:
            self.assertEqual(app['source'], app['source_path'])
            self.assertEqual(app['manifest'], app['manifest_path'])
            source = ROOT / app['source_path']
            validate_manifest(source, pathlib.Path(app['file_name']))
            self.assertEqual(json.loads(source.with_suffix('.json').read_text())['version'], app['version'])

    def test_unknown_import_rejected(self):
        with self.assertRaises(ValueError):
            validate_imports(' 1: 00000000 0 FUNC GLOBAL DEFAULT UND privileged_unsafe', {'printf'})
        with self.assertRaises(ValueError):
            validate_imports(' 1: 00000000 0 FUNC WEAK DEFAULT UND privileged_unsafe', {'printf'})
        self.assertEqual(validate_imports(' 1: 00000000 0 FUNC GLOBAL DEFAULT UND printf', {'printf'}), {'printf'})

    def test_stamped_artifact_cannot_silently_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = pathlib.Path(tmp) / 'sample.elf'
            elf.write_bytes(b'x' * 64)
            stamped = stamp_app_manifest({'file_name': elf.name, 'version': '1.0.0'}, elf)
            self.assertEqual(stamped['size_bytes'], 64)
            elf.write_bytes(b'y' * 64)
            with self.assertRaises(ValueError):
                stamp_app_manifest(stamped, elf)

    def test_inventory_cannot_escape_output_or_collide(self):
        good = {'id': 'settings', 'source_path': 'Apps/settings.c',
                'manifest_path': 'Apps/settings.json', 'file_name': 'settings.elf'}
        self.assertEqual(validate_inventory([good]), [good])
        for field, value in [('id', '../settings'), ('source_path', '../settings.c'),
                             ('manifest_path', '/tmp/settings.json'),
                             ('file_name', '../../settings.elf')]:
            with self.assertRaises(ValueError):
                validate_inventory([{**good, field: value}])
        with self.assertRaises(ValueError):
            validate_inventory([good, good])
        with self.assertRaises(ValueError):
            validate_inventory([{**good, 'additional_sources': [{'path': '../../unsafe.h'}]}])

    def test_release_parity_uses_actual_bytes_and_version(self):
        data = b'artifact'
        expected = {'version': '1.0.0', 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        self.assertTrue(compare(expected, {'version': '1.0.0'}, data))
        self.assertFalse(compare(expected, {'version': '1.0.1'}, data))
        self.assertFalse(compare(expected, {'version': '1.0.0'}, b'changed!'))

    def test_release_report_cannot_hide_nonrequired_apps(self):
        rows = [{'id': 'one', 'file_name': 'one.elf'}, {'id': 'two', 'file_name': 'two.elf'}]
        baseline = {'apps': rows, 'required_byte_parity': ['one']}
        validate_cohort(baseline, {'apps': rows}, {'apps': rows})
        for invalid in [rows[:1], rows + [rows[0]]]:
            with self.assertRaises(ValueError):
                validate_cohort(baseline, {'apps': invalid}, {'apps': rows})
        with self.assertRaises(ValueError):
            validate_cohort(baseline, {'apps': [{**rows[0], 'file_name': '../one.elf'}, rows[1]]}, {'apps': rows})

    def test_bad_manifest_name_rejected(self):
        with self.assertRaises(ValueError):
            validate_manifest(ROOT / 'Apps/gps.c', pathlib.Path('wrong.elf'))


if __name__ == '__main__':
    unittest.main()
