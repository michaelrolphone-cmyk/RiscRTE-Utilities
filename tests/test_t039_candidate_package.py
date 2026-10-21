import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import package_t039_candidate as candidate


class CandidatePackageContracts(unittest.TestCase):
    def test_numeric_version_preserves_every_other_manifest_field(self):
        manifest = candidate.read(ROOT/candidate.MANIFEST)
        old = candidate.read(ROOT/'Apps/native/ble_buttons.json')
        self.assertEqual(manifest, {**old, 'version': '0.1.31'})
        self.assertEqual(manifest, candidate.expected_manifest())
        self.assertEqual(len(manifest['requires']), 16)
        self.assertNotIn('sha256', manifest)
        self.assertNotIn('size_bytes', manifest)

    def test_frozen_source_and_gates_unchanged(self):
        for name in candidate.evidence.FROZEN_FILES:
            self.assertEqual((ROOT/name).read_bytes(), candidate.git(ROOT, 'show', candidate.QUALIFIED_SOURCE+':'+name))
        self.assertEqual(candidate.digest(ROOT/'Apps/ble_hid_app.inc'), candidate.evidence.CONTROLLER)

    def test_seven_additions_only(self):
        self.assertEqual(len(candidate.ADDITIONS), 7)
        self.assertTrue(all((ROOT/name).is_file() for name in candidate.ADDITIONS))
        for path in candidate.ADDITIONS:
            self.assertNotIn(path, candidate.evidence.FROZEN_FILES)

    def test_read_only_workflow_and_exact_head(self):
        workflow = (ROOT/'.github/workflows/t039-ble-package.yml').read_text()
        for forbidden in ('contents: write', 'continue-on-error:', 'workflow_dispatch:', ' --publish', 'release create', 'push --force'):
            self.assertNotIn(forbidden, workflow)
        self.assertIn('contents: read', workflow)
        self.assertIn('ref: ${{ github.event.pull_request.head.sha || github.sha }}', workflow)
        self.assertIn('--source-sha "${{ github.event.pull_request.head.sha || github.sha }}"', workflow)
        self.assertEqual(workflow.count('persist-credentials: false'), 8)

    def test_workflow_keeps_pr94_qualifier_in_exact_checkout(self):
        workflow = (ROOT/'.github/workflows/t039-ble-package.yml').read_text()
        for pin in [candidate.QUALIFIED_SOURCE, candidate.evidence.PARENT, candidate.evidence.WATCH,
                    candidate.evidence.LEGACY_SYSTEM, *candidate.evidence.frozen.PINS.values()]:
            self.assertIn('ref: '+pin, workflow)
        self.assertIn('python .dependencies/pr94/scripts/qualify_t039_ble_candidate.py qualify', workflow)
        self.assertIn('espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5', workflow)
        self.assertIn('--evidence-sha256 "${{ steps.evidence.outputs.receipt_sha256 }}"', workflow)
        self.assertIn('--receipt-sha256 "${{ steps.package.outputs.receipt_sha256 }}"', workflow)

    def guard(self, changes=None, status=b'', expected='source', head=b'source\n'):
        rows = ''.join('A\t'+p+'\n' for p in sorted(candidate.ADDITIONS)) if changes is None else changes
        def git(root, *args):
            if args == ('rev-parse', 'HEAD'): return head
            if args == ('status', '--porcelain'): return status
            if args[0] == 'diff': return rows.encode()
            raise AssertionError(args)
        with patch.object(candidate, 'git', side_effect=git):
            return candidate.source_guard(expected)

    def test_source_guard_accepts_complete_additive_change(self):
        self.assertEqual(self.guard(), 'source')

    def test_source_guard_rejects_wrong_head(self):
        with self.assertRaisesRegex(ValueError, 'source commit mismatch'):
            self.guard(expected='other')

    def test_source_guard_rejects_dirty_tree(self):
        with self.assertRaisesRegex(ValueError, 'must be clean'):
            self.guard(status=b' M Apps/ble_hid_app.inc\n')

    def test_source_guard_rejects_modified_frozen_gate(self):
        with self.assertRaisesRegex(ValueError, 'Only the seven'):
            self.guard(changes='M\tscripts/build_native_ble_buttons_qualification.py\n')

    def test_source_guard_rejects_unlisted_addition(self):
        with self.assertRaisesRegex(ValueError, 'Only the seven'):
            self.guard(changes='A\tApps/another_app.json\n')

    def test_source_guard_rejects_missing_candidate_file(self):
        with self.assertRaisesRegex(ValueError, 'Incomplete candidate'):
            self.guard(changes='A\t'+candidate.MANIFEST+'\n')

    def test_source_guard_rejects_manifest_contract_drift(self):
        with patch.object(candidate, 'read', return_value={}):
            with self.assertRaisesRegex(ValueError, 'identity or sixteen'):
                self.guard()

    def test_source_guard_rejects_controller_drift(self):
        with patch.object(candidate, 'digest', return_value='wrong'):
            with self.assertRaisesRegex(ValueError, 'controller changed'):
                self.guard()

    def fixture(self, root):
        (root/'app').mkdir()
        (root/'app/ble_buttons.elf').write_bytes(b'ELF candidate test fixture')
        (root/'app/ble_buttons.json').write_text('{}')
        (root/'CANDIDATE_ONLY.txt').write_text(candidate.NOTICE)
        (root/'licenses').mkdir()
        (root/'licenses/LICENSE').write_text('fixture notice')

    def test_archive_is_deterministic_and_payload_bound(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); self.fixture(root)
            candidate.create_archive(root); before = (root/candidate.ARCHIVE).read_bytes()
            candidate.check_archive(root); candidate.create_archive(root)
            self.assertEqual(before, (root/candidate.ARCHIVE).read_bytes())

    def test_archive_rejects_payload_mutation(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); self.fixture(root); candidate.create_archive(root)
            (root/'app/ble_buttons.elf').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'payload differs'): candidate.check_archive(root)

    def test_archive_rejects_extra_or_unsafe_entry(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); self.fixture(root); candidate.create_archive(root)
            with zipfile.ZipFile(root/candidate.ARCHIVE, 'a') as z: z.writestr('../escape', 'no')
            with self.assertRaisesRegex(ValueError, 'entries changed'): candidate.check_archive(root)

    def test_inventory_binds_hidden_files_and_excludes_only_root_record(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); (root/candidate.RECORD).write_text('{}'); (root/'.hidden').write_text('x')
            (root/'nested').mkdir(); (root/'nested'/candidate.RECORD).write_text('nested')
            self.assertEqual(set(candidate.inventory(root)), {'.hidden', 'nested/'+candidate.RECORD})

    def test_inventory_rejects_symlink(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); (root/'file').write_text('x'); (root/'link').symlink_to('file')
            with self.assertRaisesRegex(ValueError, 'symlinks'): candidate.inventory(root)

    def test_transport_requires_independent_receipt_digest_before_trusting_record(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); (root/candidate.RECORD).write_text('{"fake":true}')
            with patch.object(candidate, 'source_guard', return_value='source'):
                with self.assertRaisesRegex(ValueError, 'receipt changed'):
                    candidate.validate(root, 'source', 'unrelated')

    def test_target_source_correspondence_checks_committed_bytes(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            payload = b'target source'
            target = {'modules': [{'mode': m, 'source_sha256': {'utilities:Apps/ble_buttons.c': candidate.evidence.sha(payload)}}
                                  for m in ('baseline', 'legacy', 'native')]}
            candidate.write(root/'evidence/target/qualification.json', target)
            with patch.object(candidate, 'git', return_value=payload): candidate.verify_correspondence(root)
            with patch.object(candidate, 'git', return_value=b'changed'):
                with self.assertRaisesRegex(ValueError, 'compiled source differs'): candidate.verify_correspondence(root)

    def test_scope_does_not_claim_installation_or_hardware_acceptance(self):
        self.assertIn('NOT APPROVED FOR INSTALLATION', candidate.NOTICE)
        self.assertIn('frozen gate remains failed', candidate.NOTICE)
        self.assertIn('No tag, release, product pin, firmware', candidate.NOTICE)


if __name__ == '__main__': unittest.main()
