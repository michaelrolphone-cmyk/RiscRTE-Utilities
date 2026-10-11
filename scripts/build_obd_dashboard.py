#!/usr/bin/env python3
"""Compile a capability-only ELF; retain source/import/stack evidence."""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def stage(runtime,system,drivers,out):
 sys.path.insert(0,str(system/'scripts'));from scene_sdk import stage_sdk
 inc=stage_sdk(runtime,system,out)
 shutil.copyfile(drivers/'sdk/driver/RiscBluetoothSerialV1.h',inc/'RiscBluetoothSerialV1.h')
 return inc

def build(runtime,system,drivers,out):
 inc=stage(runtime,system,drivers,out);out.mkdir(parents=True,exist_ok=True)
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
 flags=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-ffreestanding','-fno-builtin','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-fstack-usage','-Wall','-Wextra','-Werror','-Wno-misleading-indentation']
 objects=[]
 for name in ('elm','app'):
  obj=out/(name+'.o');objects.append(obj)
  subprocess.run([cc,*flags,'-I'+str(inc),'-c',str(ROOT/'Apps/obd_dashboard'/f'{name}.c'),'-o',str(obj)],check=True)
 mapping=out/'exports.map';mapping.write_text('{ global: app_main; risc_resident_app_descriptor_v1; local: *; };\n')
 elf=out/'obd_dashboard.elf'
 subprocess.run([cc,*flags,'-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--gc-sections','-Wl,--version-script='+str(mapping),*map(str,objects),'-lgcc','-o',str(elf)],check=True)
 sys.path.insert(0,str(drivers/'scripts'));from normalize_xtensa_relocations import normalize
 normalize(elf)
 symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
 imports={l.split()[-1] for l in symbols.splitlines() if ' U ' in ' '+l}
 exports={l.split()[-1] for l in symbols.splitlines() if len(l.split())>=3 and l.split()[-2] in ('T','D','B','R')}
 assert imports<= {'risc_runtime_get_api','memcpy','memset','memcmp','strlen','strcmp','strncmp','strstr','strcspn','snprintf'},imports
 assert exports=={'app_main','risc_resident_app_descriptor_v1'},exports
 raw=elf.read_bytes();assert raw[:7]==b'\x7fELF\x01\x01\x01' and raw[16:20]==b'\x03\x00\x5e\x00'
 stack={}
 for p in out.glob('*.su'):
  for l in p.read_text().splitlines():
   f,n,k=l.split('\t');stack[f]=int(n)
 assert max(stack.values())<=1536,stack
 sources={n:{'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=p,text=True).strip(),'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=p,text=True).strip())} for n,p in [('utilities',ROOT),('runtime',runtime),('system',system),('drivers',drivers)]}
 result={'schema':1,'id':'obd-dashboard','version':'0.1.0','bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'imports':sorted(imports),'exports':sorted(exports),'stack_frames':stack,'sources':sources,'hardware_tested':False}
 (out/'build.json').write_text(json.dumps(result,indent=2)+'\n');shutil.copyfile(ROOT/'Apps/obd_dashboard.json',out/'obd_dashboard.json');print(json.dumps(result))
if __name__=='__main__':
 p=argparse.ArgumentParser()
 for n in ('runtime','system','drivers','output'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();build(a.runtime.resolve(),a.system.resolve(),a.drivers.resolve(),a.output.resolve())
