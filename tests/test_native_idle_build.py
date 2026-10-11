import argparse
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native_idle_build', ROOT/'scripts/native_idle_build.py')
idle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(idle)

class NativeIdleBuild(unittest.TestCase):
    def args(self, enabled):
        return argparse.Namespace(x4_idle_source=Path('explicit.c') if enabled else None,
                                  x4_idle_sdk=None, x4_idle_runtime_sdk=None)

    def test_flag_off_keeps_versions_and_authority(self):
        profile = json.loads((ROOT/'Apps/native-utc-utilities.json').read_text())
        for name, version in profile['versions'].items():
            grants = profile['common_grants'] + profile['app_grants'][name]
            self.assertEqual(idle.grants(self.args(False), grants), grants)
            self.assertEqual(idle.version(self.args(False), name, version), version)

    def test_combined_cohort_preserves_all_grants_within_limit(self):
        for filename in ['native-utc-utilities.json', 'native-utc-alarms.json']:
            profile = json.loads((ROOT/'Apps'/filename).read_text())
            for name in profile['versions']:
                old = profile['common_grants'] + profile['app_grants'][name]
                grants = idle.grants(self.args(True), old + [{'capability':'telemetry.broadcast','api':1,'instance_id':0}])
                self.assertLessEqual(len(grants),16)
                for item in old:
                    expected = {k:item[k] for k in ('capability','api','instance_id')}
                    if expected['capability']=='bluetooth.hci':
                        expected['instance_id']=16
                    self.assertIn(expected,grants)
                self.assertTrue(all(set(g)=={'capability','api','instance_id'} for g in grants))
                for cap, instance in [('x4.power',17),('storage.volume',9),('net.wifi',15),('bluetooth.hci',16)]:
                    self.assertIn({'capability':cap,'api':1,'instance_id':instance}, grants)
                if name == 'ble_scanner':
                    self.assertEqual([g['instance_id'] for g in grants if g['capability']=='bluetooth.hci'],[16])
                self.assertFalse(any(g['capability'].startswith('rtc.') or g['capability']=='runtime.realtime.control' for g in grants))
                self.assertEqual(idle.grants(self.args(True),grants),grants)

    def test_partial_selection_fails_before_source_access(self):
        with self.assertRaises(SystemExit):
            idle.validate(self.args(True),argparse.ArgumentParser(),Path('unused'),Path('unused'))

    def test_reserved_versions(self):
        self.assertEqual(idle.VERSIONS,{'battery':'1.1.9','calculator':'0.1.16','stopwatch':'0.1.16','countdown':'0.1.15','alarms':'0.2.11','ble_scanner':'0.2.11','ble_touchpad':'0.1.14','ble_buttons':'0.1.14','waterfall':'0.2.7'})
