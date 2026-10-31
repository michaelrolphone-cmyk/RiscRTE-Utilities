#!/usr/bin/env python3
"""Production Battery controller with semantic service/atomic storage doubles."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(include,output,sanitize=False):
 output.mkdir(parents=True,exist_ok=True)
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
 if sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 exe=output/'battery-scene-test'
 subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(include),str(ROOT/'test/scene/battery_scene_test.c'),'-o',str(exe)],check=True)
 env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0');subprocess.run([str(exe)],env=env,check=True)
 paths=['Apps/battery_scene.c','Apps/battery_broadcast.h','Apps/battery_preference_adapter.h','test/scene/battery_scene_test.c','test/scene/battery_broadcast_test.c','scripts/test_battery_scene.py']
 (output/'receipt.json').write_text(json.dumps({'sanitize':sanitize,'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}},indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--include',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.include.resolve(),a.output.resolve(),a.sanitize)
