#!/usr/bin/env python3
import argparse,hashlib,json,os,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False)
source=ROOT/'test/scene/lora_presenter_test.c'
receipt={'sources':{},'runs':[]}
for name,flags in [('normal',[]),('sanitized',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
 exe=a.output/name;cmd=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-g',*flags,'-I'+str(a.system_apps/'sdk/app'),str(source),'-o',str(exe)]
 deps=subprocess.check_output(cmd[:-2]+['-MM'],text=True)
 (a.output/(name+'.dependencies')).write_text(deps)
 subprocess.run(cmd,check=True)
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
 r=subprocess.run([str(exe.resolve())],env=env,text=True,capture_output=True);(a.output/(name+'.log')).write_text(r.stdout+r.stderr)
 receipt['runs'].append({'name':name,'command':cmd,'exit_code':r.returncode})
 for sourcepath in ['Apps/lora_scene_presenter.h','Apps/lora_messages_model.h','Apps/PortableLoRaV2.h','test/scene/lora_presenter_test.c']:
  receipt['sources'][sourcepath]=hashlib.sha256((ROOT/sourcepath).read_bytes()).hexdigest()
 (a.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
