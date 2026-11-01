#!/usr/bin/env python3
"""Fresh normal/sanitized session qualification; no historical receipt reuse."""
import argparse, hashlib, json, os, pathlib, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False)
sources=['Apps/lora_scene_session.h','Apps/lora_messages_model.h','Apps/PortableLoRaV2.h','test/scene/lora_session_test.c','scripts/test_lora_scene_session.py']
receipt={'sources':{x:hashlib.sha256((ROOT/x).read_bytes()).hexdigest() for x in sources},'runs':[]}
for name,flags in [('normal',[]),('sanitized',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
 exe=a.output/name
 cmd=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-g',*flags,str(ROOT/'test/scene/lora_session_test.c'),'-o',str(exe)]
 subprocess.run(cmd,check=True)
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
 r=subprocess.run([str(exe.resolve())],env=env,text=True,capture_output=True)
 (a.output/(name+'.log')).write_text(r.stdout+r.stderr)
 receipt['runs'].append({'name':name,'command':cmd,'exit_code':r.returncode})
 (a.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
