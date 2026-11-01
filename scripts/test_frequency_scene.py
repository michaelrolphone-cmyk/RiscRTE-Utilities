#!/usr/bin/env python3
"""Production Frequency session/controller tests (scene tests added separately)."""
import argparse, hashlib, json, os, shlex, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--runtime',type=Path);p.add_argument('--system',type=Path);p.add_argument('--power-key-header',type=Path);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
source=ROOT/'test/frequency_scene/session_test.c'
flags=['-std=c11','-O0','-g1','-pipe','-Wall','-Wextra','-Werror','-I'+str(ROOT/'Apps'),'-I'+str(ROOT/'lib/Alarm/include')]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']
commands=[]
def run(cmd,name):
 commands.append(cmd);(out/'commands.json').write_text(json.dumps(commands,indent=2))
 r=subprocess.run(cmd,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/(name+'.log')).write_text(r.stdout+r.stderr);r.check_returncode();return r.stdout
raw=run(['cc',*flags,'-MM',str(source)],'dependencies');deps=[Path(x).resolve() for x in shlex.split(raw.replace('\\\n',' ').split(':',1)[1])]+[Path(__file__).resolve()]
initial={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in deps}
run(['cc',*flags,str(source),'-o',str(out/'session-test')],'build-session')
modes=['default-idle','continuous','phase-edit','clamps','watchdog','rollover','no-auto-resume','failed-open','failed-open-close','failed-close','failed-write','failed-write-close','duplicate-start','invalid-output','replacement-before','replacement-open','replacement-write','replacement-close']
results=[run([str(out/'session-test'),mode],mode).strip() for mode in modes]
assert initial=={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in deps},'source changed during run'
(out/'receipt.json').write_text(json.dumps({'results':results,'sanitize':a.sanitize,'inputs':initial,'binary_sha256':hashlib.sha256((out/'session-test').read_bytes()).hexdigest(),'scope':'Actual frequency_session.h and unchanged tone_core; doubled output and parent-provided identity fence. Not actual Runtime/scene or physical audio.'},indent=2)+'\n')
print(len(results),'Frequency session cases PASS')

if a.runtime or a.system:
 assert a.runtime and a.system,'both --runtime and --system are required for production-root tests'
 sdk=out/'include';sdk.mkdir(exist_ok=True);origins={}
 for base in [ROOT/'lib',a.system/'lib/PortableApps/include',a.system/'sdk/app',a.runtime/'sdk/driver',a.runtime/'sdk/hardware',a.runtime/'sdk/app']:
  for src in sorted(base.rglob('*.h')):
   if src.is_file():
    dest=sdk/src.name;dest.write_bytes(src.read_bytes());origins[str(dest)]={'source':str(src.resolve()),'sha256':hashlib.sha256(src.read_bytes()).hexdigest()}
 assert a.power_key_header,'--power-key-header is required for root tests'
 src=a.power_key_header.resolve();dest=sdk/'RiscPowerKeyV1.h';dest.write_bytes(src.read_bytes());origins[str(dest)]={'source':str(src),'sha256':hashlib.sha256(src.read_bytes()).hexdigest()}
 (out/'sdk-inputs.json').write_text(json.dumps(origins,indent=2))
 rootflags=[f for f in flags if not f.startswith('-I')]+['-I'+str(ROOT/'Apps'),'-I'+str(sdk)]
 source=ROOT/'test/frequency_scene/root_test.c'
 raw=run(['cc',*rootflags,'-MM',str(source)],'root-dependencies');deps=[Path(x).resolve() for x in shlex.split(raw.replace('\\\n',' ').split(':',1)[1])]+[Path(__file__).resolve()]
 initial={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in deps}
 run(['cc',*rootflags,str(source),'-o',str(out/'root-test')],'build-root')
 modes=['paired-toggle','loop-quiet-off','loop-idle-off','identity-before','document','level-precision','event-fences','manual-neutral','controls','close-again','checkpoint-busy','idle-policy','idle-poll-busy','failed-close','alarm-cue','alarm-alert','alarm-blocked','alarm-emerges','alarm-ack','alarm-uncertain','alarm-malformed','alarm-label','alarm-stop-failed','timer-context','navigation','loop-idle','loop-held','loop-activity','loop-navigation','loop-flags']
 results=[run([str(out/'root-test'),m],m).strip() for m in modes]
 assert initial=={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in deps},'root source changed during test'
 (out/'root-receipt.json').write_text(json.dumps({'results':results,'sanitize':a.sanitize,'inputs':initial,'sdk_origins':origins,'binary_sha256':hashlib.sha256((out/'root-test').read_bytes()).hexdigest(),'scope':'Actual frequency_scene.c app_main and static helpers with native scene/resident/alarm/audio doubles; background optional endpoints absent. Not actual Runtime, physical output or provider rendering.'},indent=2)+'\n')
 print(len(results),'Frequency root cases PASS')
