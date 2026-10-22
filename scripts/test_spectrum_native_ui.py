#!/usr/bin/env python3
"""Native-pixel Audio Spectrum through its actual adapter and shared hosts."""
import argparse, importlib.util, json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('shared',ROOT/'scripts/test_spectrum_shared_input.py')
shared=importlib.util.module_from_spec(spec);spec.loader.exec_module(shared)
p=argparse.ArgumentParser()
for name in ('system-apps','runtime','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--recipe',type=Path)
p.add_argument('--watch-profile',type=Path)
p.add_argument('--profile',action='append',default=[])
p.add_argument('--normal-only',action='store_true')
p.add_argument('--scenario',action='append',default=[])
a=p.parse_args()
if not a.recipe:
 if not a.watch_profile:p.error('--recipe or --watch-profile is required')
 profile=json.loads((ROOT/'test/spectrum_shared_input/policy-profile.json').read_text())
 recipe_dir=a.output.resolve()/'profile';(recipe_dir/'build').mkdir(parents=True,exist_ok=True)
 profile['defines']=[x.replace('@WATCH@',str(a.watch_profile.resolve())).replace('@OUTPUT@',str(recipe_dir)) for x in profile['defines']]
 a.recipe=recipe_dir/'profile.json';a.recipe.write_text(json.dumps({'audio_spectrum':profile},indent=2)+'\n')
 (recipe_dir/'build/watch-alarm-profile.h').write_text('#include "AlarmServiceV1.h"\n')
 (recipe_dir/'build/empty_catalog.c').write_bytes((ROOT/'test/spectrum_shared_input/catalog.c').read_bytes())
