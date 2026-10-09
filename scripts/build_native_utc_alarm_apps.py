#!/usr/bin/env python3
"""Build opt-in native UTC apps and prove exact flag-off paper/Watch ELF bytes.
System source is consumed in place via temporary header symlinks, never vendored.
"""
import argparse,hashlib,importlib.util,io,json,os,shutil,subprocess,tarfile,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE='bb81ab0cdc137e1b2fddcfbe191880c1c6e649ad'
PROFILE=json.loads((ROOT/'Apps/native-utc-alarms.json').read_text())
SYSTEM=PROFILE['system_sha'];ADAPTER=PROFILE['adapter_sha'];RUNTIME=PROFILE['runtime_sha']
NATIVE_FLAGS=['-DALARM_NATIVE_UTC','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_NOVA_UI','-DPORTABLE_PAPER_UTILITIES','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_APP_LAUNCH_GUARD','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DALARM_RETURN_APP="springboard.elf"','-DPORTABLE_QUICK_ACTIONS']
IMPORTS={'risc_runtime_get_api','memcpy','memset','memcmp','strcmp','strlen','snprintf','malloc','free','strcpy'}
EXPORTS={'app_main','app_module_init','app_module_fini'}
QUICK=['quick_actions.c','quick_render.c','quick_session.c']
TIME=['PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
def run(cmd,**kw):return subprocess.run(list(map(str,cmd)),check=True,**kw)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def exact(repo,pin):
 if subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip()!=pin or subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=repo,text=True).strip():raise ValueError('Clean exact source required: '+str(repo)+' @ '+pin)
