import json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class NativeSchedulesProfiles(unittest.TestCase):
 def test_explicit_service_api_and_no_output_drivers(self):
  m=json.loads((ROOT/'Services/alarm_service/native-utc-schedules-manifest.json').read_text())
  self.assertEqual(m['version'],'0.5.0');self.assertEqual(m['provides'],[{'capability':'alarm.service','api':2}])
  self.assertEqual({r['capability'] for r in m['requires']},{'storage.key-value.bound','platform.clock','platform.realtime'})
 def test_least_authority_keys(self):
  p=json.loads((ROOT/'Services/alarm_service/native-utc-schedules-storage-policy.example.json').read_text())
  expected={'alarm_utc_cfg':(3,'read'),'timer_utc_cfg':(3,'read'),'alarm_utc_occ':(4,'read-write'),'timer_utc_occ':(4,'read-write'),'alert_mode':(1,'read'),'alert_dnd':(1,'read')}
  self.assertEqual({k:(v['namespace'],v['access']) for k,v in p.items()},expected)
 def test_old_points_selection_unchanged(self):
  m=json.loads((ROOT/'Services/alarm_service/native-utc-visual-manifest.json').read_text())
  p=json.loads((ROOT/'Services/alarm_service/native-utc-visual-storage-policy.example.json').read_text())
  self.assertEqual(m['version'],'0.4.4');self.assertEqual(len(p),9);self.assertIn('points_utc_cfg',p);self.assertIn('time_zone',p)
