#!/usr/bin/env python3
"""Admit actual generated paper manifests through production Runtime::prepare.
Only hardware manifests are simulated; no ELF loads or device access occur.
"""
import argparse,json,subprocess,tempfile,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=Path);a=p.parse_args();rt=a.runtime.resolve()
out=ROOT/'build/paper-manifests';out.mkdir(parents=True,exist_ok=True)
source=['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp']
binary=out/'validate'
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*['-I'+str(rt/d) for d in ['src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','test/drivers/stubs']],*[str(rt/f) for f in source],str(ROOT/'test/native_apps/paper_manifest_test.cpp'),'-ldl','-o',str(binary)],check=True)
profile=json.loads((ROOT/'Apps/paper-utilities.json').read_text())
with tempfile.TemporaryDirectory() as folder:
 folder=Path(folder)
 def save(name,obj): (folder/name).write_text(json.dumps(obj))
 devices=[];drivers=[];policies=[]
 for g in profile['common_grants']:
  if g['capability']=='storage.key-value':continue
  ident=g['instance_id'];name='provider-'+str(ident);hardware=ident!=0
  if hardware:devices.append(dict(instance_id=ident,chip=dict(vendor='test',model='gpio',revision='unspecified'),compatible='test,gpio',config_type='gpio.bank',config_version=1,config=dict(pins=[ident],active_high=True,pull_up=False,debounce_us=0,long_press_us=0,click_min_us=0)))
  m=dict(type='driver',id=name,version='1.0.0',driver_abi=2,architecture='xtensa-esp32s3',file_name=name+'.elf',requires=[dict(capability='hardware.device',api=1)] if hardware else [],provides=[dict(capability=g['capability'],api=g['api'])])
  if hardware:m['hardware_compatibility']=[dict(compatible='test,gpio',revisions=['unspecified'],config_type='gpio.bank',config_version=1)]
  save(name+'.json',m);drivers.append(dict(manifest=name+'.json',**({'instance_id':ident} if hardware else {})))
 for name in ['battery','alarms']:
  src=ROOT/'dist/paper-utilities'/name
  shutil.copy(src/(name+'.json'),folder/(name+'.json'))
  policies.append(json.loads((src/(name+'.boot-policy.json')).read_text()))
 save('board.json',dict(schema='riscrte.board-hardware',schema_version=1,board_id='paper-test',revision='unspecified',buses=[],devices=devices))
 save('boot.json',dict(board='board.json',default_app='default.elf',drivers=drivers,app_capabilities=policies))
 subprocess.run([str(binary),str(folder)],check=True)
 # Prove the actual parser rejects the packaging regression, not a permissive shim.
 m=json.loads((folder/'alarms.json').read_text());m['profile']='invalid-extra-field';save('alarms.json',m)
 assert subprocess.run([str(binary),str(folder)],capture_output=True).returncode!=0
print('Production parser positive and unknown-field negative checks passed; runtime '+subprocess.check_output(['git','-C',str(rt),'rev-parse','HEAD'],text=True).strip())
