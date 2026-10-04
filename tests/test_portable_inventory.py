import json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from build_portable_apps import ROOT,inventory,PIN
class PortableInventory(unittest.TestCase):
 def test_explicit_shared_original_apps(self):
  apps=inventory();self.assertEqual([x['id'] for x in apps],['calculator','stopwatch','waterfall'])
  self.assertEqual(PIN,'b28428505c9e067bb6ea8a84d12f13ec0bcc4992')
  self.assertEqual([app['version'] for app in apps],['0.1.1','0.1.1','0.1.1'])
  for app in apps:
   self.assertTrue(all((ROOT/path).is_file() for path in app['additional_sources']))
 def test_reader_release_cohort_is_preserved(self):
  root=json.loads((ROOT/'utilities-manifest.json').read_text())
  self.assertEqual([x['id'] for x in root['apps']],['gps','lora','battery'])
  self.assertEqual({x['id'] for x in json.loads((ROOT/'sdk/release-baseline.json').read_text())['apps']},{'gps','lora','battery'})
 def test_current_icons_are_available_in_pinned_client(self):
  for name,icon in [('calculator','solid:f00a'),('stopwatch','solid:f2f2'),('waterfall','solid:f012')]:
   manifest=json.loads((ROOT/'Apps'/(name+'.json')).read_text());self.assertEqual(manifest['icon'],icon)
 def test_scope_excludes_unrequested_apps(self):
  names={x['id'] for x in inventory()}
  self.assertFalse(names.intersection({'flashlight','level','steps','alarm','timer','timecard'}))
if __name__=='__main__':unittest.main()
