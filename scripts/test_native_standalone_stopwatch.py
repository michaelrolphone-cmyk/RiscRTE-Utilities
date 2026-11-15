#!/usr/bin/env python3
import argparse,pathlib,tempfile,subprocess,shutil,os
root=pathlib.Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--system',type=pathlib.Path,required=True);p.add_argument('--runtime',type=pathlib.Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as d:
 out=pathlib.Path(d);inc=out/'sdk';shutil.copytree(a.system/'lib/PortableApps/include',inc);shutil.copytree(a.system/'lib/PortableApps/time',out/'time')
 for n in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:shutil.copyfile(a.runtime/'sdk/app'/n,inc/n)
 for san in [[],['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']]:
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*san,'-I'+str(inc),'-I'+str(a.system/'lib/NativeApps/include'),str(root/'test/native_apps/stopwatch_native_standalone_test.c'),*[str(a.system/'lib/PortableApps/src'/n) for n in ['PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c']],'-o',str(out/'test')],check=True)
  for case in ['ok','unset','io','context','release']:subprocess.run([str(out/'test'),case],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
