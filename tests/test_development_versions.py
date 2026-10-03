import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from check_release_parity import newer_version,compare
class Versions(unittest.TestCase):
 def test_newer_and_equal(self):
  self.assertTrue(newer_version('1.3.3','1.3.1'))
  self.assertFalse(newer_version('1.3.1','1.3.1'))
  self.assertFalse(newer_version('1.2.9','1.3.1'))
  with self.assertRaises(ValueError):newer_version('1.3.beta','1.3.1')
