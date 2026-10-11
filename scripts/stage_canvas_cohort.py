#!/usr/bin/env python3
"""Compose the selected canvas apps without changing existing app authority."""
import argparse,hashlib,json,shutil,sys,os
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',choices=['watch','x4'],required=True)
p.add_argument('--ui-stage',type=Path,help='Watch scene/Lists integration stage; only its two apps and three providers are selected')
for n in ('baseline','apps','launchers','output','watch-tools'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();assert not a.output.exists();shutil.copytree(a.baseline,a.output)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def write(p,v):p.write_text(json.dumps(v,indent=2,sort_keys=True)+'\n')
boot=read(a.output/'boot.json');receipts={}
if a.target=='watch':
 assert a.ui_stage is not None
 ui=read(a.ui_stage/'boot.json')
 for name in ['contexts','lists']:
  for ext in ['elf','json']:shutil.copyfile(a.ui_stage/(name+'.'+ext),a.output/(name+'.'+ext))
  row=next(r for r in ui['app_capabilities'] if r['manifest']==name+'.json')
  boot['app_capabilities']=[r for r in boot['app_capabilities'] if r['manifest']!=name+'.json']+[row]
 aliases={'scene-host':'ui-scene','scene-profile-compact-color':'ui-profile','civil-clock-watch':'civil-clock'}
 for old,new in aliases.items():
  row=next(r for r in ui['drivers'] if r['manifest']==old+'/manifest.json')
  assert row=={'manifest':old+'/manifest.json'}
  (a.output/new).mkdir()
  for filename in ['driver.elf','manifest.json']:shutil.copyfile(a.ui_stage/old/filename,a.output/new/filename)
  boot['drivers'].append({'manifest':new+'/manifest.json'})
 receipts['watch_ui_stage']={str(p.relative_to(a.ui_stage)):digest(p) for p in a.ui_stage.rglob('*') if p.is_file()}

for name in ['default','springboard']:
 folder=a.launchers if a.target=='watch' else a.launchers/name
 for ext in ['elf','json']:shutil.copyfile(folder/(name+'.'+ext),a.output/(name+'.'+ext))
 receipts[name]=read(folder/('build.json' if a.target=='watch' else 'build-evidence.json' if name=='default' else 'springboard-build-record.json'))
 if a.target=='watch':receipts[name]=receipts[name][name]
 assert digest(a.output/(name+'.elf'))==receipts[name]['sha256']
for name in (['model_viewer'] if a.target=='watch' else ['model_viewer','hollow_trail']):
 folder=a.apps/name;m=read(folder/(name+'.json'));r=read(folder/'build.json');assert digest(folder/(name+'.elf'))==r['sha256']
 for ext in ['elf','json']:shutil.copyfile(folder/(name+'.'+ext),a.output/(name+'.'+ext))
 assert all(row['manifest']!=name+'.json' for row in boot['app_capabilities'])
 ids={'display.output':5 if a.target=='watch' else 3,'input.touch.raw':6 if a.target=='watch' else 4,'input.navigation':6,'storage.volume':9,'file.open':0,'storage.app-data':64}
 row={'manifest':name+'.json','grants':[{**q,'instance_id':ids[q['capability']]} for q in m['requires']]}
 boot['app_capabilities'].append(row);receipts[name]=r
owners=[r['manifest'] for r in boot['app_capabilities'] for g in r['grants'] if g['capability']=='storage.app-data' and g['instance_id']==64]
assert owners==['model_viewer.json']
assert len(boot['drivers'])==27 and len(boot['app_capabilities'])==(25 if a.target=='watch' else 24)
# Remove only unallocated toolchain metadata, with byte-for-byte loaded-section,
# relocation and export equivalence proof; never remove a game chapter or app.
sys.path.insert(0,str(a.watch_tools.resolve()/'scripts'))
from compact_current_elf import compact
cc=os.environ.get('NATIVE_APP_CC') or str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc')
compactions={}
for name in ['default','springboard','model_viewer']+(['hollow_trail'] if a.target=='x4' else []):
 compactions[name]=compact(a.output/(name+'.elf'),cc,debug_path=a.output.parent/(a.output.name+'-debug')/(name+'.elf'))
receipts['packaged_metadata_compaction']=compactions
write(a.output/'boot.json',boot)
write(a.output.parent/(a.output.name+'-stage.json'),{'target':a.target,'baseline_files':{str(p.relative_to(a.baseline)):digest(p) for p in a.baseline.rglob('*') if p.is_file()},'receipts':receipts,'stage_files':{str(p.relative_to(a.output)):digest(p) for p in a.output.rglob('*') if p.is_file()},'app_count':len(boot['app_capabilities']),'provider_count':len(boot['drivers']),'hardware_tested':False})
