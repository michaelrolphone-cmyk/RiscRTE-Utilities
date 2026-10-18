"""Countdown identity, inherited qualification and immutable-publication controls."""
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import build_native_alarms_qualification as recipe
import prepare_native_alarms_release as shared_prepare
import publish_native_alarms as shared_publish
import prepare_native_countdown_release as prepare
import publish_native_countdown as publish
from test_native_alarms_release import fixture, write


class CountdownReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        path = str(self.root/'control-fixture.json')
        self.patches = [mock.patch.object(shared_prepare, 'CONTROL_PROFILE', path),
                        mock.patch.object(shared_publish, 'CONTROL_PROFILE', path)]
        for patch in self.patches: patch.start()
        self.source = fixture(self.root)
        self.original_alarms = (self.root/'app/alarms-build-record.json').read_bytes()
        with mock.patch.object(recipe, 'git', side_effect=self.git):
            prepare.qualify(self.root)

    def git(self, root, *args):
        if args == ('rev-parse', 'HEAD'): return (self.source+'\n').encode()
        if args == ('status', '--porcelain'): return b''
        if args[0] == 'show': return (ROOT/args[1].split(':', 1)[1]).read_bytes()
        raise AssertionError(args)

    def tearDown(self):
        for patch in self.patches: patch.stop()
        self.temp.cleanup()

    def change(self, path, fn):
        path = self.root/path
        value = json.loads(path.read_text())
        fn(value)
        write(path, value)

    def test_complete_synthetic_receipt_passes(self):
        record = publish.validate(self.root, self.source)
        self.assertEqual(record['tag'], 'native-countdown-0.2.0-dev1')
        self.assertEqual(record['version'], '0.2.0')
        self.assertEqual(len(record['required_capabilities']), 15)

    def test_source_manifest_is_distinct(self):
        self.assertEqual(json.loads((ROOT/prepare.MANIFEST).read_text()), prepare.expected_manifest())
        self.assertEqual(prepare.expected_manifest()['id'], 'countdown')
        self.assertEqual(prepare.expected_manifest()['file_name'], 'countdown.elf')
        self.assertNotEqual(json.loads((ROOT/'Apps/countdown.json').read_text())['version'], prepare.VERSION)

    def test_preserves_alarms_package_and_admission(self):
        self.assertEqual((self.root/'app/alarms-build-record.json').read_bytes(), self.original_alarms)
        shared_publish.validate(self.root, self.source)
        self.assertNotIn('manifest_sha256', json.loads((self.root/'admission/countdown-elf-memory-admission.json').read_text()))

    def test_wrong_source_rejected(self):
        with self.assertRaisesRegex(ValueError, 'source identity'):
            publish.validate(self.root, '0'*40)

    def test_failed_countdown_temporal_case_rejected(self):
        def fail(value):
            next(x for x in value['results'] if x['app'] == 'countdown')['exit_code'] = 1
        self.change('temporal-sanitized/receipt.json', fail)
        with self.assertRaisesRegex(ValueError, 'Failed/incomplete'):
            publish.validate(self.root, self.source)

    def test_missing_shared_receipt_rejected(self):
        (self.root/'app/alarms-build-record.json').unlink()
        with self.assertRaises(FileNotFoundError): publish.validate(self.root, self.source)

    def test_each_direct_artifact_tamper_rejected(self):
        for name in publish.DIRECT:
            path = self.root/('target' if name == 'qualification.json' else prepare.PACKAGE)/name
            original = path.read_bytes()
            path.write_bytes(original+b' ')
            with self.assertRaises(ValueError, msg=name): publish.validate(self.root, self.source)
            path.write_bytes(original)

    def test_missing_source_rejected(self):
        (self.root/'source/utilities'/prepare.MANIFEST).unlink()
        with self.assertRaises(FileNotFoundError): publish.validate(self.root, self.source)

    def test_changed_source_rejected(self):
        (self.root/'source/utilities/scripts/publish_native_countdown.py').write_text('changed')
        with self.assertRaisesRegex(ValueError, 'release record differs'):
            publish.validate(self.root, self.source)

    def test_wrong_application_rejected(self):
        self.change(prepare.PACKAGE+'/countdown.json', lambda x: x.update(id='alarms'))
        with self.assertRaises(ValueError): publish.validate(self.root, self.source)

    def test_admission_rebinding_rejected(self):
        self.change(prepare.PACKAGE+'/elf-memory-admission.json', lambda x: x.update(manifest_sha256='0'*64))
        with self.assertRaises(ValueError): publish.validate(self.root, self.source)

    def test_hardware_claim_rejected(self):
        self.change(prepare.PACKAGE+'/countdown-build-record.json', lambda x: x.update(hardware_qualified=True))
        with self.assertRaises(ValueError): publish.validate(self.root, self.source)

    def test_dirty_source_rejected(self):
        with mock.patch.object(recipe, 'git', side_effect=lambda root, *args: b' M file\n' if args[0] == 'status' else self.git(root, *args)):
            with self.assertRaisesRegex(ValueError, 'clean exact source'): prepare.qualify(self.root)

    def test_immutable_tag_source_and_hashes(self):
        record = publish.validate(self.root, self.source)
        published = {'tag': prepare.TAG, 'source_commit': self.source,
                     'elf_sha256': record['elf_sha256'], 'manifest_sha256': record['manifest_sha256'],
                     'build_record_sha256': recipe.digest((self.root/prepare.PACKAGE/'countdown-build-record.json').read_bytes())}
        publish.verify_existing(published, self.source, record)
        with self.assertRaisesRegex(ValueError, 'tag/source'): publish.verify_existing(published, 'b'*40, record)
        published['elf_sha256'] = '0'*64
        with self.assertRaisesRegex(ValueError, 'Existing release differs'): publish.verify_existing(published, self.source, record)

    def test_release_asset_round_trip_and_tamper(self):
        record = publish.validate(self.root, self.source)
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            for name in publish.DIRECT:
                shutil.copyfile(self.root/('target' if name == 'qualification.json' else prepare.PACKAGE)/name, out/name)
            write(out/'publication.json', publish.publication_record(record))
            with zipfile.ZipFile(out/(prepare.TAG+'.zip'), 'w') as archive:
                for path in self.root.rglob('*'):
                    if path.is_file(): archive.write(path, path.relative_to(self.root))
            (out/'SHA256SUMS').write_text(''.join(recipe.digest(path.read_bytes())+'  '+path.name+'\n'
                for path in sorted(out.iterdir()) if path.is_file()))
            publish.verify_release_assets(out, record)
            (out/'countdown.elf').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'Published asset changed'):
                publish.verify_release_assets(out, record)

    def test_workflow_qualifies_exact_source_then_publishes_only_countdown(self):
        workflow = (ROOT/'.github/workflows/native-countdown.yml').read_text()
        qualification, publisher = workflow.split('  publish-development:', 1)
        self.assertNotIn('contents: write', qualification)
        self.assertIn("github.event_name == 'push' && github.ref == 'refs/heads/main'", publisher)
        self.assertIn('needs: qualification', publisher)
        self.assertIn('GH_TOKEN: ${{ github.token }}', publisher)
        self.assertIn('persist-credentials: false', publisher)
        self.assertIn('scripts/publish_native_countdown.py', publisher)
        self.assertNotIn('scripts/publish_native_alarms.py', workflow)
        self.assertIn('scripts/prepare_native_alarms_release.py', qualification)
        self.assertIn('scripts/prepare_native_countdown_release.py', qualification)
        self.assertIn('--validate-only', qualification)
        self.assertIn('include-hidden-files: true', qualification)
        self.assertIn('Validate transported Countdown artifact and workflow provenance', qualification)
        self.assertIn('--artifact "$RUNNER_TEMP/native-countdown-downloaded"', qualification)
        self.assertIn('--publish', publisher)
        for pin in recipe.PINS.values(): self.assertIn(pin, workflow)


if __name__ == '__main__': unittest.main()
