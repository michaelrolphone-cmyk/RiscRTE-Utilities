#!/usr/bin/env python3
"""Watch LoRa modal policy through the actual app/adapter/shared hosts."""
import argparse,json,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for n in ['system-apps','watch-runtime','watch-profile','output']:p.add_argument('--'+n,type=lambda v:Path(v).resolve(),required=True)
p.add_argument('--normal-only',action='store_true');a=p.parse_args();(a.output/'build').mkdir(parents=True,exist_ok=True)
profile=json.loads((ROOT/'test/lora_shared_input/policy-profile.json').read_text())
profile['defines']=[x.replace('@WATCH@',str(a.watch_profile)).replace('@OUTPUT@',str(a.output)) for x in profile['defines']]
(a.output/'profile.json').write_text(json.dumps({'lora_messages':profile},indent=2)+'\n')
(a.output/'build/watch-alarm-profile.h').write_text('#include "AlarmServiceV1.h"\n')
command=[sys.executable,str(ROOT/'scripts/test_lora_shared_input.py'),'--system-apps',str(a.system_apps),'--runtime',str(a.watch_runtime),'--recipe',str(a.output/'profile.json'),'--output',str(a.output/'workflow'),'--extra-define','PORTABLE_TEXT_INPUT_POLICY']
for case in ['policy-stopped-done','policy-stopped-cancel','policy-stopped-home','policy-listen','policy-unavailable','policy-retained','policy-missing-suffix','policy-paused-home']:command+=['--scenario',case]
if a.normal_only:command+=['--normal-only']
subprocess.run(command,check=True)
