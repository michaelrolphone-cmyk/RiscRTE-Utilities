import json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from build_badge_native_alarm_apps import FLAGS,PROFILE,exact
class BadgeNativeAlarmProfile(unittest.TestCase):
 def test_tagged_native_only(self):
  for flag in ['-DALARM_NATIVE_UTC','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_APP_LAUNCH_GUARD','-DPORTABLE_SAFE_VIEWPORT','-DPORTABLE_RETURN_APP="default.elf"']:
   self.assertIn(flag,FLAGS)
  for part in ['RTC_WALL','RTC_UTC8','QUICK_ACTIONS','POINTS_IN_TIME','BLE_BROADCAST','FOREGROUND_IDLE']:
   self.assertFalse(any(part in f for f in FLAGS))
 def test_canonical_private_namespaces(self):
  for app,key in [('alarms','alarm_utc_cfg'),('countdown','timer_utc_cfg')]:
   self.assertEqual(PROFILE['app_grants'][app],[{'capability':'storage.key-value','api':1,'instance_id':3,'keys':[key]}])
  self.assertEqual([g['instance_id'] for g in PROFILE['common_grants'] if g['capability']=='storage.key-value'],[1])
  self.assertIn({'capability':'alarm.service','api':2,'instance_id':0},PROFILE['common_grants'])
  self.assertFalse(PROFILE['sleeping_alarm_integration']);self.assertFalse(PROFILE['hardware_verified'])
 def test_incorrect_pin_refused(self):
  with self.assertRaises(ValueError):exact(ROOT,'0'*40)
 def test_legacy_profile_unchanged(self):
  legacy=json.loads((ROOT/'Apps/native-utc-alarms.json').read_text())
  self.assertEqual(legacy['profile'],'native-utc-alarms-paper-nova')
  self.assertEqual(legacy['alarm_service_version'],'0.4.4')

class BadgeForegroundSelection(unittest.TestCase):
 def test_helper_without_explicit_alarm_selection_refused(self):
  import tempfile
  from unittest.mock import patch
  import build_badge_native_alarm_apps as builder
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);out=root/'output'
   args=['builder','--runtime','unused','--system-apps','unused','--output',str(out),'--cc','compiler','--foreground-idle-helper-dir','unused']
   def compiler(command,**kwargs):
    return '15.2.0\n' if '-dumpfullversion' in command else 'xtensa (esp-15.2.0_20251204)\n'
   with patch.object(sys,'argv',args),patch.object(builder.subprocess,'check_output',side_effect=compiler),patch.object(builder,'stage',return_value=(root,{})):
    with self.assertRaises(SystemExit) as e:builder.main()
    self.assertEqual(e.exception.code,2)

if __name__=='__main__':unittest.main()
