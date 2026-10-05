"""Independent LoRa inventory/pin/ABI regression checks."""
import importlib.util,json,re,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_lora_apps
class LoRaPackage(unittest.TestCase):
 def test_inventory(self):
  rows=build_lora_apps.inventory();self.assertEqual(len(rows),1);self.assertEqual(rows[0]['id'],'lora_messages');self.assertEqual(rows[0]['version'],'0.1.0')
 def test_source_identity(self):
  source=(ROOT/'Apps/lora_messages.c').read_text();self.assertIn('#include "PortableWatchKeyboard.h"',source);self.assertIn('#include "PortableTimeFormat.h"',source)
  self.assertNotIn('T5LoRaApi',source);self.assertNotIn('915000000',source)
 def test_source_pins(self):
  pins=json.loads((ROOT/'sdk/lora-sources.json').read_text())
  for key in ('system_apps','watch_radio_abi'):self.assertRegex(pins[key],r'^[0-9a-f]{40}$')
  self.assertIn('ref: '+pins['system_apps'],(ROOT/'.github/workflows/build.yml').read_text())
 def test_existing_radio_abi(self):
  header=(ROOT/'Apps/PortableLoRaV2.h').read_text();self.assertIn('#define TWATCH_RADIO_API_V1 2u',header)
  self.assertIn('sizeof(twatch_radio_api_v2)==36',header);self.assertIn('sizeof(twatch_lora_config_v2)==16',header);self.assertIn('sizeof(twatch_lora_status_v2)==8',header)
 def test_reader_utility_stays_independent(self):
  old=json.loads((ROOT/'Apps/lora.json').read_text());self.assertEqual(old['version'],'1.0.1');self.assertEqual(old['min_firmware_version'],'1.1.15')
if __name__=='__main__':unittest.main()
