import json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from build_portable_apps import ROOT,inventory,PIN
class PortableInventory(unittest.TestCase):
 def test_explicit_shared_original_apps(self):
  apps=inventory();self.assertEqual([x['id'] for x in apps],['calculator','stopwatch'])
  self.assertEqual(PIN,'911be9e8042f1bcc46038fb70189eebe4ca106c5')
  for app in apps:
   self.assertEqual(app['version'],{'calculator':'0.1.4','stopwatch':'0.1.3'}[app['id']])
   self.assertTrue(all((ROOT/path).is_file() for path in app['additional_sources']))
 def test_reader_release_cohort_is_preserved(self):
  root=json.loads((ROOT/'utilities-manifest.json').read_text())
  self.assertEqual([x['id'] for x in root['apps']],['gps','lora','battery'])
  self.assertEqual({x['id'] for x in json.loads((ROOT/'sdk/release-baseline.json').read_text())['apps']},{'gps','lora','battery'})
 def test_current_icons_are_available_in_pinned_client(self):
  for name,icon in [('calculator','solid:f1ec'),('stopwatch','solid:f2f2')]:
   manifest=json.loads((ROOT/'Apps'/(name+'.json')).read_text());self.assertEqual(manifest['icon'],icon)
 def test_scope_excludes_unrequested_apps(self):
  names={x['id'] for x in inventory()}
  self.assertFalse(names.intersection({'flashlight','level','steps','alarm','timer','timecard'}))
if __name__=='__main__':unittest.main()
