"""Keep historical RF implementation and comparison baseline distinct."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HistoricalSignalWorkflow(unittest.TestCase):
    def test_rf_shared_replay_uses_implementation_not_its_baseline(self):
        workflow = (ROOT / '.github/workflows/rf-shared-input.yml').read_text()
        checkouts = workflow.split('- uses: actions/checkout@v4')
        self.assertEqual(len(checkouts), 4)
        self.assertIn('ref: 52157660ec573cf4639ffe3eefcad5b7b3115c4e', checkouts[1])
        self.assertIn('ref: fd4b1e52fe2a49c3c4c22f9ccd4c98a91f88afa0', checkouts[2])
        self.assertIn('ref: 82d17d0d902d427938af95aae0e976c8f78041ed', checkouts[3])
        self.assertIn('path: .dependencies/baseline', checkouts[3])
        self.assertIn('--baseline-utilities .dependencies/baseline', workflow)
        self.assertNotIn('continue-on-error:', workflow)
        self.assertNotIn('contents: write', workflow)


if __name__ == '__main__':
    unittest.main()
