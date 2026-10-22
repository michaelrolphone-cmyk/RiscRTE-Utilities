"""Historical release lanes cannot relabel an unqualified current candidate."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ConsolidatedWorkflowRouting(unittest.TestCase):
    def test_historical_and_current_lanes_are_explicit(self):
        for partition, workflow, pin, candidate in (
            ('controls-power', 'native-ble-buttons', '6b2047500905f8f45087cb49989103a926fc5c63', 't039-ble-candidate'),
            ('signal-capture', 'native-spectrum', '432e54bbf35fee6e66aa487826d719f231cb4db4', 't040-neural-guard')):
            if not (ROOT/f'docs/consolidation/20261022/{partition}.json').exists():
                continue
            with self.subTest(partition=partition):
                original=(ROOT/f'docs/consolidation/20261022/frozen-{workflow}.yml').read_text()
                current=(ROOT/f'.github/workflows/{workflow}.yml').read_text()
                self.assertIn('HISTORICAL_SOURCE: '+pin,current)
                qualification,publisher=current.split('  publish-development:',1)
                self.assertIn('ref: ${{ env.HISTORICAL_SOURCE }}',qualification)
                self.assertIn('--source-sha "${{ env.HISTORICAL_SOURCE }}"',qualification)
                self.assertIn('github.sha == \''+pin+'\'',publisher)
                self.assertIn("github.event_name == 'workflow_dispatch'",publisher)
                self.assertIn('inputs.publish_development',publisher)
                self.assertIn('needs: qualify',publisher)
                self.assertNotIn('continue-on-error:',current)
                self.assertIn('--validate-only',qualification)
                self.assertIn('--validate-only',original)
                active=(ROOT/f'.github/workflows/{candidate}.yml').read_text()
                self.assertIn('ref: ${{ github.event.pull_request.head.sha || github.sha }}',active)
                self.assertNotIn('--publish',active)
                self.assertNotIn('contents: write',active)
                self.assertNotIn('continue-on-error:',active)

if __name__ == '__main__':
    unittest.main()
