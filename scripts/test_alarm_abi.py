#!/usr/bin/env python3
"""Compile exact lineage providers/clients separately and exercise the ABI matrix.
Old cross-lineage v1 suffix dispatch is intentionally unsupported (and never called).
"""
import argparse, os, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
out=ROOT/'build/alarm-abi';out.mkdir(parents=True,exist_ok=True)
cc=os.environ.get('CC','cc');san=os.environ.get('ALARM_TEST_SANITIZERS','address,undefined')
inc=['-I'+str(a.runtime.resolve()/'sdk/driver'),'-I'+str(a.runtime.resolve()/'sdk/app'),'-I'+str(a.system_apps.resolve()/'lib/PortableApps/include')]
def run(args):subprocess.run([str(x) for x in args],check=True,timeout=120)
refs={'paper':'bb81ab0cdc137e1b2fddcfbe191880c1c6e649ad','visual':'4be6383256d0742dd724538d720bd5b5d3802f89'}
for name,ref in refs.items():
 d=out/name;d.mkdir(exist_ok=True)
 for f in subprocess.check_output(['git','ls-tree','-r','--name-only',ref,'lib/Alarm/include'],cwd=ROOT,text=True).splitlines():
  (d/Path(f).name).write_bytes(subprocess.check_output(['git','show',ref+':'+f],cwd=ROOT))
 (d/'service.c').write_bytes(subprocess.check_output(['git','show',ref+':Services/alarm_service/service.c'],cwd=ROOT))
# Frozen visual helper is actual old production header; paper guard is its documented client contract.
for name in ('paper','visual'):
 (out/name/'client.c').write_text('''#include "AlarmServiceV1.h"
#include "PortableAlarmClient.h"
static const alarm_service_v1 *candidate;
static bool acquire(const char *id,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *g) {
 (void)id;(void)instance;if(api!=candidate->api_version)return false;g->api=candidate;return true;
}
int legacy_accept(const alarm_service_v1 *s) { candidate=s;portable_alarm_client c;
 const risc_runtime_api_v1 r={.acquire=acquire};return portable_alarm_open(&c,&r); }
int legacy_suffix(const alarm_service_v1 *s) {
 if(!legacy_accept(s))return -1;
'''+('return s->struct_size>=ALARM_SERVICE_SLEEP_V1_SIZE && ((const alarm_service_sleep_v1 *)s)->resume_sleep!=0;' if name=='paper' else 'return (int)alarm_service_output_modes(s);')+'}\n')
(out/'matrix.c').write_text('''#include "AlarmServiceV2.h"
#include "RiscProviderV2.h"
#include <dlfcn.h>
#include <assert.h>
#include <stdio.h>
int main(int argc,char **argv){assert(argc==5);void *p=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);assert(p);
 const risc_driver_v2 *(*get)(uint32_t)=dlsym(p,"t5_driver_get");assert(get);const risc_driver_v2 *driver=get(2);
 const alarm_service_v1 *s=driver->capability;const alarm_service_descriptor_v2 *d=alarm_service_descriptor(s);
 int expected=argv[4][0]-'0';assert((d!=NULL)==(expected==2));
 for(int i=2;i<=3;i++){void *c=dlopen(argv[i],RTLD_NOW|RTLD_LOCAL);assert(c);
 int (*accept)(const alarm_service_v1*)=dlsym(c,"legacy_accept");assert(accept);assert(accept(s)==(expected!=2));
 /* Cross-legacy suffix calls would be unsafe; verify only matching source family. */
 if(expected==i-2){int (*suffix)(const alarm_service_v1*)=dlsym(c,"legacy_suffix");assert(suffix);assert(suffix(s)==(expected==0?1:0));}
 dlclose(c);}
 alarm_sleep_v1 ticket={.struct_size=sizeof(ticket)};
 assert(alarm_service_resume(s,&ticket)==ALARM_INVALID); /* stopped production provider or rejected v1 */
 if(d){alarm_service_descriptor_v2 bad=*d;
 bad.tag^=1;assert(!alarm_service_descriptor(&bad.base));bad=*d;bad.descriptor_version++;assert(!alarm_service_descriptor(&bad.base));
 bad=*d;bad.base.struct_size--;assert(!alarm_service_descriptor(&bad.base));bad=*d;bad.output_modes=4;assert(!alarm_service_descriptor(&bad.base));
 bad=*d;bad.features=0;assert(!alarm_service_descriptor(&bad.base));bad=*d;bad.resume_sleep=NULL;assert(!alarm_service_descriptor(&bad.base));
 bad=*d;bad.base.api_version=1;assert(!alarm_service_descriptor(&bad.base));
 assert(d->output_modes==0 || d->output_modes==3);}
 dlclose(p);puts("Production ABI matrix row passed");}
''')
for sanitized in (False,True):
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']+(['-fsanitize='+san,'-fno-sanitize-recover=all'] if sanitized else [])
 clients=[]
 for name in refs:
  target=out/(name+'-client.so');run([cc,*flags,'-shared','-fPIC','-I'+str(out/name),*inc,out/name/'client.c','-o',target]);clients.append(target)
 exe=out/'matrix';run([cc,*flags,'-I'+str(ROOT/'lib/Alarm/include'),*inc,out/'matrix.c','-ldl','-o',exe])
 profiles=[('visual',[],3),('paper',[],0),('visual',['-DALARM_VISUAL_ONLY'],1),('new',[],2),('new',['-DALARM_VISUAL_ONLY'],2),('new',['-DALARM_VISUAL_ONLY','-DALARM_NATIVE_UTC'],2)]
 for index,(name,defs,expected) in enumerate(profiles):
  include=out/name if name in refs else ROOT/'lib/Alarm/include';source=out/name/'service.c' if name in refs else ROOT/'Services/alarm_service/service.c'
  if name=='new':defs=defs+['-DALARM_SERVICE_TAGGED_V2']
  extra=[a.system_apps.resolve()/'lib/PortableApps/src'/f for f in ('PortableTimeZone.c','PortableTimeZoneCatalog.c')] if '-DALARM_NATIVE_UTC' in defs else []
  target=out/f'provider-{index}.so';run([cc,*flags,'-shared','-fPIC','-I'+str(include),*inc,'-DPOINTS_IN_TIME_SERVICE','-DALARM_DND_CONTROL',*defs,source,*extra,'-o',target]);run([exe,target,*clients,expected])
print('6 production providers x 3 clients: normal + '+san+'; ambiguous legacy cross-lineage suffix dispatch deliberately forbidden')
