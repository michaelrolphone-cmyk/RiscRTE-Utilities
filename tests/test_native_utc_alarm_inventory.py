import json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class NativeUtcInventory(unittest.TestCase):
 def test_explicit_authority_and_domain(self):
  manifest=json.loads((ROOT/'Services/alarm_service/native-utc-visual-manifest.json').read_text())
  self.assertEqual(manifest['version'],'0.4.4')
  self.assertEqual(manifest['requires'],[{'capability':'storage.key-value.bound','api':1},{'capability':'platform.clock','api':1},{'capability':'platform.realtime','api':1}])
  policy=json.loads((ROOT/'Services/alarm_service/native-utc-visual-storage-policy.example.json').read_text())
  self.assertEqual(len(policy),9)
  self.assertEqual(policy['time_zone'],{'namespace':1,'access':'read'})
  self.assertEqual(set(policy),{'alarm_utc_cfg','timer_utc_cfg','alarm_utc_occ','timer_utc_occ','points_utc_cfg','points_utc_occ','alert_mode','alert_dnd','time_zone'})
  self.assertTrue(all(len(key)<=15 for key in policy))
  self.assertEqual({key for key,val in policy.items() if val['access']=='read-write'},{'alarm_utc_occ','timer_utc_occ','points_utc_occ'})
 def test_old_profiles_keep_identity(self):
  for name,version,keys in [('points', '0.4.2',9),('visual-points','0.4.4',8)]:
   self.assertEqual(json.loads((ROOT/f'Services/alarm_service/{name}-manifest.json').read_text())['version'],version)
   self.assertEqual(len(json.loads((ROOT/f'Services/alarm_service/{name}-storage-policy.example.json').read_text())),keys)
