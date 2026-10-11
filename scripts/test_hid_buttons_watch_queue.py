#!/usr/bin/env python3
"""Production Buttons and Watch FT6336U queued-report safety regression."""
import argparse,hashlib,json,os,pathlib,subprocess
p=argparse.ArgumentParser(description=__doc__)
for name in ('system-apps','watch','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
p.add_argument('--touch-source',default='drivers/current/twatch_touch/driver.c')
p.add_argument('--sanitize',action='store_true')
p.add_argument('--seam',action='store_true',help='Build the real-HID capability bridge without running the local HID double')
a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);system=a.system_apps.resolve();watch=a.watch.resolve()
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fPIC']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
sources=[root/'test/native_apps/hid_watch_touch_backend.c',watch/a.touch_source]
commands=[];objects=[]
for i,source in enumerate(sources):
 obj=out/f'touch-{i}.o';cmd=['cc',*flags,'-I'+str(watch/'sdk/driver'),'-I'+str(watch/'include'),'-c',str(source),'-o',str(obj)];commands.append(cmd);subprocess.run(cmd,check=True);objects.append(obj)
exe=out/('app-seam.so' if a.seam else 'buttons')
includes=[root/'Apps',root/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include',system/'lib/PortableApps/src']
cmd=['cc',*flags,*(['-DHID_REPORT_SEAM','-Wno-unused-function','-shared'] if a.seam else ['-no-pie']),*['-I'+str(x) for x in includes],str(root/'tests/hid_buttons_watch_queue_test.c'),*map(str,objects),'-o',str(exe)];commands.append(cmd);subprocess.run(cmd,check=True)
cases=[] if a.seam else ['queued','move-inside','sampled','held-release','deadline100','mouse','wheel','motion','multi-contact','move-outside','changed-id','second-tap','deadline101','clock-rollback','queue-overflow','keyboard-unready','no-auth','no-encryption','generation-change','disconnected','settings','unarmed','poll-fail','press-fail','release-fail']
for case in cases:
 r=subprocess.run([str(exe),case],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
 (out/(case+'.log')).write_text(r.stdout+r.stderr);print(r.stdout,end='');assert r.returncode==0,(case,r.returncode,r.stderr)
sources += [root/'Apps/ble_hid_app.inc',root/'Apps/ble_hid_model.h',root/'tests/hid_app_test.c',root/'tests/hid_buttons_watch_queue_test.c',pathlib.Path(__file__).resolve()]
(out/'qualification.json').write_text(json.dumps({'source_hashes':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in sources},'commands':commands,'cases':cases,'sanitized':a.sanitize,'physical_provider':str(watch/a.touch_source),'hid_provider':'external real capability' if a.seam else 'declared test double','hardware_tested':False,'leak_sanitizer':False},indent=2)+'\n')
