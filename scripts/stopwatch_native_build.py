"""Independent Stopwatch qualification composition. Product recipes stay pinned."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
QUALIFICATION_VERSION='0.1.26'
COMMON=['PORTABLE_STOPWATCH_NATIVE_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_NOVA_UI','PORTABLE_FORCE_FULL_FRAMES','PORTABLE_ALARM_CLIENT','PORTABLE_INPUT_NAVIGATION','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_NATIVE','PORTABLE_UNPADDED_HOURS','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_STAGE_LOGS','PORTABLE_RETURN_APP="springboard.elf"']
UTC=['PORTABLE_STOPWATCH_NATIVE_UTC','PORTABLE_NATIVE_TIME_TOOLBAR','PORTABLE_APP_LAUNCH_GUARD','ALARM_SERVICE_TAGGED_V2']
TIME=['PortableNativeTimeSource.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
def flags(utc=False,snapshot=False):
 return ['-D'+f for f in COMMON+(UTC if utc else [])+(['PORTABLE_RASTER_SNAPSHOT','PORTABLE_RETAINED_RGB565_HANDOFF'] if snapshot else [])]
def stage(system,runtime,out,utc):
 headers=out/'include';headers.mkdir(parents=True,exist_ok=True)
 for h in (system/'lib/PortableApps/include').iterdir():
  if h.is_file():(headers/h.name).write_bytes(h.read_bytes())
 for h in (system/'lib/PortableApps/time').rglob('*'):
  if h.is_file():
   d=out/'time'/h.relative_to(system/'lib/PortableApps/time');d.parent.mkdir(parents=True,exist_ok=True);d.write_bytes(h.read_bytes())
 for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):(headers/name).write_bytes((runtime/'sdk/app'/name).read_bytes())
 if utc:
  for name in ('AlarmServiceV1.h','AlarmServiceV2.h'):(headers/name).write_bytes((ROOT/'lib/Alarm/include'/name).read_bytes())
 return headers

def source_paths(system,utc):
 return [ROOT/'Apps/stopwatch.c',*[system/'lib/PortableApps/src'/n for n in ['adapter.c','quick_actions.c','quick_render.c','quick_session.c']+(TIME if utc else [])]]
def git(repo,*args):return subprocess.check_output(['git','-C',str(repo),*args],text=True).strip()
def exact(repo,commit):
 if git(repo,'rev-parse','HEAD')!=commit or git(repo,'status','--porcelain'):raise ValueError('Exact clean source required: '+str(repo)+' @ '+commit)
def profile(utc):
 p=json.loads((ROOT/'profiles/stopwatch-native-qualification.json').read_text());key='utc' if utc else 'rtc';app=p['applications'][key]
 if p['schema']!=1 or p['publish_to_catalog'] is not False:raise ValueError('Qualification-only profile required')
 if app['id']!='stopwatch' or app['version']!=QUALIFICATION_VERSION:raise ValueError('Stopwatch identity/version mismatch')
 required=[('display.output',1,0),('input.touch.raw',1,0),('input.navigation',1,0),('board.battery',1,0),('alarm.service',2 if utc else 1,0),('runtime.realtime' if utc else 'rtc.clock',1 if utc else 2,0),('storage.key-value',1,2),('storage.key-value',1,1)]
 got=[(x['capability'],x['api'],x['instance_id']) for x in app['grants']]
 if sorted(got)!=sorted(required):raise ValueError('Stopwatch exact profile grants differ from compiled controller')
 return app

def hashes(root):
 paths=git(root,'ls-files').splitlines();return {n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in paths if n.startswith(('Apps/','lib/PortableApps/','lib/Alarm/','sdk/','test/','tests/','scripts/','profiles/')) and (root/n).is_file()}
