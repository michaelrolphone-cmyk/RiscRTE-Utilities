"""The Battery target recipe must retain full features and immutable admission."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('battery_recipe', ROOT/'scripts/build_native_battery_qualification.py')
recipe = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(recipe)


class BatteryRecipeTests(unittest.TestCase):
    def setUp(self):
        self.profile = json.loads((ROOT/recipe.PROFILE_PATH).read_text())
        self.baseline = (ROOT/self.profile['baseline_manifest']['path']).read_bytes()

    def test_exact_manifest_has_fifteen_capability_pairs(self):
        recipe.validate_profile(self.profile, self.baseline)
        self.assertEqual(len(self.profile['requires']), 15)
        self.assertEqual(len(set(recipe.CAPABILITIES)), 15)
        self.assertIn(('storage.key-value.atomic', 1), recipe.CAPABILITIES)
        self.assertIn(('rtc.clock', 2), recipe.CAPABILITIES)
        self.assertIn(('alarm.service', 1), recipe.CAPABILITIES)

    def test_default_has_no_native_selector(self):
        args = recipe.argument_parser().parse_args([
            '--system', '.', '--bridges', '.', '--runtime', '.',
            '--profile-source', '.', '--output-dir', '/tmp/battery-test'])
        self.assertFalse(args.native_ui)
        self.assertFalse(set(recipe.define_arguments()) & {'-D'+x for x in recipe.NATIVE})
        self.assertEqual(recipe.define_arguments(True), recipe.define_arguments()+['-D'+x for x in recipe.NATIVE])

    def test_no_feature_can_be_silently_omitted(self):
        for selector in recipe.COMMON:
            with self.subTest(selector=selector):
                profile = copy.deepcopy(self.profile)
                profile['common_defines'].remove(selector)
                with self.assertRaisesRegex(ValueError, 'feature selectors'):
                    recipe.validate_profile(profile, self.baseline)

    def test_no_translation_unit_can_be_dropped(self):
        for source in recipe.SOURCES:
            with self.subTest(source=source):
                profile = copy.deepcopy(self.profile)
                profile['translation_units'].remove(source)
                with self.assertRaisesRegex(ValueError, 'feature source'):
                    recipe.validate_profile(profile, self.baseline)

    def test_capability_api_drift_rejected(self):
        for index in range(15):
            profile = copy.deepcopy(self.profile)
            profile['requires'][index]['api'] += 1
            with self.assertRaisesRegex(ValueError, 'fifteen capability'):
                recipe.validate_profile(profile, self.baseline)

    def test_manifest_byte_drift_rejected(self):
        with self.assertRaisesRegex(ValueError, 'manifest bytes'):
            recipe.validate_profile(self.profile, self.baseline+b'\n')

    def test_pin_drift_rejected(self):
        for owner in recipe.PINS:
            profile = copy.deepcopy(self.profile)
            profile['pins'][owner] = '0'*40
            with self.assertRaisesRegex(ValueError, 'pins'):
                recipe.validate_profile(profile, self.baseline)

    def test_no_idle_omission_or_hardware_claim(self):
        for key in ('automatic_idle', 'manual_sleep', 'manual_only_suppression', 'shared_idle_correction_included'):
            profile = copy.deepcopy(self.profile)
            profile['lifecycle'][key] = not profile['lifecycle'][key]
            with self.assertRaisesRegex(ValueError, 'idle behavior'):
                recipe.validate_profile(profile, self.baseline)
        for key in ('historical_reproduction', 'hardware_qualified', 'published'):
            profile = copy.deepcopy(self.profile)
            profile[key] = True
            with self.assertRaisesRegex(ValueError, 'release or hardware'):
                recipe.validate_profile(profile, self.baseline)

    def test_strong_and_weak_import_admission(self):
        symbols = '\n'.join('00000000 T '+x for x in sorted(recipe.EXPORTS))+'\n U memcpy\n'
        self.assertEqual(recipe.symbol_contract(symbols)[0], ['memcpy'])
        for extra in (' U unexpected\n', ' w unexpected\n', ' v memcpy\n', '00000000 W extra\n'):
            with self.assertRaises(ValueError):
                recipe.symbol_contract(symbols+extra)
        with self.assertRaises(ValueError):
            recipe.symbol_contract('00000000 T app_main\n')

    def test_target_elf_endian_machine_and_type(self):
        data = bytearray(52)
        data[:7] = b'\x7fELF\x01\x01\x01'
        data[16:20] = b'\x03\x00\x5e\x00'
        recipe.elf_contract(data)
        for offset in (4, 5, 16, 18):
            wrong = bytearray(data)
            wrong[offset] ^= 1
            with self.assertRaises(ValueError):
                recipe.elf_contract(wrong)

    def test_git_blob_staging_ignores_dirty_external_working_tree(self):
        with tempfile.TemporaryDirectory(prefix='battery-recipe-test-') as folder:
            root = Path(folder)
            repo, out = root/'repo', root/'out'
            repo.mkdir()
            subprocess.run(['git', 'init', '-q', str(repo)], check=True)
            (repo/'source.h').write_text('immutable\n')
            subprocess.run(['git', '-C', str(repo), 'add', 'source.h'], check=True)
            subprocess.run(['git', '-C', str(repo), '-c', 'user.name=Fixture', '-c',
                            'user.email=fixture@example.invalid', 'commit', '-qm', 'fixture'], check=True)
            pin = recipe.git(repo, 'rev-parse', 'HEAD').decode().strip()
            (repo/'source.h').write_text('dirty forbidden replacement\n')
            identities = {}
            recipe.stage(repo, pin, out, ['source.h'], identities, 'fixture')
            self.assertEqual((out/'source.h').read_text(), 'immutable\n')
            self.assertEqual(identities[(out/'source.h').resolve()], ('fixture', 'source.h'))


if __name__ == '__main__':
    unittest.main()
