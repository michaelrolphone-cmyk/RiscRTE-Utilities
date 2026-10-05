#!/usr/bin/env python3
"""Build the opt-in ordinary Alarm service0.4.0; no Runtime ABI changes."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from build_alarm_apps import ROOT,verify_system,verify_runtime,run,PIN,RUNTIME_PIN

def build(system,runtime):
 verify_system(system)
 actual_runtime=subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip()
 if actual_runtime not in (RUNTIME_PIN,'8688f92069b99547f0d25ecbd12cfcad3bb52c53') or subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=runtime,text=True).strip():raise ValueError('Clean reviewed Runtime0.1.7/0.1.8 required')
 cc=os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
 out=ROOT/'dist/points-service';out.mkdir(parents=True,exist_ok=True)
 mapping=out/'exports.map';mapping.write_text('{ global: t5_driver_get; local: *; };\n')
 elf=out/'driver.elf'
 flags=['-DALARM_VOLUME_CONTROL','-DPOINTS_IN_TIME_SERVICE','-DPORTABLE_RTC_UTC8_DENVER']
 run([cc,'-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in [ROOT/'lib/Alarm/include',runtime/'sdk/driver',system/'lib/PortableApps/include']],'-Wl,--version-script='+str(mapping),ROOT/'Services/alarm_service/service.c','-lgcc','-o',elf])
 symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',str(elf)],text=True)
 imports={s.split()[-1] for s in symbols.splitlines() if ' U ' in ' '+s}
 exports={s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
 if not imports<={'memcpy','memset','memcmp','strcmp','strlen'} or exports!={'t5_driver_get'}:raise ValueError((imports,exports))
 data=elf.read_bytes()
 if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise ValueError('Wrong target ABI')
 validator=out/'validate-elf';run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator]);run([validator,elf])
 manifest=(ROOT/'Services/alarm_service/points-manifest.json').read_bytes();side=json.loads(manifest)
 if side['version']!='0.4.0' or side['provides']!=[{'capability':'alarm.service','api':1}]:raise ValueError('Wrong opted-in service identity')
 (out/'manifest.json').write_bytes(manifest)
 sources=['Services/alarm_service/service.c','lib/Alarm/include/PointsRecords.h','lib/Alarm/include/PointsSchedule.h','lib/Alarm/include/AlarmServiceV1.h','lib/Alarm/include/AlarmRecords.h','lib/Alarm/include/AlarmVolume.h','Services/alarm_service/points-storage-policy.example.json']
 evidence={'schema':1,'service_version':side['version'],'source_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'system_apps_sha':PIN,'runtime_sha':actual_runtime,'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],'defines':flags,'elf_sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),'imports':sorted(imports),'exports':sorted(exports),'sources':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources},'hardware_verified':False}
 (out/'build-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print('Points ordinary Alarm service0.4.0 target ELF/import/ABI validation passed')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args();build(a.system_apps.resolve(),a.runtime.resolve())
