#!/usr/bin/env python3
"""Actual Countdown controller with deterministic semantic boundaries; no hardware claim."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--include',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
source=ROOT/'test/scene/countdown_scene_test.c';exe=a.output/'countdown-scene-test'
cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
cmd+=['-I'+str(a.include),'-I'+str(ROOT/'lib/Alarm/include'),str(source),'-o',str(exe)]
subprocess.run(cmd,check=True)
run=subprocess.run([str(exe)],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=60)
(a.output/'run.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='')
inputs=[source,ROOT/'Apps/countdown_scene.c',ROOT/'lib/Alarm/include/CountdownControlV1.h',Path(__file__)]+list(a.include.glob('*.h'))
(a.output/'receipt.json').write_text(json.dumps({'command':cmd,'returncode':run.returncode,'sanitized':a.sanitize,'hardware_tested':False,'scope':'Production Countdown scene app; deterministic semantic service and runtime doubles','sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}},indent=2)+'\n')
run.check_returncode()
