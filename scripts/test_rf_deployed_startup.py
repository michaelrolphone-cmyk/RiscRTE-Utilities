#!/usr/bin/env python3
"""Actual Waterfall restore + full Contexts service with deployed storage policy."""
from pathlib import Path
import argparse,json,os,shutil,subprocess
U=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser(description=__doc__)
for n in ('system-apps','driver-sdk','runtime','watch','build-receipt','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--mode',action='append');p.add_argument('--normal-only',action='store_true');a=p.parse_args();S=a.system_apps;out=a.output;out.mkdir(parents=True,exist_ok=True);sdk=out/'sdk/include';sdk.mkdir(parents=True,exist_ok=True)
for src in (S/'lib/PortableApps/include',U/'lib/Alarm/include',U/'lib/Contexts/include',a.watch/'sdk/driver',a.watch/'sdk/app',a.runtime/'sdk/driver',a.driver_sdk,a.runtime/'sdk/app'):
 for f in src.glob('*.h'):shutil.copyfile(f,sdk/f.name)
shutil.copytree(S/'lib/PortableApps/time',sdk.parent/'time',dirs_exist_ok=True)
r=json.loads(a.build_receipt.read_text());c=r if isinstance(r,list) else r['command'];flags=[x for x in c if x.startswith('-D')];incs=['-I'+str(x) for x in (sdk,U/'Apps',S/'lib/NativeApps/include',S/'lib/PortableApps/src',a.watch/'include')];results=[]
for san in [False] if a.normal_only else [False,True]:
 common=['cc','-std=c11','-O1','-g','-ffunction-sections','-fdata-sections']+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []);objects=[]
 for name in ['rf_deployed_startup_test.c','rf_deployed_startup_adapter.c','rf_deployed_startup_service.c','service.c']:
  src=U/'Services/contexts/service.c' if name=='service.c' else U/'test/native_apps'/name
  obj=out/(name+str(san)+'.o');subprocess.run(common+incs+(flags if name in ['rf_deployed_startup_test.c','rf_deployed_startup_adapter.c'] else ['-DCONTEXTS_RF_ONLY=0'])+['-c',str(src),'-o',str(obj)],check=True);objects.append(str(obj))
 exe=out/('startup-'+str(san));subprocess.run(common+objects+['-Wl,--gc-sections','-lm','-o',str(exe)],check=True)
 for mode in a.mode or ['startup','checkpoint-first','retained-load','denied-policy']:
  run=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=15,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});(out/(mode+'-'+str(san)+'.log')).write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,flush=True);results.append({'mode':mode,'asan_ubsan':san,'returncode':run.returncode});run.check_returncode()
(out/'evidence.json').write_text(json.dumps({'runs':results,'full_contexts_service':True,'hardware_verified':False},indent=2)+'\n')
