#!/usr/bin/env python3
"""Native BLE Buttons controller + production adapter qualification.

Modeled display/touch/Bluetooth services, physical coordinates and native raster
calls. No hardware, transport, RF, target-instruction or physical touch claim.
"""
import argparse, gzip, hashlib, json, os, re, shlex, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SYSTEM_PIN='f3731ab02127dea86a308472e3780068c5a0a1cf'
SIZES=((128,128),(160,128),(240,240),(410,502),(320,180),(502,410),(480,800))
TEMPORAL_CASES={
 0:'Complete contact before initial visibility is discarded; fresh contact recovers',
 1:'Contact held across initial visibility is discarded on release',
 2:'Owned release survives same-layout redraw',
 3:'Changed layout rejects stale release and requires fresh contact',
 4:'Repeated native begin cannot inherit old pending token visibility',
 5:'Ordinary overlay owner zero cannot arm the native page',
 6:'Owned queued release delivered once across redraw',
 7:'Snapshot observation failure retains custody',
 8:'Backward snapshot sequence rejects input and recovers',
 9:'Malformed snapshot dimensions reject input and recover',
 10:'Unobservable presentation retains token/image/grant custody',
 11:'Raw held HID assignment releases during same-layout redraw',
 12:'Multicontact interruption releases already held raw key',
 13:'Layout change releases held raw assignment',
 14:'New raw contact during pending redraw cannot activate assignment',
 15:'Two ordered fast raw contacts preserve both neutralized reports',
}
TEMPORAL_SCENARIOS=tuple(f'temporal-{i}' for i in TEMPORAL_CASES)
BASE_DEFINES=('PORTABLE_NOVA_UI','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RADIO_SESSION',
 'PORTABLE_ALARM_CLIENT','PORTABLE_FORCE_FULL_FRAMES','PORTABLE_APP_SLEEP_LOCAL',
 'PORTABLE_INPUT_NAVIGATION','PORTABLE_INPUT_NAVIGATION_LOCAL','PORTABLE_QUICK_ACTIONS',
 'PORTABLE_QUICK_RADIOS','PORTABLE_RETURN_APP="springboard.elf"','PORTABLE_PAPER_HID',
 'PORTABLE_HOME_APP="default.elf"','PORTABLE_LOW_BATTERY',
 'PORTABLE_BLE_BUTTONS_NATIVE_UI','PORTABLE_NATIVE_CANVAS')
