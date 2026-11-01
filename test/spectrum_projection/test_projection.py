import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from project_spectrum_scene import project,replace_scene,project_utc_successor,replace_rtc
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


class UtcTests(unittest.TestCase):
 def setUp(self):
  self.original=base();self.original['drivers'].append({'manifest':'rtc/manifest.json','instance_id':8})
  self.original['app_capabilities'][0]['grants'].append(g('rtc.clock',2,8))
  self.original['app_capabilities'][1]['grants'].append(g('rtc.clock',2,8))
  self.m=copy.deepcopy(M);self.m['optional'].append({'capability':'rtc.clock','api':2})
  self.current,_=project(self.original,self.m,'a'*64)
  self.current['app_capabilities'][1]['grants'].remove(g('rtc.clock',2,8))
  self.old={'type':'driver','id':'test-rtc','architecture':'xtensa-esp32s3','driver_abi':2,'file_name':'driver.elf','provides':[{'api':2,'capability':'rtc.clock'}],'requires':[{'api':1,'capability':'i2c.bus'}],'version':'0.1.0'}
  self.new=dict(self.old,version='0.2.0')
 def run_project(self):return project_utc_successor(self.current,self.original,self.m,'b'*64)
 def test_restore_exact_original(self):
  before=copy.deepcopy(self.current);out,m=self.run_project();expected=copy.deepcopy(before);expected['app_capabilities'][1]['grants'].append(g('rtc.clock',2,8));self.assertEqual(out,expected);self.assertEqual(before,self.current);self.assertIn({'capability':'rtc.clock','api':2},m['optional'])
 def test_idempotent_restoration(self):
  self.current['app_capabilities'][1]['grants'].append(g('rtc.clock',2,8));out,_=self.run_project();self.assertEqual(out,self.current)
 def test_no_new_rtc_authority(self):
  self.original['app_capabilities'].pop();out,m=self.run_project();self.assertEqual(out,self.current);self.assertNotIn({'capability':'rtc.clock','api':2},m['optional'])
 def test_original_wrong_instance(self):
  self.original['app_capabilities'][1]['grants'][-1]['instance_id']=9
  with self.assertRaises(ValueError):self.run_project()
 def test_original_duplicate(self):
  self.original['app_capabilities'][1]['grants'].append(g('rtc.clock',2,8))
  with self.assertRaises(ValueError):self.run_project()
 def test_no_fabricated_existing_grant(self):
  self.original['app_capabilities'].pop();self.current['app_capabilities'][1]['grants'].append(g('rtc.clock',2,8))
  with self.assertRaises(ValueError):self.run_project()
 def test_binding_drift(self):
  self.current['drivers'][-1]['instance_id']=9
  with self.assertRaises(ValueError):self.run_project()
 def test_missing_declaration(self):
  self.m['optional'].pop()
  with self.assertRaises(ValueError):self.run_project()
 def test_wrong_api_declaration(self):
  self.m['optional'][-1]['api']=1
  with self.assertRaises(ValueError):self.run_project()
 def test_rtc_provider_preserves_graph(self):
  self.assertEqual(replace_rtc(self.current,self.old,self.new,offset_seconds=28800),self.current)
 def test_offset_must_be_explicit(self):
  for offset in (None,0,True,28800.0,-28800):
   with self.subTest(offset=offset),self.assertRaises(ValueError):replace_rtc(self.current,self.old,self.new,offset_seconds=offset)
 def test_descriptor_drift_rejected(self):
  for field,value in [('id','other'),('driver_abi',True),('provides',[{'api':2,'capability':'other'}]),('requires',[]),('requires',[{'api':True,'capability':'i2c.bus'}]),('status','new'),('version','0.1.0')]:
   m=copy.deepcopy(self.new);m[field]=value
   with self.subTest(field=field),self.assertRaises(ValueError):replace_rtc(self.current,self.old,m,offset_seconds=28800)
 def test_rtc_route_not_inferred(self):
  self.current['drivers'][-1]['image_source']={'path':'rtc.elf','volume_instance':9}
  with self.assertRaises(ValueError):replace_rtc(self.current,self.old,self.new,offset_seconds=28800)
if __name__=='__main__':unittest.main()
