#!/usr/bin/env python3
"""Real app/adapter temporal screens, synthetic PCM only; no microphone access."""
import argparse,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
menu=[(3,120,120),(40,225,26)]
for n in (60,70,80,90,100):menu +=[(n,150,175),(n+1,150,65)]
menu += [(130,178,166)]
name=menu+[(160,120,204),(180,90,54),(200,52,84),(210,79,84)]
saved=name+[(220,190,190),(240,200,54)]
record=saved+[(430,60,120)]
scenes={
 'monitor-learn-record-save': ([(3,120,120),(60,136,26),(80,152,222),(100,120,202),(120,52,84),(130,79,84),(140,190,190),(160,60,120),(550,60,193)],580,{'SPECTRUM_TEMPORAL_SIGNAL':'1','SPECTRUM_EXPECT_EVENT_EXAMPLES':'1','SPECTRUM_EXPECT_EVENT_WRITES':'2'}),
 'live-idle-empty-recovery': ([(3,120,120)],9000,{'SPECTRUM_CONTINUOUS_TEST':'1'}),
 'events-empty':(menu,260,{}),
 'events-keyboard':(name,220,{}),
 'events-label':(name+[(220,190,190)],240,{}),
 'events-review':(record,540,{'SPECTRUM_TEMPORAL_SIGNAL':'1','SPECTRUM_EXPECT_EVENT_EXAMPLES':'0','SPECTRUM_EXPECT_EVENT_WRITES':'1'}),
 'events-saved':(record+[(550,60,193)],580,{'SPECTRUM_TEMPORAL_SIGNAL':'1','SPECTRUM_EXPECT_EVENT_EXAMPLES':'1','SPECTRUM_EXPECT_EVENT_WRITES':'2'}),
 'events-clipped':(record,740,{'SPECTRUM_TEMPORAL_LONG':'1','SPECTRUM_EXPECT_EVENT_EXAMPLES':'0','SPECTRUM_EXPECT_EVENT_WRITES':'1'}),
 'events-examples':(menu+[(155,70,78),(180,60,218)],240,{'SPECTRUM_TEMPORAL_FIXTURE':'normal','SPECTRUM_EXPECT_EVENT_EXAMPLES':'3','SPECTRUM_EXPECT_EVENT_WRITES':'0'}),
 'events-full':(menu+[(155,70,78),(180,60,120)],230,{'SPECTRUM_TEMPORAL_FIXTURE':'full','SPECTRUM_EXPECT_EVENT_EXAMPLES':'6','SPECTRUM_EXPECT_EVENT_WRITES':'0'}),
 'events-no-space':(saved+[(265,175,218)],290,{'SPECTRUM_EVENT_STORAGE':'full','SPECTRUM_EXPECT_EVENT_WRITES':'1'}),
 'events-unknown-retry':(saved+[(265,175,218),(285,55,203)],310,{'SPECTRUM_EVENT_STORAGE':'unknown','SPECTRUM_EXPECT_EVENT_EXAMPLES':'0','SPECTRUM_EXPECT_EVENT_WRITES':'1'}),
 'events-retained':(saved,300,{'SPECTRUM_EVENT_STORAGE':'retained','SPECTRUM_EXPECT_EVENT_WRITES':'1'}),
 'events-stopped-before-onset':(record+[(450,50,202)],500,{'SPECTRUM_EXPECT_EVENT_EXAMPLES':'0','SPECTRUM_EXPECT_EVENT_WRITES':'1'}),
 'events-whole-review':(record+[(495,50,202),(515,120,163)],530,{'SPECTRUM_TEMPORAL_SIGNAL':'1','SPECTRUM_EXPECT_EVENT_EXAMPLES':'0','SPECTRUM_EXPECT_EVENT_WRITES':'1'}),
 'events-whole-saved':(record+[(495,50,202),(515,120,163),(535,50,202)],575,{'SPECTRUM_TEMPORAL_SIGNAL':'1','SPECTRUM_EXPECT_EVENT_EXAMPLES':'1','SPECTRUM_EXPECT_EVENT_WRITES':'2','SPECTRUM_EXPECT_CONFIRMED_END':'1'}),
 'events-match':(record+[(550,60,193),(625,176,219),(630,40,232),(635,120,214)],910,{'SPECTRUM_TEMPORAL_SIGNAL':'1','SPECTRUM_EXPECT_EVENT_EXAMPLES':'1','SPECTRUM_EXPECT_EVENT_WRITES':'2'}),
}
for sanitized in (False,True):
 for name,(actions,polls,options) in scenes.items():
  out=ROOT/'build/temporal-renderer'/str(int(sanitized))/name;out.mkdir(parents=True,exist_ok=True)
  for stale in out.glob('frame-*.ppm'):stale.unlink()
  path=out/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions))
  env=dict(os.environ,SPECTRUM_CAPTURE_POLLS=str(polls),SPECTRUM_KEEP_LAST_FRAME='1',**options)
  subprocess.run([sys.executable,str(ROOT/'scripts/capture_spectrum.py'),'--system-apps',str(a.system_apps.resolve()),'--output',str(out),'--actions',str(path),*(['--sanitize'] if sanitized else [])],check=True,env=env,timeout=120)
  frames=list(out.glob('frame-*.ppm'));assert len(frames)==1
  data=frames[0].read_bytes();header=b'P6\n240 240\n255\n';assert data.startswith(header) and len(data)==len(header)+240*240*3
  pixels=data[len(header):]
  assert any(pixels),name
  if name=='events-match':
   green=sum(1 for y in range(175,195) for x in range(10,228) if pixels[(y*240+x)*3+1]>180 and pixels[(y*240+x)*3]<150 and pixels[(y*240+x)*3+2]<200)
   assert green>15, 'recognized temporal event overlay missing'
  if name=='events-keyboard':
   def rgb(x,y):return tuple(pixels[(y*240+x)*3:(y*240+x+1)*3])
   assert any(rgb(12+k*27,76)[1]>150 for k in range(8))
print('Temporal Nova:17 actual-app/adapter scenes x normal/ASan+UBSan, live synthetic onset/capture/review/save, standard keyboard, examples/capacity/full/unknown-retry/retained, cleanup and raster bounds passed')
