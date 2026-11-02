"""Validate repeat-built generic Home packages; no authority or firmware mutation."""
import hashlib,json,re
from pathlib import Path
LOCK={"schema":1,"extraction":{"repository":"michaelrolphone-cmyk/RiscRTE-T-Watch-S3","commit":"851b8998e804c57b2c9f42d9f8fd0a69e79f2b41"},"system":{"repository":"michaelrolphone-cmyk/RiscRTE-System-Apps","commit":"bb0cadf3cec3445a64e11513b47b2676f3fdd92d"},"utilities":{"repository":"michaelrolphone-cmyk/RiscRTE-Utilities","commit":"4fdcad2495c33fe63f5f6faca9b11ced3c739a5b"},"generic_power_providers":{"repository":"michaelrolphone-cmyk/RiscRTE-Drivers","commit":"422e1e81a498fb6bdab9abf086325afc05a532dc","version":"0.1.0"},"compiler":"xtensa-esp32s3-elf-gcc 15.2.0 esp-15.2.0_20251204"}
BASE_DEFINES=["-DWATCH_RESIDENT_HOST","-DWATCH_COMPACT_NOVA_ASSETS","-DWATCH_CRASH_REPORT","-DCRASH_REPORT_SESSION_FILES=1","-DWATCH_CRASH_REPORT_NAMESPACE=61","-DWATCH_SHARED_PREFERENCE_HEADERS","-DPORTABLE_PREFERENCE_STORE","-DWATCH_ATOMIC_PREFERENCES","-DPORTABLE_QUICK_ATOMIC_PREFERENCES","-DPORTABLE_QUICK_RADIOS","-DWATCH_COMPRESSED_CALENDAR_ALPHA","-DWATCH_COMPRESSED_FACE_ALPHA","-DWATCH_FAST_WAKE","-DWATCH_COLD_BOOT_ANIMATION","-DPORTABLE_WATCH_NATIVE_HEIGHT","-DPORTABLE_LOW_BATTERY","-DWATCH_RUNTIME_FEATURES","-DWATCH_BLE_BROADCAST","-DWATCH_CLOCK_LAUNCHER","-DWATCH_CLOCK_ALARMS","-DWATCH_CLOCK_POINTS","-DPORTABLE_RTC_UTC8_DENVER","-DWATCH_PAIRED_BOOT_CONFIRM","-DWATCH_QUICK_ACTIONS","-DWATCH_QUICK_RADIOS","-DWATCH_MOTION_WAKE","-DWATCH_ALARM_SLEEP_RESUME","-DWATCH_CONTEXTS_CLIENT","-flto","-Wl,--undefined=__udivdi3","-Wl,--undefined=__umoddi3","-Wl,--undefined=__divdi3","-Wl,--undefined=__moddi3"]
VERSIONS={'watch':'0.25.1','wave':'0.25.2'}
def sha(raw):return hashlib.sha256(raw).hexdigest()
def record(raw):return {'bytes':len(raw),'sha256':sha(raw)}
def require(ok,message):
 if not ok:raise ValueError(message)
def digest_map(value,label):
 require(isinstance(value,dict) and bool(value),label+' missing')
 require(all(isinstance(k,str) and k and not k.startswith('/') and '..' not in k.split('/') and isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) for k,v in value.items()),label+' malformed')
def validate_home_pair(first,repeat,baseline,lane):
 """Inputs are dicts of default.elf/default.json/build.json bytes."""
 require(lane in VERSIONS,'Home lane unsupported')
 old=json.loads(baseline);receipts=[]
 for package in (first,repeat):
  require(set(package)=={'default.elf','default.json','build.json'},'Home package members')
  elf=package['default.elf'];manifest=json.loads(package['default.json']);b=json.loads(package['build.json'])
  require(len(elf)>=52 and elf[:7]==b'\x7fELF\x01\x01\x01' and elf[16:20]==b'\x03\x00\x5e\x00','Home target ELF')
  require(manifest.get('version')==VERSIONS[lane],'Home version/profile')
  require({k:v for k,v in manifest.items() if k!='version'}=={k:v for k,v in old.items() if k!='version'},'Home manifest authority changed')
  require(b.get('schema')==1 and b.get('build_variant')=='shared-no-board-defines' and b.get('preferences')=='runtime-provider','Home build identity')
  require(b.get('automatic_provision') is True and b.get('serial_transfer_mode',False) is (lane=='wave'),'Home selected feature profile')
  require(b.get('hardware_profile_inputs') is False and b.get('hardware_verified') is False,'Home proof scope')
  require(b.get('dependency_lock')==LOCK,'Home dependency lock')
  compiler=b.get('compiler','')
  require('15.2.0' in compiler and 'esp-15.2.0_20251204' in compiler,'Home compiler identity')
  require(re.fullmatch('[0-9a-f]{64}',b.get('compiler_sha256','')) is not None,'Home compiler custody')
  verified=b.get('verified_dependencies',{})
  require(set(verified)=={'system','utilities'},'Home dependency roles')
  for role in ('system','utilities'):
   require(verified[role].get('commit')==LOCK[role]['commit'],'Home verified dependency revision')
   digest_map(verified[role].get('files_sha256'),role+' verified inputs')
  digest_map(b.get('home_files_sha256'),'Home source inputs')
  for path in ('dependencies.lock.json','scripts/build.py','apps/clock/clock_resident.inc','apps/clock/clock_alarm.inc','sdk/app/RiscResidentAlarmV1.h'):
   require(path in b['home_files_sha256'],'Home alarm input not bound: '+path)
  a=b.get('artifacts',{}).get('default',{})
  require(a.get('role')=='home' and a.get('file_name')=='default.elf','Home descriptor role')
  require(a.get('bytes')==len(elf) and a.get('sha256')==sha(elf),'Home artifact receipt')
  require(a.get('exports')==['app_main','risc_resident_app_descriptor_v1'],'Home exports')
  allowed={'risc_runtime_get_api','memcpy','memset','memcmp','memmove','strcmp','strncmp','strlen','snprintf','malloc','calloc','free','strcpy','memchr','strrchr'}
  require(isinstance(a.get('imports'),list) and all(isinstance(x,str) for x in a['imports']) and set(a['imports'])<=allowed,'Home imports')
  expected=BASE_DEFINES+['-DWATCH_AUTOMATIC_PROVISION']+(['-DWATCH_SERIAL_TRANSFER_MODE'] if lane=='wave' else [])
  require(a.get('defines')==expected,'Home compile profile')
  receipts.append(b)
 require(first['default.elf']==repeat['default.elf'],'Home repeat ELF mismatch')
 require(first['default.json']==repeat['default.json'],'Home repeat manifest mismatch')
 for key in ('compiler','compiler_sha256','dependency_lock','verified_dependencies','home_files_sha256','build_variant','preferences','automatic_provision','serial_transfer_mode'):
  require(receipts[0].get(key)==receipts[1].get(key),'Home repeat inputs differ: '+key)
 return first['default.elf'],first['default.json'],{'version':VERSIONS[lane],'elf':record(first['default.elf']),'manifest':record(first['default.json']),'first_build':record(first['build.json']),'repeat_build':record(repeat['build.json']),'dependency_lock':LOCK,'profile':'automatic+serial' if lane=='wave' else 'automatic','repeat_identical':True,'manifest_only_version_changed':True,'firmware_execution':False}
def load_home_package(root):
 root=Path(root)
 return {name:(root/name).read_bytes() for name in ('default.elf','default.json','build.json')}