def sha(data):return hashlib.sha256(data).hexdigest()
def scenes(l):
 def at(n,key):
  x,y,w,h=l[key];return (n,x+w//2,y+h//2)
 def seq(*keys):return [at(10+20*i,key) for i,key in enumerate(keys)]
 def checks(*keys,**values):return {**{k:'1' for k in keys},**{k:str(v) for k,v in values.items()}}
 s={
 'idle':([],{}),
 'alarm':([at(10,'footer0'),at(38,'button0'),at(39,'button0'),(60,(l['content'][0]*2+l['content'][2])//2,166*(l['content'][0]*2+l['content'][2])//240)],checks('ACTIVE','ALARM')),
 'quick-controls':([at(10,'footer0'),(30,(l['content'][0]*2+l['content'][2])//2,2),(31,(l['content'][0]*2+l['content'][2])//2,(l['footer0'][1]+l['footer0'][3])//4),(32,(l['content'][0]*2+l['content'][2])//2,(l['footer0'][1]+l['footer0'][3])*3//4),(90,(l['content'][0]*2+l['content'][2])//2,l['footer0'][1]+l['footer0'][3]-18)],checks('ACTIVE','QUICK')),
 'back-top':(seq('back'),checks('BACK')),
 'reconnect':(seq('footer0'),checks('ACTIVE',EXPECT_OPENS=1)),
 'stop':(seq('footer0','footer0'),checks('ACTIVE',EXPECT_OPENS=1)),
 'settings':(seq('footer1'),checks(EXPECT_VIEW=1)),
 'settings-back':(seq('footer1','back'),checks(EXPECT_VIEW=0)),
 'forget-back':(seq('footer1','setting_forget','back'),checks(EXPECT_VIEW=0,EXPECT_FORGETS=0)),
 'pair-back':(seq('footer1','setting_pair0','back'),checks('ACTIVE','BACK','PENDING_CANCEL')),
 'pair-timeout-reconnect':([at(10,'footer1'),at(30,'setting_pair0'),at(100,'footer0')],checks('ACTIVE','PAIR_TIMEOUT')),
 'pair-error-reconnect':([at(10,'footer1'),at(30,'setting_pair0'),at(100,'footer0')],checks('ACTIVE','PAIR_ERROR')),
 'settings-reconnect':(seq('footer1','setting_pair1'),checks('ACTIVE',EXPECT_OPENS=1)),
 'forget-cancel':(seq('footer1','setting_forget','footer0'),checks(EXPECT_VIEW=0,EXPECT_FORGETS=0,EXPECT_VIEWS=19)),
 'forget-confirm':(seq('footer1','setting_forget','footer1'),checks(EXPECT_VIEW=0,EXPECT_FORGETS=1,EXPECT_VIEWS=19)),
 'edit-cancel':(seq('footer1','setting_button0','edit_next','footer0'),checks('UNSAVED',EXPECT_VIEW=0,EXPECT_VIEWS=7)),
 'edit-save':(seq('footer1','setting_button0','edit_next','edit_modifiers','modifier0','back','footer1'),checks('SAVED',EXPECT_VIEW=0,EXPECT_VIEWS=15)),
 'save-retry':(seq('footer1','setting_button0','edit_next','footer1','footer1'),checks('SAVED','SAVE_FAIL',EXPECT_VIEW=0)),
 'kind-cycle':(seq('footer1','setting_button0','edit_kind','edit_kind','edit_kind','edit_kind'),checks('UNSAVED',EXPECT_VIEW=2,EXPECT_KIND=0)),
 'value-roundtrip':(seq('footer1','setting_button0','edit_next','edit_prev'),checks('UNSAVED',EXPECT_VIEW=2,EXPECT_DRAFT_KEY=75)),
 'modifiers-all':(seq('footer1','setting_button0','edit_modifiers',*[f'modifier{i}' for i in range(8)]),checks('UNSAVED',EXPECT_VIEW=3,EXPECT_MODIFIERS=255,STOP_POLLS=280)),
 'nested-back':(seq('footer1','setting_button0','edit_modifiers','back','back','back'),checks('BACK',EXPECT_VIEWS=15)),
 'pair-accept':(seq('footer1','setting_pair0','footer1'),checks('ACTIVE','PAIR','PAIR_ACCEPT')),
 'pair-reject':(seq('footer1','setting_pair0','footer0'),checks('ACTIVE','PAIR','PAIR_REJECT')),
 'pair-move':([at(10,'footer1'),at(30,'setting_pair0'),at(60,'footer1'),(61,l['footer1'][0]+1,l['footer1'][1]+1),at(90,'footer1')],checks('ACTIVE','PAIR','PAIR_ACCEPT','PAIR_RECOVERED','PAIR_MOVED',PAIR_RECOVERY_AT=90)),
 'pair-multitouch':([at(10,'footer1'),at(30,'setting_pair0'),at(60,'footer1'),at(60,'footer0'),at(90,'footer1')],checks('ACTIVE','PAIR','PAIR_ACCEPT','PAIR_RECOVERED','PAIR_MULTITOUCH',PAIR_RECOVERY_AT=90)),
 'disconnect':([at(10,'footer0'),at(38,'button0'),at(39,'button0')],checks('ACTIVE','DISCONNECT')),
 'low-battery':(seq('footer0'),checks('ACTIVE','LOW_BATTERY')),
 'sleep':(seq('footer0'),checks('ACTIVE','SLEEP')),
 'raw-gap':([at(10,'footer0'),at(38,'button0'),at(39,'button0'),at(40,'button0'),at(41,'button0')],checks('ACTIVE','GAP')),
 'cleanup':(seq('footer0','back'),checks('ACTIVE','BACK','CLEANUP')),
 'home':(seq('footer0'),checks('ACTIVE','HOME')),
 'raw-home':(seq('footer0'),checks('ACTIVE','HOME','RAW_HOME')),
 'edit-home':(seq('footer1','setting_button0','edit_next'),checks('HOME','UNSAVED')),
 'repeat-entry':(seq('footer1'),checks('REPEAT',EXPECT_VIEW=1)),
 'previsible-tap':([at(2,'footer1')],checks('PREVISIBLE',PRESENT_DELAY=120,STOP_POLLS=170)),
 'previsible-held':([at(i,'footer1') for i in range(2,100)],checks('PREVISIBLE','HELD_PREVISIBLE',PRESENT_DELAY=120,STOP_POLLS=190)),
 }
 if 'alarm_dismiss' in l:
  actions,flags=s['alarm'];s['alarm']=(actions[:-1]+[at(60,'alarm_dismiss')],flags)
 if 'quick_handle' in l:
  actions,flags=s['quick-controls'];s['quick-controls']=(actions[:-1]+[at(90,'quick_handle')],flags)
 width=l['content'][0]*2+l['content'][2];height=l['footer0'][1]+l['footer0'][3]
 if 'alarm_dismiss' not in l and 144*width//240>=height:
  actions,flags=s['alarm'];s['alarm']=(actions[:-1],{**flags,'ALARM_BACK':'1'})
 for i in range(4):
  s[f'configure-button{i+1}']=(seq('footer1',f'setting_button{i}'),checks(EXPECT_VIEW=2,EXPECT_SELECTED=i))
  s[f'press-button{i+1}']=(seq('footer0',f'button{i}'),checks('ACTIVE','KEYS'))
 # Gap belongs to the second raw subscriber; interruption occurs after session open.
 s['pair-gap']=([at(10,'footer1'),at(30,'setting_pair0'),at(38,'footer1'),at(39,'footer1'),at(40,'footer1'),at(80,'footer1')],checks('ACTIVE','PAIR','PAIR_ACCEPT','PAIR_RECOVERED','GAP',PAIR_RECOVERY_AT=80))
 return s

def full_scenes(layout):
 planned=scenes(layout)
 planned.pop('home');planned.pop('edit-home')
 planned['physical-back']=(planned['reconnect'][0],{'ACTIVE':'1','BACK':'1','FULL_BACK':'1'})
 return planned

def dependency_identity(path, *, roots, utilities, system, catalog, target=None):
 if path==catalog:return 'generated:catalog.c'
 if target:
  if path.is_relative_to(target/'sdk'):
   for owner,relative in [('runtime','sdk/app'),('runtime','sdk/driver'),('system','sdk/app'),('system','lib/PortableApps/include'),('utilities','lib/Alarm/include'),('utilities','lib/Bluetooth/include'),('utilities','lib/Contexts/include')]:
    other=roots[owner]/relative/path.name
    if other.exists() and sha(other.read_bytes())==sha(path.read_bytes()):return owner+':'+relative+'/'+path.name
   if path.is_relative_to(target/'sdk/time'):return 'system:lib/PortableApps/time/'+str(path.relative_to(target/'sdk/time'))
   raise ValueError('Unowned aggregate header '+str(path))
 candidates=[*roots.items(),('system',system),('utilities',utilities)]
 # Keep staged Utilities and the checkout as separate aliases of one owner.
 # Most-specific paths win when CI nests every dependency under the checkout.
 for owner,root in sorted(candidates,key=lambda item:len(item[1].parts),reverse=True):
  if path.is_relative_to(root):return owner+':'+str(path.relative_to(root))
 raise ValueError('Unowned dependency '+str(path))

def receipt_source_hashes(before, identity):
 # Files inside the checkout are not necessarily Utilities-owned: the target
 # SDK and pinned dependency inputs are nested below it on GitHub Actions.
 dependencies={}
 for path,digest in sorted(before.items()):
  name=identity(path)
  if name in dependencies and dependencies[name]!=digest:
   raise ValueError('Conflicting consumed source identity: '+name)
  dependencies[name]=digest
 sources={name.split(':',1)[1]:digest for name,digest in dependencies.items() if name.startswith('utilities:')}
 return sources,dependencies

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
 p.add_argument('--allow-system-changes',action='store_true',help='Diagnostic worktree run; exact compiled hashes recorded, no pin-only claim');p.add_argument('--full-target',type=Path,help='Full-profile target qualification folder with immutable inputs and aggregate SDK');p.add_argument('--runtime',type=Path);p.add_argument('--sanitize',action='store_true');p.add_argument('--async',dest='async_rgb565',action='store_true');p.add_argument('--scene',action='append');p.add_argument('--temporal',action='store_true');p.add_argument('--snapshot',action='store_true');p.add_argument('--size',action='append')
 p.add_argument('--mixed-contacts',action='store_true',help='Run the sampled-hold/queued-repeat controller regression; leaves the frozen release matrix unchanged')
 a=p.parse_args();system=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 if a.mixed_contacts and not (a.temporal and a.async_rgb565):p.error('--mixed-contacts requires --temporal --async')
 if subprocess.check_output(['git','-C',str(system),'rev-parse','HEAD'],text=True).strip()!=SYSTEM_PIN:raise ValueError('System checkout must match pinned adapter')
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 defines=['-D'+s for s in BASE_DEFINES]+(['-DPORTABLE_ASYNC_RGB565'] if a.async_rgb565 else [])
 flags=defines+['-I'+str(x) for x in (system/'Apps',ROOT/'Apps',ROOT/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include')]
 sources=[ROOT/'test/native_apps/ble_buttons_native_test.c',system/'lib/PortableApps/src/adapter.c',catalog,*[system/'lib/PortableApps/src'/s for s in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]]
 target=a.full_target.resolve() if a.full_target else None
 roots={}
 if target:
  roots={owner:target/'inputs'/owner for owner in ('system','runtime','bridges','utilities','generated')}
  deployed=json.loads((roots['system']/'docs/shared-app-crashes/replay/wave.json').read_text())['ble_buttons']['command']
  target_receipt=json.loads((target/'qualification.json').read_text()) if (target/'qualification.json').exists() else {}
  native_selectors=target_receipt.get('native_selectors',['-DPORTABLE_BLE_BUTTONS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS'])
  defines=[x for x in deployed if x.startswith('-D')]+native_selectors
  if not a.async_rgb565:defines.remove('-DPORTABLE_ASYNC_RGB565')
  includes=(target/'sdk/include',roots['system']/'lib/PortableApps/include',roots['system']/'lib/NativeApps/include',roots['system']/'Apps',ROOT/'Apps',ROOT/'lib/Alarm/include',roots['bridges']/'sdk/app',roots['bridges']/'sdk/driver',roots['bridges']/'include',ROOT/'lib/Bluetooth/include',ROOT/'lib/Contexts/include',roots['generated'],roots['runtime']/'sdk/app',ROOT/'tests')
  flags=defines+['-DHID_RENDER_FULL_PROFILE','-I'+str(ROOT/'test/native_apps')]+['-I'+str(x) for x in includes]+['-include',str(roots['bridges']/'apps/clock/faces/ContextsFaces.h'),'-include',str(roots['generated']/'watch-alarm-profile.h')]
  sources=[ROOT/'test/native_apps/ble_buttons_native_test.c',roots['system']/'lib/PortableApps/src/adapter.c',roots['generated']/'empty_catalog.c',*[roots['bridges']/'apps/clock'/s for s in ('portable_navigation.c','portable_sleep.c')],*[roots['system']/'lib/PortableApps/src'/s for s in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]]
 if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT']
 adapter_source=sources[1];sources[1]=ROOT/'test/native_apps/ble_buttons_native_adapter.c';flags+=['-DBLE_BUTTONS_ADAPTER_SOURCE='+json.dumps(str(adapter_source))]
 cc=os.getenv('CC','cc');deps={Path(__file__).resolve()}
 for source in sources:
  raw=subprocess.check_output([cc,'-std=c11','-MM',*flags,str(source)],text=True)
  deps.update(Path(s).resolve() for s in shlex.split(raw.replace('\\\n',' ').split(':',1)[1]))
 def identity(path):
  return dependency_identity(path,roots=roots,utilities=ROOT,system=system,catalog=catalog,target=target)
 before={path:sha(path.read_bytes()) for path in deps}
 for path in deps:
  if path.is_relative_to(system) and not target and not a.allow_system_changes:
   original=subprocess.check_output(['git','-C',str(system),'show',SYSTEM_PIN+':'+str(path.relative_to(system))])
   if path.read_bytes()!=original:raise ValueError('System source differs from pin '+str(path))
 binary=out/'ble-buttons-native-test'
 command=[cc,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*(['-Wno-unused-function','-Wno-unused-variable'] if target else []),*flags]
 if a.sanitize:command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 command += [*map(str,sources),'-Wl,--wrap=portable_nova_native_text','-Wl,--wrap=portable_nova_native_icon','-o',str(binary)]
 (out/'command.json').write_text(json.dumps(command,indent=2)+'\n')
 result=subprocess.run(command,text=True,capture_output=True);(out/'build.log').write_text(result.stdout+result.stderr)
 if result.returncode:raise RuntimeError('Build failed: '+result.stderr)
 results=[];captures={};counterexamples=[];raster_titles=set();raster_icons={}
 for w,h in SIZES:
  if a.size and f'{w}x{h}' not in a.size:continue
  layout=json.loads(subprocess.check_output([str(binary),'--layout','unused',str(w),str(h)],text=True))
  (out/f'layout-{w}x{h}.json').write_text(json.dumps(layout,indent=2)+'\n')
  planned=full_scenes(layout) if target else scenes(layout)
  if a.temporal:
   if not a.async_rgb565:raise ValueError('--temporal requires --async')
   planned={f'temporal-{i}':([],{'TEMPORAL':str(i)}) for i in range(16) if target or i not in (7,10)}
   if a.mixed_contacts:planned={'temporal-16':([],{'TEMPORAL':'16'})}
  if a.scene and set(a.scene)-planned.keys():raise ValueError('Unknown scene')
  for name,(actions,checks) in planned.items():
   if a.scene and name not in a.scene:continue
   folder=out/f'{w}x{h}'/name;folder.mkdir(parents=True,exist_ok=True)
   for f in folder.glob('frame-*.*'):f.unlink()
   path=folder/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions))
   env={k:v for k,v in os.environ.items() if not k.startswith('HID_RENDER_')};env.update({'HID_RENDER_'+k:v for k,v in checks.items()});env.update(ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
   try:run=subprocess.run([str(binary),str(folder),str(path),str(w),str(h)],env=env,text=True,capture_output=True,timeout=40)
   except subprocess.TimeoutExpired as e:
    stdout=e.stdout.decode() if isinstance(e.stdout,bytes) else e.stdout or '';stderr=e.stderr.decode() if isinstance(e.stderr,bytes) else e.stderr or '';run=subprocess.CompletedProcess([],124,stdout,stderr+'\nHOST TIMEOUT after40s\n')
   log=run.stdout+run.stderr;(folder/'run.log').write_text(log)
   for match in re.findall(r'NATIVE_TITLE face=(\d+) width=(\d+) height=(\d+) pixels=(\d+) hash=(\d+)',log):raster_titles.add(tuple(map(int,match)))
   for match in re.findall(r'NATIVE_ICON size=(\d+) pixels=(\d+) hash=(\d+)',log):
    size,pixels,hash_value=map(int,match);raster_icons.setdefault(size,set()).add((pixels,hash_value))
   frame_records=[]
   for f in sorted(folder.glob('frame-*.ppm')):
    raw=f.read_bytes();header=f'P6\n{w} {h}\n255\n'.encode();assert raw.startswith(header) and len(raw)==len(header)+w*h*3
    assert name in ('sleep','temporal-5') or len(set(raw[len(header):]))>8,'Blank or degenerate raster'
    packed=f.with_suffix('.ppm.gz');packed.write_bytes(gzip.compress(raw,mtime=0));f.unlink();relative=str(packed.relative_to(out));captures[relative]=sha(packed.read_bytes());frame_records.append(relative)
   if 'observed=1' in log:counterexamples.append({'scene':name,'width':w,'height':h,'observation':'Input changed app state before initial presentation completed','log':str((folder/'run.log').relative_to(out))})
   results.append({'width':w,'height':h,'scene':name,'exit_code':run.returncode,'log':str((folder/'run.log').relative_to(out)),'log_sha256':sha(log.encode()),'captures':frame_records})
   print(f'{w}x{h} {name}: '+('PASS' if run.returncode==0 else 'FAIL'),flush=True)
 for path,digest in before.items():
  if sha(path.read_bytes())!=digest:raise RuntimeError('Source changed during qualification: '+str(path))
 assert all(len(values)==1 for values in raster_icons.values()),'Icon changed pixel raster for the same physical box'
 assert a.snapshot or len(raster_titles)==1,'Title raster unexpectedly changes with display dimensions'
 source_hashes,dependency_hashes=receipt_source_hashes(before,identity)
 receipt={'schema':1,'recipe':'deployed full-feature BLE Buttons recipe; actual System adapter and Watch navigation/sleep bridges' if target else 'expanded existing HID host recipe; modeled local navigation/sleep','system_git_pin':SYSTEM_PIN,'system_working_tree_allowed':a.allow_system_changes,'build_defines':defines,'command':command,'sanitized':a.sanitize,'async_rgb565':a.async_rgb565,'required_scenarios':list(planned),'temporal_cases':{f'temporal-{i}':v for i,v in TEMPORAL_CASES.items()} if a.temporal else {},'temporal':a.temporal,'snapshot':a.snapshot,'full_profile':bool(target),'full_feature_profile':bool(target),'source_roots':{'utilities':str(ROOT),**{owner:str(path) for owner,path in roots.items() if owner!='utilities'}},'target_qualification_sha256':sha((target/'qualification.json').read_bytes()) if target and (target/'qualification.json').exists() else None,'results':results,'counterexamples':counterexamples,'source_sha256':source_hashes,'compiled_dependency_sha256':dependency_hashes,'source_unchanged_during_run':True,'binary_sha256':sha(binary.read_bytes()),'native_pixel_audit':{'title_raster':list(next(iter(raster_titles))) if raster_titles else None,'title_fields':['face','width','height','changed_pixels','fnv64_rgb565'],'icon_rasters':{str(size):list(next(iter(values))) for size,values in raster_icons.items()},'icon_fields':['changed_pixels','fnv64_rgb565'],'description':'Production native raster called directly; identical physical glyph pixels across capability dimensions. Icon box is independently sized.'},'capture_sha256':captures,'hardware_tested':False,'target_instructions_executed':False,'coverage_limits':['Modeled raw touch, RGB565 display, Bluetooth, alarm, radio, storage, power-key and power services; actual Watch navigation/sleep bridge code is linked in full-profile mode.','Native raster calls are production implementations; wrappers verify physical bounds, allowed faces and arrow-left icon.','The historical recipe without native overlays placed Alarm Dismiss below320x180 and rejected non240 Quick surfaces. Candidate explicitly selects the existing native Alarm/Quick overlays; touch dismissal and Quick open/close are tested on all seven profiles.','Async delay is modeled; no physical panel, touch controller, Bluetooth transport, radio behavior or target instructions executed.','Executed host temporal controls validate the native presentation/input owner, layout generations, stale contact suppression, held releases, queued taps, and terminal custody under modeled faults. They do not qualify physical timing or real hardware.','ASan/UBSan enabled only with --sanitize; LeakSanitizer disabled under ptrace.']}
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 failures=[r for r in results if r['exit_code']]
 print(f'Native BLE Buttons: {len(results)-len(failures)}/{len(results)} passed; {len(counterexamples)} previsibility counterexamples')
 if failures or counterexamples:raise SystemExit(1)
if __name__=='__main__':main()
