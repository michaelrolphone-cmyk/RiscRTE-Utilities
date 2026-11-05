#!/usr/bin/env python3
"""Focused RF modal policy using the actual app/adapter/text and scene hosts."""
import argparse,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for n in ['system-apps','watch-runtime','x4-runtime','watch-profile','output']:p.add_argument('--'+n,type=lambda v:Path(v).resolve(),required=True)
p.add_argument('--normal-only',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
profiles=json.loads((ROOT/'test/rf_shared_input/policy-profiles.json').read_text())
(a.output/'watch-alarm-profile.h').write_text('#include "'+str(ROOT/'lib/Alarm/include/AlarmServiceV1.h')+'"\n#define ALARM_MODE_VISUAL 0u\nstatic inline uint32_t alarm_service_output_modes(const alarm_service_v1*s){return s?ALARM_MODE_BOTH:ALARM_MODE_VISUAL;}\n')
(a.output/'watch-rf-presentation.h').write_text('#include "'+str(a.system_apps/'Apps/PaperPresentation.h')+'"\n#define RF_PRESENTATION_H\n')
for device,profile in profiles.items():
 profile['defines']=[x.replace('@WATCH@',str(a.watch_profile)).replace('@OUTPUT@',str(a.output)) for x in profile['defines']]
 (a.output/(device+'-profile.json')).write_text(json.dumps(profile,indent=2)+'\n')
 cases=['policy-stopped-done','policy-stopped-cancel','policy-active','policy-unavailable']+(['policy-continuity','policy-retained'] if device=='x4' else [])
 command=[sys.executable,str(ROOT/'scripts/test_rf_shared_input.py'),'--system-apps',str(a.system_apps),'--x4-host-system',str(a.system_apps),'--runtime',str(a.watch_runtime),'--x4-runtime',str(a.x4_runtime),'--watch-recipe',str(a.output/'watch-profile.json'),'--x4-recipe',str(a.output/'x4-profile.json'),'--baseline-utilities',str(ROOT),'--target',device,'--output',str(a.output/device),'--extra-define','PORTABLE_TEXT_INPUT_POLICY']
 for case in cases:command+=['--scenario',case]
 if a.normal_only:command+=['--normal-only']
 subprocess.run(command,check=True)
