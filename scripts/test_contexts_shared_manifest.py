#!/usr/bin/env python3
"""Regress the actual Runtime declaration format before building/publishing."""
import copy,json,unittest
from pathlib import Path
from build_contexts_shared import validate_declarations
ROOT=Path(__file__).resolve().parents[1]
class ManifestContract(unittest.TestCase):
 def setUp(self):self.manifest=json.loads((ROOT/'Apps/contexts-shared.json').read_text())
 def test_common_integer_contract(self):
  validate_declarations(self.manifest)
  rows={r['capability']:r['api'] for r in self.manifest['optional']}
  self.assertEqual(rows['alarm.service'],1)
  self.assertEqual(rows['alarm.service.facade'],2)
  self.assertEqual(rows['storage.key-value.atomic'],1)
  self.assertEqual(rows['rtc.clock'],2)
  self.assertEqual(rows['runtime.realtime'],1)
 def test_old_range_is_rejected(self):
  for value in ('>=1','1',True,0,-1,4294967296):
   with self.subTest(value=value):
    m=copy.deepcopy(self.manifest);m['optional'][0]['api']=value
    with self.assertRaises(ValueError):validate_declarations(m)
 def test_duplicate_capability_is_not_api_alternative(self):
  m=copy.deepcopy(self.manifest);m['optional'].append({'capability':'alarm.service','api':2})
  with self.assertRaises(ValueError):validate_declarations(m)
if __name__=='__main__':unittest.main()
