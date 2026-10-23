import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import qualify_t039_ble_candidate as candidate


class CandidateContracts(unittest.TestCase):
    def test_frozen_files_match_captured_objects(self):
        for name in candidate.FROZEN_FILES:
            with self.subTest(path=name):
                self.assertEqual(candidate.digest(ROOT/name), candidate.FROZEN_SOURCE_SHA256[name])

    def test_historical_and_current_qualification_are_separate(self):
        historical = (ROOT/'.github/workflows/native-ble-buttons.yml').read_text()
        current = (ROOT/'.github/workflows/t039-ble-candidate.yml').read_text()
        self.assertIn('HISTORICAL_SOURCE: '+candidate.PARENT, historical)
        self.assertIn('ref: ${{ env.HISTORICAL_SOURCE }}', historical)
        self.assertIn('"${{ env.HISTORICAL_SOURCE }}"', historical)
        self.assertIn('github.sha == '+repr(candidate.PARENT), historical)
        self.assertIn('ref: ${{ github.event.pull_request.head.sha || github.sha }}', current)
        self.assertNotIn('continue-on-error:', historical+current)
        self.assertIn('candidate-frozen-target', (ROOT/'scripts/qualify_t039_ble_candidate.py').read_text())

    def test_shared_repair_remains_exact_captured_source(self):
        self.assertEqual(candidate.digest(ROOT/'Apps/ble_hid_app.inc'), candidate.CONTROLLER)

    def test_workflow_has_no_publication_route(self):
        workflow = (ROOT/'.github/workflows/t039-ble-candidate.yml').read_text()
        for forbidden in ('contents: write', 'continue-on-error:', 'workflow_dispatch:', ' --publish', 'publish-development:', 'release create'):
            self.assertNotIn(forbidden, workflow)
        self.assertIn('contents: read', workflow)
        self.assertIn('NOT FOR INSTALLATION', candidate.NOTICE)
        self.assertIn('--receipt-sha256 "${{ steps.evidence.outputs.receipt_sha256 }}"', workflow)
        self.assertIn('ref: ${{ github.event.pull_request.head.sha || github.sha }}', workflow)
        self.assertEqual(workflow.count('persist-credentials: false'), 7)

    def test_workflow_pins_all_dependencies(self):
        workflow = (ROOT/'.github/workflows/t039-ble-candidate.yml').read_text()
        for pin in [candidate.PARENT, candidate.WATCH, candidate.LEGACY_SYSTEM, *candidate.frozen.PINS.values()]:
            self.assertIn('ref: '+pin, workflow)
        self.assertIn('repository: michaelrolphone-cmyk/RiscRTE-T-Watch-S3', workflow)
        self.assertIn('espressif/toolchain-xtensa-esp32s3@8.4.0+2021r2-patch5', workflow)

    def test_complete_native_control_matrix(self):
        rows = candidate.control_matrix()
        self.assertEqual(len(rows), 8)
        self.assertEqual(len(candidate.host.SIZES), 7)
        for label, san, asynchronous, temporal, scenes, flags in rows:
            self.assertEqual(san, 'sanitized' in label)
            if label.startswith('temporal/'):
                self.assertEqual(scenes, {f'temporal-{i}' for i in range(16)})
                self.assertTrue(asynchronous and temporal)
            elif label.startswith('mixed/'):
                self.assertEqual(scenes, {'temporal-16'})
                self.assertIn('--mixed-contacts', flags)
                self.assertTrue(asynchronous and temporal)
            else:
                self.assertGreater(len(scenes), 40)
                self.assertFalse(temporal)

    def test_mandatory_runs_include_all_control_families(self):
        runs = candidate.required_runs()
        self.assertEqual(len(runs), 31)
        self.assertTrue({'runs/parent-frozen-target.json', 'runs/candidate-frozen-target.json',
                         'runs/geometry-normal.json', 'runs/geometry-sanitized.json',
                         'runs/hid-apps.json', 'runs/renderer-color.json', 'runs/renderer-paper.json',
                         'runs/queue-normal.json', 'runs/queue-sanitized.json'} <= runs)
        self.assertTrue({'runs/'+r[0].replace('/', '-')+'.json' for r in candidate.control_matrix()} <= runs)

    def test_queue_controls_use_exact_captured_case_names(self):
        cases = candidate.queue_cases()
        self.assertEqual(len(cases), 50)
        self.assertEqual(len([x for x in cases if x.startswith('mixed-')]), 18)
        self.assertTrue({'mixed-long-hold', 'mixed-release-fail', 'mixed-press-fail', 'queue-overflow'} <= cases)

    def test_host_rejects_invented_full_profile_claim(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            candidate.write(root/'target/qualification.json', {})
            candidate.write(root/'target/native/command.json', ['cc', '-DPORTABLE_BLE_BUTTONS_NATIVE_UI'])
            r = {'sanitized': False, 'async_rgb565': False, 'temporal': False, 'full_feature_profile': True,
                 'full_profile': True, 'source_unchanged_during_run': True, 'counterexamples': [],
                 'hardware_tested': False, 'target_instructions_executed': False,
                 'target_qualification_sha256': candidate.digest(root/'target/qualification.json'),
                 'build_defines': [], 'command': ['cc']}
            candidate.write(root/'host/receipt.json', r)
            with self.assertRaisesRegex(ValueError, 'Host recipe differs'):
                candidate.check_host(root, 'host', False, False, False, {'idle'}, 'source')

    def test_target_rejects_frozen_release_identity(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            r = {'utilities_commit': 'source', 'utilities_status': [],
                 'script_sha256': candidate.digest(ROOT/'scripts/build_native_ble_buttons_qualification.py'),
                 'utilities_base': candidate.frozen.BASE, 'baseline_system_pin': candidate.frozen.BASE_SYSTEM,
                 'exact_external_git_pins': candidate.frozen.PINS, 'selector_off_exact_baseline_bytes': False,
                 'legacy_manifest_exact_base': True, 'native_opt_in': True, 'native_selectors': candidate.frozen.NATIVE,
                 'native_manifest': {'version': '0.1.30-dev1'}, 'native_manifest_sha256': 'not-allowed'}
            candidate.write(root/'qualification.json', r)
            with self.assertRaisesRegex(ValueError, 'must not acquire a release identity'):
                candidate.validate_target(root, source='source', parity=False)

    def test_target_allows_frozen_sdk_alias_but_not_external_escape(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)/'target'; header = root/'inputs/system/header.h'
            header.parent.mkdir(parents=True); (root/'sdk').mkdir()
            (root/'sdk/header.h').write_text('shared')
            header.symlink_to(root/'sdk/header.h')
            self.assertEqual(candidate.target_input(root, 'system', 'header.h').read_text(), 'shared')
            header.unlink(); (Path(d)/'outside.h').write_text('outside')
            header.symlink_to(Path(d)/'outside.h')
            with self.assertRaisesRegex(ValueError, 'escapes root'):
                candidate.target_input(root, 'system', 'header.h')
            with self.assertRaisesRegex(ValueError, 'Unknown target source owner'):
                candidate.target_input(root, 'unknown', 'header.h')

    def test_safe_paths(self):
        with tempfile.TemporaryDirectory() as d:
            for relative in ('../outside', '/tmp/outside', 'a//b', 'a/./b', 'a/../b', 'a\\b', ''):
                with self.subTest(path=relative), self.assertRaises(ValueError):
                    candidate.safe(Path(d), relative)
            self.assertEqual(candidate.safe(Path(d), 'target/receipt.json'), Path(d)/'target/receipt.json')

    def fixtures(self, root):
        def module(mode, controller, digest):
            return {'mode': mode, 'sha256': digest, 'source_sha256': {'utilities:Apps/ble_hid_app.inc': controller,
                                                                  'system:adapter.c': 'same'}}
        parent = {'modules': [module('baseline', 'old', 'base'), module('legacy', 'old', 'base'), module('native', 'old', 'native-old')]}
        current = {'modules': [module('baseline', 'old', 'base'), module('legacy', candidate.CONTROLLER, 'legacy-new'),
                               module('native', candidate.CONTROLLER, 'native-new')]}
        for field in ('tools', 'toolchain_inputs', 'retained_recipe_sha256', 'native_selectors', 'script_sha256'):
            parent[field] = current[field] = field+'-same'
        candidate.write(root/'parent-target/qualification.json', parent)
        candidate.write(root/'target/qualification.json', current)
        candidate.write(root/'target/failure.json', {'type': 'ValueError', 'error': candidate.PARITY_ERROR, 'qualified': False})
        return parent, current

    def delta(self, root, parent, current):
        with patch.object(candidate, 'validate_target', side_effect=[parent, current]):
            return candidate.target_delta(root, 'source')

    def test_intentional_delta_retains_failed_frozen_gate(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); parent, current = self.fixtures(root)
            r = self.delta(root, parent, current)
            self.assertEqual(r['frozen_gate_status'], 'failed')
            self.assertTrue(r['parent_selector_off_exact'])
            self.assertEqual(r['intentional_changed_inputs']['native'], ['utilities:Apps/ble_hid_app.inc'])

    def test_arbitrary_failure_never_counts_as_parity_refusal(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); parent, current = self.fixtures(root)
            candidate.write(root/'target/failure.json', {'type': 'RuntimeError', 'error': 'compiler failed', 'qualified': False})
            with self.assertRaisesRegex(ValueError, 'expected exact parity'):
                self.delta(root, parent, current)

    def test_parent_target_failure_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); parent, current = self.fixtures(root)
            candidate.write(root/'parent-target/failure.json', {})
            with self.assertRaisesRegex(ValueError, 'Parent target unexpectedly failed'):
                self.delta(root, parent, current)

    def test_unrelated_compiled_change_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); parent, current = self.fixtures(root)
            current['modules'][1]['source_sha256']['system:adapter.c'] = 'changed'
            with self.assertRaisesRegex(ValueError, 'Unrelated compiled source'):
                self.delta(root, parent, current)

    def test_recipe_pin_and_toolchain_difference_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); parent, current = self.fixtures(root)
            current['toolchain_inputs'] = 'changed'
            with self.assertRaisesRegex(ValueError, 'recipe mismatch'):
                self.delta(root, parent, current)

    def test_changed_parent_baseline_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); parent, current = self.fixtures(root)
            parent['modules'][1]['sha256'] = 'different'
            with self.assertRaisesRegex(ValueError, 'Parent no longer reproduces'):
                self.delta(root, parent, current)

    def test_missing_intentional_change_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); parent, current = self.fixtures(root)
            current['modules'][1]['sha256'] = parent['modules'][1]['sha256']
            with self.assertRaisesRegex(ValueError, 'Expected Buttons change absent'):
                self.delta(root, parent, current)

    def test_inventory_detects_byte_changes_and_extra_files(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); (root/'one').write_bytes(b'original')
            before = candidate.inventory(root)
            (root/'candidate-evidence.json').write_bytes(b'excluded to avoid recursion')
            self.assertEqual(candidate.inventory(root), before)
            (root/'one').write_bytes(b'modified')
            self.assertNotEqual(candidate.inventory(root), before)
            (root/'two').write_bytes(b'extra')
            self.assertEqual(set(candidate.inventory(root)), {'one', 'two'})

    @patch('consolidated_source_guard.available', new=lambda root: False)
    def test_source_guard_rejects_existing_file_changes(self):
        with patch.object(candidate, 'git', side_effect=[b'', b'M\tApps/ble_hid_app.inc\n']):
            with self.assertRaisesRegex(ValueError, 'may add qualification files only'):
                candidate.source_guard()

    @patch('consolidated_source_guard.available', new=lambda root: False)
    def test_source_guard_rejects_unapproved_additions(self):
        with patch.object(candidate, 'git', side_effect=[b'', b'A\tApps/new_manifest.json\n']):
            with self.assertRaisesRegex(ValueError, 'may add qualification files only'):
                candidate.source_guard()


if __name__ == '__main__':
    unittest.main()
