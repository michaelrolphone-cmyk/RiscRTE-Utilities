"""Padding removal preserves actionable bytes/order and refuses bad DT_RELASZ."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('normalize',Path(__file__).resolve().parents[1]/'scripts/normalize_xtensa_relocations.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class NormalizeTest(unittest.TestCase):
 def test_middle_tail_and_idempotence(self):
  for wrong in (False,True):
   data=bytearray(512);ident=b'\x7fELF\x01\x01\x01'+bytes(9)
   struct.pack_into('<16sHHIIIIIHHHHHH',data,0,ident,3,94,1,0,0,64,0,52,0,0,40,3,0)
   records=[struct.pack('<III',0x1000,2,7),bytes(12),struct.pack('<III',0x1004,3,9),bytes(12)]
   struct.pack_into('<10I',data,104,0,4,0,0x2000,256,48,0,0,4,12)
   struct.pack_into('<10I',data,144,0,6,0,0x3000,320,16,0,0,4,8)
   data[256:304]=b''.join(records);struct.pack_into('<IIII',data,320,7,0x2000,8,47 if wrong else 48)
   with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'sample.elf';p.write_bytes(data)
    if wrong:
     with self.assertRaises(ValueError):module.normalize(p)
     self.assertEqual(p.read_bytes(),data)
    else:
     self.assertEqual(module.normalize(p),2);actual=p.read_bytes()
     self.assertEqual(actual[256:280],records[0]+records[2]);self.assertEqual(actual[280:304],bytes(24))
     self.assertEqual(struct.unpack_from('<I',actual,124)[0],24);self.assertEqual(struct.unpack_from('<I',actual,332)[0],24)
     self.assertEqual(module.normalize(p),0)
if __name__=='__main__':unittest.main()
