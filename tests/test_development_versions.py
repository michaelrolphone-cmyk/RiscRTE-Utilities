import contextlib
import hashlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import check_release_parity
from check_release_parity import newer_version


class Versions(unittest.TestCase):
    def test_numeric_version_order(self):
        for actual, published, expected in [
                ('1.0.4', '1.0.2', True),
                ('1.0.5', '1.0.2', True),
                ('1.10.0', '1.9.9', True),
                ('2.0.0', '1.99.99', True),
                ('1.0.2', '1.0.2', False),
                ('1.0.1', '1.0.2', False)]:
            with self.subTest(actual=actual, published=published):
                self.assertEqual(newer_version(actual, published), expected)

    def test_invalid_version_rejected(self):
        for invalid in ('1.3.beta', '1.0', 'v1.0.4', '1.0.4-dev', '1.0.4\n'):
            for actual, published in ((invalid, '1.0.2'), ('1.0.4', invalid)):
                with self.subTest(actual=actual, published=published):
                    with self.assertRaises(ValueError):
                        newer_version(actual, published)

    def run_parity(self, version, data, should_fail=False):
        published = b'published battery payload'
        expected = {
            'id': 'battery', 'file_name': 'battery.elf', 'version': '1.0.2',
            'size': len(published), 'sha256': hashlib.sha256(published).hexdigest(),
        }
        actual = {'id': 'battery', 'file_name': 'battery.elf', 'version': version}
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'sdk').mkdir()
            (root / 'dist/apps').mkdir(parents=True)
            (root / 'sdk/release-baseline.json').write_text(json.dumps({
                'reader_source_commit': 'immutable-reader-snapshot',
                'required_byte_parity': ['battery'], 'apps': [expected],
            }))
            (root / 'utilities-manifest.json').write_text(json.dumps({'apps': [actual]}))
            (root / 'dist/apps/build-evidence.json').write_text(json.dumps({'apps': [actual]}))
            (root / 'dist/apps/battery.elf').write_bytes(data)
            with patch.object(check_release_parity, 'ROOT', root), contextlib.redirect_stdout(io.StringIO()):
                if should_fail:
                    with self.assertRaisesRegex(ValueError, 'Required published-byte parity failed: battery'):
                        check_release_parity.main()
                else:
                    check_release_parity.main()
            report = json.loads((root / 'dist/apps/release-parity.json').read_text())
        self.assertEqual(report['reader_source_commit'], 'immutable-reader-snapshot')
        self.assertEqual(len(report['apps']), 1)
        row = report['apps'][0]
        self.assertTrue(row['required'])
        self.assertEqual(row['version'], version)
        self.assertEqual(row['published_version'], '1.0.2')
        self.assertEqual(row['built_sha256'], hashlib.sha256(data).hexdigest())
        return row

    def test_unchanged_release_requires_byte_parity(self):
        row = self.run_parity('1.0.2', b'published battery payload')
        self.assertTrue(row['byte_parity'])
        self.assertFalse(row['new_development_version'])

    def test_same_version_changed_payload_is_rejected(self):
        row = self.run_parity('1.0.2', b'changed payload', should_fail=True)
        self.assertFalse(row['byte_parity'])
        self.assertFalse(row['new_development_version'])

    def test_older_version_is_rejected_even_with_identical_bytes(self):
        row = self.run_parity('1.0.1', b'published battery payload', should_fail=True)
        self.assertFalse(row['byte_parity'])
        self.assertFalse(row['new_development_version'])

    def test_newer_versions_are_separate_development_builds(self):
        for version in ('1.0.4', '1.0.5'):
            with self.subTest(version=version):
                row = self.run_parity(version, b'changed development payload')
                self.assertFalse(row['byte_parity'])
                self.assertTrue(row['new_development_version'])

    def test_newer_version_with_identical_bytes_is_not_release_parity(self):
        row = self.run_parity('1.0.4', b'published battery payload')
        self.assertFalse(row['byte_parity'])
        self.assertTrue(row['new_development_version'])


if __name__ == '__main__':
    unittest.main()
