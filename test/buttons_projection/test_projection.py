import copy, importlib.util, pathlib, unittest, sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
spec=importlib.util.spec_from_file_location('buttons_projection',ROOT/'scripts/buttons_product_projection.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Projection(unittest.TestCase):
 def setUp(self):
  caps=[('display.output',1,5),('input.touch.raw',1,6),('input.navigation',1,7),('storage.key-value',1,11),('storage.key-value',1,1),('bluetooth.hid',1,0),('alarm.service',1,0),('board.battery',1,4),('input.power-key',1,0),('contexts.service',1,0),('net.wifi',1,15)]
  self.row={'manifest':'ble_buttons.json','grants':[dict(capability=c,api=a,instance_id=i) for c,a,i in caps],'image_source':{'path':'Apps/ble_buttons/ble_buttons.elf','volume_instance':12},'catalog_authority':True}
  self.boot={'drivers':[{'manifest':'ble-hid/manifest.json','storage':{'namespace':10,'bond':'exact'}}],'app_capabilities':[copy.deepcopy(self.row),{'manifest':'other.json','grants':[]}],'activation':{'unchanged':True}}
  declarations={c:a for c,a,i in caps if c not in m.RETIRED};declarations.update({'ui.scene':1,'storage.preferences':1});declarations['alarm.service']=2
  self.canonical={'architecture':'xtensa-esp32s3','entry':'app_main','type':'application','id':'ble_buttons','file_name':'ble_buttons.elf','version':'0.2.0','requires':[],'optional':[{'capability':c,'api':a} for c,a in declarations.items()],'display_name':'new','icon':'new'}
 def selected(self):return m.project_buttons_manifest(self.canonical,self.row)
 def test_only_three_retired(self):
  before=copy.deepcopy(self.boot);after,r=m.project_buttons_boot(self.boot,self.row,self.selected());self.assertEqual(self.boot,before);self.assertEqual({g['capability'] for g in r['retired']},m.RETIRED);self.assertEqual(after['drivers'],before['drivers']);self.assertEqual(after['app_capabilities'][1],before['app_capabilities'][1]);self.assertEqual(len(r['added']),1)
 def test_power_and_battery_retained(self):
  after,_=m.project_buttons_boot(self.boot,self.row,self.selected());grants=after['app_capabilities'][0]['grants'];self.assertTrue(all(g in grants for g in self.row['grants'] if g['capability'] in ('input.power-key','board.battery')))
 def test_alarm_original_versions(self):
  for api in (1,2):
   row=copy.deepcopy(self.row);next(g for g in row['grants'] if g['capability']=='alarm.service')['api']=api
   selected=m.project_buttons_manifest(self.canonical,row);self.assertEqual(next(g for g in selected['optional'] if g['capability']=='alarm.service')['api'],api)
 def test_menu_exact(self):
  original={'id':'ble_buttons','file_name':'ble_buttons.elf','display_name':'Original Buttons','icon':'solid:123'};s=m.project_buttons_manifest(self.canonical,self.row,original);self.assertEqual(s['display_name'],original['display_name']);self.assertEqual(s['icon'],original['icon'])
 def test_absent_menu_stays_absent(self):
  s=m.project_buttons_manifest(self.canonical,self.row,{'id':'ble_buttons','file_name':'ble_buttons.elf'});self.assertNotIn('display_name',s);self.assertNotIn('icon',s)
 def test_undeclared_original_rejected(self):
  c=self.selected();c['optional']=[x for x in c['optional'] if x['capability']!='net.wifi'];self.assertRaises(ValueError,m.project_buttons_boot,self.boot,self.row,c)
 def test_wrong_row_rejected(self):
  row=copy.deepcopy(self.row);row['catalog_authority']=False;self.assertRaises(ValueError,m.project_buttons_boot,self.boot,row,self.selected())
 def test_hbv1_namespace_required(self):
  b=copy.deepcopy(self.boot);r=b['app_capabilities'][0];r['grants']=[g for g in r['grants'] if g.get('instance_id')!=11];self.assertRaises(ValueError,m.project_buttons_boot,b,r,self.selected())
 def test_hid_required(self):
  b=copy.deepcopy(self.boot);r=b['app_capabilities'][0];r['grants']=[g for g in r['grants'] if g['capability']!='bluetooth.hid'];self.assertRaises(ValueError,m.project_buttons_boot,b,r,self.selected())
 def test_no_implicit_facade(self):
  a,_=m.project_buttons_boot(self.boot,self.row,self.selected());self.assertNotIn('storage.preferences',[x['capability'] for x in a['app_capabilities'][0]['grants']])
 def test_explicit_same_namespace_facade(self):
  a,r=m.project_buttons_boot(self.boot,self.row,self.selected(),preferences=True);self.assertIn({'capability':'storage.preferences','api':1,'instance_id':1},r['added'])
 def test_facade_without_original_namespace_rejected(self):
  b=copy.deepcopy(self.boot);r=b['app_capabilities'][0];r['grants']=[g for g in r['grants'] if not(g['capability']=='storage.key-value' and g['instance_id']==1)];self.assertRaises(ValueError,m.project_buttons_boot,b,r,self.selected(),preferences=True)
 def test_duplicate_grant_rejected(self):
  b=copy.deepcopy(self.boot);r=b['app_capabilities'][0];r['grants'].append(copy.deepcopy(r['grants'][3]));self.assertRaises(ValueError,m.project_buttons_boot,b,r,self.selected())
 def test_duplicate_declaration_rejected(self):
  c=copy.deepcopy(self.canonical);c['optional'].append(c['optional'][0]);self.assertRaises(ValueError,m.project_buttons_manifest,c,self.row)
 def test_wrong_identity_rejected(self):
  c=copy.deepcopy(self.canonical);c['id']='other';self.assertRaises(ValueError,m.project_buttons_manifest,c,self.row)
 def test_simple_route_exact(self):self.assertEqual(m.update_buttons_route(self.boot,self.row,'a'*64,'b'*64),self.boot)
 def test_content_route_only_digest_changes(self):
  b=copy.deepcopy(self.boot);r=b['app_capabilities'][0];r['image_source']['path']='Apps/image-'+('a'*64)+'/ble_buttons.elf';a=m.update_buttons_route(b,r,'a'*64,'b'*64);self.assertEqual(a['app_capabilities'][0]['image_source']['path'],'Apps/image-'+('b'*64)+'/ble_buttons.elf');self.assertEqual(a['drivers'],b['drivers'])
 def test_stale_content_digest_rejected(self):
  b=copy.deepcopy(self.boot);r=b['app_capabilities'][0];r['image_source']['path']='Apps/image-'+('c'*64)+'/ble_buttons.elf';self.assertRaises(ValueError,m.update_buttons_route,b,r,'a'*64,'b'*64)
 def test_traversal_and_absolute_routes_rejected(self):
  for path in ('/Apps/ble_buttons.elf','Apps/../ble_buttons.elf','Apps//ble_buttons.elf','Apps\\bad/ble_buttons.elf'):
   b=copy.deepcopy(self.boot);r=b['app_capabilities'][0];r['image_source']['path']=path;self.assertRaises(ValueError,m.update_buttons_route,b,r,'a'*64,'b'*64)
 def test_digest_invalid_rejected(self):self.assertRaises(ValueError,m.update_buttons_route,self.boot,self.row,'invalid','b'*64)
 def test_wrong_application_abi_rejected(self):
  for field in ('architecture','entry','type'):
   c=copy.deepcopy(self.canonical);c[field]='wrong';self.assertRaises(ValueError,m.project_buttons_manifest,c,self.row)
 def test_ungranted_required_capability_rejected(self):
  c=self.selected();c['requires']=[{'capability':'ungranted.required','api':1}];self.assertRaises(ValueError,m.project_buttons_boot,self.boot,self.row,c)
 def test_control_and_digest_suffix_routes_rejected(self):
  for path in ('Apps/bad\n/ble_buttons.elf','Apps/image-'+('a'*64)+'suffix/ble_buttons.elf'):
   b=copy.deepcopy(self.boot);r=b['app_capabilities'][0];r['image_source']['path']=path;self.assertRaises(ValueError,m.update_buttons_route,b,r,'a'*64,'b'*64)
class IdleHome(unittest.TestCase):
 def test_required_qualified_version_and_identity(self):
  from assemble_buttons_products import validate_idle_home
  old={'id':'twatch-clock','file_name':'default.elf','version':'0.23.1','requires':[]}
  new={**old,'version':'0.24.1'};row={'manifest':'default.json','grants':[{'capability':'power.sleep','api':1,'instance_id':0}]}
  validate_idle_home(old,new,row,'watch')
  for bad in [{**new,'version':'0.23.1'},{**new,'id':'other'},{**new,'requires':[{'capability':'new'}]}]:self.assertRaises(ValueError,validate_idle_home,old,bad,row,'watch')
 def test_routes_and_authority_not_invented(self):
  from assemble_buttons_products import validate_idle_home
  old={'id':'twatch-clock','file_name':'default.elf','version':'0.23.2'};new={**old,'version':'0.24.2'};row={'manifest':'default.json','grants':[{'capability':'power.sleep'}]}
  validate_idle_home(old,new,row,'wave')
  for bad in [{**row,'image_source':{'path':'elsewhere'}},{**row,'grants':[]}]:self.assertRaises(ValueError,validate_idle_home,old,new,bad,'wave')
  self.assertRaises(ValueError,validate_idle_home,old,new,row,'x4')
if __name__=='__main__':unittest.main()
