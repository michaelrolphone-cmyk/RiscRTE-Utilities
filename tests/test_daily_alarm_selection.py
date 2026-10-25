import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_portable_apps as builder

class DailyAlarmSelection(unittest.TestCase):
    def test_defaults_keep_historical_selection(self):
        args=builder.argument_parser().parse_args(['--system-apps','unused'])
        self.assertFalse(args.foreground_alarm_sleep)
        self.assertFalse(args.alarm_client)
        self.assertIsNone(args.tagged_alarm_utilities)
        self.assertEqual(args.system_apps_revision,builder.PIN)

    def test_partial_alarm_selection_fails_before_source_access(self):
        for fields in ({'foreground_alarm_sleep':True}, {'alarm_client':True},
                       {'tagged_alarm_utilities':Path('unused')},
                       {'foreground_alarm_sleep':True,'alarm_client':True}):
            with patch.object(builder,'verify_system',side_effect=AssertionError('source accessed')):
                with self.assertRaises(ValueError):builder.build(Path('unused'),**fields)

    def test_explicit_complete_cli(self):
        args=builder.argument_parser().parse_args(['--system-apps','unused',
            '--foreground-alarm-sleep','--alarm-client','--tagged-alarm-utilities','canonical',
            '--native-safe-content','--safe-viewport','--navigation'])
        self.assertTrue(args.foreground_alarm_sleep and args.alarm_client and args.native_safe_content)
        self.assertEqual(args.tagged_alarm_utilities,Path('canonical'))
