import json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from build_alarm_apps import ROOT,inventory,RUNTIME_PIN,RUNTIME_TREE
class AlarmInventory(unittest.TestCase):
 def test_new_apps_and_ordinary_service(self):
  apps,service=inventory();self.assertEqual([x['id'] for x in apps],['alarms','countdown']);self.assertEqual(service['id'],'alarm-service')
  for app in apps:
   manifest=json.loads((ROOT/app['manifest_path']).read_text());caps={x['capability'] for x in manifest['requires']}
   self.assertEqual(caps,{'display.output','input.touch.raw','rtc.clock','storage.key-value','alarm.service'})
   self.assertEqual(manifest['version'],'0.2.0' if app['id']=='alarms' else '0.1.3')
  self.assertEqual(RUNTIME_PIN,'b2fc83280c54ca3ebd567184cc50e4785daa3951');self.assertEqual(RUNTIME_TREE,'b3399091995ec67d84a151e8e18be59fe4475ee9')
 def test_delivered_subset_unchanged(self):
  root=json.loads((ROOT/'utilities-manifest.json').read_text());self.assertEqual([x['id'] for x in root['portable_apps']],['calculator','stopwatch'])
  self.assertNotIn('timecard',{x['id'] for x in root['alarm_apps']})
 def test_namespace_contract(self):
  policy=json.loads((ROOT/'Services/alarm_service/storage-policy.example.json').read_text())
  self.assertEqual(policy,{'alarm_cfg':{'namespace':3,'access':'read'},'timer_cfg':{'namespace':3,'access':'read'},'alarm_occ':{'namespace':4,'access':'read-write'},'timer_occ':{'namespace':4,'access':'read-write'},'alert_mode':{'namespace':1,'access':'read'}})
if __name__=='__main__':unittest.main()
