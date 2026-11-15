import argparse
import unittest
import tempfile
from pathlib import Path
from unittest.mock import patch
import build_native_schedules_alarm as build

class RevisionSelection(unittest.TestCase):
 def test_historical_source_bytes(self):
  source=build.ROOT/'test/provider_lifecycle/historical_alarm_service.c'
  self.assertEqual(len(build.historical_source(source)), 58629)
  with tempfile.TemporaryDirectory() as tmp:
   with self.assertRaises(FileNotFoundError):build.historical_source(Path(tmp)/'missing.c')
   changed=Path(tmp)/'changed.c';changed.write_bytes(source.read_bytes()+b'\n')
   with self.assertRaisesRegex(ValueError, 'Historical service witness mismatch'):build.historical_source(changed)
 def test_historical_default_preserved(self):
  with patch.object(build, 'exact') as exact, patch.object(build.runtime_sdk_compat, 'verify', return_value={'ok': True}) as verify:
   selected, receipt = build.runtime_selection('system', 'runtime')
   self.assertEqual(selected, build.RUNTIME)
   exact.assert_not_called()
   verify.assert_called_once_with('system', 'runtime', build.RUNTIME)
 def test_explicit_pin_checked_and_reported(self):
  pin = 'a' * 40
  with patch.object(build, 'exact') as exact, patch.object(build.runtime_sdk_compat, 'verify', return_value={'ok': True}) as verify:
   selected, receipt = build.runtime_selection('system', 'runtime', pin)
   self.assertEqual(selected, pin)
   exact.assert_called_once_with('runtime', pin)
   verify.assert_called_once_with('system', 'runtime', pin)
 def test_wrong_or_dirty_checkout_refused_before_compat(self):
  with patch.object(build, 'exact', side_effect=ValueError('not exact')), patch.object(build.runtime_sdk_compat, 'verify') as verify:
   with self.assertRaises(ValueError): build.runtime_selection('system', 'runtime', 'a' * 40)
   verify.assert_not_called()
 def test_invalid_revision_rejected(self):
  for value in ['main', 'A' * 40, 'a' * 39, 'a' * 41, '']:
   with self.assertRaises(argparse.ArgumentTypeError): build.revision(value)
 def test_exact_checks_head_and_dirty_state(self):
  for output in [['b' * 40], ['a' * 40, ' M file']]:
   with patch.object(build, 'run', side_effect=output):
    with self.assertRaises(ValueError): build.exact('runtime', 'a' * 40)
  with patch.object(build, 'run', side_effect=['a' * 40, '']): build.exact('runtime', 'a' * 40)

if __name__ == '__main__': unittest.main()
