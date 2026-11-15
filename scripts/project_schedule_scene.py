"""Pure policy projection for a semantic schedule app; never edits device data.

The caller supplies the reviewed raw/UTC configuration key and capability.
Only that app's grants and one additive provider row change. The deployed
scheduler, old storage namespaces, source routing and unrelated rows stay exact.
"""
import copy
from pathlib import PurePosixPath

def one(values, predicate, label):
    found=[value for value in values if predicate(value)]
    if len(found)!=1: raise ValueError(label+' must resolve uniquely')
    return found[0]

def safe(path):
    p=PurePosixPath(path)
    if not isinstance(path,str) or not path or path=='.' or any(ord(c)<32 or ord(c)==127 for c in path) or p.is_absolute() or '..' in p.parts or str(p)!=path or '\\' in path:
        raise ValueError('Unsafe package path')
    return path

def project(boot, manifests, app_manifest, control_manifests, namespace, used_namespaces,
            *, app_id, control_capability, config_keys):
    if not app_id or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-_' for c in app_id):
        raise ValueError('Unsafe app ID')
    if not isinstance(namespace,int) or isinstance(namespace,bool) or namespace<=0 or namespace>0x7fffffff or namespace in used_namespaces:
        raise ValueError('Private draft namespace is not unallocated')
    app_path=app_id+'.json'; control_path=app_id+'-control/manifest.json'
    oldrow=one(boot['app_capabilities'],lambda row:row.get('manifest')==app_path,'Application policy')
    old=manifests[app_path]
    for key in ('type','architecture','entry','id','file_name'):
        if old.get(key)!=app_manifest.get(key):raise ValueError('Application execution identity differs')
    if any(g.get('capability')=='storage.app-data' for g in oldrow['grants']):
        raise ValueError('Existing draft namespace must be reconciled, not reallocated')
    def offered(row,cap):return [v for v in manifests[row['manifest']].get('provides',[]) if v.get('capability')==cap]
    scheduler=one(boot['drivers'],lambda row:bool(offered(row,'alarm.service')),'Selected scheduler')
    version=one(offered(scheduler,'alarm.service'),lambda item:item['api'] in (1,2),'Scheduler API')['api']
    mode='raw' if version==1 else 'utc'; control=copy.deepcopy(control_manifests[mode])
    expected_clock={'capability':'rtc.clock','api':2} if version==1 else {'capability':'platform.realtime','api':1}
    expected_dependencies=[{'capability':'storage.key-value.bound','api':1},{'capability':'alarm.service','api':version},expected_clock]
    if sorted(control.get('requires',[]),key=lambda d:d.get('capability',''))!=sorted(expected_dependencies,key=lambda d:d['capability']) or control.get('optional',[]):
        raise ValueError('Unreviewed control dependency contract')
    identity={'type':'driver','driver_abi':2,'architecture':app_manifest['architecture'],'file_name':'driver.elf','id':app_id+'-control-'+mode}
    if any(control.get(k)!=v for k,v in identity.items()):raise ValueError('Control package identity differs')
    if control.get('provides')!=[{'capability':control_capability,'api':1}]:raise ValueError('Unreviewed provided capabilities')
    required=one(control['requires'],lambda d:d['capability']=='alarm.service','Control scheduler dependency')
    if required['api']!=version:raise ValueError('Control/scheduler API mismatch')
    key=config_keys[mode]
    binding=one(scheduler.get('key_value',[]),lambda row:row['key']==key,'Existing schedule config')
    if not any(g.get('capability')=='storage.key-value' and g.get('instance_id')==binding['namespace'] for g in oldrow['grants']):
        raise ValueError('New control would access previously unauthorized data')
    if control_path in manifests or any(offered(row,control_capability) for row in boot['drivers']):
        raise ValueError('Existing control needs explicit reconciliation')
    if {'capability':control_capability,'api':1} not in control['provides']:
        raise ValueError('Control capability mismatch')
    scene=one(boot['drivers'],lambda row:{'capability':'ui.scene','api':1} in offered(row,'ui.scene'),'Scene provider')
    grants=[{'capability':'ui.scene','api':1,'instance_id':scene.get('instance_id',0)},
            {'capability':control_capability,'api':1,'instance_id':0},
            {'capability':'storage.app-data','api':1,'instance_id':namespace}]
    selected={(g['capability'],g['api']) for g in grants}
    required={(d['capability'],d['api']) for d in app_manifest.get('requires',[])}
    declared=required|{(d['capability'],d['api']) for d in app_manifest.get('optional',[])}
    if not required<=selected<=declared:raise ValueError('Application capability contract differs')
    out=copy.deepcopy(boot)
    policy=one(out['app_capabilities'],lambda row:row.get('manifest')==app_path,'Projected app')
    policy['grants']=grants
    row={'manifest':control_path,'key_value':[{'key':key,'namespace':binding['namespace'],'access':'read-write'}]}
    out['drivers'].append(row)
    deployed=copy.deepcopy(old);deployed.update(copy.deepcopy(app_manifest))
    route={'internal_manifest':app_path,'manifest':deployed}
    if 'image_source' in oldrow:
        route['image_source']=copy.deepcopy(oldrow['image_source']);safe(route['image_source']['path'])
        if PurePosixPath(route['image_source']['path']).name!=deployed['file_name']:
            raise ValueError('Content-addressed or renamed image source requires exact new artifact mapping')
        route['sd_manifest']=str(PurePosixPath(route['image_source']['path']).parent/'manifest.json')
        if route['image_source'].get('internal_fallback'):route['internal_image']=safe(deployed['file_name'])
    else:route['internal_image']=safe(deployed['file_name'])
    assert out['drivers'][:-1]==boot['drivers']
    assert [r for r in out['app_capabilities'] if r['manifest']!=app_path]==[r for r in boot['app_capabilities'] if r['manifest']!=app_path]
    assert {k:v for k,v in policy.items() if k!='grants'}=={k:v for k,v in oldrow.items() if k!='grants'}
    return {'boot':out,'app':route,'control':{'clock_binding':mode,'manifest':control,'driver':row,'internal_manifest':control_path,'internal_image':app_id+'-control/driver.elf'},'preserved_scheduler':copy.deepcopy(scheduler),'new_private_draft_namespace':namespace,'scope':'Policy projection, not a packaged image, target execution or physical acceptance.'}
