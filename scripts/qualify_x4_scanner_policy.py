#!/usr/bin/env python3
"""Reproduce deployed X4 Scanner, then qualify the cumulative modal successor."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PINS={'system':'ab388f92324c90dc892b709a5fd5870548f03e08','runtime':'1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70','hosts':'a3504d4f8f342d9d78383978e8b70032d0097c23','drivers':'422e1e81a498fb6bdab9abf086325afc05a532dc','baseline_app':'b0b32919a12d7fd12326091f2c6eb0bcfad5b4ad'}
BASELINE='a21d03b37168feb9f1dd7ae1d7c7620c6d4ab67960bc65db7189d0f5a9431b8d'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser()
 for n in [*PINS,'compiler','output']:p.add_argument('--'+n.replace('_','-'),type=Path,required=True)
 p.add_argument('--sanitizer',default='address,undefined',choices=['undefined','address,undefined']);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 for n,pin in PINS.items():
  root=getattr(a,n);actual=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
  assert actual==pin,(n,actual,pin)
  assert not subprocess.check_output(['git','-C',str(root),'status','--porcelain','--untracked-files=no'],text=True),n
 env={**os.environ,'NATIVE_APP_CC':str(a.compiler.resolve())};hashes={}
 for label,app,policy in [('baseline',a.baseline_app,False),('policy-off',ROOT,False),('candidate',ROOT,True)]:
  cmd=[sys.executable,str(ROOT/'scripts/build_x4_scanner_policy.py'),'--system',str(a.system),'--runtime',str(a.runtime),'--utilities',str(app),'--output',str(out/label)]
  if policy:cmd+=['--policy']
  subprocess.run(cmd,env=env,check=True);hashes[label]=sha(out/label/'ble_scanner/ble_scanner.elf')
 assert hashes['baseline']==hashes['policy-off']==BASELINE,hashes
 subprocess.run([sys.executable,str(ROOT/'scripts/test_x4_scanner_controller.py'),'--build',str(out/'candidate'),'--system',str(a.system),'--output',str(out/'controller'),'--sanitizer',a.sanitizer],check=True)
 results={}
 for label,build,san,reg in [('baseline-host','baseline','normal',True),('candidate-host','candidate','normal',False),('candidate-sanitized','candidate',a.sanitizer,False)]:
  cmd=[sys.executable,str(ROOT/'scripts/test_x4_scanner_hosts.py'),'--build',str(out/build),'--hosts',str(a.hosts),'--drivers',str(a.drivers),'--output',str(out/label),'--sanitizer',san]
  if reg:cmd+=['--regression-only']
  subprocess.run(cmd,check=True);results[label]=json.loads((out/label/'qualification.json').read_text())
 base={x['case']:x['frames'] for x in results['baseline-host']['results']}
 for label in ['candidate-host','candidate-sanitized']:
  other={x['case']:x['frames'] for x in results[label]['results']}
  assert all(frames==other[case] for case,frames in base.items()),'Regression pixels changed: '+label
 record=dict(pins=PINS,target_hashes=hashes,baseline_exact=True,policy_off_exact=True,unchanged_regression_frames=sum(map(len,base.values())),host_runs={k:len(v['results']) for k,v in results.items()},controller_runs=len(json.loads((out/'controller/qualification.json').read_text())['runs']),hardware_tested=False)
 (out/'qualification.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record))
if __name__=='__main__':main()
