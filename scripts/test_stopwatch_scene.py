#!/usr/bin/env python3
"""Strict deterministic host cases for the single shared Stopwatch controller."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(include,output,sanitize=False):
 output.mkdir(parents=True,exist_ok=True)
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
 if sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
 for name in ('stopwatch_test','stopwatch_lifecycle_test'):
  exe=output/name
  subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(include),str(ROOT/'test/scene'/f'{name}.c'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],env=env,check=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--include',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.include,a.output,a.sanitize)
