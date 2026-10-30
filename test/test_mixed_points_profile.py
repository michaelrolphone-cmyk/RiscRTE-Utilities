import json,pathlib,subprocess,sys,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_native_schedules_alarm as builder
class MixedProfile(unittest.TestCase):
 def test_explicit_selection(self):
  with self.assertRaisesRegex(ValueError,'requires clock-edit-authority'):builder.selection(False,True)
  old=builder.selection(True)
  self.assertIn('ALARM_NATIVE_SCHEDULES_ONLY',old)
  self.assertNotIn('POINTS_IN_TIME_SERVICE',old)
  mixed=builder.selection(True,True)
  self.assertNotIn('ALARM_NATIVE_SCHEDULES_ONLY',mixed)
  self.assertEqual(set(mixed)-set(old),{'POINTS_IN_TIME_SERVICE','POINTS_SERVICE_CONTEXT','ALARM_NATIVE_MIXED_POINTS'})
  self.assertEqual(set(old)-set(mixed),{'ALARM_NATIVE_SCHEDULES_ONLY'})
 def test_owning_policy_has_only_required_bindings(self):
  manifest,policy=builder.profile_files(True,True)
  m=json.loads(manifest.read_text());p=json.loads(policy.read_text())
  old=json.loads(builder.profile_files(True)[1].read_text())
  self.assertEqual({k:p[k] for k in old},old)
  self.assertEqual(set(p)-set(old),{'points_utc_cfg','points_utc_occ','time_zone'})
  self.assertEqual(p['points_utc_cfg'],{'namespace':5,'access':'read'})
  self.assertEqual(p['points_utc_occ'],{'namespace':4,'access':'read-write'})
  self.assertEqual(p['time_zone'],{'namespace':1,'access':'read'})
  self.assertEqual(m['provides'],[{'capability':'alarm.service','api':2}])
  self.assertEqual(m['requires'],json.loads(builder.profile_files(True)[0].read_text())['requires'])
  self.assertEqual(m['physical_verification'],'pending')
 def test_unselected_profiles_stay_distinct(self):
  self.assertEqual(builder.profile_files(False)[0].name,'native-utc-schedules-manifest.json')
  self.assertEqual(builder.profile_files(True)[0].name,'native-utc-clock-edit-manifest.json')
 def test_cli_rejects_ambiguous_selection_before_source_access(self):
  args=[sys.executable,str(ROOT/'scripts/build_native_schedules_alarm.py'),'--include-points','--runtime','/missing','--system-apps','/missing','--output','/missing','--cc','/missing']
  result=subprocess.run(args,capture_output=True,text=True)
  self.assertEqual(result.returncode,2)
  self.assertIn('include-points requires clock-edit-authority',result.stderr)
  self.assertNotIn('Traceback',result.stderr)
if __name__=='__main__':unittest.main()
