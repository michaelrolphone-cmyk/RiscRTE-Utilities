#!/usr/bin/env python3
"""Generic policy projection guards, not device execution."""
import copy,unittest
from project_schedule_scene import project
class Projection(unittest.TestCase):
 def fixture(self,version=1):
  key='timer_cfg' if version==1 else 'timer_utc_cfg'
  app={'type':'application','architecture':'xtensa-esp32s3','entry':'app_main','id':'countdown','file_name':'countdown.elf','requires':[],'optional':[{'capability':c,'api':1} for c in ('ui.scene','countdown.control','storage.app-data')]}
  scheduler={'manifest':'scheduler.json','key_value':[{'key':key,'namespace':3,'access':'read-write'},{'key':'occurrence','namespace':4,'access':'read-write'}]}
  boot={'drivers':[scheduler,{'manifest':'scene.json'}],'app_capabilities':[{'manifest':'countdown.json','grants':[{'capability':'storage.key-value','api':1,'instance_id':3}],'image_source':{'path':'Apps/countdown/countdown.elf','volume_instance':12},'retained_metadata':True},{'manifest':'other.json','grants':[]}]}
  manifests={'countdown.json':dict(app,icon='old-icon'),'scheduler.json':{'provides':[{'capability':'alarm.service','api':version}]},'scene.json':{'provides':[{'capability':'ui.scene','api':1}]}}
  controls={k:{'type':'driver','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':'driver.elf','id':'countdown-control-'+k,'provides':[{'capability':'countdown.control','api':1}],'requires':[{'capability':'storage.key-value.bound','api':1},{'capability':'alarm.service','api':v},{'capability':'rtc.clock' if v==1 else 'platform.realtime','api':2 if v==1 else 1}]} for k,v in [('raw',1),('utc',2)]}
  return [boot,manifests,app,controls,66,{1,2,3,4,65}]
 def call(self,args,**kw):return project(*args,app_id=kw.get('app_id','countdown'),control_capability='countdown.control',config_keys={'raw':'timer_cfg','utc':'timer_utc_cfg'})
 def test_raw_utc_routes(self):
  for version in (1,2):
   a=self.fixture(version);original=copy.deepcopy(a);p=self.call(a)
   self.assertEqual(a,original);self.assertEqual(p['preserved_scheduler'],a[0]['drivers'][0]);self.assertEqual(p['app']['image_source'],a[0]['app_capabilities'][0]['image_source']);self.assertEqual(p['app']['manifest']['icon'],'old-icon');self.assertEqual(p['control']['driver']['key_value'][0]['namespace'],3)
 def test_invalid_namespace(self):
  for n in (0,-1,True,3,65,0x80000000):
   a=self.fixture();a[4]=n
   with self.assertRaises(ValueError):self.call(a)
 def test_identity(self):
  a=self.fixture();a[2]['entry']='other'
  with self.assertRaises(ValueError):self.call(a)
 def test_redirected_authority(self):
  a=self.fixture();a[0]['drivers'][0]['key_value'][0]['namespace']=99
  with self.assertRaises(ValueError):self.call(a)
 def test_existing_draft(self):
  a=self.fixture();a[0]['app_capabilities'][0]['grants'].append({'capability':'storage.app-data','instance_id':62})
  with self.assertRaises(ValueError):self.call(a)
 def test_ambiguous_scheduler(self):
  a=self.fixture();a[0]['drivers'].append(copy.deepcopy(a[0]['drivers'][0]))
  with self.assertRaises(ValueError):self.call(a)
 def test_existing_control(self):
  a=self.fixture();a[1]['countdown-control/manifest.json']={}
  with self.assertRaises(ValueError):self.call(a)
 def test_capability_contract(self):
  a=self.fixture();a[2]['optional']=[]
  with self.assertRaises(ValueError):self.call(a)
 def test_traversal(self):
  for path in ('/escape','../escape','Apps/../escape','Apps\\escape','.','Apps/\n/countdown.elf','Apps/countdown/image-deadbeef.elf'):
   a=self.fixture();a[0]['app_capabilities'][0]['image_source']['path']=path
   with self.assertRaises(ValueError):self.call(a)
 def test_provider_identity(self):
  for key,value in [('id','other'),('type','application'),('driver_abi',1),('architecture','other'),('file_name','other.elf')]:
   a=self.fixture();a[3]['raw'][key]=value
   with self.assertRaises(ValueError):self.call(a)
 def test_extra_dependency(self):
  a=self.fixture();a[3]['raw']['requires'].append({'capability':'secret','api':1})
  with self.assertRaises(ValueError):self.call(a)
 def test_extra_provider(self):
  a=self.fixture();a[3]['raw']['provides'].append({'capability':'secret','api':1})
  with self.assertRaises(ValueError):self.call(a)
 def test_unsafe_id(self):
  with self.assertRaises(ValueError):self.call(self.fixture(),app_id='../escape')
if __name__=='__main__':unittest.main()
