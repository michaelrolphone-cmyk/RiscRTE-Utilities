"""Stage owning published contracts; no device compiler profile."""
import hashlib,importlib.util,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def stage(runtime,system,power_key_header,output):
 spec=importlib.util.spec_from_file_location('frequency_sdk',system/'scripts/scene_sdk.py')
 module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 include=module.stage_sdk(runtime,system,output)
 origins=[]
 for directory in (system/'lib/PortableApps/include',ROOT/'lib/Alarm/include',ROOT/'lib/Bluetooth/include'):
  for source in sorted(directory.glob('*.h')):
   target=include/source.name
   if not target.exists():shutil.copyfile(source,target)
   origins.append({'source':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
 utc=system/'sdk/app/RiscRtcUtcV1.h'
 shutil.copyfile(utc,include/utc.name)
 origins.append({'source':str(utc),'sha256':hashlib.sha256(utc.read_bytes()).hexdigest()})
 idle=system/'sdk/app/RiscResidentIdleV1.h'
 shutil.copyfile(idle,include/idle.name)
 origins.append({'source':str(idle),'sha256':hashlib.sha256(idle.read_bytes()).hexdigest()})
 source=power_key_header
 if source.name!='RiscPowerKeyV1.h':raise ValueError('Expected owning generic power-key contract')
 target=include/source.name
 if target.exists() and target.read_bytes()!=source.read_bytes():raise ValueError('Conflicting generic power-key contract')
 shutil.copyfile(source,target)
 origins.append({'source':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
 (output/'frequency-sdk-origins.json').write_text(json.dumps(origins,indent=2))
 return include