def cases(w,h):
 margin=min(12,min(w,h)//40);gap=min(6,min(w,h)//80);head=min(44,h//4);foot=min(44,h//4)
 c=(margin,head+gap,w-2*margin,h-head-foot-2*gap)
 def row(r,i,n):
  x,y,rw,rh=r;usable=rh-gap*(n-1);start=usable*i//n;end=usable*(i+1)//n
  return (x,y+start+gap*i,rw,end-start)
 def center(r):return(r[0]+r[2]//2,r[1]+r[3]//2)
 def col(r,i,n):
  x,y,rw,rh=r;usable=rw-gap*(n-1);start=usable*i//n;end=usable*(i+1)//n
  return (x+start+gap*i,y,end-start,rh)
 footer=(0,h-foot,w,foot)
 def tap(x,y,delay=160):return shared.tap(x,y,delay=delay)
 def check(k,v=0):return[(220,'app','verify',k,v)]
 def capture():return[(220,'app','capture',9,0)]
 def scene(i):return[(160,'app','scene',i,0)]
 nav=shared.nav
 result={}
 result['native-pages']=(sum((scene(i)+check(6)+capture() for i in range(18)),[])+nav('home'),{'SPECTRUM_EXPECT_OPENS':'0'})
 start=center(row(c,1,3));tab_w=(w-3*gap)//4;fall_x=(w-3*gap)//4+gap+tab_w//2
 controls_x=w-tab_w//2;toast_y=h-foot-22
 # Numeric history remains finite while the native plot follows display size.
 actions=tap(*start)+[(700,'app','idle',0,0)]+check(5)+check(1,0)+capture()
 actions+=tap(fall_x,head//2)+check(1,1)+capture()
 actions+=tap(tab_w//2,head//2)+check(1,0)
 actions+=tap(c[0],c[1]+c[3]//2)+check(2)
 actions+=[(160,'app','down',c[0],c[1]+c[3]//2),(30,'app','move',c[0]+c[2]-1,c[1]+c[3]//2),(30,'app','up',c[0]+c[2]-1,c[1]+c[3]//2)]+check(3)+capture()
 actions+=check(7)+tap(w//2,toast_y)+check(8)+nav('home')
 result['native-plot-history-toast']=(actions,{'SPECTRUM_EXPECT_OPENS':'0'})
 actions=tap(*start)+tap(controls_x,head//2)+check(0,1)
 # A real held-contact drag, not repeated taps or direct controller calls.
 actions+= [(160,'app','down',w//2,c[1]+c[3]-10),(30,'app','move',w//2,c[1]+10),(30,'app','up',w//2,c[1]+10)]+check(9)+capture()+nav('home')
 result['native-controls-scroll']=(actions,{'SPECTRUM_EXPECT_OPENS':'0'})
 delayed=list(result['native-plot-history-toast'][0]);first=delayed[0];delayed[0]=(first[0],'visible',*first[2:])
 result['native-plot-delayed']=(delayed,{'SPECTRUM_EXPECT_OPENS':'0','SPECTRUM_APP_DELAY_MS':'450'})
 result['native-repeated-entry']=(tap(*start)+[(700,'app','idle',0,0)]+check(5)+nav('home'),{'SPECTRUM_REPEAT_ENTRY':'1','SPECTRUM_EXPECT_OPENS':'0'})
 # Full/wrapped history is a separate boundary from the short live captures
 # above. Do not claim these seeded synthetic frames reproduce a device crash.
 for ring in (0,227):
  transitions=tap(*start)+[(700,'app','idle',0,0)]+check(10,ring)
  for _ in range(12):
   transitions+=tap(fall_x,head//2)+check(11,1)
   transitions+=tap(tab_w//2,head//2)+check(11,0)
  transitions+=tap(fall_x,head//2)+check(11,1)+capture()+nav('home')
  result[f'native-full-history-switch-{ring}']=(transitions,{'SPECTRUM_EXPECT_OPENS':'0','SPECTRUM_EXPECT_KV_WRITES':'0','SPECTRUM_EXPECT_DATA_WRITES':'0'})
  result[f'native-full-history-delayed-{ring}']=(transitions,{'SPECTRUM_EXPECT_OPENS':'0','SPECTRUM_EXPECT_KV_WRITES':'0','SPECTRUM_EXPECT_DATA_WRITES':'0','SPECTRUM_APP_DELAY_MS':'450'})

 names=[center(row(c,1,3)),center(row(c,1,4)),center(row(c,0,5)),center(row(c,1,4))]
 # Event-name and shared-name controls have their own native row partitions;
 # explicit positions below are checked against the renderer's target geometry.
 names[2]=center(row(c,0,5))
 pages=[2,5,7,11]
 done=shared.key('\x03',220)
 save_at=[center(col(footer,1,2)),center(col(footer,0,2)),center(col(row(c,0,5),3,4)),center(row(c,2,4))]
 for editor,title in enumerate(('frequency','signature','temporal','shared')):
  edit=tap(*names[editor]);begin=shared.setup(editor)+edit
  def add(suffix,actions,draft='',opens=1,**env):
   result['native-'+title+'-'+suffix]=(actions,{'SPECTRUM_EXPECT_DRAFT':draft,'SPECTRUM_EXPECT_OPENS':str(opens),'SPECTRUM_EXPECT_KV_WRITES':'0','SPECTRUM_EXPECT_DATA_WRITES':'0','SPECTRUM_EXPECT_EDITOR_PAGE':str(pages[editor]),**{k:str(v) for k,v in env.items()}})
  add('cancel',begin+shared.key('Q')+nav('back','text')+shared.check())
  add('done',begin+shared.key('Q')+done+shared.check(),'Q')
  add('reopen',begin+shared.key('Q')+done+edit+shared.key('W')+nav('back','text')+edit+shared.key('E')+done+shared.check(),'QE',opens=3,SPECTRUM_EXPECT_REOPEN=1)
  add('limit',begin+shared.key('Q')*19+done+shared.check()+edit+nav('back','text'),'Q'*16,opens=2)
  add('delayed',begin+shared.key('Q')+done+shared.check(),'Q',SPECTRUM_TEST_DELAY_MS=3000,SPECTRUM_EXPECT_CLOSE_AGAIN=1)
  add('no-present',begin+shared.key('Q')*4+done+shared.check(),'Q'*4,SPECTRUM_NO_PRESENT=1)
  for ending,terminal in [('done',done),('back',nav('back','text')),('home',nav('home','text'))]:
   actions=tap(*start)+begin+shared.key('Q')*8+terminal
   if ending!='home':actions+=shared.check()
   add('live-delayed-'+ending,actions,'Q'*8 if ending=='done' else '',SPECTRUM_TEST_DELAY_MS=3000,SPECTRUM_EXPECT_PROGRESS=1,SPECTRUM_EXPECT_CLOSE_AGAIN=1,SPECTRUM_EXPECT_CLOSE_PROGRESS=1)
  actions=begin+shared.key('Q')+done+shared.check()+tap(*save_at[editor])+shared.check(1,delay=100)
  env={}
  if editor==0:env['SPECTRUM_EXPECT_KV_WRITES']=1
  elif editor==2:env['SPECTRUM_EXPECT_DATA_WRITES']=1
  elif editor==1:actions+=tap(*center(col(row(c,2,5),1,2)))
  else:actions+=tap(*center(footer))
  add('explicit-save',actions,'Q',**env)
 base=shared.setup(0)+tap(*names[0])
 for name in ('missing-host','old-host','unavailable-open'):
  result[name]=(base+shared.check(2),{'SPECTRUM_EXPECT_DRAFT':'','SPECTRUM_EXPECT_OPENS':str(name=='unavailable-open' and 1 or 0),'SPECTRUM_EXPECT_KV_WRITES':'0','SPECTRUM_EXPECT_DATA_WRITES':'0'})
 for name in ('retained','loss-close'):
  result[name]=(tap(*start)+base+shared.key('Q')+nav('back','text'),{})
 for name in ('loss-open','loss-poll'):
  result[name]=(tap(*start)+base,{})
 begin=base+shared.key('Q')
 for suffix,end in [('done',done+shared.check()),('cancel',nav('back','text')+shared.check()),('home',nav('home','text'))]:
  result['policy-stopped-'+suffix]=(begin+[(65000,'text','idle',0,0)]+end,{'SPECTRUM_POLICY_PROBE':'1','SPECTRUM_POLICY_STOPPED':'1','SPECTRUM_EXPECT_DRAFT':'Q' if suffix=='done' else '','SPECTRUM_EXPECT_KV_WRITES':'0','SPECTRUM_EXPECT_DATA_WRITES':'0'})
 result['policy-active']=(tap(*start)+begin+[(65000,'text','idle',0,0)]+done+shared.check(),{'SPECTRUM_POLICY_PROBE':'1','SPECTRUM_EXPECT_DRAFT':'Q','SPECTRUM_EXPECT_PROGRESS':'1'})
 result['policy-unavailable']=(shared.setup(0,1)+tap(*names[0])+shared.check(2),{'SPECTRUM_POLICY_UNAVAILABLE':'1','SPECTRUM_EXPECT_DRAFT':'Seed','SPECTRUM_EXPECT_OPENS':'1'})
 result['policy-retained']=(begin,{})
 result['policy-missing-suffix']=(shared.setup(0,1)+tap(*names[0])+shared.check(2),{'SPECTRUM_POLICY_MISSING_SUFFIX':'1','SPECTRUM_EXPECT_DRAFT':'Seed','SPECTRUM_EXPECT_OPENS':'1'})
 result['policy-paused-home']=(begin,{'SPECTRUM_POLICY_PAUSED_HOME':'1','SPECTRUM_EXPECT_DRAFT':''})
 result['full-contexts-startup']=(nav('home'),{'SPECTRUM_FULL_CONTEXTS':'1'})
 return result
for profile in a.profile or ['240x240','410x502','502x410','480x800','800x480']:
 w,h=map(int,profile.split('x'));assert w>=160 and h>=160
 selected=cases(w,h)
 if a.scenario:selected={k:v for k,v in selected.items() if k in a.scenario}
 assert selected
 shared.scenarios=lambda:selected
 args=argparse.Namespace(system_apps=a.system_apps,runtime=a.runtime,recipe=a.recipe,output=a.output/profile,baseline_utilities=ROOT,native_height=False,native_ui=True,width=w,height=h,normal_only=a.normal_only,extra_define=['PORTABLE_TEXT_INPUT_POLICY'],scenario=list(selected))
 shared.run(args)
print('Native Spectrum actual-adapter qualification completed')
