#!/usr/bin/env python3
"""Strict compile and canonical source receipt; not a runtime behavior test."""
import argparse,hashlib,json,os,pathlib,subprocess
from scanner_support import sdk
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
for n in ('runtime','system','output'):p.add_argument('--'+n,type=pathlib.Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
inc=sdk(a.runtime,a.system,a.output/'sdk')
source=ROOT/'Apps/lora_scene.c'
cmd=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(inc),'-fsyntax-only',str(source)]
r=subprocess.run(cmd,text=True,capture_output=True);(a.output/'compile.log').write_text(r.stdout+r.stderr)
files=['Apps/lora_scene.c','Apps/lora_scene_session.h','Apps/lora_scene_presenter.h','Apps/lora_scene.json','Apps/scanner_alarm.h','Apps/scanner_background.h']
(a.output/'receipt.json').write_text(json.dumps({'command':cmd,'exit_code':r.returncode,'sources':{f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
