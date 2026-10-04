import json
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from build_audio_apps import inventory

class AudioInventory(unittest.TestCase):
    def test_exact_generator_identity(self):
        apps=inventory()
        self.assertEqual([x['id'] for x in apps],['frequency_generator','audio_spectrum'])
        side=json.loads((ROOT/'Apps/frequency_generator.json').read_text())
        self.assertEqual(side['version'],'0.1.1')
        self.assertEqual(side['min_firmware_version'],'0.1.16')
        self.assertEqual([r['capability'] for r in side['requires']],
            ['display.output','input.touch.raw','audio.output','alarm.service'])
        self.assertNotIn('storage.key-value',json.dumps(side))

    def test_build_requires_audio_adapter(self):
        code=(ROOT/'scripts/build_audio_apps.py').read_text()
        for value in ('PORTABLE_AUDIO_SESSION','PORTABLE_ALARM_CLIENT','Exact clean audio lifecycle adapter required','Wrong target ABI'):
            self.assertIn(value,code)

    def test_hooks_do_not_change_provider_gain(self):
        code=(ROOT/'Apps/frequency_generator.c').read_text()
        self.assertNotIn('->set_gain(',code)
        self.assertNotIn('->silence(',code)
        self.assertIn('if(!owned)return true;',code)
        self.assertIn('if(uncertain)return false;',code)

    def test_bundled_font_notices_are_preserved(self):
        code=(ROOT/'scripts/build_audio_apps.py').read_text()
        for name in ('LICENSE-FontAwesome.txt','LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json','LICENSE-Utilities.txt'):
            self.assertIn(name,code)

    def test_spectrum_declares_alarm_and_input(self):
        side=json.loads((ROOT/'Apps/audio_spectrum.json').read_text())
        self.assertEqual(side['category'],['Audio Tools'])
        self.assertEqual(side['version'],'0.1.1')
        self.assertEqual(side['min_firmware_version'],'0.1.16')
        self.assertEqual([r['capability'] for r in side['requires']],
            ['display.output','input.touch.raw','audio.input','alarm.service'])
        self.assertNotIn('storage.key-value',json.dumps(side))
