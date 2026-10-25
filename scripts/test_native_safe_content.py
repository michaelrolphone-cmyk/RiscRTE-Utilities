#!/usr/bin/env python3
"""Production Utilities and selected System native renderer, modeled providers."""
import argparse, hashlib, json, os, pathlib, shutil, subprocess
from build_portable_apps import verify_system
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system',type=pathlib.Path,required=True);p.add_argument('--system-revision',required=True);p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,default=root/'build/native-safe-content-tests');a=p.parse_args()
s=a.system.resolve();r=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);verify_system(s,a.system_revision)
inc=out/'sdk';shutil.copytree(s/'lib/PortableApps/include',inc,dirs_exist_ok=True);shutil.copytree(s/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for n in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:shutil.copyfile(r/'sdk/app'/n,inc/n)
flags=['-D'+n for n in ['PORTABLE_UTILITIES_NATIVE_SAFE_CONTENT','PORTABLE_NOVA_UI','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RF_NATIVE_CANVAS','PORTABLE_SAFE_VIEWPORT','PORTABLE_WATCH_SAFE_INSETS','PORTABLE_WATCH_NATIVE_HEIGHT','PORTABLE_WATCH_COLOR_ONLY']]
rasters=out/'rasters';rasters.mkdir(exist_ok=True)
results=[]
for kind in (1,2,3):
 for san in (False,True):
  target=out/f'app-{kind}-{int(san)}';extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*flags,*extra,f'-DAPP_KIND={kind}','-DNATIVE_RENDERER_SOURCE="'+str(s/'lib/PortableApps/src/nova_ui.inc')+'"','-I'+str(inc),'-I'+str(s/'lib/NativeApps/include'),str(root/'test/native_apps/native_geometry_test.c'),'-o',str(target)]
  subprocess.run(command,check=True)
  result=subprocess.run([target],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','NATIVE_GEOMETRY_RASTERS':str(rasters)})
  print(result.stdout,end='');results.append({'app_kind':kind,'sanitized':san,'command':command,'stdout':result.stdout})
# Unlike the raster-owner fixture above, this links the real adapter and starts
# with no display surface. It catches missing native frame acquisition.
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
for san in (False,True):
 target=out/f'adapter-start-{int(san)}';extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*flags,*extra,'-DPORTABLE_FORCE_FULL_FRAMES','-I'+str(inc),'-I'+str(root/'lib/Alarm/include'),'-I'+str(s/'lib/NativeApps/include'),str(root/'test/native_apps/native_geometry_adapter_start_test.c'),str(root/'Apps/calculator.c'),str(s/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(target)]
 subprocess.run(command,check=True)
 frames=out/f'adapter-start-{int(san)}-frames';frames.mkdir(exist_ok=True)
 result=subprocess.run([target,frames],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 print(result.stdout,end='');results.append({'app_kind':'actual-adapter-cold-calculator','sanitized':san,'command':command,'stdout':result.stdout})
for san in (False,True):
 target=out/f'adapter-touch-{int(san)}';extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*flags,*extra,'-DPORTABLE_FORCE_FULL_FRAMES','-I'+str(inc),'-I'+str(root/'lib/Alarm/include'),'-I'+str(s/'lib/NativeApps/include'),str(root/'test/native_apps/native_geometry_adapter_touch_test.c'),str(s/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(target)]
 subprocess.run(command,check=True)
 frames=out/f'adapter-touch-{int(san)}-frames';frames.mkdir(exist_ok=True)
 result=subprocess.run([target,frames],check=True,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 print(result.stdout,end='');results.append({'app_kind':'actual-adapter-native-touch','sanitized':san,'command':command,'stdout':result.stdout})
inputs=[*list((root/'Apps').rglob('*')),*list((root/'test/native_apps').rglob('*')),pathlib.Path(__file__),root/'scripts/build_portable_apps.py']
source_hashes={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in inputs if x.is_file() and '__pycache__' not in x.parts}
(out/'results.json').write_text(json.dumps({'utilities_revision':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'source_sha256':source_hashes,'system_revision':a.system_revision,'runtime_revision':subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip(),'tests':results},indent=2)+'\n')
