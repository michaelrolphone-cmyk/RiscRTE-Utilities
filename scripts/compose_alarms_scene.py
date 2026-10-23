#!/usr/bin/env python3
"""Stage the Alarms prototype into a copy of an existing product store.

The output is a component stage, NOT a flash image or admitted product cohort.
No existing source, store, native image, application data, or device is mutated.
The product's normal native/cohort/image builder must bind this stage afterward.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath


def encoded(value):return (json.dumps(value,sort_keys=True,indent=2)+'\n').encode()
def digest(data):return {'size_bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
def require(condition,message):
    if not condition:raise ValueError(message)
def safe(name):
    require(isinstance(name,str) and name and not name.startswith('/') and '\\' not in name and
            '..' not in PurePosixPath(name).parts and '.' not in PurePosixPath(name).parts,'Unsafe relative path')
    return name

def files_at(folder):
    folder=Path(folder);require(folder.is_dir() and not folder.is_symlink(),'Expected a real input directory')
    result={}
    for path in folder.rglob('*'):
        require(not path.is_symlink(),'Input contains a symlink')
        if path.is_file():result[safe(path.relative_to(folder).as_posix())]=path.read_bytes()
    require(sum(map(len,result.values()))<=160*1024*1024,'Store input is too large')
    return result

def package(folder,application=False):
    folder=Path(folder);manifest_name='alarms.json' if application else 'manifest.json'
    require(not folder.is_symlink(),'Package link is not supported')
    manifest=json.loads((folder/manifest_name).read_bytes());receipt=json.loads((folder/'build.json').read_bytes())
    filename=safe(manifest['file_name']);require('/' not in filename,'Package file must be local')
    elf=(folder/filename).read_bytes()
    require(elf[:7]==b'\x7fELF\x01\x01\x01' and elf[16:20]==b'\x03\x00\x5e\x00','Expected Xtensa application/provider ELF')
    require(digest(elf)=={k:receipt[k] for k in ('size_bytes','sha256')} and
            (receipt['id'],receipt['version'])==(manifest['id'],manifest['version']),'Package and receipt differ')
    require(receipt['exports']==(['app_main'] if application else ['t5_driver_get']),'Unexpected exported package ABI')
    return manifest,elf,receipt

def compose(store,system_packages,alarm_packages,profile,output,runtime=None,native_elf=None):
    output=Path(output);require(not output.exists(),'Output must be a new directory')
    profile=json.loads(Path(profile).read_bytes())
    require(profile.get('schema')=='riscrte.declarative-alarms.prototype' and profile.get('schema_version')==1,'Unknown prototype profile')
    require(profile.get('version')=='0.1.0' and profile.get('presentation') in ('compact-color','portrait-monochrome') and
            profile.get('alarm_control') in ('alarm-control-raw','alarm-control-utc'),'Invalid presentation/domain selection')
    ns=profile.get('scene_state_namespace');require(type(ns) is int and 1<=ns<=0x7fffffff,'Invalid scene state namespace')
    capacity=profile.get('provider_capacity');require(type(capacity) is int and 1<=capacity<=64,'Invalid capacity requirement')
    original=files_at(store);result=dict(original);boot=json.loads(result['boot.json'])
    drivers=boot['drivers'];policies=boot['app_capabilities'];require(isinstance(drivers,list) and isinstance(policies,list),'Invalid boot graph')
    # Resolve the scheduler's existing storage binding instead of changing its
    # key namespace, time basis, occurrence ledger or output policy.
    native=profile['alarm_control']=='alarm-control-utc';service_api=2 if native else 1
    matching=[]
    for row in drivers:
        m=json.loads(result[safe(row['manifest'])])
        if {'capability':'alarm.service','api':service_api} in m.get('provides',[]):matching.append((row,m))
    require(len(matching)==1,'Select exactly one compatible existing alarm scheduler')
    scheduler,scheduler_manifest=matching[0]
    key='alarm_utc_cfg' if native else 'alarm_cfg';preference='time_zone' if native else 'alarm_volume'
    bindings=scheduler.get('key_value',[])
    def binding(name,access):
        matches=[b for b in bindings if b.get('key')==name]
        require(len(matches)==1 and type(matches[0].get('namespace')) is int,'Missing exact scheduler key binding: '+name)
        return {'key':name,'namespace':matches[0]['namespace'],'access':access}
    control_bindings=[binding(key,'read-write'),binding(preference,'read' if native else 'read-write')]
    for row in policies:
        for grant in row.get('grants',[]):
            require(not(grant.get('capability')=='storage.app-data' and grant.get('instance_id')==ns),'Scene namespace already belongs to an application')
    for row in drivers:
        for grant in row.get('app_data',[]):
            require(grant.get('namespace')!=ns,'Scene namespace already belongs to a provider')
    app_rows=[row for row in policies if row.get('manifest')=='alarms.json']
    require(len(app_rows)==1,'Existing Alarms policy must be unique')
    require('alarms.elf' in result and 'alarms.json' in result,'Existing Alarms package is missing')
    old=json.loads(result['alarms.json']);require(old.get('id')=='alarms','Unexpected Alarms identity')
    manifest,elf,app_receipt=package(Path(alarm_packages)/'alarms',True)
    def version(value):
        parts=value.split('.');require(len(parts)==3 and all(p.isdecimal() for p in parts),'Invalid package version');return tuple(map(int,parts))
    require(version(manifest['version'])>version(old['version']),'Alarms prototype must increment the installed version')
    require(manifest['requires']==[{'capability':name,'api':1} for name in ('ui.scene','alarm.control','storage.app-data')],
            'Application must depend only on intent, domain and state services')
    result['alarms.json']=encoded(manifest);result['alarms.elf']=elf
    app_rows[0].clear();app_rows[0].update({'manifest':'alarms.json','grants':[
        {'capability':'ui.scene','api':1,'instance_id':0},
        {'capability':'alarm.control','api':1,'instance_id':0},
        {'capability':'storage.app-data','api':1,'instance_id':ns}]})
    selected=[]
    for folder in (Path(system_packages)/profile['presentation'],Path(system_packages)/'scene-host',Path(alarm_packages)/profile['alarm_control']):
        m,blob,receipt=package(folder);identity=m['id'];safe(identity);require('/' not in identity,'Invalid package id')
        name=identity+'/manifest.json';elf_name=identity+'/'+safe(m['file_name'])
        require(name not in result and elf_name not in result,'Prototype provider already exists')
        result[name]=encoded(m);result[elf_name]=blob;row={'manifest':name}
        if identity==profile['alarm_control']:row['key_value']=control_bindings
        drivers.append(row);selected.append(receipt)
    require(len(drivers)<=capacity,'Selected providers exceed the requested native capacity')
    result['boot.json']=encoded(boot)
    # A previous product/cohort hash must never masquerade as the modified store.
    prior_cohort=result.pop('cohort.json',None)
    linked=None
    if native_elf is not None:
        require(runtime is not None,'Native capacity verification requires the Runtime source')
        spec=importlib.util.spec_from_file_location('runtime_capacity',Path(runtime)/'scripts/runtime_capacity.py')
        require(spec is not None and spec.loader is not None,'Missing native capacity verifier')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        raw=Path(native_elf).read_bytes();linked=module.read_capacity(raw)
        require(linked['providers']>=len(drivers),'Linked native capacity is too small')
        linked['elf']=digest(raw)
    changed=sorted(n for n in result if n not in original or result[n]!=original[n])
    allowed={'alarms.elf','alarms.json','boot.json'}|{p for p in result if p not in original}
    require(set(changed)<=allowed,'Unexpected store modification')
    record={'schema':'riscrte.declarative-alarms.stage','schema_version':1,'profile':profile,
            'app':app_receipt,'providers':selected,'provider_count':len(drivers),'native_capacity':linked,
            'native_build_flags':[f'-DRISC_RUNTIME_PROVIDER_CAPACITY={capacity}','-Wl,-u,risc_runtime_provider_capacity'],
            'selected_scheduler':{'id':scheduler_manifest['id'],'version':scheduler_manifest['version'],
                                  'manifest_sha256':digest(original[scheduler['manifest']])['sha256']},
            'source_store':{n:digest(b) for n,b in sorted(original.items())},
            'files':{n:digest(b) for n,b in sorted(result.items())},'changed_files':changed,
            'removed_files':['cohort.json'] if prior_cohort is not None else [],
            'flashable':False,'product_native_cohort_binding_required':True,'physical_testing':'not performed'}
    output.mkdir(parents=True)
    for name,raw in result.items():
        path=output/'store'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw)
    (output/'stage.json').write_bytes(encoded(record))
    return record

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('store','system-packages','alarm-packages','profile','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--runtime',type=Path);p.add_argument('--native-elf',type=Path)
    a=p.parse_args();r=compose(a.store,a.system_packages,a.alarm_packages,a.profile,a.output,a.runtime,a.native_elf)
    print(json.dumps({'output':str(a.output),'provider_count':r['provider_count'],'app_sha256':r['app']['sha256'],
                      'flashable':False,'native_cohort_binding_required':True},indent=2))
