import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('scanner_shared_test', ROOT / 'scripts/test_x4_ble_shared_text.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class ScannerSourceRoles(unittest.TestCase):
    def test_current_scenes_preserve_modal_custody_coverage(self):
        scenes = runner.scenarios()
        self.assertEqual(len(scenes), 24)
        self.assertTrue({'name-save', 'name-cancel', 'name-repeat', 'name-retained',
                         'name-unavailable', 'name-navigation-home', 'name-back-return'} <= scenes.keys())
        self.assertEqual(len([name for name in scenes if name.startswith('name-loss-')]), 10)

    def test_source_only_cannot_mix_receipt_mode(self):
        result = subprocess.run(['python3', str(ROOT / 'scripts/test_x4_ble_shared_text.py'),
                                 '--source-only', '--build', '.', '--drivers', '.', '--output', '.'],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('exactly one', result.stderr)

    def test_source_only_requires_all_source_roles(self):
        result = subprocess.run(['python3', str(ROOT / 'scripts/test_x4_ble_shared_text.py'),
                                 '--source-only', '--drivers', '.', '--output', '.'],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('--reader and --x4', result.stderr)

    def test_clean_exact_source_rejects_wrong_revision_and_dirty_files(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            subprocess.run(['git', 'init', '-q', folder], check=True)
            (path / 'input.h').write_text('exact SDK\n')
            subprocess.run(['git', '-C', folder, 'add', 'input.h'], check=True)
            subprocess.run(['git', '-C', folder, '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                            'commit', '-qm', 'fixture'], check=True)
            revision = runner.git(path, 'rev-parse', 'HEAD')
            self.assertEqual(runner.exact(path, revision)['revision'], revision)
            with self.assertRaises(ValueError):
                runner.exact(path, revision[:8])
            with self.assertRaises(ValueError):
                runner.exact(path, '0' * 40)
            (path / 'input.h').write_text('changed SDK\n')
            with self.assertRaises(ValueError):
                runner.exact(path, revision)

    def test_native_utc_uses_separate_pinned_system_roles(self):
        workflow = (ROOT / '.github/workflows/native-utc-alarm.yml').read_text()
        frozen = json.loads((ROOT / 'sdk/native-utc-alarm-sources.json').read_text())
        self.assertIn('ref: ' + frozen['system_sha'] + '\n          path: .dependencies/native-system', workflow)
        self.assertIn('ref: d52a74bfb4acc01cc3f0a9dda2c95ba2ba679ee9\n          path: .dependencies/regression-system', workflow)
        for test in ('points_apps', 'alarm_apps', 'alarm_volume', 'dnd_service'):
            self.assertIn('test_' + test + '.py --system-apps .dependencies/regression-system', workflow)
        self.assertIn('build_native_utc_alarm.py --system-apps .dependencies/native-system', workflow)
        self.assertNotIn('--skip-loader', workflow)

    def test_scanner_retains_legacy_controls_and_current_source_lane(self):
        workflow = (ROOT / '.github/workflows/ble-scanner.yml').read_text()
        self.assertIn('legacy-scanner-control:', workflow)
        self.assertIn('current-shared-host-scanner:', workflow)
        self.assertIn('include-hidden-files: true', workflow.split('current-shared-host-scanner:')[0])
        self.assertIn('ref: ce1c949d797a406d8e16646f5abe1aac0691ef52', workflow)
        for test in ('test_ble_scanner.py', 'test_ble_renderer.py', 'test_ble_paper.py'):
            self.assertIn(test, workflow)
        self.assertIn('test_x4_ble_shared_text.py --source-only', workflow)
        self.assertIn('check_scanner_ci_profile.py', workflow)
        self.assertIn('build_x4_resident_clients.py --resident-shell-client --app ble_scanner', workflow)
        self.assertNotIn('--development-system', workflow)
        self.assertIn('--runtime-revision b25b1d467a557e8d693211cff2eff959eb9c79de', workflow)
        self.assertNotIn('0f17a435f99d02d60ca50df1d1a51fcef123db89', workflow)
        for role in ('system', 'runtime', 'drivers', 'reader', 'x4'):
            self.assertRegex(workflow, '--' + role + '-revision [0-9a-f]{40}')


if __name__ == '__main__':
    unittest.main()
