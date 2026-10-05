import json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from build_ble_apps import inventory
class BleInventory(unittest.TestCase):
 def test_scanner_identity_and_capabilities(self):
  rows=inventory();self.assertEqual([a['id'] for a in rows],['ble_scanner'])
  side=json.loads((ROOT/'Apps/ble_scanner.json').read_text())
  self.assertEqual(side['version'],'0.1.0');self.assertEqual(side['icon'],'solid:f7c0')
  self.assertEqual({r['capability'] for r in side['requires']},{'display.output','input.touch.raw','bluetooth.hci','alarm.service','storage.key-value'})
  self.assertEqual({r['capability'] for r in side['optional']},{'rtc.clock','net.wifi'})
 def test_no_new_keyboard_or_settings_pages(self):
  source=(ROOT/'Apps/ble_scanner.c').read_text()
  for expected in ('take_touch_swipe','detail_scroll','PORTABLE_RADIO_AIRPLANE','portable_radio_suspend','retain()','scrollbar'):
   self.assertIn(expected,source)
  self.assertNotIn('keyboard',source.lower())
 def test_pinned_radio_hooks_and_notices(self):
  source=(ROOT/'scripts/build_ble_apps.py').read_text()
  for expected in ('PORTABLE_RADIO_SESSION','PORTABLE_QUICK_RADIOS','PortableRadioSession.h','LICENSE-Utilities.txt','LICENSE-FontAwesome.txt','Wrong target ABI'):
   self.assertIn(expected,source)
  pin=json.loads((ROOT/'sdk/ble-sources.json').read_text())['system_apps']
  self.assertIn(pin,(ROOT/'.github/workflows/ble-scanner.yml').read_text())
