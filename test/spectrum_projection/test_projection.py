import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from project_spectrum_scene import project,replace_scene
M={'type':'application','id':'audio_spectrum','file_name':'audio_spectrum.elf','architecture':'xtensa-esp32s3','entry':'app_main','requires':[],'optional':[{'capability':c,'api':a} for c,a in [('ui.scene',1),('ui.text-input',1),('runtime.realtime',1),('storage.key-value',2),('storage.key-value',1),('storage.app-data',1),('storage.shared-data',1)]]}
def g(c,a=1,i=0):return {'capability':c,'api':a,'instance_id':i}
def base():return {'default_app':'default.elf','drivers':[{'manifest':'ui-scene/manifest.json'}],'app_capabilities':[{'manifest':'default.json','grants':[g('display.output',1,5),g('alarm.service')]},{'manifest':'audio_spectrum.json','grants':[g('display.output',1,5),g('alarm.service'),g('storage.key-value',2,7),g('storage.app-data',1,2),dict(g('storage.shared-data',1,4),file='context-rules.ctx'),g('storage.key-value',1,3),g('storage.key-value.atomic',1,1)],'image_source':{'path':'Apps/audio_spectrum/audio_spectrum.elf','volume_instance':12}}],'resident_shell':{'api':1,'host':'default.elf','foreground':['battery.elf'],'legacy':['audio_spectrum.elf','file_browser.elf']}}
class Tests(unittest.TestCase):
 def test_preserve(self):
  b=base();old=copy.deepcopy(b);out,m=project(b,M,'a'*64);self.assertEqual(b,old);r=out['app_capabilities'][1];self.assertEqual(r['image_source'],b['app_capabilities'][1]['image_source']);self.assertIn(dict(g('storage.shared-data',1,4),file='context-rules.ctx'),r['grants']);self.assertIn(g('storage.key-value',1,3),r['grants']);self.assertNotIn(g('storage.key-value.atomic',1,1),r['grants']);self.assertEqual(out['drivers'],b['drivers']);self.assertEqual(out['app_capabilities'][0],b['app_capabilities'][0]);self.assertIn('audio_spectrum.elf',out['resident_shell']['foreground'])
 def test_new_requires_explicit(self):
  b=base();b['app_capabilities'].pop()
  with self.assertRaises(ValueError):project(b,M,'a'*64)
  out,m=project(b,M,'a'*64,new_grants=[g('ui.scene'),g('ui.text-input'),g('storage.key-value',2,7),g('storage.app-data',1,2),g('runtime.realtime')]);self.assertEqual(len(out['app_capabilities'][-1]['grants']),5)
  with self.assertRaises(ValueError):project(b,M,'a'*64,new_grants=[g('ui.scene'),g('storage.key-value',2,7)])
 def test_new_namespace_collision(self):
  for grant in (g('storage.app-data',1,2),g('storage.key-value',1,7)):
   b=base();b['app_capabilities'].pop();b['app_capabilities'][0]['grants'].append(grant)
   with self.assertRaises(ValueError):project(b,M,'a'*64,new_grants=[g('ui.scene'),g('ui.text-input'),g('storage.key-value',2,7),g('storage.app-data',1,2),g('runtime.realtime')])
 def test_identity_authority(self):
  for field,value in [('id','other'),('abi_version',True),('provides',[{'capability':'bad','api':1}]),('file_name','../x.elf')]:
   m=copy.deepcopy(M);m[field]=value
   with self.assertRaises(ValueError):project(base(),m,'a'*64)
 def test_bad_route(self):
  for path in ('../x.elf','/x.elf','Apps/./audio_spectrum.elf','Apps/other.elf','Apps/a\n.elf'):
   b=base();b['app_capabilities'][1]['image_source']['path']=path
   with self.assertRaises(ValueError):project(b,M,'a'*64)
 def test_host_witness(self):
  b=base();b['app_capabilities'][0]['grants']=[]
  with self.assertRaises(ValueError):project(b,M,'a'*64)
 def test_undeclared_semantic(self):
  b=base();b['app_capabilities'][1]['grants'].append(g('important.service'))
  with self.assertRaises(ValueError):project(b,M,'a'*64)
 def test_bad_hash_duplicate(self):
  with self.assertRaises(ValueError):project(base(),M,'z'*64)
  b=base();b['app_capabilities'].append(copy.deepcopy(b['app_capabilities'][1]))
  with self.assertRaises(ValueError):project(b,M,'a'*64)
 def test_scene_no_new_provider(self):
  old={'type':'driver','id':'scene-host','architecture':'xtensa-esp32s3','driver_abi':2,'file_name':'driver.elf','provides':[{'api':1,'capability':'ui.scene'}],'requires':[{'api':1,'capability':'display.output'}],'version':'0.4.9'};new=dict(old,version='0.4.12');b=base();self.assertEqual(replace_scene(b,old,new,'a'*64),b)
  new['provides'].append({'api':1,'capability':'ui.canvas'})
  with self.assertRaises(ValueError):replace_scene(b,old,new,'a'*64)
if __name__=='__main__':unittest.main()
