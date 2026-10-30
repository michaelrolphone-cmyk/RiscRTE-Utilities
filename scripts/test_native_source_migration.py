"""Exercise the actual target recipe's source guard without requiring target tools."""
import ast,hashlib,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class MigrationSource(unittest.TestCase):
 def setUp(self):
  source=ROOT/'scripts/build_native_schedules_alarm.py';tree=ast.parse(source.read_text())
  nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ('sha','qualify_native_migration_source') or isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='NATIVE_IO_RETRY_SOURCE_SHA256' for t in n.targets)]
  self.namespace={'Path':Path,'hashlib':hashlib};exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),self.namespace)
 def test_exact_reviewed_source(self):self.namespace['qualify_native_migration_source'](ROOT/'Services/alarm_service/service.c')
 def test_future_delta_refused(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp)/'service.c';p.write_bytes((ROOT/'Services/alarm_service/service.c').read_bytes()+b'\n/* unreviewed */\n')
   with self.assertRaisesRegex(ValueError,'Unqualified native-points source migration'):self.namespace['qualify_native_migration_source'](p)
if __name__=='__main__':unittest.main()
