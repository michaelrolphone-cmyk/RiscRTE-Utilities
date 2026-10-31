#!/usr/bin/env python3
"""Strict deterministic semantic Calculator and ownership cases."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(include,output,sanitize=False):
 output.mkdir(parents=True,exist_ok=True)
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
 if sanitize:flags+=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 exe=output/'calculator-test'
 subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(include),str(ROOT/'test/shared_calculator/app_test.c'),'-o',str(exe)],check=True)
 env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
 for mode in range(27):subprocess.run([str(exe),str(mode)],env=env,check=True)
 core=output/'calculator-core-test'
 subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'tests/calculator_core_failures.c'),'-o',str(core)],check=True)
 subprocess.run([str(core)],env=env,check=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--include',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.include,a.output,a.sanitize)
