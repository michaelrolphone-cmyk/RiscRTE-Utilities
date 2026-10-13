#!/usr/bin/env python3
"""Bounded real-provider regression; native owner transports are test fixtures.

No source/provider edits, network requests, firmware images, or publication.
Full source profile retains all fonts, telemetry and optional presenter APIs.
"""
import argparse,sys,subprocess,os,shutil,json,hashlib,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description="Real final Contexts app with separately compiled actual scene/text providers; host qualification only.")
for name in ("runtime","system-apps","catalog-system","face-header","telemetry-font","output"):
 p.add_argument("--"+name,type=Path,required=True)
p.add_argument("--utilities-source",type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument("--legacy-system",type=Path,help="Materialized 1aaf53e scene source and original scene headers, required for mixed pair")
p.add_argument("--pair",choices=("mixed","cumulative"),default="cumulative")
p.add_argument("--geometry",choices=("240x240","410x502"),default="240x240")
p.add_argument("--sanitize",action="store_true")
p.add_argument("--all",action="store_true")
p.add_argument("--no-navigation",action="store_true")
p.add_argument("--catalog",action="store_true")
p.add_argument("--cases",nargs="+")
a=p.parse_args()
APP=a.utilities_source.resolve();SYSTEM=a.system_apps.resolve();RUNTIME=a.runtime.resolve();CATALOG=a.catalog_system.resolve();FACE=a.face_header.resolve();BASE=a.output.resolve();BASE.mkdir(parents=True,exist_ok=True)
if a.pair=="mixed" and not a.legacy_system:p.error("--legacy-system is required for mixed pairing")
if a.catalog and (a.pair!="cumulative" or not a.no_navigation):p.error("--catalog requires cumulative pair and --no-navigation")
if a.all and a.no_navigation:p.error("--all includes navigation-specific cases; omit --all for bounded touch/native-exit qualification")
if a.catalog and (a.cases or a.all):p.error("--catalog chooses its three real-presenter cases")
if hashlib.sha256(a.telemetry_font.read_bytes()).hexdigest()!="2d7501e42d6223ebbc191817715431182c435fea944a1a02166a0748a577eaf1":raise ValueError("Unexpected full-profile telemetry font input")
sys.path.insert(0,str(APP/"scripts"))
from build_contexts_scene import stage
if not a.cases:
 a.cases=["text-2-checkpoint-enable-unavailable"] if a.pair=="mixed" else ["text-2-"+mode for mode in ("done","header-back","header-home","native-home","delayed","frozen","hardware","checkpoint-idle","checkpoint-policy-exit","close-retained","release-fail","loss-poll")]
if a.all:
 from test_contexts_shared_input import LEGACY,EDIT,FAULT,CHECKPOINT
 a.cases=list(LEGACY)+[f'text-{r}-{m}' for r in range(5) for m in EDIT]+[f'text-2-{m}' for m in FAULT+CHECKPOINT]+[f'text-{r}-{m}' for r in (0,1,3,4) for m in ('checkpoint-idle','home-checkpoint-resume')]+['text-2-retry','text-2-reopen','text-2-prior-edit-cancel','text-0-learn-save']+[f'text-{r}-explicit-save' for r in (2,3,4)]
CATALOG_PROBE='\nstatic void review_update(void){\n cs_declare();assert(cs_components->update(cs_scene->context,cs_session,&cs_document)==RISC_SCENE_OK);session=cs_session;\n}\nstatic void review_event(void){\n risc_scene_event_v1 e;int rc=RISC_SCENE_IDLE;\n for(unsigned i=0;i<1000&&rc==RISC_SCENE_IDLE;i++)rc=tick(&e);\n assert(rc==RISC_SCENE_OK);assert(e.kind==RISC_SCENE_ACTION_EVENT||e.kind==RISC_SCENE_VALUE_EVENT);cs_event(&e);review_update();\n}\nstatic void review_back(void){back_input();review_event();settle();}\nint main(int argc,char**argv){\n assert(argc==2);setup();\n if(!strcmp(argv[1],"boundary96")){\n  inventory=96;cu_action(CU_ROW_LAUNCH);cs_declare();\n  assert(cs_catalog.count==96&&cs_document.node_count==2);\n  for(unsigned i=0;i<cs_document.node_count;i++)assert(cs_document.nodes[i].action<CU_RUNTIME_APP||cs_document.nodes[i].action>=CU_RUNTIME_APP+RISC_APP_CATALOG_MAX);\n  assert(!strcmp(cu_rows[0].title,"CATALOG EXCEEDS DISPLAY LIMIT"));cu_action(CU_RUNTIME_APP+95);assert(!cu_store.rules[0].steps);\n  inventory=95;publication++;cu_action(CU_CATALOG_REFRESH);cs_declare();assert(cs_document.node_count==96);cu_action(CU_RUNTIME_APP+94);assert(!strcmp(cu_store.rules[0].workflow[0].text,"app-94.elf"));\n }else if(!strcmp(argv[1],"key-boundaries")){\n  static const char fit[]="aaaaaaaaaaaaaaaaaaaaaaaaaaaa.elf";\n  static const char longkey[]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaa.elf";\n  assert(strlen(fit)==32&&strlen(longkey)==33);\n  route_override=fit;cu_action(CU_ROW_LAUNCH);cu_action(CU_RUNTIME_APP);assert(cu_store.rules[0].steps==1&&!strcmp(cu_store.rules[0].workflow[0].text,fit));\n  route_override=longkey;publication++;cu_action(CU_ROW_LAUNCH);cu_action(CU_RUNTIME_APP);assert(cu_store.rules[0].steps==1);\n }else if(!strcmp(argv[1],"smooth-back")){\n  startup();assert(api->close(NULL,session)==RISC_SCENE_OK);expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;\n  cs_scene=api;cs_components=risc_scene_components_get_v1(api);assert(cs_components);\n  inventory=95;cu_action(CU_ROW_LAUNCH);cs_declare();assert(cs_document.node_count==96);assert(cs_open_scene(NULL));session=cs_session;settle();\n  assert(contexts_test_geometry(w,h));int x,y;assert(contexts_test_target(CU_RUNTIME_APP,-999,&x,&y));assert(!contexts_test_target(CU_RUNTIME_APP+94,-999,&x,&y));\n  uint32_t key=cs_document.screen_key;tap(CU_RUNTIME_APP+94,-999);review_event();settle();\n  assert(cu_page==CU_WORKFLOW&&cu_edit&&cu_store.rules[0].steps==1&&!strcmp(cu_store.rules[0].workflow[0].text,"app-94.elf"));\n  review_back();assert(cu_page==CU_RULE&&cu_edit);\n  allow_complete=false;review_update();begin_frame();assert(presenting);back_input();review_event();allow_complete=true;settle();assert(cu_page==CU_CANCEL&&cu_edit);\n  review_back();assert(cu_page==CU_RULE&&cu_edit&&cu_store.rules[0].steps==1&&!strcmp(cu_store.rules[0].workflow[0].text,"app-94.elf"));\n  cu_action(CU_ROW_LAUNCH);review_update();settle();assert(cs_document.screen_key==key);assert(contexts_test_geometry(w,h));\n  inventory=0;publication++;tap(CU_CATALOG_REFRESH,-999);review_event();settle();assert(cs_catalog.count==0&&cu_page==CU_APPS&&cs_document.node_count==2);\n  review_back();assert(cu_page==CU_WORKFLOW&&cu_edit&&cu_store.rules[0].steps==1);\n  assert(cs_close_scene());assert(driver->quiesce());driver->stop();\n }else assert(0);\n catalog_finish();printf("Independent Contexts %s PASS\\\\n",argv[1]);return 0;\n}\n'
with tempfile.TemporaryDirectory(prefix='pair-',dir=BASE) as td:
 t=Path(td);inc=stage(RUNTIME,SYSTEM,t/'sdk',CATALOG);(inc/'fonts').mkdir();shutil.copyfile(a.telemetry_font,inc/'fonts/telemetry.inc');old=t/'old';shutil.copytree(inc,old)
 if a.pair=='mixed':
  for path in ['sdk/app/RiscSceneLifecycleV1.h','sdk/app/RiscSceneComponentsV1.h','sdk/app/RiscSceneCheckpointV1.h','Services/text_input/SceneKeyboardV1.h']:
   (old/Path(path).name).write_bytes((a.legacy_system/path).read_bytes())
 host=a.legacy_system/'Services/scene_host/host.c' if a.pair=='mixed' else SYSTEM/'Services/scene_host/host.c'
 cc=os.environ.get('CC','cc');flags=['-std=c11','-O1','-g0','-Wall','-Wextra','-Werror']
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 def run(cmd):subprocess.run(cmd,check=True)
 # Separate translation-unit headers preserve each provider's actual source ABI.
 (t/'probe.c').write_text((APP/'test/contexts/host_probe.c').read_text()+'\nbool contexts_test_geometry(unsigned w,unsigned h){if(width!=w||height!=h)return false;bool bottom=false;for(unsigned i=0;i<visible.count;i++){const hit*q=&visible.items[i];if(q->y+q->h>(int)h-70)bottom=true;if(q->x<0||q->y<0||q->x+q->w>(int)w||q->y+q->h>(int)h)return false;}return bottom;}\n')
 run([cc,*flags,'-I'+str(old),'-DSCENE_HOST_SOURCE="'+str(host)+'"','-c',str(t/'probe.c'),'-o',str(t/'scene.o')])
 run([cc,*flags,'-I'+str(inc),'-Dt5_driver_get=contexts_text_driver_get','-c',str(SYSTEM/'Services/text_input/host.c'),'-o',str(t/'text.o')])
 w,h=a.geometry.split('x'); fixture=(SYSTEM/'test/scene/host_test.c').read_text().replace('w=240,h=240,','w='+w+',h='+h+',');
 if a.no_navigation:
  fixture=fixture.replace('{"input.navigation",1,&nav},','').replace('driver->start(deps,5)','driver->start(deps,4)')
 (t/'host_test.c').write_text(fixture)
 app_source=APP/'test/contexts/app_test.c'
 if a.pair=='mixed':
  shutil.copyfile(app_source,t/'app_test.c');app_source=t/'app_test.c'
  shared=(APP/'test/contexts/shared_input_fixture.inc').read_text()
  shared=shared.replace('if(tm("checkpoint-enable-unavailable"))return RISC_TEXT_ENTRY_UNAVAILABLE;','/* Pair qualification: invoke the real provider; no unavailable injection. */')
  shared=shared.replace('int rc=text_checkpoint_real->checkpoint_enable(text_real->context,token);','int rc=text_checkpoint_real->checkpoint_enable(text_real->context,token);if(tm("checkpoint-enable-unavailable")){assert(rc==RISC_TEXT_ENTRY_UNAVAILABLE);fprintf(stderr,"real text provider checkpoint_enable returned UNAVAILABLE\\n");}')
  (t/'shared_input_fixture.inc').write_text(shared)
 if a.no_navigation:
  app=(app_source).read_text()
  app=app.replace('static void back_input(void){nav_pressed=RISC_NAV_BACK;}','static void back_input(void){int x,y;assert(contexts_test_header(false,&x,&y));touch_tap(x,y);}')
  app=app.replace('nav_pressed=RISC_NAV_DOWN;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);','pair_swipe();')
  swipe='''static void pair_swipe(void){
 settle();event_at=0;event_count=3;unsigned x=w/2,y=h-50;
 events[0]=(risc_touch_event_v1){++seq,ms,1,0,x,y};
 events[1]=(risc_touch_event_v1){++seq,ms+30,2,0,x,80};
 events[2]=(risc_touch_event_v1){++seq,ms+60,3,0,x,80};
 risc_scene_event_v1 e;for(unsigned i=0;i<8;i++)assert(tick(&e)==RISC_SCENE_IDLE);settle();
}
'''
  app=app.replace('static void tap(unsigned action,int value){',swipe+'static void tap(unsigned action,int value){')
  (t/'app_test.c').write_text(app);app_source=t/'app_test.c'
 if a.catalog:
  raw=(APP/'test/contexts/runtime_catalog_test.c').read_text().replace('#define PORTABLE_CONTEXT_FACE_COUNT 3','').replace('#define PORTABLE_CONTEXT_FACE_NAMES {"ONE","TWO","THREE"}','').replace('int main(int argc,char**argv){','int original_catalog_suite_main(int argc,char**argv){')
  (t/'catalog.c').write_text(raw+'\nextern bool contexts_test_geometry(unsigned,unsigned);\n'+CATALOG_PROBE)
  app_source=t/'catalog.c';a.cases=['boundary96','key-boundaries','smooth-back']
 run([cc,*flags,'-I'+str(t),'-I'+str(inc),'-I'+str(SYSTEM/'Services/text_input'),'-DPORTABLE_CONTEXTS_SHARED_TEXT_INPUT','-DCONTEXTS_APP_SOURCE=\"'+str(APP/'Apps/contexts.c')+'\"','-I'+str(APP/'test/contexts'),'-include',str(FACE),str(app_source),str(t/'scene.o'),str(t/'text.o'),'-o',str(t/'app')])
 frames=t/'frames';frames.mkdir();results=[]
 for case in a.cases:
  try:r=subprocess.run(([str(t/'app'),case] if a.catalog else [str(t/'app'),'watch',str(frames),case]),capture_output=True,text=True,timeout=20,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}); row={'case':case,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr}
  except subprocess.TimeoutExpired as e:row={'case':case,'timeout':True,'stdout':str(e.stdout),'stderr':str(e.stderr)}
  print(json.dumps(row),flush=True);results.append(row)
  for f in frames.iterdir():f.unlink()
 source_hashes={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set([host,SYSTEM/'Services/text_input/host.c',SYSTEM/'Services/text_input/checkpoint.inc',FACE,a.telemetry_font,*APP.glob('Apps/contexts*.inc'),APP/'Apps/contexts.c',*host.parent.glob('*.inc'),*host.parent.glob('fonts/*.inc')]))}
 evidence={'pair':a.pair,'geometry':a.geometry,'navigation_declared':not a.no_navigation,'scene_profile':'full; no explicit -D','sanitized':a.sanitize,'catalog_probe':a.catalog,'source_sha256':source_hashes,'sdk_sha256':{'scene_provider':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(old.glob('*.h'))},'app_and_text_provider':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(inc.glob('*.h'))}},'cases':results,'physical_hardware':False,'xtensa_execution':False}
 name=f'{a.pair}-{a.geometry}-{"catalog-" if a.catalog else ""}{"no-nav-" if a.no_navigation else ""}{"sanitized" if a.sanitize else "normal"}.json'
 (BASE/name).write_text(json.dumps(evidence,indent=2)+'\n')
 if any(row.get('returncode')!=0 for row in results):raise SystemExit(1)
 print(f"PASS {len(results)} real-provider cases; receipt {BASE/name}")
