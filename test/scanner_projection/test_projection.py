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

from scanner_home_package import validate_home_pair,LOCK,BASE_DEFINES,sha
def home_fixture(lane):
 old={'id':'default','file_name':'default.elf','version':'0.23.1','requires':[{'capability':'alarm.service','api':1}],'optional':[{'capability':'storage.volume','api':1}]}
 elf=b'\x7fELF\x01\x01\x01'+b'\0'*9+b'\x03\x00\x5e\x00'+b'\0'*44
 manifest={**old,'version':'0.25.1' if lane=='watch' else '0.25.2'}
 b={'schema':1,'build_variant':'shared-no-board-defines','preferences':'runtime-provider','automatic_provision':True,'hardware_profile_inputs':False,'hardware_verified':False,'dependency_lock':copy.deepcopy(LOCK),'compiler':'xtensa-esp32s3-elf-gcc 15.2.0 esp-15.2.0_20251204','compiler_sha256':'a'*64,'verified_dependencies':{r:{'commit':LOCK[r]['commit'],'files_sha256':{'fixture.c':'b'*64}} for r in ('system','utilities')},'home_files_sha256':{p:'c'*64 for p in ('dependencies.lock.json','scripts/build.py','apps/clock/clock_resident.inc','apps/clock/clock_alarm.inc','sdk/app/RiscResidentAlarmV1.h')},'artifacts':{'default':{'role':'home','file_name':'default.elf','bytes':len(elf),'sha256':sha(elf),'exports':['app_main','risc_resident_app_descriptor_v1'],'imports':['risc_runtime_get_api'],'defines':BASE_DEFINES+['-DWATCH_AUTOMATIC_PROVISION']+(['-DWATCH_SERIAL_TRANSFER_MODE'] if lane=='wave' else [])}}}
 if lane=='wave':b['serial_transfer_mode']=True
 return {'default.elf':elf,'default.json':json.dumps(manifest).encode(),'build.json':json.dumps(b).encode()},json.dumps(old).encode()
def change_build(package,fn):
 b=json.loads(package['build.json']);fn(b);package['build.json']=json.dumps(b).encode()
class HomePackages(unittest.TestCase):
 def test_selected_profiles(self):
  for lane in ('watch','wave'):
   first,old=home_fixture(lane);prior=copy.deepcopy(first)
   elf,manifest,receipt=validate_home_pair(first,copy.deepcopy(first),old,lane)
   self.assertEqual(first,prior);self.assertEqual(elf,first['default.elf']);self.assertEqual(manifest,first['default.json']);self.assertTrue(receipt['repeat_identical']);self.assertFalse(receipt['firmware_execution'])
 def test_no_x4_replacement(self):
  first,old=home_fixture('watch')
  with self.assertRaises(ValueError):validate_home_pair(first,first,old,'x4')
 def test_repeat_elf(self):
  first,old=home_fixture('watch');second=copy.deepcopy(first);second['default.elf']+=b'X'
  change_build(second,lambda b:b['artifacts']['default'].update(bytes=len(second['default.elf']),sha256=sha(second['default.elf'])))
  with self.assertRaises(ValueError):validate_home_pair(first,second,old,'watch')
 def test_manifest_authority(self):
  first,old=home_fixture('watch');m=json.loads(first['default.json']);m['optional'].append({'capability':'unreviewed','api':1});first['default.json']=json.dumps(m).encode()
  with self.assertRaises(ValueError):validate_home_pair(first,first,old,'watch')
 def test_receipt_hash(self):
  first,old=home_fixture('watch');change_build(first,lambda b:b['artifacts']['default'].update(sha256='0'*64))
  with self.assertRaises(ValueError):validate_home_pair(first,first,old,'watch')
 def test_exact_lock(self):
  first,old=home_fixture('watch');change_build(first,lambda b:b['dependency_lock']['system'].update(commit='0'*40))
  with self.assertRaises(ValueError):validate_home_pair(first,first,old,'watch')
 def test_verified_revision(self):
  first,old=home_fixture('watch');change_build(first,lambda b:b['verified_dependencies']['utilities'].update(commit='0'*40))
  with self.assertRaises(ValueError):validate_home_pair(first,first,old,'watch')
 def test_repeat_source(self):
  first,old=home_fixture('watch');second=copy.deepcopy(first);change_build(second,lambda b:b['home_files_sha256'].update({'apps/clock/clock_alarm.inc':'d'*64}))
  with self.assertRaises(ValueError):validate_home_pair(first,second,old,'watch')
 def test_profile(self):
  for field,value in (('automatic_provision',False),('serial_transfer_mode',True),('hardware_profile_inputs',True)):
   first,old=home_fixture('watch');change_build(first,lambda b:b.update({field:value}))
   with self.assertRaises(ValueError):validate_home_pair(first,first,old,'watch')
 def test_defines(self):
  first,old=home_fixture('wave');change_build(first,lambda b:b['artifacts']['default']['defines'].pop())
  with self.assertRaises(ValueError):validate_home_pair(first,first,old,'wave')
 def test_role(self):
  first,old=home_fixture('watch');change_build(first,lambda b:b['artifacts']['default'].update(role='return-to-default'))
  with self.assertRaises(ValueError):validate_home_pair(first,first,old,'watch')
 def test_missing_alarm_header(self):
  first,old=home_fixture('watch');change_build(first,lambda b:b['home_files_sha256'].pop('sdk/app/RiscResidentAlarmV1.h'))
  with self.assertRaises(ValueError):validate_home_pair(first,first,old,'watch')

if __name__=='__main__':unittest.main()
