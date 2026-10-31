#!/usr/bin/env python3
"""Actual Runtime/Spectrum/scene/canvas/text; PCM, storage and physical endpoints doubled."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--system',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);rt=a.runtime.resolve();system=a.system.resolve();sys.path.insert(0,str(system/'scripts'));from scene_sdk import stage_sdk
inc=stage_sdk(rt,system,out/'sdk')
for directory in [system/'lib/PortableApps/include',system/'sdk/app',rt/'sdk/app',rt/'sdk/driver',rt/'sdk/hardware']:
 for h in directory.glob('*.h'):shutil.copyfile(h,inc/h.name)
shutil.copyfile(ROOT/'lib/Bluetooth/include/TelemetryBroadcastPolicyV1.h',inc/'TelemetryBroadcastPolicyV1.h');fix=ROOT/'test/spectrum_runtime'
tracked=set([Path(__file__).resolve(),*fix.glob('*'),*ROOT.glob('Apps/spectrum_scene*'),ROOT/'Apps/audio_spectrum_scene.c']);tracked.update(p for base in [system/'Services/scene_host',system/'Services/scene_profile',system/'Services/canvas_host',system/'Services/text_input',rt/'src',rt/'sdk'] for p in base.rglob('*') if p.is_file());initial={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked if p.is_file()};(out/'initial-source.json').write_text(json.dumps(initial,indent=2)+'\n')
flags=['-O0','-g0','-pipe','-Wall','-Wextra','-Werror'];san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if a.sanitize else [];incs=['-I'+str(inc),'-I'+str(system/'Services/scene_profile'),'-I'+str(system/'Services/text_input'),'-I'+str(ROOT/'Apps')];commands=[];env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
def run(cmd):
 commands.append(list(map(str,cmd)));(out/'commands.json').write_text(json.dumps(commands,indent=2));r=subprocess.run(cmd,text=True,capture_output=True,env=env);(out/f'command-{len(commands):03}.log').write_text(r.stdout+r.stderr);r.check_returncode();return r.stdout
obj=out/'objects';obj.mkdir(exist_ok=True)
actual_dependencies={}
def dependencies(compiler,src,options):
 dep=obj/('closure-'+str(len(actual_dependencies))+'.d')
 run([compiler,*options,'-MM','-MF',str(dep),str(src)])
 for path in dep.read_text().replace('\\\n',' ').split(':',1)[1].split():
  p=Path(path).resolve();actual_dependencies[str(p)]=hashlib.sha256(p.read_bytes()).hexdigest()
 (out/'actual-dependencies.json').write_text(json.dumps(actual_dependencies,indent=2)+'\n')
def build(src,name,extra=[]):
 for unit in src:dependencies('cc',unit,['-std=c11',*incs,*extra])
 run(['cc','-std=c11',*flags,*san,*incs,*extra,'-fPIC','-fvisibility=hidden','-shared','-MMD','-MF',str(obj/(name+'.d')),*map(str,src),'-o',str(obj/name)])
build([fix/'app.c'],'default.elf')
build([fix/'host.c'],'host.elf')
build([system/'Services/scene_host/host.c'],'scene.elf')
for paper in [0,1]:build([system/'Services/scene_profile/profile.c'],f'profile{paper}.elf',['-DSCENE_PROFILE_ID="profile"',f'-DSCENE_PROFILE_PAPER={paper}','-DSCENE_DISPLAY_ROTATION=0'])
build([system/'Services/canvas_host/host.c'],'canvas.elf')
build([system/'Services/text_input/host.c'],'text.elf')
providers=[('display','display.output'),('touch','input.touch.raw'),('nav','input.navigation'),('mic','audio.input')]
for n,c in providers:build([fix/'provider.c'],n+'.elf',[f'-DTEST_ID="{n}"',f'-DTEST_CAP="{c}"',f'-DTEST_API={2 if n=="kv" else 1}'])
sources=['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']
cpp_includes=[*incs,'-I'+str(rt/'src'),'-I'+str(rt/'lib/ArduinoJson/src'),'-I'+str(rt/'test/drivers/stubs'),'-I'+str(rt/'test'),'-I'+str(fix)]
for unit in [*[rt/'src'/x for x in sources],fix/'runtime_test.cpp']:dependencies('c++',unit,['-std=c++17',*cpp_includes])
run(['c++','-std=c++17',*flags,*san,'-Wno-missing-field-initializers','-fno-pie','-no-pie','-rdynamic',*incs,'-I'+str(rt/'src'),'-I'+str(rt/'lib/ArduinoJson/src'),'-I'+str(rt/'test/drivers/stubs'),'-I'+str(rt/'test'),'-I'+str(fix),*[str(rt/'src'/s) for s in sources],str(fix/'runtime_test.cpp'),'-ldl','-o',str(out/'test')])
def req(c):return {'capability':c,'api':2 if c=='storage.key-value' else 1}
def manifest(n,c):return {'type':'driver','id':n,'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':n+'.elf','requires':[],'provides':[req(c)]}
def write(d,n,v):(d/n).write_text(json.dumps(v,indent=2)+'\n')
results=[]
for label,w,h,f in [('paper',800,480,1),('watch',240,240,5),('wave',410,502,5)]:
 for activation in ['demand','demand-retained']:
  for mode in ['composed','standalone','no-ui','attention']:
   d=out/f'{label}-{activation}-{mode}';d.mkdir(exist_ok=True)
   for n in ['default','host','scene','canvas','text',*[n for n,c in providers]]:
    dest=d/(n+'.elf')
    if dest.exists():dest.unlink()
    os.link(obj/(n+'.elf'),dest)
   shutil.copyfile(obj/f'profile{int(f==1)}.elf',d/'profile.elf')
   for n,c in providers:write(d,n+'.json',manifest(n,c))
   for n,folder in [('scene','scene_host'),('canvas','canvas_host'),('text','text_input')]:
    m=json.loads((system/'Services'/folder/'manifest.json').read_text());m['file_name']=n+'.elf';write(d,n+'.json',m)
   write(d,'profile.json',manifest('profile','ui.presentation-profile'))
   write(d,'board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':[]})
   caps=['ui.scene','ui.canvas','ui.text-input','storage.key-value','audio.input']
   write(d,'default.json',{'type':'application','id':'default','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':[],'optional':[req(c) for c in caps]})
   grants=[{**req(c),'instance_id':7 if c=='storage.key-value' else 0} for c in ['ui.text-input','storage.key-value','audio.input']]
   if mode!='no-ui':grants.append({**req('ui.scene'),'instance_id':0})
   if mode=='standalone':grants.append({**req('ui.canvas'),'instance_id':0})
   boot={'board':'board.json','default_app':'default.elf','provider_activation':activation,'drivers':[{'manifest':n+'.json'} for n in ['scene','canvas','text','profile',*[n for n,c in providers]]],'app_capabilities':[{'manifest':'default.json','grants':grants}]}
   if mode=='attention':
    write(d,'host.json',{'type':'application','id':'host','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'host.elf','entry':'app_main','requires':[],'optional':[]})
    boot['default_app']='host.elf';boot['resident_shell']={'api':1,'host':'host.elf','foreground':['default.elf']};boot['app_capabilities'].append({'manifest':'host.json','grants':[]})
   write(d,'boot.json',boot)
   result=run([str(out/'test'),str(d),mode,str(w),str(h),str(f)]).strip();print(result,flush=True);results.append(result)
for path,digest in actual_dependencies.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,'Compiled dependency changed: '+path
for path,digest in initial.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,'Source changed: '+path
(out/'receipt.json').write_text(json.dumps({'results':results,'sanitize':a.sanitize,'app_sha256':hashlib.sha256((obj/'default.elf').read_bytes()).hexdigest(),'actual_compiled_dependencies':actual_dependencies,'scope':'Real Runtime/app/scene/canvas/text; doubled physical, microphone and KV2 endpoints. Native input and actual unload/reopen; no physical or full-learning claim.'},indent=2)+'\n')
