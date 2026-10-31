"""Pure Spectrum policy projection; package bytes and native admission are separate."""
import copy,re
from pathlib import PurePosixPath
IDENTITY={'type':'application','id':'audio_spectrum','file_name':'audio_spectrum.elf','architecture':'xtensa-esp32s3','entry':'app_main'}
RETIRED={'display.output','input.touch.raw','input.navigation','input.power-key','power.sleep','display.power','rtc.clock','alarm.service','board.battery','net.wifi','bluetooth.hci','motion.accel','app.catalog','storage.key-value.atomic'}
ADDED={('ui.scene',1,0),('runtime.realtime',1,0)}
def safe(path):
 if not isinstance(path,str) or not path or any(ord(c)<32 or ord(c)==127 for c in path):raise ValueError('invalid path')
 if str(PurePosixPath(path))!=path or path.startswith('/') or any(x in ('','.','..') for x in path.split('/')):raise ValueError('unsafe path')
 return path
def key(g):
 if not isinstance(g,dict) or not isinstance(g.get('capability'),str) or type(g.get('api'))!=int or type(g.get('instance_id'))!=int or g['api']<=0 or g['instance_id']<0:raise ValueError('invalid grant')
 return g['capability'],g['api'],g['instance_id']
def project(boot,canonical,elf_sha256,*,new_grants=None):
 if not re.fullmatch('[0-9a-f]{64}',elf_sha256):raise ValueError('invalid ELF digest')
 if any(canonical.get(k)!=v for k,v in IDENTITY.items()) or canonical.get('requires')!=[] or canonical.get('provides') not in (None,[]) or type(canonical.get('abi_version',1))!=int or canonical.get('abi_version',1)!=1:raise ValueError('unexpected canonical Spectrum manifest')
 declared=set()
 for d in canonical.get('optional',[]):
  if set(d)!={'capability','api'} or not isinstance(d['capability'],str) or type(d['api'])!=int or d['api']<=0 or (d['capability'],d['api']) in declared:raise ValueError('malformed optional contract')
  declared.add((d['capability'],d['api']))
 if any(cap in RETIRED for cap,api in declared):raise ValueError('physical and automatic-setting duties must remain host-owned')
 out=copy.deepcopy(boot);rows=[r for r in out['app_capabilities'] if r.get('manifest')=='audio_spectrum.json']
 if len(rows)>1:raise ValueError('duplicate Spectrum policy')
 if rows:
  if new_grants is not None:raise ValueError('do not replace existing authority')
  row=rows[0];old=copy.deepcopy(row);grants=[];seen=set()
  for g in old['grants']:
   k=key(g)
   if k in seen:raise ValueError('duplicate original grant')
   seen.add(k)
   if k[0] in RETIRED:continue
   if k[:2] not in declared:raise ValueError('undeclared original semantic authority')
   grants.append(g)
  host=out.get('resident_shell',{}).get('host');hosts=[r for r in out['app_capabilities'] if r.get('manifest')==str(PurePosixPath(host or '').with_suffix('.json'))]
  if len(hosts)!=1:raise ValueError('original resident host required')
  host_keys={key(g) for g in hosts[0]['grants']}
  for g in old['grants']:
   k=key(g)
   if k[0] in {'display.output','input.touch.raw','alarm.service','power.sleep','input.power-key','rtc.clock','board.battery','net.wifi','bluetooth.hci','motion.accel','display.power'} and k not in host_keys:raise ValueError('missing host lifecycle authority '+k[0])
  for k in sorted(ADDED):
   if k[:2] not in declared:raise ValueError('missing replacement declaration')
   if k not in {key(g) for g in grants}:grants.append(dict(capability=k[0],api=k[1],instance_id=k[2]))
  row['grants']=grants
 else:
  # New optional installation uses source-qualified generic UI and saved-data
  # backends. No microphone, settings, or Contexts authority is inferred.
  allowed={('ui.scene',1,0),('ui.text-input',1,0),('storage.key-value',2,7),('storage.app-data',1,2),('runtime.realtime',1,0)}
  if new_grants is None or {key(g) for g in new_grants}!=allowed or len(new_grants)!=len(allowed) or any(set(g)!={'capability','api','instance_id'} for g in new_grants):raise ValueError('explicit reviewed new optional policy required')
  if any(key(g)[:2] not in declared for g in new_grants):raise ValueError('undeclared new UI authority')
  for original in out['app_capabilities']:
   for g in original['grants']:
    if (g['capability']=='storage.app-data' and g['instance_id']==2) or (g['capability']=='storage.key-value' and g['instance_id']==7):raise ValueError('new saved-data authority collides with existing app')
  row={'manifest':'audio_spectrum.json','grants':copy.deepcopy(new_grants)};out['app_capabilities'].append(row)
 if 'image_source' in row:
  route=row['image_source'];path=safe(route['path'])
  if type(route.get('volume_instance'))!=int or route['volume_instance']<=0:raise ValueError('invalid SD volume')
  if re.search(r'image-[0-9a-f]{64}\.elf$',path):route['path']=re.sub(r'(?<=image-)[0-9a-f]{64}(?=\.elf$)',elf_sha256,path)
  elif PurePosixPath(path).name!='audio_spectrum.elf':raise ValueError('unreviewed original route')
 shell=out.get('resident_shell')
 if not isinstance(shell,dict) or shell.get('api')!=1 or shell.get('host')!=out.get('default_app'):raise ValueError('admitted resident host required')
 fg=shell.setdefault('foreground',[])
 if 'audio_spectrum.elf' not in fg:fg.append('audio_spectrum.elf')
 shell['legacy']=[p for p in shell.get('legacy',[]) if p!='audio_spectrum.elf']
 # Selected Runtime declaration arrays are bounded: project optional declarations
 # to exactly the granted subset, never change executable bytes or grant scope.
 selected=copy.deepcopy(canonical);pairs={(g['capability'],g['api']) for g in row['grants']};selected['optional']=[d for d in canonical['optional'] if (d['capability'],d['api']) in pairs]
 return out,selected

