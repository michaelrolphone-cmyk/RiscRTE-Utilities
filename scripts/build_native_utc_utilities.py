#!/usr/bin/env python3
"""Build all explicit native utilities and compare fourteen default profile ELFs."""
import argparse,json,os,shutil,subprocess,tempfile
from pathlib import Path
from native_utility_build import *

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--app',choices=APPS,action='append');p.add_argument('--output',type=Path);a=p.parse_args()
 system=a.system_apps.resolve();runtime=a.runtime.resolve();exact(system,SYSTEM);exact(runtime,RUNTIME)
 cc=os.environ.get('NATIVE_APP_CC')
 if not cc:raise ValueError('Set NATIVE_APP_CC to the existing pinned GCC8.4 compiler')
 compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0]
 assert '8.4.0' in compiler and '2021r2-patch5' in compiler
 out=(a.output or ROOT/'dist/native-utc-utilities').resolve();out.mkdir(parents=True,exist_ok=True)
 mapping=out/'exports.map';mapping.write_text('{ global: '+'; '.join(sorted(EXPORTS))+'; local: *; };\n')
 common=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-nostdlib','-nostartfiles','-shared','-Wl,--no-relax','-Wl,--hash-style=sysv','-Wl,--version-script='+str(mapping),'-Wall','-Wextra','-Werror']
 record={'schema':1,'source_sha':git(ROOT,'rev-parse','HEAD'),'source_dirty':bool(git(ROOT,'status','--porcelain')),'system_sha':SYSTEM,'runtime_sha':RUNTIME,'app_baseline_sha':BASE,'compiler':compiler,'modules':[],'sources':{},'hardware_verified':False,'publication':'local only'}
 with tempfile.TemporaryDirectory(prefix='native-utilities-') as temp:
  folder=Path(temp);headers=stage(system,runtime,folder/'native',out)
  catalog=folder/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
  baseline=folder/'baseline';extract(ROOT,BASE,baseline,'Apps','lib/Bluetooth/include')
  raw=folder/'raw-system';extract(system,'a7f08a9db7342a69ef5b9bc03e3b1ea60dafbb7c',raw)
  watch=folder/'watch-system';extract(system,'13f32d3e5fee262a960f5fe0add63db835381893',watch)
  batterywatch=folder/'battery-watch-system';extract(system,'4cf36b1c46641b00d88535eb9a0e9b0797928aff',batterywatch)
  rfwatch=folder/'rf-watch-system';extract(system,'fcdb5b0a54a11a407bf68c6e35ac2548471cd9f1',rfwatch)
  validator=out/'validate-elf';run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),ROOT/'lib/elf_loader/src/esp_elf_validate.c',ROOT/'test/native_apps/validate_test.c','-o',validator])
  for name in a.app or APPS:
   for label in ('native','paper','watch'):
    native=label=='native';paper=label!='watch';sys=system if native else raw if paper or name.startswith('ble_') else rfwatch if name=='waterfall' else batterywatch if name=='battery' else watch
    inc=headers if native else sys/'lib/PortableApps/include';defines=flags(name,native,paper);includes=['-I'+str(d) for d in include_paths(sys,inc)]
    dest=out/label/name;dest.mkdir(parents=True,exist_ok=True);elf=dest/(name+'.elf');src=sources(name,sys,native,paper)
    run([cc,*common,*defines,*includes,*src,catalog,'-lgcc','-o',elf])
    if not native:
     old=dest/(name+'-baseline.elf');oldinc=['-I'+str(d) for d in include_paths(sys,inc,baseline)]
     run([cc,*common,*defines,*oldinc,*sources(name,sys,native,paper,baseline),catalog,'-lgcc','-o',old])
     if old.read_bytes()!=elf.read_bytes():raise ValueError('Flag-off ELF bytes changed: '+label+'/'+name)
    syms=subprocess.check_output([cc.removesuffix('gcc')+'nm','-D',elf],text=True);imports={s.split()[-1] for s in syms.splitlines() if ' U ' in ' '+s};exports={s.split()[-1] for s in syms.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T','D','B','R')}
    if not imports<=IMPORTS or exports!=EXPORTS:raise ValueError((name,label,imports-IMPORTS,exports))
    run([validator,elf]);row=dict(app=name,profile=label,version=PROFILE['versions'][name] if native else 'unchanged',sha256=sha(elf),size_bytes=elf.stat().st_size,imports=sorted(imports),exports=sorted(exports),defines=defines,target_validation='passed',exact_baseline_bytes=None if native else True)
    if native:
     row['manifest']=manifests(name,dest)
     app_receipt={'schema':1,'app':name,'version':PROFILE['versions'][name],'source_repo':'michaelrolphone-cmyk/RiscRTE-Utilities','source_revision':record['source_sha'],'system_source_revision':SYSTEM,'runtime_source_revision':RUNTIME,'alarm_source_revision':PROFILE['alarm_sdk_sha'],'alarm_api':2,'time_policy':'native-realtime-iana','elf_sha256':sha(elf),'elf_bytes':elf.stat().st_size,'requires':row['manifest']['requires'],'sdk_sha256':{header:sha(headers/header) for header in ['RiscRuntimeV1.h','RiscRealtimeV1.h','AlarmServiceV1.h','AlarmServiceV2.h']}}
     (dest/'x4-native-app.json').write_text(json.dumps(app_receipt,indent=2)+'\n')
     for source in src:
      dep=subprocess.check_output([cc,'-std=c11','-M',*defines,*includes,source],text=True).replace('\\\n',' ')
      for token in dep.split()[1:]:
       path=Path(token).resolve()
       for prefix,base in [('Utilities',ROOT),('System',system),('Runtime',runtime)]:
        if path.is_relative_to(base):record['sources'][prefix+'/'+str(path.relative_to(base))]=sha(path);break
    record['modules'].append(row);print(name+' '+label+' passed',flush=True)
 licenses=out/'licenses';licenses.mkdir(exist_ok=True)
 for repo,name in [(ROOT,'Utilities-MIT.txt'),(system,'System-MIT.txt'),(runtime,'Runtime-MIT.txt')]:shutil.copy2(repo/'LICENSE',licenses/name)
 shutil.copy2(system/'lib/PortableApps/time/TIMEZONE_PROVENANCE.json',licenses/'TIMEZONE_PROVENANCE.json')
 for directory in ['fonts','paper_fonts','quick_fonts']:
  dest=licenses/directory;dest.mkdir(exist_ok=True)
  for path in (system/'lib/PortableApps'/directory).iterdir():
   if 'LICENSE' in path.name or 'OFL' in path.name or path.name=='SOURCES.json':shutil.copy2(path,dest/path.name)
 record['licenses']={str(p.relative_to(licenses)):sha(p) for p in licenses.rglob('*') if p.is_file()}
 (out/'build-evidence.json').write_text(json.dumps(record,indent=2)+'\n')
 print('Native utility target imports/manifests/loader and flag-off byte comparison passed')
if __name__=='__main__':main()
