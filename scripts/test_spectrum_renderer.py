#!/usr/bin/env python3
"""Actual app/adapter regression, deterministic input, real RGB565 frame bounds."""
import argparse,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
scenes={'spec':[(3,120,120)],'fall':[(3,120,120),(60,84,26)],'monitor':[(3,120,120),(60,136,26)],'controls':[(3,120,120),(60,225,26)],'cursor':[(3,120,120),(60,100,120),(61,140,120),(62,180,120)],'nested-back':[(3,120,120),(60,225,26),(90,-1,1),(110,84,26),(180,-1,1)],'touch-exit':[(3,120,120),(60,190,232),(90,190,232)],'exit-retry':[(3,120,120),(60,190,232),(90,190,232),(120,190,232)],'controls-scroll':[(3,120,120),(60,225,26),(80,150,175),(81,150,75),(100,150,175),(101,150,75)],'keyboard':[(3,120,120),(30,136,26),(35,190,62),(40,70,103),(45,120,120)]}
sample_menu=[(3,120,120),(40,225,26),(60,150,175),(61,150,65),(70,150,175),(71,150,65),(80,150,175),(81,150,65),(90,150,175),(91,150,65),(100,178,166)]
room_edit=sample_menu+[(120,72,208),(140,100,89),(160,52,84),(170,79,84),(180,190,190)]
event_edit=sample_menu+[(120,150,208),(140,100,89),(160,52,84),(170,79,84),(180,190,190)]
scenes.update({'samples-keyboard':room_edit[:-1], 'samples-room':room_edit+[(200,70,165),(360,120,60)], 'samples-event':event_edit+[(200,70,165)], 'samples-pause':room_edit+[(200,70,165),(230,50,125)], 'samples-manual':sample_menu+[(120,200,120)], 'samples-ambiguous':sample_menu+[(120,120,60)]})
for sanitized in (False,True):
 for name,actions in scenes.items():
  out=ROOT/'build/spectrum-renderer'/str(int(sanitized))/name;out.mkdir(parents=True,exist_ok=True)
  # These scene directories are generated fixtures owned by this script.
  for stale in out.glob('frame-*.ppm'):stale.unlink()
  path=out/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions))
  env=dict(os.environ)
  if name.startswith('samples-'):env['SPECTRUM_CAPTURE_POLLS']='560'
  if name=='samples-room':env['SPECTRUM_EXPECT_SAMPLE']='1'
  if name=='samples-event':env['SPECTRUM_EXPECT_SAMPLE']='2'
  if name=='samples-manual':env['SPECTRUM_SAMPLE_FIXTURE']='1'
  if name=='samples-ambiguous':env.update(SPECTRUM_SAMPLE_FIXTURE='2',SPECTRUM_EXPECT_NO_WRITES='1')
  if name in ('controls-scroll','keyboard','samples-keyboard','samples-pause'):env['SPECTRUM_EXPECT_NO_WRITES']='1'
  if name=='nested-back':env['SPECTRUM_EXIT_POLL']='180'
  elif name=='touch-exit':env['SPECTRUM_EXIT_POLL']='91'
  elif name=='exit-retry':env.update(SPECTRUM_EXIT_POLL='121',SPECTRUM_LAUNCH_REFUSE_ONCE='1')
  subprocess.run([sys.executable,str(ROOT/'scripts/capture_spectrum.py'),'--system-apps',str(a.system_apps.resolve()),'--output',str(out),'--actions',str(path),*(['--sanitize'] if sanitized else [])],check=True,env=env,timeout=120)
  frames=sorted(out.glob('frame-*.ppm'));assert len(frames)>=(3 if name in ('touch-exit','exit-retry') else 10),(name,'missing rendered frames')
  raw=frames[-1].read_bytes();header=b'P6\n240 240\n255\n';assert raw.startswith(header) and len(raw)==len(header)+240*240*3
  pixels=raw[len(header):]
  def rgb(x,y):return tuple(pixels[(y*240+x)*3:(y*240+x+1)*3])
  if name in ('spec','fall','monitor'):
   x={'spec':30,'fall':74,'monitor':119}[name];r,g,b=rgb(x,17);assert g>150 and b>150,(name,'tab is not active',rgb(x,17))
  if name=='controls-scroll':
   baseline=sorted((out.parent/'controls').glob('frame-*.ppm'))[-1].read_bytes()[len(header):]
   assert pixels[:48*240*3]==baseline[:48*240*3], 'scrolled content changed fixed header'
   assert pixels[194*240*3:]==baseline[194*240*3:], 'scrolled content changed fixed footer'
   assert pixels[48*240*3:194*240*3]!=baseline[48*240*3:194*240*3], 'controls did not scroll continuously'
  if name in ('keyboard','samples-keyboard'):
   assert any(rgb(12+k*27,76)[1]>150 and rgb(12+k*27,76)[2]>150 for k in range(8)), 'standard keyboard selected cell missing'
   assert rgb(12,148)!=(0,0,0), 'fourth key row missing'
  if name=='cursor':assert any(min(rgb(180,y))>200 for y in range(80,195)), 'drag did not create visible cursor'
print('Real Spectrum renderer: sixteen scenes x normal/ASan+UBSan; saved room/event samples, paused collection, manual/ambiguous rooms; active SPEC/FALL/MONITOR tabs, standard keyboard, continuous controls scroll/fixed chrome, cursor drag, nested hardware Back, root touch return/refusal/retry, stride and grant cleanup passed')
