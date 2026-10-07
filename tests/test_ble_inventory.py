import json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from build_ble_apps import inventory
class BleInventory(unittest.TestCase):
 def test_scanner_identity_and_capabilities(self):
  rows=inventory();self.assertEqual([a['id'] for a in rows],['ble_scanner'])
  side=json.loads((ROOT/'Apps/ble_scanner.json').read_text())
  self.assertEqual(side['version'],'0.2.1');self.assertEqual(side['icon'],'solid:f7c0')
  self.assertEqual({r['capability'] for r in side['requires']},{'display.output','input.touch.raw','bluetooth.sensors','bluetooth.hci','alarm.service','storage.key-value'})
  self.assertEqual({r['capability'] for r in side['optional']},{'rtc.clock','net.wifi'})
 def test_reuse_keyboard_and_provider(self):
  source=(ROOT/'Apps/ble_scanner.c').read_text()
  for expected in ('touch_contact','detail_scroll','PORTABLE_RADIO_AIRPLANE','portable_radio_suspend','retain()','scrollbar'):
   self.assertIn(expected,source)
  self.assertIn('PortableWatchKeyboard.h',source)
  self.assertNotIn('send_owned',source)
  self.assertNotIn('ble_scan_core.h',source)
 def test_pinned_radio_hooks_and_notices(self):
  source=(ROOT/'scripts/build_ble_apps.py').read_text()
  for expected in ('PORTABLE_RADIO_SESSION','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_QUICK_RADIOS','PortableRadioSession.h','LICENSE-Utilities.txt','LICENSE-FontAwesome.txt','Wrong target ABI'):
   self.assertIn(expected,source)
  pin=json.loads((ROOT/'sdk/ble-sources.json').read_text())['system_apps']
  self.assertIn(pin,(ROOT/'.github/workflows/ble-scanner.yml').read_text())

 def test_sensor_driver_pin_and_copied_contracts(self):
  pins=json.loads((ROOT/'sdk/ble-sources.json').read_text())
  self.assertEqual(pins['sensor_driver_minimum'],'0.1.0')
  self.assertIn(pins['sensor_driver_commit'],(ROOT/'.github/workflows/ble-scanner.yml').read_text())
  self.assertFalse((ROOT/'Apps/ble_scan_core.h').exists())
  self.assertIn('ble_sensor_names.h',(ROOT/'Apps/ble_scanner.c').read_text())
