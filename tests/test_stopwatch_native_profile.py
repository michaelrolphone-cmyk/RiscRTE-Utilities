"""Native Stopwatch qualification profiles stay isolated and capability-complete."""
import copy
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('stopwatch_native_build',ROOT/'scripts/stopwatch_native_build.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
class Profile(unittest.TestCase):
 def test_grants_and_features(self):
  for utc in (False,True):
   with self.subTest(utc=utc):
    app=helper.profile(utc);self.assertEqual(app['version'],'0.1.26')
    selected=set(helper.flags(utc,True));self.assertIn('-DPORTABLE_STOPWATCH_NATIVE_UI',selected);self.assertIn('-DPORTABLE_NATIVE_CUSTODY_FENCE',selected);self.assertIn('-DPORTABLE_RASTER_SNAPSHOT',selected)
    self.assertEqual('-DPORTABLE_STOPWATCH_NATIVE_UTC' in selected,utc)
 def test_missing_or_added_grants_rejected(self):
  original=helper.ROOT;document=json.loads((ROOT/'profiles/stopwatch-native-qualification.json').read_text())
  try:
   with tempfile.TemporaryDirectory() as d:
    temp=Path(d);(temp/'profiles').mkdir();(temp/'Apps').mkdir();(temp/'Apps/stopwatch.json').write_bytes((ROOT/'Apps/stopwatch.json').read_bytes());helper.ROOT=temp
    for utc in (False,True):
     key='utc' if utc else 'rtc'
     for i in range(len(document['applications'][key]['grants'])):
      bad=copy.deepcopy(document);bad['applications'][key]['grants'].pop(i);(temp/'profiles/stopwatch-native-qualification.json').write_text(json.dumps(bad))
      with self.assertRaises(ValueError):helper.profile(utc)
     for grant in ({'capability':'storage.key-value','api':1,'instance_id':0},
                   {'capability':'runtime.realtime.control','api':1,'instance_id':0}):
      bad=copy.deepcopy(document);bad['applications'][key]['grants'].append(grant);(temp/'profiles/stopwatch-native-qualification.json').write_text(json.dumps(bad))
      with self.assertRaises(ValueError):helper.profile(utc)
     bad=copy.deepcopy(document);bad['applications'][key]['version']='0.1.6';(temp/'profiles/stopwatch-native-qualification.json').write_text(json.dumps(bad))
     with self.assertRaises(ValueError):helper.profile(utc)
     for field,value in (('publish_to_catalog',True),('schema',2)):
      bad=copy.deepcopy(document);bad[field]=value;(temp/'profiles/stopwatch-native-qualification.json').write_text(json.dumps(bad))
      with self.assertRaises(ValueError):helper.profile(utc)
  finally:helper.ROOT=original
if __name__=='__main__':unittest.main()
