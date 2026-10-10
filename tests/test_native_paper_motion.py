import argparse
import contextlib
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import native_paper_motion as motion
import native_utility_build as utility

# Exact historical bytes from 86f10c953bc51b0977d2b517d7565f24fd07cb0d.
# See fixtures/native-paper-motion/README.md for source and blob provenance.
# Keep witnesses in-tree so custody checks work without historical Git objects.
LEGACY_PROFILE_SHA256={
 'native-utc-utilities.json':'87ac5ab88ff2a8bf60f94ff6331b16f843247310ea28cc12db1dc17350d2f3fc',
 'native-utc-alarms.json':'ab6d0716a9a49d4891c13c4d0ec80c9a3d4bad372d04784a2e7bbf37ffc0815b',
}


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
  for name,digest in LEGACY_PROFILE_SHA256.items():
   with self.subTest(profile=name):
    expected=(ROOT/'tests/fixtures/native-paper-motion'/name).read_bytes()
    self.assertEqual(hashlib.sha256(expected).hexdigest(),digest,'Historical profile witness changed')
    self.assertEqual((ROOT/'Apps'/name).read_bytes(),expected,'Preserved profile changed')
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
