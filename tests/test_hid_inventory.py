import hashlib,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from build_hid_apps import inventory
class HidInventory(unittest.TestCase):
 def test_separate_profile(self):
  self.assertEqual([a['id'] for a in inventory()],['ble_touchpad','ble_buttons'])
  for a in inventory():
   side=json.loads((ROOT/a['manifest_path']).read_text())
   self.assertEqual(a['version'],'0.1.3')
   self.assertIn({'capability':'bluetooth.hid','api':'>=1'},side['requires'])
   self.assertIn({'capability':'input.touch.raw','api':'>=1'},side['requires'])
  scanner=json.loads((ROOT/'utilities-manifest.json').read_text())['ble_apps']
  self.assertEqual([a['id'] for a in scanner],['ble_scanner'])
 def test_exact_contract(self):
  pin=json.loads((ROOT/'sdk/hid-sources.json').read_text())
  self.assertEqual(pin['hid_instance'],0);self.assertEqual(pin['raw_touch_instance'],6)
  self.assertEqual(pin['button_namespace'],11);self.assertEqual(pin['radio_policy_namespace'],1)
  self.assertEqual(pin['hid_header_sha256'],hashlib.sha256((ROOT/'lib/Bluetooth/include/RiscBluetoothHidV1.h').read_bytes()).hexdigest())
  self.assertIn(pin['system_apps'],(ROOT/'.github/workflows/ble-hid.yml').read_text())
 def test_scoped_integration_contract(self):
  doc=(ROOT/'docs/BLE_HID_INTEGRATION.md').read_text()
  for name in ('ble_touchpad','ble_buttons'):
   self.assertIn('"application_id": "'+name+'", "api": 1, "namespace": 1',doc)
  self.assertIn('20',doc);self.assertIn('11',doc);self.assertIn('Global0',doc)
