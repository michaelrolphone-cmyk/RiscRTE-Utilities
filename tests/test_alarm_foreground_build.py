import unittest
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from build_badge_native_alarm_apps import argument_parser,PROFILE,FLAGS
class AlarmForegroundBuild(unittest.TestCase):
    def args(self,*args):
        return argument_parser().parse_args(['--runtime','runtime','--system-apps','system','--output','out','--cc','cc',*args])
    def test_omission_preserves_historical_recipe(self):
        a=self.args()
        self.assertFalse(a.foreground_alarm_sleep)
        self.assertIsNone(a.foreground_idle_helper_dir)
        self.assertEqual(a.system_revision,PROFILE['system_sha'])
        self.assertNotIn('-DPORTABLE_FOREGROUND_ALARM_SLEEP',FLAGS)
    def test_explicit_helper_selection(self):
        a=self.args('--foreground-alarm-sleep','--foreground-idle-helper-dir','helper',
                    '--foreground-idle-runtime-dir','runtime','--foreground-idle-drivers-sdk-dir','sdk',
                    '--foreground-idle-fallback-seconds','15')
        self.assertTrue(a.foreground_alarm_sleep)
        self.assertEqual(a.foreground_idle_helper_dir,Path('helper'))
        self.assertEqual(a.foreground_idle_fallback_seconds,15)
