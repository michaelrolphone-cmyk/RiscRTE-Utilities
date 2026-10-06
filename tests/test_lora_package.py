"""Independent LoRa inventory/pin/ABI regression checks."""
import importlib.util,json,re,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_lora_apps
class LoRaPackage(unittest.TestCase):
 def test_inventory(self):
  rows=build_lora_apps.inventory();self.assertEqual(len(rows),1);self.assertEqual(rows[0]['id'],'lora_messages');self.assertEqual(rows[0]['version'],'0.1.2')
 def test_source_identity(self):
  source=(ROOT/'Apps/lora_messages.c').read_text();self.assertIn('#include "PortableWatchKeyboard.h"',source);self.assertIn('#include "PortableTimeFormat.h"',source)
  self.assertNotIn('T5LoRaApi',source);self.assertNotIn('915000000',source)
 def test_source_pins(self):
  pins=json.loads((ROOT/'sdk/lora-sources.json').read_text())
  for key in ('system_apps','watch_radio_abi'):self.assertRegex(pins[key],r'^[0-9a-f]{40}$')
  self.assertIn('ref: '+pins['system_apps'],(ROOT/'.github/workflows/build.yml').read_text());self.assertIn('ref: '+pins['watch_radio_abi'],(ROOT/'.github/workflows/build.yml').read_text())
 def test_existing_radio_abi(self):
  header=(ROOT/'Apps/PortableLoRaV2.h').read_text();self.assertIn('#define TWATCH_RADIO_API_V1 2u',header)
  self.assertIn('sizeof(twatch_radio_api_v2)==44',header);self.assertIn('offsetof(twatch_radio_api_v2,profile_info)==36',header);self.assertIn('sizeof(twatch_lora_config_v2)==16',header);self.assertIn('sizeof(twatch_lora_status_v2)==8',header)
 def test_reader_utility_stays_independent(self):
  old=json.loads((ROOT/'Apps/lora.json').read_text());self.assertEqual(old['version'],'1.0.1');self.assertEqual(old['min_firmware_version'],'1.1.15')

class LoRaAbiPin(unittest.TestCase):
 def setUp(self):
  import tempfile
  self.temp=tempfile.TemporaryDirectory()
  self.addCleanup(self.temp.cleanup)
  self.watch=Path(self.temp.name);(self.watch/'include').mkdir()
  consumer=(ROOT/'Apps/PortableLoRaV2.h').read_text()
  self.declared=consumer[consumer.index('#define TWATCH_RADIO_API_V1'):consumer.index('_Static_assert(sizeof(twatch_lora_config_v2)')]
  (self.watch/'include/twatch_caps.h').write_text(self.declared+'/* Speaker and microphone */\n')
  self.pin=json.loads((ROOT/'sdk/lora-sources.json').read_text())['watch_radio_abi']
 def check(self,sha,dirty=''):
  from unittest.mock import patch
  import check_lora_abi
  with patch.object(check_lora_abi.subprocess,'check_output',side_effect=[sha+'\n',dirty]):check_lora_abi.verify(self.watch)
 def test_exact_clean_declarations(self):self.check(self.pin)
 def test_rejects_wrong_source(self):
  with self.assertRaises(ValueError):self.check('f'*40)
 def test_rejects_dirty_source(self):
  with self.assertRaises(ValueError):self.check(self.pin,' M include/twatch_caps.h\n')
 def test_rejects_changed_declaration(self):
  path=self.watch/'include/twatch_caps.h';path.write_text(path.read_text().replace('profile_info','different_info'))
  with self.assertRaises(ValueError):self.check(self.pin)

if __name__=='__main__':unittest.main()