def replace_scene(boot,old_manifest,new_manifest,elf_sha256):
 if not re.fullmatch('[0-9a-f]{64}',elf_sha256):raise ValueError('invalid provider digest')
 if any(new_manifest.get(k)!=v for k,v in {'type':'driver','id':'scene-host','architecture':'xtensa-esp32s3','driver_abi':2,'file_name':'driver.elf'}.items()) or type(new_manifest.get('driver_abi'))!=int:raise ValueError('invalid scene identity')
 for field in ('provides','requires'):
  if not isinstance(new_manifest.get(field),list) or any(set(d)!={'api','capability'} or type(d['api'])!=int or not isinstance(d['capability'],str) for d in new_manifest[field]):raise ValueError('invalid scene declaration')
 for field in ('type','id','architecture','driver_abi','file_name','provides','requires'):
  if new_manifest.get(field)!=old_manifest.get(field):raise ValueError('scene contract drift: '+field)
 if new_manifest.get('provides')!=[{'api':1,'capability':'ui.scene'}] or new_manifest.get('version')!='0.4.12':raise ValueError('unexpected scene canvas package')
 out=copy.deepcopy(boot);rows=[r for r in out['drivers'] if r.get('manifest')=='ui-scene/manifest.json']
 if len(rows)!=1:raise ValueError('unique original scene provider required')
 if 'image_source' in rows[0]:
  route=rows[0]['image_source'];path=safe(route['path'])
  if type(route.get('volume_instance'))!=int or route['volume_instance']<=0:raise ValueError('invalid provider SD volume')
  if not re.search(r'image-[0-9a-f]{64}\.elf$',path):raise ValueError('unreviewed provider route')
  route['path']=re.sub(r'(?<=image-)[0-9a-f]{64}(?=\.elf$)',elf_sha256,path)
 return out
