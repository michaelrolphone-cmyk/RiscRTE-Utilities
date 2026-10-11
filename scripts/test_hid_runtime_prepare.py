#!/usr/bin/env python3
"""Admit actual built manifests with the production Runtime parser and graph."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--packages',type=Path,required=True);a=p.parse_args();r=a.runtime.resolve();out=ROOT/'build/hid-runtime-prepare';out.mkdir(parents=True,exist_ok=True)
pin=json.loads((ROOT/'sdk/hid-sources.json').read_text())['runtime_prepare']
assert subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip()==pin
assert not subprocess.check_output(['git','-C',str(r),'status','--porcelain','--untracked-files=no'],text=True).strip()
exe=out/'prepare'
subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*['-I'+str(r/d) for d in ['src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','test/drivers/stubs']],*[str(r/s) for s in ['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp']],str(ROOT/'tests/hid_runtime_prepare.cpp'),'-ldl','-o',str(exe)],check=True)
subprocess.run([str(exe),str(a.packages.resolve()),str(out/'store')],check=True)
print('Runtime source:',subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip())
