"""Regression guards for the frozen Watch power/charging qualification inputs."""
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SYSTEM_PIN = '96742a20af660808cae6cd3a36eadd12a83a460f'


class FrozenBatteryPowerCI(unittest.TestCase):
    def test_host_and_target_use_the_frozen_adapter(self):
        workflow = (ROOT / '.github/workflows/battery-power.yml').read_text()
        builder = (ROOT / 'scripts/build_battery_power.py').read_text()
        self.assertIn('ref: ' + SYSTEM_PIN, workflow)
        self.assertEqual(re.search(r"^SYSTEM_PIN='([^']+)'$", builder, re.M)[1], SYSTEM_PIN)

    def test_both_unchanged_fixtures_are_exercised(self):
        workflow = (ROOT / '.github/workflows/battery-power.yml').read_text()
        for suite in ('power', 'charging'):
            self.assertIn('python scripts/test_battery_' + suite + '.py --system-apps .dependencies/power-system', workflow)
            self.assertIn('build/battery-' + suite + '/frames-0-*/', workflow)

    def test_artifact_identity_matches_the_tested_checkout(self):
        workflow = (ROOT / '.github/workflows/battery-power.yml').read_text()
        ref = '${{ github.event.pull_request.head.sha || github.sha }}'
        self.assertIn('ref: ' + ref, workflow)
        self.assertIn('name: battery-power-qualification-only-' + ref, workflow)


if __name__ == '__main__':
    unittest.main()
