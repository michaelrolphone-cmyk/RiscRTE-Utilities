#!/usr/bin/env python3
import copy,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from scanner_product_projection import project_scanner_boot,update_scanner_route,project_scanner_manifest
ROWS=json.loads((Path(__file__).parent/'published-scanner-rows.json').read_text())
MANIFEST=json.loads((ROOT/'Apps/ble_scanner_scene.json').read_text())
def fixture(lane):
 row=copy.deepcopy(ROWS[lane]);return {'drivers':[{'manifest':'unchanged/manifest.json'}],'provider_activation':{'unchanged':'demand'},'app_capabilities':[{'manifest':'other.json','grants':[]},row]},row
class Projection(unittest.TestCase):
 def test_three_exact_rows(self):
  for lane in ROWS:
   with self.subTest(lane=lane):
    boot,row=fixture(lane);prior=copy.deepcopy(boot);out,receipt=project_scanner_boot(boot,row,project_scanner_manifest(MANIFEST,row),preferences=lane=='watch')
    self.assertEqual(boot,prior);self.assertEqual(out['drivers'],boot['drivers']);self.assertEqual(out['app_capabilities'][0],boot['app_capabilities'][0]);self.assertEqual(out['provider_activation'],boot['provider_activation']);self.assertIn({'capability':'bluetooth.sensors','api':1,'instance_id':0},out['app_capabilities'][1]['grants']);self.assertEqual(len(receipt['added']),2 if lane=='watch' else 1)
 def test_data_authority_exact(self):
  for lane in ROWS:
   boot,row=fixture(lane);out,_=project_scanner_boot(boot,row,project_scanner_manifest(MANIFEST,row),preferences=lane=='watch');before=[x for x in row['grants'] if x['capability'] in ['storage.shared-data','contexts.service','rtc.clock']];after=[x for x in out['app_capabilities'][1]['grants'] if x['capability'] in ['storage.shared-data','contexts.service','rtc.clock']];self.assertEqual(before,after)
 def test_alarm_authority_exact(self):
  for lane in ROWS:
   b,r=fixture(lane);out,receipt=project_scanner_boot(b,r,project_scanner_manifest(MANIFEST,r),preferences=lane=='watch')
   alarms=[x for x in r['grants'] if x['capability']=='alarm.service']
   self.assertEqual(len(alarms),1)
   self.assertEqual(alarms,[x for x in out['app_capabilities'][1]['grants'] if x['capability']=='alarm.service'])
   self.assertFalse(any(x['capability']=='alarm.service' for x in receipt['retired']))
 def test_alarm_declaration_required(self):
  b,r=fixture('x4');m=copy.deepcopy(MANIFEST);m['optional']=[x for x in m['optional'] if x['capability']!='alarm.service']
  with self.assertRaises(ValueError):project_scanner_boot(b,r,m)
 def test_alarm_manifest_projection(self):
  for lane in ROWS:
   _,r=fixture(lane);m=project_scanner_manifest(MANIFEST,r)
   self.assertEqual([x['api'] for x in m['optional'] if x['capability']=='alarm.service'],[2 if lane=='x4' else 1])
   self.assertEqual([x for x in m['optional'] if x['capability']!='alarm.service'],[x for x in MANIFEST['optional'] if x['capability']!='alarm.service'])
 def test_alarm_manifest_duplicate(self):
  _,r=fixture('watch');m=copy.deepcopy(MANIFEST);m['optional'].append({'capability':'alarm.service','api':1})
  with self.assertRaises(ValueError):project_scanner_manifest(m,r)
 def test_alarm_manifest_ambiguous_grant(self):
  _,r=fixture('watch');r['grants'].append({'capability':'alarm.service','api':2,'instance_id':0})
  with self.assertRaises(ValueError):project_scanner_manifest(MANIFEST,r)
 def test_wrong_row(self):
  b,r=fixture('watch');r=copy.deepcopy(r);r['catalog_authority']=True
  with self.assertRaises(ValueError):project_scanner_boot(b,r,MANIFEST)
 def test_wrong_identity(self):
  b,r=fixture('watch');m=copy.deepcopy(MANIFEST);m['id']='other'
  with self.assertRaises(ValueError):project_scanner_boot(b,r,m)
 def test_missing_declaration(self):
  b,r=fixture('watch');m=copy.deepcopy(MANIFEST);m['optional']=[x for x in m['optional'] if x['capability']!='storage.shared-data']
  with self.assertRaises(ValueError):project_scanner_boot(b,r,m)
 def test_unknown_authority(self):
  b,r=fixture('watch');r['grants'].append({'capability':'unreviewed','api':1,'instance_id':0})
  with self.assertRaises(ValueError):project_scanner_boot(b,r,MANIFEST)
 def test_missing_namespace(self):
  b,r=fixture('watch');r['grants']=[x for x in r['grants'] if not (x['capability']=='storage.key-value' and x['instance_id']==1)]
  with self.assertRaises(ValueError):project_scanner_boot(b,r,MANIFEST)
 def test_missing_sensors(self):
  b,r=fixture('x4');r['grants']=[x for x in r['grants'] if x['capability']!='bluetooth.sensors']
  with self.assertRaises(ValueError):project_scanner_boot(b,r,MANIFEST)
 def test_missing_hci(self):
  b,r=fixture('x4');r['grants']=[x for x in r['grants'] if x['capability']!='bluetooth.hci']
  with self.assertRaises(ValueError):project_scanner_boot(b,r,MANIFEST)
 def test_duplicate_declaration(self):
  b,r=fixture('watch');m=copy.deepcopy(MANIFEST);m['optional'].append(m['optional'][0])
  with self.assertRaises(ValueError):project_scanner_boot(b,r,m)
 def test_duplicate_row(self):
  b,r=fixture('watch');b['app_capabilities'].append(copy.deepcopy(r))
  with self.assertRaises(ValueError):project_scanner_boot(b,r,MANIFEST)
 def test_internal_and_simple_route_unchanged(self):
  for lane in ['watch','wave']:
   b,r=fixture(lane);self.assertEqual(b,update_scanner_route(b,r,'a'*64,'b'*64))
 def test_content_route_only(self):
  b,r=fixture('x4');old='2fd57fe781737b6203f7d35479152825e8c636705e0284594007c0f260c2754c';out=update_scanner_route(b,r,old,'a'*64);actual=out['app_capabilities'][1];self.assertEqual(actual['image_source']['volume_instance'],9);self.assertTrue(actual['catalog_authority']);self.assertIn('image-'+'a'*64,actual['image_source']['path']);actual['image_source']['path']=r['image_source']['path'];self.assertEqual(out,b)
 def test_wrong_content_hash(self):
  b,r=fixture('x4')
  with self.assertRaises(ValueError):update_scanner_route(b,r,'a'*64,'b'*64)
 def test_invalid_hash(self):
  b,r=fixture('watch')
  with self.assertRaises(ValueError):update_scanner_route(b,r,'short','b'*64)
 def test_invalid_path(self):
  b,r=fixture('wave');r['image_source']['path']='../ble_scanner.elf'
  with self.assertRaises(ValueError):update_scanner_route(b,r,'a'*64,'b'*64)
if __name__=='__main__':unittest.main()
