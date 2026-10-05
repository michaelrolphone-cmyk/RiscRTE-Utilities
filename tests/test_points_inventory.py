import hashlib
import json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class PointsInventory(unittest.TestCase):
 def test_unchanged_service_abi(self):
  self.assertEqual(hashlib.sha256((ROOT/'lib/Alarm/include/AlarmServiceV1.h').read_bytes()).hexdigest(),'3f5546fadf7131cdae82ed5316d4ff49157fac881af34fba4663d00c54e2a6fb')
 def test_distinct_opt_in_identity(self):
  old=json.loads((ROOT/'Services/alarm_service/manifest.json').read_text())
  new=json.loads((ROOT/'Services/alarm_service/points-manifest.json').read_text())
  self.assertEqual((old['version'],new['version']),('0.1.0','0.3.0'))
  self.assertEqual(old['requires'],new['requires']);self.assertEqual(new['provides'],[{'capability':'alarm.service','api':1}])
  self.assertIn('-DPOINTS_IN_TIME_SERVICE',(ROOT/'scripts/build_points_service.py').read_text())
 def test_bound_namespace_mapping(self):
  old=json.loads((ROOT/'Services/alarm_service/storage-policy.example.json').read_text())
  new=json.loads((ROOT/'Services/alarm_service/points-storage-policy.example.json').read_text())
  self.assertEqual(len(new),7)
  self.assertEqual({k:new[k] for k in old},old)
  self.assertEqual(new['points_cfg'],{'namespace':5,'access':'read'})
  self.assertEqual(new['points_occ'],{'namespace':4,'access':'read-write'})
  self.assertTrue(all(len(k)<=15 for k in new))