def symlink_headers(adapter,system,runtime,stage,out):
 include=stage/'include';include.mkdir()
 for path in (adapter/'lib/PortableApps/include').iterdir():
  if path.is_file():(include/path.name).symlink_to(path)
 for path in (system/'lib/PortableApps/time').rglob('*'):
  dest=stage/'time'/path.relative_to(system/'lib/PortableApps/time')
  if path.is_dir():dest.mkdir(parents=True,exist_ok=True)
  else:dest.parent.mkdir(parents=True,exist_ok=True);dest.symlink_to(path)
 for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h']:
  p=include/name
  if p.exists():p.unlink()
  p.symlink_to(runtime/'sdk/app'/name)
 spec=importlib.util.spec_from_file_location('alarm_sdk',system/'scripts/portable_alarm_build.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
 if helper.UTILITIES_COMMIT!=PROFILE['alarm_sdk_sha']:raise ValueError('Pinned alarm SDK mismatch')
 for name in helper.HEADERS:
  path=include/name
  if path.exists():path.unlink()
 helper.stage(argparse.Namespace(tagged_alarm_utilities=ROOT,alarm_client=True),argparse.ArgumentParser(),out,include)
 return include
def main():
 p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--adapter',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--raw-system',type=Path,required=True);p.add_argument('--watch-system',type=Path,required=True);a=p.parse_args()
 system,adapter,runtime,raw,watch=[getattr(a,k).resolve() for k in ['system_apps','adapter','runtime','raw_system','watch_system']]
 for repo,pin in [(system,SYSTEM),(adapter,ADAPTER),(runtime,RUNTIME),(raw,'a7f08a9db7342a69ef5b9bc03e3b1ea60dafbb7c'),(watch,'13f32d3e5fee262a960f5fe0add63db835381893')]:exact(repo,pin)
 cc=os.environ.get('NATIVE_APP_CC')
 if not cc:raise ValueError('Set NATIVE_APP_CC to the existing pinned Xtensa GCC; no installs')
 compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
 if '8.4.0' not in compiler or '2021r2-patch5' not in compiler:raise ValueError('Pinned GCC 8.4.0 2021r2-patch5 required')
 out=ROOT/'dist/native-utc-alarm-apps';out.mkdir(parents=True,exist_ok=True)
 mapping=out/'exports.map';mapping.write_text('{ global: '+'; '.join(sorted(EXPORTS))+'; local: *; };\n')
 common=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror']
 profile=json.loads((ROOT/'Apps/native-utc-alarms.json').read_text());rows=[];source_hashes={}
 with tempfile.TemporaryDirectory(prefix='native-alarm-target-') as tmp:
  stage=Path(tmp);include=symlink_headers(adapter,system,runtime,stage,out)
  catalog=stage/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
  baseline=stage/'base';baseline.mkdir();archive=subprocess.check_output(['git','archive',BASE,'Apps','lib/Alarm/include'],cwd=ROOT)
  with tarfile.open(fileobj=io.BytesIO(archive)) as tar:tar.extractall(baseline,filter='data')
  for name in ['alarms','countdown']:
   for label,repo,sys,headers,defines in [('native',ROOT,adapter,include,NATIVE_FLAGS),('paper',ROOT,raw,raw/'lib/PortableApps/include',['-DPORTABLE_NOVA_UI','-DPORTABLE_PAPER_UTILITIES','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"','-DALARM_RETURN_APP="springboard.elf"','-DPORTABLE_QUICK_ACTIONS']),('watch',ROOT,watch,watch/'lib/PortableApps/include',['-DPORTABLE_NOVA_UI','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_UTC8_DENVER','-DALARM_RETURN_APP="springboard.elf"'])]:
    folder=out/label/name;folder.mkdir(parents=True,exist_ok=True);elf=folder/(name+'.elf')
    sources=[repo/'Apps'/(name+'.c'),sys/'lib/PortableApps/src/adapter.c',catalog]
    if label!='watch':sources += [sys/'lib/PortableApps/src'/s for s in QUICK]
    if label=='native':sources += [system/'lib/PortableApps/src'/s for s in TIME]
    includes=['-I'+str(x) for x in [headers,sys/'lib/NativeApps/include',ROOT/'lib/Alarm/include',sys/'Apps']]
    run([cc,*common,*defines,*includes,*sources,'-lgcc','-o',elf])
    identical=None
    if label!='native':
     old=folder/(name+'-baseline.elf');old_sources=[baseline/'Apps'/(name+'.c'),*sources[1:]];old_includes=[x.replace(str(ROOT/'lib/Alarm/include'),str(baseline/'lib/Alarm/include')) for x in includes]
     run([cc,*common,*defines,*old_includes,*old_sources,'-lgcc','-o',old]);identical=old.read_bytes()==elf.read_bytes()
     if not identical:raise ValueError('Flag-off bytes changed: '+label+'/'+name)
    symbols=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',elf],text=True);imports={l.split()[-1] for l in symbols.splitlines() if ' U ' in ' '+l};exports={l.split()[-1] for l in symbols.splitlines() if len(l.split())>=3 and l.split()[-2] in ('T','D','B','R')}
    if not imports<=IMPORTS or exports!=EXPORTS:raise ValueError((label,name,imports-IMPORTS,exports))
    row={'profile':label,'app':name,'version':profile['versions'][name] if label=='native' else 'unchanged','sha256':sha(elf),'size_bytes':elf.stat().st_size,'imports':sorted(imports),'exports':sorted(exports),'exact_baseline_bytes':identical};rows.append(row)
    if label=='native':
     grants=profile['common_grants']+profile['app_grants'][name]
     manifest={'type':'application','id':name,'version':profile['versions'][name],'architecture':'xtensa-esp32s3','file_name':name+'.elf','entry':'app_main','requires':[dict(capability=c,api=v) for c,v in dict.fromkeys((g['capability'],g['api']) for g in grants)]}
     (folder/(name+'.json')).write_text(json.dumps(manifest,indent=2)+'\n');(folder/(name+'.boot-policy.json')).write_text(json.dumps({'manifest':name+'.json','grants':[{k:g[k] for k in ['capability','api','instance_id']} for g in grants]},indent=2)+'\n')
     receipt={'schema':1,'app':name,'version':manifest['version'],'source_repo':'michaelrolphone-cmyk/RiscRTE-Utilities','source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'system_source_revision':ADAPTER,'runtime_source_revision':RUNTIME,'alarm_source_revision':PROFILE['alarm_sdk_sha'],'alarm_api':2,'time_policy':'native-realtime-iana','elf_sha256':sha(elf),'elf_bytes':elf.stat().st_size,'requires':manifest['requires'],'sdk_sha256':{header:sha(include/header) for header in ('RiscRuntimeV1.h','RiscRealtimeV1.h','AlarmServiceV1.h','AlarmServiceV2.h')}}
     (folder/'x4-native-app.json').write_text(json.dumps(receipt,indent=2)+'\n')
     for source in sources:
      if source==catalog:continue
      dep=subprocess.check_output([cc,'-std=c11','-M',*defines,*includes,source],text=True).replace('\\\n',' ')
      for part in dep.split()[1:]:
       path=Path(part).resolve()
       for prefix,base in [('Utilities',ROOT),('System-adapter',adapter),('System-time',system),('Runtime',runtime)]:
        if path.is_relative_to(base):source_hashes[prefix+'/'+str(path.relative_to(base))]=sha(path);break
  validator=out/'validate-elf';run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator])
  for row in rows:run([validator,out/row['profile']/row['app']/(row['app']+'.elf')])
 licenses=out/'licenses';licenses.mkdir(exist_ok=True)
 for repo,name in [(ROOT,'Utilities-MIT.txt'),(system,'System-MIT.txt'),(runtime,'Runtime-MIT.txt')]:shutil.copy2(repo/'LICENSE',licenses/name)
 shutil.copy2(system/'lib/PortableApps/time/TIMEZONE_PROVENANCE.json',licenses/'TIMEZONE_PROVENANCE.json')
 for folder in ['fonts','paper_fonts','quick_fonts']:
  dest=licenses/folder;dest.mkdir(exist_ok=True)
  for path in (adapter/'lib/PortableApps'/folder).iterdir():
   if 'LICENSE' in path.name or 'OFL' in path.name or path.name=='SOURCES.json':shutil.copy2(path,dest/path.name)
 record={'schema':1,'source_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'system_time_sha':SYSTEM,'system_adapter_sha':ADAPTER,'runtime_sha':RUNTIME,'provider_sha':'637e13b0bce62ad49b756bec2468a6271d163fc7','app_baseline_sha':BASE,'compiler':compiler,'native_defines':NATIVE_FLAGS,'modules':rows,'sources':source_hashes,'licenses':{str(p.relative_to(licenses)):sha(p) for p in licenses.rglob('*') if p.is_file()},'hardware_verified':False,'publication':'Local development only; separately owned System publication remains blocked'}
 record['test_sources']={str(x.relative_to(ROOT)):sha(x) for base in ('test/native_apps','tests','scripts') for x in (ROOT/base).rglob('*') if x.is_file() and x.suffix in ('.py','.c','.h','.sh')}
 record['system_sources']={str(x.relative_to(system)):sha(x) for x in (system/'lib/PortableApps').rglob('*') if x.is_file()}
 (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n');print('Native Alarms/Countdown target ELF validation passed; four flag-off paper/Watch ELFs exactly match bb81ab0')
if __name__=='__main__':main()
