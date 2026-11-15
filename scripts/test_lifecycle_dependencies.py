import os,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from lifecycle_dependencies import Closure
class DependencyClosure(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.top=Path(self.tmp.name)
  self.root=self.top/'repo';self.root.mkdir();self.source=self.root/'source.cpp'
  self.source.write_text('#include "masked.h"\nint x;\n')
  (self.root/'masked.h').write_text('#pragma GCC system_header\n#include "child.h"\n')
  (self.root/'child.h').write_text('typedef int pinned;\n')
  self.closure=Closure([('repo',self.root)],self.top/'receipt.json')
  self.command=['c++','-std=c++17','-I'+str(self.root),str(self.source),'-o',str(self.top/'out')]
 def test_masked_descendant_pinned(self):
  self.closure.record(self.command);self.assertIn('repo/child.h',self.closure.inputs);self.assertEqual(len(self.closure.units),1)
 def test_external_and_symlink_refused(self):
  external=self.top/'external.h';external.write_text('#pragma GCC system_header\n')
  for symlink in (False,True):
   h=self.root/'child.h';h.unlink(missing_ok=True)
   if symlink:h.symlink_to(external)
   else:h.write_text('#include "'+str(external)+'"\n')
   with self.assertRaisesRegex(ValueError,'Undeclared external'):self.closure.record(self.command)
 def test_environment_refused(self):
  for key in ('CPATH','CPLUS_INCLUDE_PATH','C_INCLUDE_PATH','OBJC_INCLUDE_PATH','GCC_EXEC_PREFIX','COMPILER_PATH','DEPENDENCIES_OUTPUT','SUNPRO_DEPENDENCIES'):
   with patch.dict(os.environ,{key:''}),self.assertRaisesRegex(ValueError,'Unqualified compiler environment'):self.closure.record(self.command)
 def test_standard_separate_repository_first(self):
  self.source.write_text('#include <stdio.h>\n')
  self.closure.record(self.command);self.assertIn('repo/source.cpp',self.closure.inputs);self.assertTrue(self.closure.system_inputs)
 def test_changed_input_refused(self):
  self.closure.record(self.command);(self.root/'child.h').write_text('typedef long pinned;\n')
  with self.assertRaisesRegex(ValueError,'Input changed'):self.closure.record(self.command)
 def test_generated_requires_exact_source(self):
  generated=self.top/'sdk';generated.mkdir();h=generated/'copy.h';h.write_text('typedef int copy;\n')
  self.source.write_text('#include "copy.h"\n');self.command.insert(1,'-I'+str(generated))
  self.closure.roots.append(('generated-sdk',generated))
  with self.assertRaisesRegex(ValueError,'no exact declared source'):self.closure.record(self.command)
  (self.root/'original').mkdir();(self.root/'original/copy.h').write_bytes(h.read_bytes());self.closure.record(self.command)
  self.assertEqual(self.closure.generated_origins['generated-sdk/copy.h'],['repo/original/copy.h'])
 def test_every_tu_and_pic_prefix(self):
  second=self.root/'second.c';second.write_text('#if !__PIC__\n#error PIC required\n#endif\n')
  self.closure.record(['cc','-std=c11','-fPIC','-fvisibility=hidden','-shared',str(second),'-o',str(self.top/'obj')]);self.closure.record(self.command)
  self.assertEqual(len(self.closure.units),2);self.assertIn('-fPIC',self.closure.units[0]['compile_prefix'])
if __name__=='__main__':unittest.main()
