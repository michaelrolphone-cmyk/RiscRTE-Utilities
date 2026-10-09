"""Explicit native-only utility profile; no activation in default builders."""
import argparse
import hashlib
import importlib.util
import io
import json
import subprocess
import tarfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
PROFILE=json.loads((ROOT/'Apps/native-utc-utilities.json').read_text())
SYSTEM=PROFILE['system_sha'];RUNTIME=PROFILE['runtime_sha']
BASE='dd366baa2fada5daa626e258b3b78e718648d6d9'
QUICK=['quick_actions.c','quick_render.c','quick_session.c']
TIME=['PortableNativeTimeSource.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
APPS=list(PROFILE['versions'])
IMPORTS={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','calloc','free','strcpy'}
EXPORTS={'app_main','app_module_init','app_module_fini'}
def run(cmd,**kw):return subprocess.run(list(map(str,cmd)),check=True,**kw)
def git(repo,*args):return subprocess.check_output(['git','-C',str(repo),*args],text=True).strip()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def exact(repo,pin):
 if git(repo,'rev-parse','HEAD')!=pin or git(repo,'status','--porcelain','--untracked-files=no'):raise ValueError('Clean exact source required: '+str(repo)+' @ '+pin)
def extract(repo,pin,destination,*paths):
 destination.mkdir(parents=True,exist_ok=True)
 data=subprocess.check_output(['git','-C',str(repo),'archive',pin,*paths])
 with tarfile.open(fileobj=io.BytesIO(data)) as tar:tar.extractall(destination,filter='data')
def stage(system,runtime,folder,out):
 include=folder/'include';include.mkdir(parents=True)
 for path in (system/'lib/PortableApps/include').iterdir():
  if path.is_file():(include/path.name).symlink_to(path)
 (folder/'time').symlink_to(system/'lib/PortableApps/time',target_is_directory=True)
 for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:
  path=include/name
  if path.exists():path.unlink()
  path.symlink_to(runtime/'sdk/app'/name)
 spec=importlib.util.spec_from_file_location('native_utility_alarm_sdk',system/'scripts/portable_alarm_build.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
 for name in helper.HEADERS:
  path=include/name
  if path.exists():path.unlink()
 helper.stage(argparse.Namespace(tagged_alarm_utilities=ROOT,alarm_client=True),argparse.ArgumentParser(),out,include)
 return include

def flags(name,native=True,paper=True):
 f=['-DPORTABLE_NOVA_UI','-DPORTABLE_ALARM_CLIENT']
 if paper:f+=['-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_QUICK_ACTIONS']
 else:f+=['-DPORTABLE_FORCE_FULL_FRAMES']
 if name in ('calculator','stopwatch','battery'):
  if paper:f+=['-DPORTABLE_PAPER_UTILITIES']
  if not native:f+=['-DPORTABLE_RTC_WALL_TIME' if paper else '-DPORTABLE_RTC_UTC8_DENVER']
  if name=='calculator':f+=['-DCALCULATOR_RETURN_APP="springboard.elf"']
  if name=='battery':f+=['-DPORTABLE_POWER_STATUS','-DBATTERY_RETURN_APP="springboard.elf"']
 else:
  f+=['-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RADIO_SESSION']
  if name.startswith('ble_'):f+=['-DPORTABLE_RETURN_APP="springboard.elf"']
  if name in ('ble_touchpad','ble_buttons'):f+=['-DPORTABLE_PAPER_HID']
  if not paper:f+=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS'] if name.startswith('ble_') else []
 if name=='waterfall':f+=['-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_APP_LAUNCH_GUARD','-DRF_RETURN_APP="springboard.elf"','-DRF_STORAGE_INSTANCE=8','-DRF_APP_DATA_INSTANCE=3'] if paper else ['-DRF_RETURN_APP="springboard.elf"']
 if native:
  f+=['-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR']
  if name=='stopwatch':f+=['-DPORTABLE_STOPWATCH_NATIVE_UTC','-DPORTABLE_APP_LAUNCH_GUARD']
 return f

def sources(name,system,native=True,paper=True,repo=ROOT):
 result=[repo/'Apps'/(name+'.c'),system/'lib/PortableApps/src/adapter.c']
 if paper or name.startswith('ble_'):result += [system/'lib/PortableApps/src'/n for n in QUICK]
 if not paper and name.startswith('ble_'):result += [system/'lib/PortableApps/src/quick_radios.c']
 if native:result += [system/'lib/PortableApps/src'/n for n in TIME]
 if name=='waterfall':result += [system/'lib/NativeApps/src/SingleFloatDivisionCompat.c']
 return result

def include_paths(system,headers,repo=ROOT):return [headers,system/'lib/NativeApps/include',repo/'lib/Bluetooth/include',repo/'Apps',system/'Apps']
def manifests(name,out):
 grants=PROFILE['common_grants']+PROFILE['app_grants'][name]
 assert len(grants)==len({(g['capability'],g['api'],g['instance_id']) for g in grants})
 pairs=list(dict.fromkeys((g['capability'],g['api']) for g in grants))
 assert ('alarm.service',2) in pairs and ('alarm.service',1) not in pairs and not any(c.startswith('rtc.') or c=='runtime.realtime.control' for c,v in pairs)
 manifest=dict(type='application',id=name,version=PROFILE['versions'][name],architecture='xtensa-esp32s3',file_name=name+'.elf',entry='app_main',requires=[dict(capability=c,api=v) for c,v in pairs])
 (out/(name+'.json')).write_text(json.dumps(manifest,indent=2)+'\n')
 (out/(name+'.boot-policy.json')).write_text(json.dumps(dict(manifest=name+'.json',grants=[{k:g[k] for k in ('capability','api','instance_id')} for g in grants]),indent=2)+'\n')
 return manifest
