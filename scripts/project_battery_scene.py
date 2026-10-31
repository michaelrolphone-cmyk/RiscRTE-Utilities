#!/usr/bin/env python3
"""Project an already-selected Battery policy; never seed or rewrite user settings."""
import copy,re

def project(boot,app,provider,provider_sha256,*,default_enabled,preference_capability="storage.preferences"):
 if preference_capability not in ("storage.preferences","storage.key-value.atomic"):raise ValueError("Unknown preference contract")
 if type(default_enabled) is not bool or not re.fullmatch('[0-9a-f]{64}',provider_sha256):raise ValueError('Explicit verified provider policy/hash required')
 if any(app.get(k)!=v for k,v in {'id':'battery','type':'application','architecture':'xtensa-esp32s3','file_name':'battery.elf','entry':'app_main'}.items()):raise ValueError('Unexpected Battery identity')
 expected={'ui.scene','board.battery','telemetry.broadcast','storage.key-value','storage.preferences','storage.key-value.atomic'}
 if app.get('requires') or len(app.get('optional',[]))!=len(expected) or {(x.get('capability'),x.get('api')) for x in app.get('optional',[])}!={(x,1) for x in expected}:raise ValueError('Unexpected Battery dependencies')
 pid='telemetry-broadcast' if default_enabled else 'telemetry-broadcast-default-off'
 if any(provider.get(k)!=v for k,v in {'id':pid,'type':'driver','architecture':'xtensa-esp32s3','file_name':'driver.elf','driver_abi':2}.items()):raise ValueError('Provider policy identity mismatch')
 if provider.get('provides')!=[{'capability':'telemetry.broadcast','api':1}] or len(provider.get('requires',[]))!=2 or {(x.get('capability'),x.get('api')) for x in provider.get('requires',[])}!={('platform.clock',1),('bluetooth.telemetry',1)}:raise ValueError('Provider authority changed')
 result=copy.deepcopy(boot);rows=[x for x in result['app_capabilities'] if x.get('manifest')=='battery.json']
 if len(rows)!=1:raise ValueError('Selected Battery policy must be unique')
 row=rows[0];old=row.get('grants',[])
 def one(cap,instance=None):
  found=[copy.deepcopy(x) for x in old if x.get('capability')==cap and x.get('api')==1 and (instance is None or x.get('instance_id')==instance)]
  if len(found)!=1:raise ValueError('Expected one existing '+cap+' grant')
  return found[0]
 gauge=one('board.battery');kv=one('storage.key-value',1);broadcast=one('telemetry.broadcast')
 if kv.get('instance_id')!=1 or broadcast.get('instance_id')!=0:raise ValueError('Existing shared preference/service identity changed')
 # Presentation and native preference endpoints are separately admitted by the
 # selected Runtime; this projection is not evidence of endpoint execution.
 row['grants']=[{'api':1,'capability':'ui.scene','instance_id':0},gauge,broadcast,kv,{'api':1,'capability':preference_capability,'instance_id':1}]
 drivers=[x for x in result['drivers'] if x.get('manifest')=='broadcast/manifest.json']
 if len(drivers)!=1:raise ValueError('Selected broadcast driver must be unique')
 driver=drivers[0];sd=driver.get('image_source')
 if sd:
  path=sd.get('path','')
  if not re.fullmatch(r'Drivers/telemetry-broadcast/image-[0-9a-f]{64}\.elf',path) or not isinstance(sd.get('volume_instance'),int) or sd['volume_instance']<=0:raise ValueError('Unknown content-addressed provider route')
  sd['path']='Drivers/telemetry-broadcast/image-'+provider_sha256+'.elf'
 shell=result.get('resident_shell')
 if not isinstance(shell,dict) or sum(xs.count('battery.elf') for xs in [shell.get('legacy',[]),shell.get('foreground',[])])!=1:raise ValueError('Expected one existing resident Battery selection')
 if 'battery.elf' in shell.get('legacy',[]):shell['legacy'].remove('battery.elf')
 if 'battery.elf' not in shell['foreground']:shell['foreground'].append('battery.elf')
 return result
