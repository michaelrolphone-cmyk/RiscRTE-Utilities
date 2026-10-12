"""Keep the selected opt-in distinct from the historical scene recipe."""
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    'contexts_scene_build_contract', ROOT / 'scripts/build_contexts_scene.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class ContextsSharedBuild(unittest.TestCase):
    def selected(self, device, shared, faces=False):
        captured = {}

        def record(cc, include, output, manifest, sources, **kwargs):
            captured.update(cc=cc, manifest=manifest, sources=sources, **kwargs)
            return {'id': 'contexts'}

        builder = SimpleNamespace(
            compiler_path=lambda: 'target-cc', build=record,
            json_write=lambda path, value: path.write_text(json.dumps(value)))
        with tempfile.TemporaryDirectory() as temporary:
            tmp = Path(temporary)
            catalog = tmp / 'catalog.json'
            catalog.write_text(json.dumps([{'file_name': 'calculator.elf',
                                           'display_name': 'Calculator'}]))
            with patch.object(build, 'stage', return_value=tmp), \
                 patch.object(build, 'load', return_value=builder), \
                 patch.object(build.subprocess, 'run'):
                build.run(tmp, tmp, tmp / 'out', catalog, device,
                          tmp / 'ContextsFaces.h' if faces else None, shared)
            captured['catalog'] = (tmp / 'out/catalog.c').read_text()
        return captured

    def test_watch_shared_flag_and_original_faces(self):
        selected = self.selected('watch', True, faces=True)
        self.assertEqual(selected['defines'][0], '-DPORTABLE_CONTEXTS_SHARED_TEXT_INPUT')
        self.assertEqual(selected['defines'][1], '-include')
        self.assertTrue(selected['defines'][2].endswith('/ContextsFaces.h'))
        self.assertEqual(selected['manifest']['version'], '0.4.3')
        self.assertIn({'capability': 'ui.text-input', 'api': 1},
                      selected['manifest']['requires'])
        self.assertIn('calculator.elf', selected['catalog'])
        self.assertEqual(selected['exports'],
                         {'app_main', 'risc_resident_app_descriptor_v1'})

    def test_x4_retains_opt_in_and_all_existing_defines(self):
        selected = self.selected('x4', True)
        self.assertEqual(selected['defines'], [
            '-DPORTABLE_CONTEXTS_SHARED_TEXT_INPUT',
            '-DCONTEXTS_ALARM_API=2', '-DCONTEXTS_SETTING_MASK=0x274u',
            '-DPORTABLE_NATIVE_TIME_TOOLBAR',
            '-DPORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO'])
        requirements = selected['manifest']['requires']
        self.assertIn({'capability': 'alarm.service', 'api': 2}, requirements)
        self.assertIn({'capability': 'runtime.realtime', 'api': 1}, requirements)
        self.assertEqual(sum(r['capability'] == 'ui.text-input'
                             for r in requirements), 1)
        self.assertEqual(sum(r['capability'] == 'ui.scene'
                             for r in requirements), 1)

    def test_flag_off_generic_build_remains_legacy(self):
        for device in ('watch', 'x4'):
            with self.subTest(device=device):
                selected = self.selected(device, False)
                self.assertNotIn('-DPORTABLE_CONTEXTS_SHARED_TEXT_INPUT',
                                 selected['defines'])
                self.assertEqual(selected['manifest']['version'], '0.4.2')
                self.assertFalse(any(r['capability'] == 'ui.text-input'
                                     for r in selected['manifest']['requires']))

    def test_source_manifest_is_not_product_opt_in(self):
        manifest = json.loads((ROOT / 'Apps/contexts-scene.json').read_text())
        self.assertEqual(manifest['version'], '0.4.2')
        self.assertFalse(any(r['capability'] == 'ui.text-input'
                             for r in manifest['requires']))


if __name__ == '__main__':
    unittest.main()
