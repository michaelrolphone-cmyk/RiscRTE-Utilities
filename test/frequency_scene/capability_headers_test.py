import pathlib,tempfile,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from build_shared_scene_apps import stage_capability_headers
class Contracts(unittest.TestCase):
 def setUp(self):
  self.t=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.t.name);self.app=self.root/'sdk/app';self.app.mkdir(parents=True);self.inc=self.root/'stage';self.inc.mkdir();(self.app/'Good.h').write_text('canonical');(self.root/'other').mkdir();(self.root/'other/Injected.h').write_text('injected')
 def tearDown(self):self.t.cleanup()
 def stage(self,v):stage_capability_headers([{'capability_headers':v}],self.root,self.inc)
 def test_positive_duplicate(self):self.stage(['sdk/app/Good.h']*2);self.assertEqual((self.inc/'Good.h').read_text(),'canonical')
 def test_traversal(self):
  with self.assertRaises(ValueError):self.stage(['sdk/app/../../other/Injected.h'])
 def test_symlink_escape(self):
  (self.app/'Link.h').symlink_to(self.root/'other/Injected.h')
  with self.assertRaises(ValueError):self.stage(['sdk/app/Link.h'])
 def test_missing(self):
  with self.assertRaises(ValueError):self.stage(['sdk/app/Missing.h'])
 def test_extension(self):
  (self.app/'Wrong.c').write_text('x')
  with self.assertRaises(ValueError):self.stage(['sdk/app/Wrong.c'])
 def test_conflict(self):
  (self.inc/'Good.h').write_text('different')
  with self.assertRaises(ValueError):self.stage(['sdk/app/Good.h'])
 def test_string(self):
  with self.assertRaises(ValueError):self.stage('sdk/app/Good.h')
 def test_dict(self):
  with self.assertRaises(ValueError):self.stage({'sdk/app/Good.h':True})
 def test_non_string(self):
  with self.assertRaises(ValueError):self.stage([123])
 def test_absolute(self):
  with self.assertRaises(ValueError):self.stage([str(self.app/'Good.h')])
if __name__=='__main__':unittest.main()
