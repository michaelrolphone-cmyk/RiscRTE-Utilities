import argparse
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import native_paper_motion as motion
import native_utility_build as utility


class NativePaperMotion(unittest.TestCase):
 def test_selection_is_explicit_and_paired(self):
  parser=argparse.ArgumentParser();motion.options(parser)
  self.assertEqual(motion.select(parser.parse_args([]),parser,ROOT),ROOT)
  for args in (['--paper-transitions'],['--motion-system',str(ROOT)]):
   with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
    motion.select(parser.parse_args(args),parser,ROOT)
  with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
   motion.select(parser.parse_args(['--paper-transitions','--motion-system',str(ROOT)]),parser,ROOT)

 def test_watch_and_legacy_profiles_keep_original_pins(self):
  for name in ('Apps/native-utc-utilities.json','Apps/native-utc-alarms.json'):
   expected=subprocess.check_output(['git','show','45cffd33f6d258017cf784ed6b4534654c221736:'+name],cwd=ROOT)
   self.assertEqual((ROOT/name).read_bytes(),expected)
  for name in utility.APPS:
   self.assertNotIn(motion.DEFINE,utility.flags(name))
   for native,paper in ((False,False),(False,True),(True,False)):
    with self.assertRaises(ValueError):utility.flags(name,native,paper,paper_transitions=True)

 def test_motion_keeps_guard_and_exact_authority(self):
  with tempfile.TemporaryDirectory() as directory:
   out=Path(directory)
   for name in utility.APPS:
    old=utility.manifests(name,out);new=utility.manifests(name,out,True)
    self.assertEqual({k:v for k,v in old.items() if k!='version'},
                     {k:v for k,v in new.items() if k!='version'})
    self.assertNotEqual(old['version'],new['version'])
    flags=utility.flags(name,paper_transitions=True)
    self.assertIn(motion.DEFINE,flags)
    self.assertIn('-DPORTABLE_NATIVE_CUSTODY_FENCE',flags)
    self.assertNotIn('-DPORTABLE_PAPER_CROSSFADE',flags)
    if name in ('stopwatch','waterfall'):self.assertIn('-DPORTABLE_APP_LAUNCH_GUARD',flags)


if __name__=='__main__':unittest.main()
