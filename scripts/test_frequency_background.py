#!/usr/bin/env python3
"""Checked public-protocol Frequency background ownership; native endpoint doubles."""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);sdk=out/'include';sdk.mkdir(exist_ok=True)
origins={}
for base in [ROOT/'lib',a.system/'lib/PortableApps/include',a.system/'sdk/app',a.runtime/'sdk/driver',a.runtime/'sdk/hardware',a.runtime/'sdk/app']:
 for src in sorted(base.rglob('*.h')):
  if not src.is_file():continue
  dest=sdk/src.name;dest.write_bytes(src.read_bytes());origins[str(dest)]={'source':str(src.resolve()),'sha256':hashlib.sha256(src.read_bytes()).hexdigest()}
(out/'sdk-inputs.json').write_text(json.dumps(origins,indent=2))
flags=['-std=c11','-O0','-g1','-pipe','-Wall','-Wextra','-Werror','-I'+str(ROOT/'Apps'),'-I'+str(sdk)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']
commands=[]
def run(c,n):
 commands.append(c);(out/'commands.json').write_text(json.dumps(commands,indent=2));r=subprocess.run(c,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/(n+'.log')).write_text(r.stdout+r.stderr);r.check_returncode();return r.stdout
source=ROOT/'test/frequency_scene/background_test.c'
raw=run(['cc',*flags,'-MM',str(source)],'dependencies');deps=[Path(x).resolve() for x in shlex.split(raw.replace('\\\n',' ').split(':',1)[1])]+[Path(__file__).resolve()]
initial={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in deps}
run(['cc',*flags,str(source),'-o',str(out/'background-test')],'build')
modes=['clock-submicro','clock-micro','rtc-version','rtc-null','rtc-identity','rtc-low','rtc-high','rtc-failed','rtc-dirty','rtc-prefix','availability','clock-dirty-error','save-retry','export-retry','dirty-acquire','dirty-release','defaults-invalid','ble-pause-failed','capture-active','absent','prefix-context','prefix-ble','defaults-off','bluetooth','airplane','continuous','policy-refresh','import','import-rejected','save','save-unknown','save-retained','stat-context','stat-retained','stat-unknown','read-context','init-reentry','init-identity','clock-context','kv-context','kv-unknown','step-failed','pause-failed','status-cleanup','identity-before','step-reentry']
results=[run([str(out/'background-test'),m],m).strip() for m in modes]
assert initial=={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in deps},'source changed during test'
(out/'receipt.json').write_text(json.dumps({'results':results,'sanitize':a.sanitize,'inputs':initial,'sdk_origins':origins,'binary_sha256':hashlib.sha256((out/'background-test').read_bytes()).hexdigest(),'scope':'Actual new background orchestration using public codecs/contracts; doubled Context/BLE/KV/shared-data/clock/Runtime endpoints. No physical RF, production provider or native persistence proof.'},indent=2)+'\n');print(len(results),'Frequency background cases PASS')
