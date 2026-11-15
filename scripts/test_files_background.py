#!/usr/bin/env python3
"""Files Context audio/RF, BLE, persistence, workflow and cleanup with endpoint doubles."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for key in ['runtime','system','utilities','output']:p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);inc=out/'include';inc.mkdir(exist_ok=True)
origins={}
for d in [a.system/'lib/PortableApps/include',a.system/'sdk/app',a.runtime/'sdk/app',a.runtime/'sdk/driver',a.runtime/'sdk/hardware']:
 for h in d.glob('*.h'):origins[h.name]=h.resolve()
public={'Apps/battery_broadcast.h':'751944f139a3fec9c946d788ac0db9b3b217f833','Apps/battery_preference_adapter.h':'b3be055f03497cdf57e0ab21cea5296b9ecbc9ba','lib/Bluetooth/include/TelemetryBroadcastPolicyV1.h':'5b9c10bff2d5e62f90762713a728b4eac4a28d0e'}
for name,blob in public.items():
 h=(a.utilities/name).resolve();b=h.read_bytes();assert hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()==blob,'Not published bf111 input: '+name;origins[h.name]=h
utc=a.system/'sdk/app/RiscRtcUtcV1.h';assert hashlib.sha256(utc.read_bytes()).hexdigest()=='f50ed950c38cc103f4d0076c14470eb77092ee44611d5e11bb0c212bea6803ac';origins[utc.name]=utc
for name,h in origins.items():shutil.copyfile(h,inc/name)
origins['alarm_writer.h']=(a.utilities/'Apps/alarm_writer.h').resolve()
for d in [a.utilities/'lib/Contexts/include',a.utilities/'lib/Alarm/include']:
 for h in d.glob('*.h'):origins[h.name]=h.resolve()
for name,h in origins.items():shutil.copyfile(h,inc/name)
source=ROOT/'test/files_background/controller_test.c';dep=out/'test.d';binary=out/'test';commands=[]
flags=['cc','-std=c11','-O0','-g0','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-Wno-unused-function','-I'+str(ROOT/'Apps'),'-I'+str(inc)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
def run(cmd,label):
 commands.append(cmd);r=subprocess.run(cmd,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/(label+'.log')).write_text(r.stdout+r.stderr);r.check_returncode();return r.stdout
run(flags+['-MM','-MF',str(dep),str(source)],'dependencies')
paths=[Path(x).resolve() for x in dep.read_text().replace('\\\n',' ').split(':',1)[1].split()]
closure={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths}
run(flags+[str(source),'-lm','-o',str(binary)],'build');result=run([str(binary)],'test');print(result,end='')
civil=ROOT/'test/files_background/civil_clock_test.c'
run(flags+['-MM','-MF',str(out/'civil.d'),str(civil)],'civil-dependencies')
for x in (out/'civil.d').read_text().replace('\\\n',' ').split(':',1)[1].split():
 path=Path(x).resolve();closure[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
run(flags+[str(civil),'-lm','-o',str(out/'civil-test')],'civil-build')
result+=run([str(out/'civil-test')],'civil-test');print(result.splitlines()[-1])

for path,digest in closure.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,path
closure[str(Path(__file__).resolve())]=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
(out/'receipt.json').write_text(json.dumps({'result':result,'sanitize':a.sanitize,'commands':commands,'source_sha256':closure,'header_origins':{k:str(v) for k,v in origins.items()},'published_utilities_blobs':public,'scope':'Files audio+RF applicability, mutual exclusion, BLE progression, CFP/rules persistence, claim-write-result ordering, legacy/native atomic settings, pending cleanup, timer no-overwrite, workflow root boundary and capture continuity. 67 custody/UTC cases plus 15 semantic scenarios. Context/KV/atomic/app-data/RTC/BLE/alarm/Runtime endpoints doubled; no physical-provider or composed-root qualification.'},indent=2)+'\n')
