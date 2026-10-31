#!/usr/bin/env python3
"""Guard shared-app identities, profile-free recipes and byte-identical selections."""
import copy,json,tempfile,unittest
from pathlib import Path
from build_shared_scene_apps import inventory,distribute
class SharedApps(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);(self.root/'Apps').mkdir()
  (self.root/'Apps/app.c').write_text('void app_main(void){}')
  self.manifest={'id':'app','version':'1','file_name':'app.elf','type':'application','architecture':'xtensa-esp32s3','entry':'app_main'}
  self.row={'id':'app','version':'1','file_name':'app.elf','runtime_profile':'shared-scene-v1','manifest_path':'Apps/app.json','sources':['Apps/app.c'],'exports':['app_main']}
 def tearDown(self):self.temp.cleanup()
 def save(self,rows=None):
  (self.root/'Apps/app.json').write_text(json.dumps(self.manifest));(self.root/'utilities-manifest.json').write_text(json.dumps({'shared_scene_apps':rows or [self.row]}))
 def test_valid(self):self.save();self.assertEqual(inventory(self.root)[0],self.row)
 def test_identity(self):self.row['version']='2';self.save();self.assertRaises(ValueError,inventory,self.root)
 def test_device_flags(self):self.row['defines']=['-DWATCH'];self.save();self.assertRaises(ValueError,inventory,self.root)
 def test_duplicate(self):self.save([self.row,copy.deepcopy(self.row)]);self.assertRaises(ValueError,inventory,self.root)
 def test_escape(self):self.row['sources']=['../outside.c'];self.save();self.assertRaises(ValueError,inventory,self.root)
 def test_id_traversal(self):
  self.row['id']=self.manifest['id']='../escaped';self.save();self.assertRaises(ValueError,inventory,self.root)
 def test_id_absolute(self):
  self.row['id']=self.manifest['id']='/tmp/escaped';self.save();self.assertRaises(ValueError,inventory,self.root)
 def test_exports(self):self.row['exports']=['app_main','physical_device_driver'];self.save();self.assertRaises(ValueError,inventory,self.root)
 def test_non_elf(self):
  self.row['file_name']=self.manifest['file_name']='app.bin';self.save();self.assertRaises(ValueError,inventory,self.root)
 def test_receipt_collision(self):
  self.row['file_name']=self.manifest['file_name']='build.json';self.save();self.assertRaises(ValueError,inventory,self.root)
 def test_manifest_receipt_collision(self):
  self.row['file_name']=self.manifest['file_name']='build.elf';self.save();self.assertRaises(ValueError,inventory,self.root)
 def test_wrong_sidecar(self):
  package=self.root/'package';package.mkdir();(package/'app.elf').write_bytes(b'module');(package/'app.json').write_text('{}')
  self.assertRaises(ValueError,distribute,package,self.manifest,self.root/'out')
 def test_selection(self):
  package=self.root/'package';package.mkdir();(package/'app.elf').write_bytes(b'exact compiled module');(package/'app.json').write_text(json.dumps(self.manifest))
  out=self.root/'out';result=distribute(package,self.manifest,out)
  self.assertEqual(set(result),{'x4','twatch','waveshare'});self.assertEqual(len({v['sha256'] for v in result.values()}),1)
  for value in result.values():self.assertEqual((out/value['path']).read_bytes(),(package/'app.elf').read_bytes())
if __name__=='__main__':unittest.main()
