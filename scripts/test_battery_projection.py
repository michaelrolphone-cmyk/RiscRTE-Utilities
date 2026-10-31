#!/usr/bin/env python3
import copy,json,unittest
from pathlib import Path
from project_battery_scene import project
ROOT=Path(__file__).resolve().parents[1]
class Projection(unittest.TestCase):
 def setUp(self):
  self.app=json.loads((ROOT/'Apps/battery_scene.json').read_text());self.provider=json.loads((ROOT/'Services/telemetry_broadcast/manifest.json').read_text())
  self.boot={'app_capabilities':[{'manifest':'battery.json','grants':[{'api':1,'capability':c,'instance_id':i} for c,i in [('board.battery',4),('storage.key-value',1),('telemetry.broadcast',0)]]},{'manifest':'unchanged.json','grants':[]}],'drivers':[{'manifest':'broadcast/manifest.json'},{'manifest':'scheduler/manifest.json','key_value':[{'key':'timer_occ','namespace':4,'access':'read-write'}]}],'resident_shell':{'host':'default.elf','legacy':['battery.elf','unchanged.elf'],'foreground':[]}}
 def call(self):return project(self.boot,self.app,self.provider,'a'*64,default_enabled=True)
 def test_preserves(self):
  old=copy.deepcopy(self.boot);p=self.call();self.assertEqual(old,self.boot);self.assertEqual(p['drivers'],old['drivers']);self.assertEqual(p['app_capabilities'][1],old['app_capabilities'][1]);self.assertEqual(p['resident_shell']['foreground'],['battery.elf']);self.assertEqual([g['instance_id'] for g in p['app_capabilities'][0]['grants']],[0,4,0,1,1])
 def test_content_address(self):
  self.boot['drivers'][0]['image_source']={'path':'Drivers/telemetry-broadcast/image-'+('b'*64)+'.elf','volume_instance':9,'internal_fallback':True};p=self.call();s=p['drivers'][0]['image_source'];self.assertEqual(s['path'],'Drivers/telemetry-broadcast/image-'+('a'*64)+'.elf');self.assertTrue(s['internal_fallback']);self.assertEqual(s['volume_instance'],9)
 def test_bad_inputs(self):
  changes=[lambda:self.app.update(file_name='../other.elf'),lambda:self.app['optional'].append({'capability':'net.wifi','api':1}),lambda:self.provider.update(id='unrelated'),lambda:self.provider['provides'].append({'capability':'net.wifi','api':1}),lambda:self.provider['requires'].append({'capability':'net.wifi','api':1}),lambda:self.boot['app_capabilities'].append(copy.deepcopy(self.boot['app_capabilities'][0])),lambda:self.boot['app_capabilities'][0]['grants'][1].update(instance_id=2),lambda:self.boot['drivers'][0].update(image_source={'path':'../bad','volume_instance':9}),lambda:self.boot['resident_shell']['legacy'].remove('battery.elf')]
  for f in changes:
   self.setUp();f()
   with self.assertRaises(ValueError):self.call()
 def test_default_policy_identity(self):
  with self.assertRaises(ValueError):project(self.boot,self.app,self.provider,'a'*64,default_enabled=False)
  self.provider=json.loads((ROOT/'Services/telemetry_broadcast/default-off.json').read_text());self.assertTrue(project(self.boot,self.app,self.provider,'a'*64,default_enabled=False))
if __name__=='__main__':unittest.main()
