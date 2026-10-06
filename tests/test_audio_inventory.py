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
        self.assertEqual(side['version'],'0.1.3')
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
        self.assertIn('volume=100',code)
        tone=(ROOT/'Apps/tone_core.h').read_text()
        self.assertIn('#define TONE_MAX_PERCENT 100u',tone)
        self.assertIn('s->gain>32767||s->target>32767',tone)

    def test_bundled_font_notices_are_preserved(self):
        code=(ROOT/'scripts/build_audio_apps.py').read_text()
        for name in ('LICENSE-FontAwesome.txt','LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json','LICENSE-Utilities.txt'):
            self.assertIn(name,code)

    def test_speech_sources_and_host_only_fixture(self):
        import hashlib
        root=ROOT/'lib/VoiceActivity'
        source=json.loads((root/'SOURCES.json').read_text())
        for name,digest in source['vendored_files_sha256'].items():
            self.assertEqual(hashlib.sha256((root/name).read_bytes()).hexdigest(),digest,name)
        fixture=ROOT/'tests/fixtures/voice'
        record=json.loads((fixture/'SOURCES.json').read_text())
        pcm=(fixture/'jfk16.pcm').read_bytes()
        self.assertEqual(len(pcm),2*record['samples'])
        self.assertEqual(hashlib.sha256(pcm).hexdigest(),record['fixture_sha256'])
        app=next(a for a in inventory() if a['id']=='audio_spectrum')
        self.assertNotIn('jfk',json.dumps(app))
        self.assertIn('Copyright (c) 2022 OpenAI',(fixture/'LICENSE-Whisper.txt').read_text())
        build=(ROOT/'scripts/build_audio_apps.py').read_text()
        for name in ('LICENSE','AUTHORS','PATENTS','SOURCES.json','PATCHES.md'):
            self.assertIn(name,build)

    def test_spectrum_declares_alarm_and_input(self):
        side=json.loads((ROOT/'Apps/audio_spectrum.json').read_text())
        self.assertEqual(side['category'],['Audio Tools'])
        self.assertEqual(side['version'],'0.4.3')
        self.assertEqual(side['requires'][-2]['api'],'>=2')
        self.assertEqual(side['requires'][-1],{'capability':'storage.app-data','api':'>=1'})
        self.assertEqual(side['min_firmware_version'],'0.1.32')
        self.assertEqual([r['capability'] for r in side['requires']],
            ['display.output','input.touch.raw','audio.input','alarm.service','storage.key-value','storage.app-data'])

    def test_watch_spectrum_product_controls(self):
        code=(ROOT/'Apps/audio_spectrum.c').read_text()
        self.assertIn('PortableWatchKeyboard.h',code)
        self.assertNotIn('PortableNovaKeyboard.h',code)
        self.assertNotIn('portable_nova_key_character',code)
        self.assertNotIn('demo_pcm',code)
        self.assertIn('#define PLOT_X 6',code)
        self.assertIn('#define PLOT_W 228',code)
        controls=(ROOT/'Apps/spectrum_controls.inc').read_text()
        self.assertIn('controls_scroll',controls)
        self.assertNotIn('SOURCE',controls)
        self.assertIn('tab(111,74,"MONITOR",view==2)',code)
        self.assertNotIn('tab(111,74,"LABELS",view==2)',code)
        self.assertIn('monitor_items(monitor_item items[17])',code)
        self.assertIn('"AMP %d dB"',code)

    def test_temporal_backend_and_verified_watch_profile_are_pinned(self):
        import hashlib
        dependency=json.loads((ROOT/'sdk/spectrum-temporal-sources.json').read_text())
        self.assertEqual(dependency['namespace'],2)
        self.assertEqual(dependency['required_layout_abi'],2)
        self.assertTrue(dependency['watch_installable'])
        self.assertEqual(dependency['runtime_publication'],'published')
        self.assertRegex(dependency['runtime_commit'],r'^[0-9a-f]{40}$')
        workflow=(ROOT/'.github/workflows/build.yml').read_text()
        self.assertIn('ref: '+dependency['runtime_commit'],workflow)
        self.assertIn('bash scripts/test_spectrum_app_data.sh .dependencies/spectrum-runtime',workflow)
        profile=dependency['validated_watch_profile']
        self.assertEqual(profile['name'],'watch-current-apps-v1')
        self.assertEqual((profile['requirements'],profile['grants']),(12,12))
        self.assertRegex(profile['system_apps_commit'],r'^[0-9a-f]{40}$')
        for field in ('spectrum_manifest_sha256','spectrum_policy_sha256'):
            self.assertRegex(profile[field],r'^[0-9a-f]{64}$')
        self.assertEqual(dependency['api_header_sha256'],hashlib.sha256((ROOT/'Apps/RiscAppDataV1.h').read_bytes()).hexdigest())
        self.assertEqual(dependency['preserved_key_value_namespaces'],{'spectrum_api2':7,'shared_preferences_api1':1})
