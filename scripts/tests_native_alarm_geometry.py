#!/usr/bin/env python3
"""Additive production native Alarm/Countdown geometry and controller qualification."""
import argparse,pathlib,subprocess,json,hashlib,os
from build_badge_native_alarm_apps import stage,TIME,NATIVE_PRESENTATION_FLAGS,FLAGS
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--system-apps',type=pathlib.Path,required=True);p.add_argument('--system-revision',required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--clock-edit-authority',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);s=a.system_apps.resolve();r=a.runtime.resolve()
inc,sdk=stage(s,r,out,a.system_revision,a.clock_edit_authority)
rasters=out/'rasters';rasters.mkdir(exist_ok=True);runs=[]
flags=[*FLAGS,*NATIVE_PRESENTATION_FLAGS,*(['-DALARM_SERVICE_CLOCK_EDIT_V2'] if a.clock_edit_authority else [])]
for kind in (1,2):
 for san in (False,True):
  binary=out/f'geometry-{kind}-{int(san)}';extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*flags,*extra,f'-DDAILY_ALARM_KIND={kind}','-DNATIVE_RENDERER_SOURCE="'+str(s/'lib/PortableApps/src/nova_ui.inc')+'"',*['-I'+str(x) for x in [inc,root/'lib/Alarm/include',r/'sdk/driver',s/'lib/NativeApps/include']],str(root/'test/native_apps/native_alarm_geometry_test.c'),*[str(s/'lib/PortableApps/src'/n) for n in TIME],'-o',str(binary)]
  subprocess.run(cmd,check=True);result=subprocess.run([binary],check=True,text=True,capture_output=True,timeout=90,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1','NATIVE_GEOMETRY_RASTERS':str(rasters)});print(result.stdout,end='');runs.append({'kind':kind,'sanitized':san,'command':cmd,'stdout':result.stdout})
files=[*list((root/'Apps').glob('alarm*')),*list((root/'test/native_apps').glob('native_alarm_geometry*')),root/'test/native_apps/native_utc_alarm_app_test.c',root/'test/native_apps/native_geometry_raster.h',pathlib.Path(__file__),s/'lib/PortableApps/src/nova_ui.inc']
(out/'evidence.json').write_text(json.dumps({'passed':True,'utilities_sha':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','-C',str(root),'status','--porcelain'],text=True).strip()),'system_sha':a.system_revision,'runtime_sha':subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip(),'hardware_tested':False,'native_alarm_geometry':True,'runs':runs,'sdk':sdk,'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files if f.is_file()},'frames_sha256':{str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in rasters.glob('*.ppm')}},indent=2)+'\n')
