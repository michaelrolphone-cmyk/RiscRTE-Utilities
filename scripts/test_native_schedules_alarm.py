#!/usr/bin/env python3
"""Schedules-only and legacy owning service fixtures, exact current SDKs."""
import argparse,os,subprocess,json
import runtime_sdk_compat
from pathlib import Path
from build_native_schedules_alarm import ROOT,RUNTIME,SYSTEM,DEFINES,exact,includes,sha,revision
p=argparse.ArgumentParser();p.add_argument('--clock-edit-authority',action='store_true');p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--system-revision',type=revision,default=SYSTEM);a=p.parse_args();runtime=a.runtime.resolve();system=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);exact(system,a.system_revision);runtime_identity=runtime_sdk_compat.verify(system,runtime,RUNTIME)
commands=[];results=[]
defines=[*DEFINES,*(['ALARM_SERVICE_CLOCK_EDIT_V2'] if a.clock_edit_authority else [])]
tracked=[f for root in [ROOT/'Services/alarm_service',ROOT/'lib/Alarm/include',ROOT/'test/native_apps',runtime/'src',runtime/'sdk',system/'lib/PortableApps'] for f in root.rglob('*') if f.is_file()]
tracked += [ROOT/'test/native_schedules_alarm_runtime_test.cpp',ROOT/'test/fixtures/native_utc_alarm_client.c',runtime/'test/RuntimeFileFixture.h',ROOT/'scripts/build_native_schedules_alarm.py',Path(__file__)]
source_hashes={str(f):sha(f) for f in tracked}
def run(cmd):
 commands.append(list(map(str,cmd)));r=subprocess.run(list(map(str,cmd)),check=False,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'});print(r.stdout,end='');print(r.stderr,end='');r.check_returncode();return r.stdout
inc=['-I'+str(x) for x in includes(runtime,system)]
# Selection ambiguity is rejected at compile time, not defaulted at runtime.
for flags in ([*DEFINES,'POINTS_IN_TIME_SERVICE'],[*DEFINES,'POINTS_CATALOG_SERVICE'],[x for x in DEFINES if x!='ALARM_SERVICE_TAGGED_V2'],[x for x in DEFINES if x!='ALARM_NATIVE_UTC']):
 cmd=['cc','-std=c11','-fsyntax-only',*inc,*['-D'+x for x in flags],str(ROOT/'Services/alarm_service/service.c')]
 commands.append(cmd);r=subprocess.run(cmd,capture_output=True,text=True)
 assert r.returncode and 'Schedules-only requires tagged native UTC and excludes Points profiles' in r.stderr
results.append({'profile':'ambiguous-selection-refusals','cases':4,'passed':True})
for san in (False,True):
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if san else []
 common=['-O1','-g','-Wall','-Wextra','-Werror',*flags];label='sanitized' if san else 'normal'
 for profile,file in [('schedules-only','native_schedules_alarm_service_test.c'),('legacy-native-points','native_utc_alarm_service_test.c'),*([('clock-editor','native_clock_edit_service_test.c')] if a.clock_edit_authority else [])]:
  exe=out/(profile+'-'+label);src=[ROOT/'test/native_apps'/file]
  if profile=='legacy-native-points':src += [system/'lib/PortableApps/src/PortableTimeZone.c',system/'lib/PortableApps/src/PortableTimeZoneCatalog.c']
  run(['cc','-std=c11',*common,'-no-pie',*inc,*src,'-o',exe]);results.append({'profile':profile,'sanitize':san,'stdout':run([exe])})
 folder=out/('runtime-'+label);folder.mkdir(exist_ok=True)
 run(['cc','-std=c11',*common,'-fPIC','-fvisibility=hidden','-shared',*inc,*['-D'+x for x in defines],ROOT/'Services/alarm_service/service.c','-o',folder/'driver.elf'])
 (folder/'service.json').write_bytes((ROOT/'Services/alarm_service'/('native-utc-clock-edit-manifest.json' if a.clock_edit_authority else 'native-utc-schedules-manifest.json')).read_bytes())
 run(['cc','-std=c11',*common,'-fPIC','-fvisibility=hidden','-shared',ROOT/'test/fixtures/native_utc_alarm_client.c','-o',folder/'app.elf'])
 sources=[runtime/x for x in ['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp','src/runtime/streams/AppStreamSessions.cpp','src/runtime/streams/ProviderQueueHost.cpp','src/runtime/storage/AppDataFiles.cpp']]
 # Match the existing product harness: GCC's instrumented ArduinoJson O1
 # reports maybe-uninitialized; O0 preserves all warnings and both sanitizers.
 run(['c++','-std=c++17',*common,*(['-O0'] if san else []),'-Wno-missing-field-initializers','-rdynamic','-no-pie',*inc,*['-I'+str(runtime/x) for x in ['src','sdk/hardware','lib/ArduinoJson/src','test','test/drivers/stubs']],*(['-DALARM_SERVICE_CLOCK_EDIT_V2'] if a.clock_edit_authority else []),*sources,ROOT/'test/native_schedules_alarm_runtime_test.cpp','-ldl','-o',folder/'test'])
 results.append({'profile':'actual-Runtime-API2-seven-keys-clock-edit' if a.clock_edit_authority else 'actual-Runtime-API2-six-keys','sanitize':san,'stdout':run([folder/'test',folder])})
receipt={'clock_edit_authority':a.clock_edit_authority,'runtime_revision':RUNTIME,'runtime_sdk_identity':runtime_identity,'system_revision':a.system_revision,'passed':True,'results':results,'commands':commands,'physical_tested':False,'sleeping_alarm_integration':False,'source_sha256':source_hashes,'source_revision':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),'source_dirty':bool(subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']))}
assert all(sha(path)==digest for path,digest in source_hashes.items()),'Test source changed during execution'
(out/'test-evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
