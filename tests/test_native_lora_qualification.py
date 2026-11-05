"""Fail-closed controls for the standalone qualification boundary."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('lora_qualification', ROOT / 'scripts/build_native_lora_qualification.py')
recipe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe)


class SourceBoundaryTests(unittest.TestCase):
    def test_wrong_head_rejected_before_status(self):
        with patch.object(recipe, 'git', return_value=b'b' * 40) as git:
            with self.assertRaisesRegex(ValueError, 'HEAD must equal exact pinned'):
                recipe.exact_clean(Path('.'), 'a' * 40, 'System')
            self.assertEqual(git.call_count, 1)

    def test_dirty_tracked_staged_and_untracked_rejected(self):
        for status in (b' M a.c\n', b'M  a.c\n', b'?? surprise.h\n'):
            with self.subTest(status=status), patch.object(recipe, 'git', side_effect=[b'a' * 40, status]):
                with self.assertRaisesRegex(ValueError, 'source must be clean'):
                    recipe.exact_clean(Path('.'), 'a' * 40, 'System')

    def test_abbreviated_and_non_string_pins_rejected(self):
        for pin in ('main', '1234567', None, 'a' * 39):
            with self.subTest(pin=pin), patch.object(recipe, 'git') as git:
                with self.assertRaisesRegex(ValueError, 'complete immutable commit'):
                    recipe.exact_clean(Path('.'), pin, 'System')
                git.assert_not_called()

    def test_clean_exact_source_has_verified_tree(self):
        with patch.object(recipe, 'git', side_effect=[b'a' * 40, b'', b'b' * 40]):
            self.assertEqual(recipe.exact_clean(Path('.'), 'a' * 40, 'System'),
                             {'commit': 'a' * 40, 'tree': 'b' * 40, 'clean': True})

    def test_real_git_source_checks(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            def git(*args):
                return subprocess.check_output(['git', '-C', tmp, *args], stderr=subprocess.STDOUT)
            git('init', '-q')
            git('config', 'user.name', 'Qualification Test')
            git('config', 'user.email', 'qualification@example.invalid')
            (root / 'source.c').write_text('original\n')
            git('add', 'source.c')
            git('commit', '-qm', 'test source')
            pin = git('rev-parse', 'HEAD').decode().strip()
            self.assertTrue(recipe.exact_clean(root, pin, 'System')['clean'])
            (root / 'untracked.h').write_text('shadow\n')
            with self.assertRaisesRegex(ValueError, 'source must be clean'):
                recipe.exact_clean(root, pin, 'System')
            (root / 'untracked.h').unlink()
            (root / 'source.c').write_text('changed\n')
            with self.assertRaisesRegex(ValueError, 'source must be clean'):
                recipe.exact_clean(root, pin, 'System')
            git('add', 'source.c')
            with self.assertRaisesRegex(ValueError, 'source must be clean'):
                recipe.exact_clean(root, pin, 'System')

    def test_exact_parity_fails_closed(self):
        records = [{'mode': m, 'sha256': 'a'} for m in
                   ('baseline-minimal', 'legacy-minimal', 'baseline-shared', 'legacy-shared')]
        recipe.check_parity(records)
        for index in (1, 3):
            altered = [dict(r) for r in records]
            altered[index]['sha256'] = 'changed'
            with self.assertRaisesRegex(ValueError, 'Selector-off target bytes changed'):
                recipe.check_parity(altered)


class ProfileBoundaryTests(unittest.TestCase):
    def test_reviewed_profile_accepted_in_builder(self):
        profile = json.loads(recipe.PROFILE.read_text())
        self.assertEqual(recipe.validate_profile(profile), profile)

    def test_profile_pin_selector_capability_and_scope_tampering_rejected(self):
        profile = json.loads(recipe.PROFILE.read_text())
        changes = [
            lambda p: p['pins'].update(system='a' * 40),
            lambda p: p['pins'].update(runtime='b' * 40),
            lambda p: p.update(utilities_base='c' * 40),
            lambda p: p.update(system_base='d' * 40),
            lambda p: p['native_selectors'].append('PORTABLE_QUICK_NATIVE'),
            lambda p: p['shared_input_defines'].remove('PORTABLE_NATIVE_CUSTODY_FENCE'),
            lambda p: p['common_defines'].append('PORTABLE_INPUT_NAVIGATION_LOCAL'),
            lambda p: p['translation_units'].append('host:fake.c'),
            lambda p: p['capability_contract'].update(installation_manifest_emitted=True),
            lambda p: p['display_contract'].update(native='MONO1 native supported'),
            lambda p: p.update(scope='Full product qualification'),
            lambda p: p.update(default='native'),
            lambda p: p.update(allow_dirty=True),
        ]
        for index, change in enumerate(changes):
            altered = copy.deepcopy(profile)
            change(altered)
            with self.subTest(index=index), self.assertRaisesRegex(ValueError, 'exact pinned'):
                recipe.validate_profile(altered)

    def test_invalid_profile_type_rejected(self):
        with self.assertRaisesRegex(ValueError, 'must be an object'):
            recipe.validate_profile([])

    def test_utilities_are_immutable_and_clean(self):
        source = (ROOT / 'scripts/build_native_lora_qualification.py').read_text()
        self.assertNotIn('def snapshot(', source)
        self.assertIn("exact_clean(ROOT, args.utilities_commit, 'Utilities')", source)
        self.assertIn("stage(ROOT, head, roots['utilities']", source)


class TargetBoundaryTests(unittest.TestCase):
    @staticmethod
    def listing(extra=''):
        return '\n'.join(' 1: 00000100 4 FUNC GLOBAL DEFAULT 6 ' + s for s in sorted(recipe.EXPORTS)) + extra

    def test_expected_export_contract(self):
        imports, exports = recipe.symbols(self.listing('\n 2: 0 0 NOTYPE GLOBAL DEFAULT UND memcpy'))
        self.assertEqual(imports, {'memcpy'})
        self.assertEqual(exports, recipe.EXPORTS)

    def test_unexpected_import_export_weak_and_data_rejected(self):
        for extra in ('\n 2: 0 0 NOTYPE GLOBAL DEFAULT UND firmware_private_api',
                      '\n 2: 0 0 NOTYPE WEAK DEFAULT UND memcpy',
                      '\n 2: 4 4 FUNC GLOBAL DEFAULT 6 surprise',
                      '\n 2: 4 4 OBJECT GLOBAL DEFAULT 6 secret'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                recipe.symbols(self.listing(extra))

    def test_missing_required_export_rejected(self):
        with self.assertRaises(ValueError):
            recipe.symbols(self.listing().replace('app_main', 'other_main'))

    def test_non_elf_short_wrong_arch_and_bad_table_rejected(self):
        good = bytearray(52)
        struct.pack_into('<16sHHIIIIIHHHHHH', good, 0, b'\x7fELF\x01\x01\x01' + b'\0'*9,
                         3, 94, 1, 0, 0, 52, 0, 52, 0, 0, 40, 1, 0)
        for data in (b'', b'not elf', bytes(good[:51]), bytes(good)):
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                recipe.elf_layout(data)

    def test_profile_is_minimal_explicit_and_non_deployable(self):
        profile = json.loads(recipe.PROFILE.read_text())
        self.assertEqual(profile['native_selectors'], ['PORTABLE_LORA_NATIVE_UI', 'PORTABLE_NATIVE_CANVAS'])
        self.assertEqual(profile['common_defines'], ['PORTABLE_FORCE_FULL_FRAMES', 'PORTABLE_ALARM_CLIENT',
                         'PORTABLE_RADIO_SESSION', 'PORTABLE_NOVA_UI', 'PORTABLE_APP_OWNS_TOUCH_CHROME'])
        self.assertEqual(set(profile['shared_input_defines']), {'PORTABLE_LORA_SHARED_TEXT_INPUT',
                         'PORTABLE_TEXT_INPUT_CLIENT', 'PORTABLE_APP_LAUNCH_GUARD', 'PORTABLE_CHECKED_STORAGE_RUNTIME',
                         'PORTABLE_NATIVE_CUSTODY_FENCE', 'PORTABLE_ALARM_TERMINAL_RETENTION'})
        self.assertEqual(profile['default'], 'legacy')
        self.assertFalse(profile['capability_contract']['installation_manifest_emitted'])
        self.assertIn({'capability': 'ui.text-input', 'api': 1}, profile['capability_contract']['required_for_selected_features'])
        for pin in [profile['utilities_base'], profile['system_base'], *profile['pins'].values()]:
            self.assertRegex(pin, '^[0-9a-f]{40}$')

    def test_no_relaxed_or_release_option(self):
        proc = subprocess.run(['python', str(ROOT / 'scripts/build_native_lora_qualification.py'), '--help'],
                              text=True, capture_output=True, check=True)
        self.assertIn('--native-ui', proc.stdout)
        self.assertIn('--utilities-commit', proc.stdout)
        for option in ('--allow-dirty', '--candidate', '--system-pin', '--publish', '--release'):
            self.assertNotIn(option, proc.stdout)

    def test_workflow_is_qualification_only(self):
        workflow = (ROOT / '.github/workflows/native-lora-qualification.yml').read_text()
        for forbidden in ('contents: write', 'publish-development:', '--publish', 'gh release', 'GITHUB_TOKEN:'):
            self.assertNotIn(forbidden, workflow)
        self.assertIn('--native-ui', workflow)
        self.assertIn('contents: read', workflow)
        profile = json.loads(recipe.PROFILE.read_text())
        for pin in profile['pins'].values():
            self.assertIn('ref: ' + pin, workflow)


if __name__ == '__main__':
    unittest.main()
