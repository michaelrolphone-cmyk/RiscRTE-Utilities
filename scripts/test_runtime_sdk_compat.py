#!/usr/bin/env python3
"""Host staging probes only: no target compilation or native execution."""
import argparse,json,subprocess,sys
from pathlib import Path
import runtime_sdk_compat as sdk
import build_portable_apps as daily
import build_badge_native_alarm_apps as alarms
p=argparse.ArgumentParser();p.add_argument('--system',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
canonical='273bdbccf65647a803ab0586a418f8283732baec';identity=sdk.verify(a.system,a.runtime,canonical)
sys.path[:]=[x for x in sys.path if Path(x or '.').resolve()!=a.system.resolve()/'scripts']
args=argparse.Namespace(foreground_idle_helper_dir=a.output,foreground_idle_runtime_dir=a.runtime,foreground_idle_drivers_sdk_dir=a.output,foreground_idle_fallback_seconds=15,foreground_alarm_sleep=False,navigation=True)
helper=daily.foreground_idle_module(a.system,args,p);headers=helper._runtime_headers(a.runtime,p)
assert headers['sdk/app/RiscRuntimeV1.h']==sdk.header(a.runtime,canonical,'RiscRuntimeV1.h')
for editor in [False,True]:
 out=a.output/('editor' if editor else 'historical');out.mkdir(exist_ok=True)
 include,_=alarms.stage(a.system,a.runtime,out,subprocess.check_output(['git','-C',str(a.system),'rev-parse','HEAD'],text=True).strip(),editor)
 assert (include/'RiscRuntimeV1.h').read_bytes()==headers['sdk/app/RiscRuntimeV1.h']
 assert (include/'RiscRealtimeV1.h').read_bytes()==headers['sdk/app/RiscRealtimeV1.h']
legacy=a.output/'legacy-system';legacy.mkdir(exist_ok=True)
try:sdk.verify(legacy,a.runtime,canonical)
except ValueError:pass
else:raise AssertionError('Legacy profile accepted a changed Runtime head')
receipt={'passed':True,'identity':identity,'actual_system':subprocess.check_output(['git','-C',str(a.system),'rev-parse','HEAD'],text=True).strip(),'actual_utilities':subprocess.check_output(['git','-C',str(daily.ROOT),'rev-parse','HEAD'],text=True).strip(),'cases':['dynamic-foreground-import','canonical-foreground-archive','alarm-old-SDK-staging','alarm-editor-SDK-staging','legacy-head-refusal'],'target_compiled':False}
(a.output/'receipt.json').write_text(json.dumps(receipt,indent=2));print('PASS actual selected System dynamic helper + native alarm canonical staging, historical strict refusal')
