#!/usr/bin/env python3
import pathlib,tempfile,sys,subprocess,json,hashlib,os,datetime,argparse
from lifecycle_dependencies import Closure
REPOSITORY=pathlib.Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description="Actual Runtime/ProviderGraph canvas lifecycle qualification")
parser.add_argument('--canvas',type=pathlib.Path,required=True)
parser.add_argument('--runtime',type=pathlib.Path,required=True)
parser.add_argument('--system',type=pathlib.Path,default=REPOSITORY)
parser.add_argument('--output',type=pathlib.Path,required=True)
parser.add_argument('--sanitize',action='store_true')
args=parser.parse_args()
ROOT=REPOSITORY/'test/provider_lifecycle/canvas';SYSTEM=args.system.resolve();RUNTIME=args.runtime.resolve();CANVAS=args.canvas.resolve()
OUTPUT=args.output.resolve();OUTPUT.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(SYSTEM/'scripts'));from scene_sdk import stage_sdk
SANITIZED=args.sanitize
MODE="sanitized" if SANITIZED else "normal"
commands=[]
closure=None
def run(command,env):
 if closure is not None:closure.record(command)
 commands.append(list(map(str,command)));(OUTPUT/('commands-'+MODE+'.json')).write_text(json.dumps(commands,indent=2)+'\n')
 result=subprocess.run(command,capture_output=True,text=True,env=env)
 if result.returncode:
  (OUTPUT/('FAILURE-'+MODE+'.json')).write_text(json.dumps({'command':commands[-1],'exit':result.returncode,'stdout':result.stdout,'stderr':result.stderr},indent=2)+'\n');raise RuntimeError(result.stderr)
 return result.stdout
with tempfile.TemporaryDirectory(prefix='canvas-native-runtime-') as temporary:
 out=pathlib.Path(temporary);include=stage_sdk(RUNTIME,SYSTEM,out/'sdk')
 closure=Closure([('candidate',REPOSITORY),('runtime',RUNTIME),('system',SYSTEM),('canvas',CANVAS),('generated-sdk',include)],OUTPUT/('DEPENDENCIES-'+MODE+'.json'))
 flags=['-O1','-Wall','-Wextra','-Werror']+(['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if SANITIZED else []);env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0');incs=['-I'+str(include),'-I'+str(CANVAS/'sdk/app')]
 def shared(source,name):run(['cc','-std=c11',*flags,*incs,'-fPIC','-fvisibility=hidden','-shared',str(source),'-o',str(out/name)],env)
 shared(CANVAS/'Services/canvas_host/host.c','canvas.elf');shared(SYSTEM/'Services/scene_profile/profile.c','profile.elf');shared(ROOT/'provider.c','display.elf');shared(ROOT/'app.c','default.elf')
 def write(name,value):(out/name).write_text(json.dumps(value,indent=2)+'\n')
 def req(cap):return {'capability':cap,'api':1}
 def manifest(name,cap,requires):return {'type':'driver','id':{'canvas':'canvas-host','profile':'scene-presentation-profile'}.get(name,name),'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':name+'.elf','requires':[req(c) for c in requires],'provides':[req(cap)]}
 write('canvas.json',manifest('canvas','ui.canvas',['display.output','ui.presentation-profile']));write('profile.json',manifest('profile','ui.presentation-profile',[]));write('display.json',manifest('display','display.output',[]))
 write('board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':[]})
 write('default.json',{'type':'application','id':'default','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':[req('ui.canvas')]})
 write('boot.json',{'board':'board.json','default_app':'default.elf','provider_activation':'eager','drivers':[{'manifest':name+'.json'} for name in ['canvas','profile','display']],'app_capabilities':[{'manifest':'default.json','grants':[{**req('ui.canvas'),'instance_id':0}]}]})
 sources=['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']
 run(['c++','-std=c++17',*flags,'-Wno-missing-field-initializers','-O0','-fno-pie','-no-pie','-rdynamic',*incs,'-I'+str(RUNTIME/'src'),'-I'+str(RUNTIME/'test'),'-I'+str(RUNTIME/'lib/ArduinoJson/src'),'-I'+str(RUNTIME/'test/drivers/stubs'),*[str(RUNTIME/'src'/p) for p in sources],str(ROOT/'runtime_test.cpp'),'-Wl,--wrap=dlopen','-Wl,--wrap=dlclose','-ldl','-o',str(out/'test')],env)
 stdout='';case_inputs=[]
 for case in range(5):
  boot=json.loads((out/'boot.json').read_text());boot['provider_activation']='demand' if case==3 else 'eager';write('boot.json',boot)
  case_inputs.append({'case':case,'provider_activation':boot['provider_activation'],'boot_sha256':hashlib.sha256((out/'boot.json').read_bytes()).hexdigest()})
  stdout+=run([str(out/'test'),str(out),str(case)],env)
 (OUTPUT/('RESULT-'+MODE+'.json')).write_text(json.dumps({'case_inputs':case_inputs,'observedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'stdout':stdout,'sanitized':SANITIZED,'binary_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in list(out.glob('*.elf'))+[out/'test']},'scope':'Five actual Runtime/ProviderGraph host cases, modeled display; no native target or physical execution. Full compiled translation-unit dependency closure recorded separately; generated SDK inputs explicit.'},indent=2)+'\n');print(stdout)
