import sys,argparse,json,os,shutil
from pathlib import Path
p=argparse.ArgumentParser()
for n in ['system','utilities','runtime','output']:p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--policy',action='store_true');a=p.parse_args()
for n in ['system','utilities','runtime','output']:setattr(a,n,getattr(a,n).resolve())
sys.path.insert(0,str(a.utilities/'scripts'))
import resident_client_build as r
args=argparse.Namespace(system_apps=a.system,system_revision=r.git(a.system,'rev-parse','HEAD'),development_system=False,runtime=a.runtime,runtime_revision=r.git(a.runtime,'rev-parse','HEAD'),output=a.output,display_sdk=Path(__file__).resolve().parents[1]/'docs/recovery/x4-scanner-025/display-sdk',raster_snapshot=True)
c=r.prepare(args,p,a.utilities)
q=json.loads((a.utilities/'Apps/x4-ble-resident-profile.json').read_text())
flags=q['build_defines']+['-DPORTABLE_UTILITIES_ATOMIC_PREFERENCES','-DPORTABLE_UTILITIES_TARGET_X4','-DPORTABLE_BROADCAST_RESULT_ONLY']
if a.policy:flags+=['-DPORTABLE_TEXT_INPUT_POLICY']
grants=q['required_grants']+[dict(capability='storage.key-value.atomic',api=1,instance_id=1)]
r.build(c,a.utilities,'ble_scanner','0.2.26' if a.policy else '0.2.25',flags,[a.utilities/'Apps/ble_scanner.c'],grants,q['features'])
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'docs/recovery/x4-scanner-025'))
from compact_current_elf import compact
elf=a.output/'ble_scanner/ble_scanner.elf';shutil.copyfile(elf,elf.with_suffix('.original.elf'));compact(elf,c['cc'])
print('COMPACT',r.sha(elf),elf.stat().st_size)

receipt=elf.parent/'x4-native-app.json';data=json.loads(receipt.read_text());data['original_elf_sha256']=data['elf_sha256'];data['elf_sha256']=r.sha(elf);data['elf_bytes']=elf.stat().st_size;receipt.write_text(json.dumps(data,indent=2)+'\n')
