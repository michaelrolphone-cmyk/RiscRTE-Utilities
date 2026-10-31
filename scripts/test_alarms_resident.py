#!/usr/bin/env python3
"""Production Alarms resident handoff regressions with semantic service doubles."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def run(include,output,sanitize=False):
 output.mkdir(parents=True,exist_ok=True)
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
 if sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 exe=output/'alarms-resident-test'
 subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(include),'-I'+str(ROOT/'lib/Alarm/include'),str(ROOT/'test/scene/alarms_resident_test.c'),'-o',str(exe)],check=True)
 env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0');subprocess.run([str(exe)],env=env,check=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--include',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();run(a.include.resolve(),a.output.resolve(),a.sanitize)
