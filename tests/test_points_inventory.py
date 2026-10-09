import hashlib
import json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class PointsInventory(unittest.TestCase):
 def test_copied_service_status_contract(self):
  self.assertEqual(hashlib.sha256((ROOT/'lib/Alarm/include/AlarmServiceV1.h').read_bytes()).hexdigest(),'19984058bf5ec47f2772cf56acf0d3c41a389b7edc5bd4e41aaddab5a32a017b')
 def test_distinct_opt_in_identity(self):
  old=json.loads((ROOT/'Services/alarm_service/manifest.json').read_text())
  new=json.loads((ROOT/'Services/alarm_service/points-manifest.json').read_text())
  self.assertEqual((old['version'],new['version']),('0.1.0','0.4.1'))
  self.assertEqual(old['requires'],new['requires']);self.assertEqual(new['provides'],[{'capability':'alarm.service','api':1}])
  self.assertIn('-DPOINTS_IN_TIME_SERVICE',(ROOT/'scripts/build_points_service.py').read_text())
 def test_bound_namespace_mapping(self):
  old=json.loads((ROOT/'Services/alarm_service/storage-policy.example.json').read_text())
  new=json.loads((ROOT/'Services/alarm_service/points-storage-policy.example.json').read_text())
  self.assertEqual(len(new),9)
  self.assertEqual({k:new[k] for k in old},old)
  self.assertEqual(new['points_cfg'],{'namespace':5,'access':'read'})
  self.assertEqual(new['points_occ'],{'namespace':4,'access':'read-write'})
  self.assertEqual(new['alarm_volume'],{'namespace':1,'access':'read'})
  self.assertEqual(new['alert_dnd'],{'namespace':1,'access':'read'})
  self.assertTrue(all(len(k)<=15 for k in new))
