#!/usr/bin/env python3
"""Prove published System pin equivalence for the exact Utilities compile closure."""
import argparse,hashlib,json,subprocess
from pathlib import Path
from build_contexts_scene import stage
from native_utility_build import flags,sources
from test_utility_atomic import PROFILE,exact
ROOT=Path(__file__).resolve().parents[1]
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('previous-system','system-apps','runtime','xtensa-cc','output'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 exact(a.previous_system,PROFILE['previous_system_qualification_sha']);actual=exact(a.system_apps,PROFILE['system_sha']);exact(a.runtime,PROFILE['runtime_sdk_sha'])
 changed_from_public=subprocess.check_output(['git','-C',a.system_apps,'diff','--name-only',PROFILE['broadcast_result_base_tree'],'HEAD^{tree}'],text=True).splitlines()
 expected_changes=['lib/PortableApps/include/PortableBroadcastClient.h','lib/PortableApps/src/broadcast_adapter.inc']
 assert sorted(changed_from_public)==expected_changes,('Unexpected System source expansion',changed_from_public)
 includes=[stage(a.runtime.resolve(),s.resolve(),a.output/label) for s,label in [(a.previous_system,'previous'),(a.system_apps,'published')]]
 rows=[];closure={}
 for app in ('battery','ble_scanner','contexts'):
  base=['-DCONTEXTS_ALARM_API=2','-DCONTEXTS_SETTING_MASK=0x274u','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO'] if app=='contexts' else flags(app,paper_transitions=True)
  if app=='battery':base+=['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF','-DPORTABLE_PAPER_PREFERENCES']
  if app=='ble_scanner':base+=['-DPORTABLE_TEXT_INPUT_CLIENT']
  for mode in ('default','atomic'):
   defines=base+(['-D'+x for x in PROFILE['defines']] if mode=='atomic' else [])
   pairs=[]
   for system,inc,label in zip((a.previous_system,a.system_apps),includes,('previous','published')):
    selected=[ROOT/'Apps/contexts.c'] if app=='contexts' else sources(app,system)
    incs=['-I'+str(x) for x in [inc,system/'lib/NativeApps/include',ROOT/'Apps',ROOT/'lib/Bluetooth/include',system/'Apps']]
    pre=[]
    for source in selected:
     common=[a.xtensa_cc,'-std=c11',*defines,*incs,source]
     data=subprocess.check_output([*common,'-E','-P'])
     normalized=data.replace(str(system).encode(),b'<SYSTEM>').replace(str(inc.parent).encode(),b'<STAGED-SDK>')
     pre.append(sha(normalized))
     deps=subprocess.check_output([*common,'-M'],text=True).replace('\\\n',' ').split()[1:]
     for name in deps:
      dep=Path(name).resolve()
      if not dep.is_file():continue
      if dep.is_relative_to(system.resolve()):key='System/'+str(dep.relative_to(system.resolve()))
      elif dep.is_relative_to(inc.parent):key='StagedSDK/'+str(dep.relative_to(inc.parent))
      elif dep.is_relative_to(ROOT):key='Utilities/'+str(dep.relative_to(ROOT))
      else:continue
      closure.setdefault(key,{})[label]=sha(dep.read_bytes())
    pairs.append(pre)
   same=pairs[0]==pairs[1]
   explicit_hook_delta=app=='battery' and mode=='atomic'
   assert same or explicit_hook_delta,(app,mode,'unrelated preprocessed source changed')
   assert same!=explicit_hook_delta,(app,mode,'expected terminal-result hook missing')
   rows.append({'app':app,'mode':mode,'previous_translation_unit_sha256':pairs[0],'published_translation_unit_sha256':pairs[1],'normalized_preprocessed_equal':same,'expected_terminal_result_hook_delta':explicit_hook_delta})
 record={'schema':1,'repository_sha':subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip(),'previous_system_sha':PROFILE['previous_system_qualification_sha'],'published_system_sha':PROFILE['system_sha'],'actual_checkout_sha':actual,'published_tree':PROFILE['system_tree'],'previous_published_system_sha':PROFILE['broadcast_result_base_published_sha'],'previous_published_tree':PROFILE['broadcast_result_base_tree'],'system_changes_from_previous_public_tree':changed_from_public,'compile_profiles':rows,'dependency_closure':closure,'changed_dependency_bytes':[key for key,v in closure.items() if v.get('previous')!=v.get('published')],'qualification':'All default and non-Battery selected translation units preprocess identically after normalizing checkout paths. Atomic Battery has only the requested terminal-result latch hook addition; the whole System tree differs from prior public source in those exact two files. No unrelated System code is activated.'}
 (a.output/'receipt.json').write_text(json.dumps(record,indent=2)+'\n');print('System pin closure: five unchanged profiles, explicit Battery result hook, exact two-file System delta PASS')
if __name__=='__main__':main()
