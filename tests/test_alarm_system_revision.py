"""Selected System is explicit, immutable and clean; historical default stays."""
import argparse,inspect,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import build_badge_native_alarm_apps as apps
import build_native_schedules_alarm as service
class AlarmSystemRevision(unittest.TestCase):
 def test_default_unchanged(self):
  historical='6e4cda9072905c15b9a5ebc9b22913fec444a89d'
  self.assertEqual(apps.PROFILE['system_sha'],historical)
  self.assertEqual(service.SYSTEM,historical)
  self.assertEqual(inspect.signature(apps.stage).parameters['system_revision'].default,historical)
 def test_full_revision_only(self):
  for module in (apps,service):
   self.assertEqual(module.revision('a'*40),'a'*40)
   for value in ('HEAD','main','9c2a049','a'*39,'a'*41,'A'*40,'g'*40,'a'*40+'\n'):
    with self.assertRaises(argparse.ArgumentTypeError):module.revision(value)
 def test_exact_selected_clean_checkout(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)
   def git(*args):return subprocess.check_output(['git','-C',d,*args],text=True).strip()
   git('init','-q');git('config','user.name','Fixture');git('config','user.email','fixture@example.invalid')
   tracked=root/'tracked';tracked.write_text('original');git('add','.');git('commit','-qm','fixture');pin=git('rev-parse','HEAD')
   for module in (apps,service):
    module.exact(root,pin)
    with self.assertRaises(ValueError):module.exact(root,'0'*40)
   tracked.write_text('dirty')
   for module in (apps,service):
    with self.assertRaises(ValueError):module.exact(root,pin)
   git('checkout','--','tracked');(root/'untracked').write_text('untracked')
   for module in (apps,service):
    with self.assertRaises(ValueError):module.exact(root,pin)
if __name__=='__main__':unittest.main()
