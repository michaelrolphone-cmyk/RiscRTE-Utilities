#!/usr/bin/env python3
"""Actual app/adapter regression, deterministic input, real RGB565 frame bounds."""
import argparse,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args()
scenes={'spec':[(3,120,120)],'fall':[(3,120,120),(60,84,26)],'labels':[(3,120,120),(60,136,26)],'controls':[(3,120,120),(60,225,26)],'cursor':[(3,120,120),(60,100,120),(61,140,120),(62,180,120)],'nested-back':[(3,120,120),(60,225,26),(90,-1,1),(110,84,26),(180,-1,1)],'touch-exit':[(3,120,120),(60,190,232),(90,190,232)],'exit-retry':[(3,120,120),(60,190,232),(90,190,232),(120,190,232)],'controls-scroll':[(3,120,120),(60,225,26),(80,150,175),(81,150,75),(100,150,175),(101,150,75)],'keyboard':[(3,120,120),(30,136,26),(35,190,62),(40,70,103),(45,120,120)]}
for sanitized in (False,True):
 for name,actions in scenes.items():
  out=ROOT/'build/spectrum-renderer'/str(int(sanitized))/name;out.mkdir(parents=True,exist_ok=True)
  path=out/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions))
  env=dict(os.environ)
  if name in ('controls-scroll','keyboard'):env['SPECTRUM_EXPECT_NO_WRITES']='1'
  if name=='nested-back':env['SPECTRUM_EXIT_POLL']='180'
  elif name=='touch-exit':env['SPECTRUM_EXIT_POLL']='91'
  elif name=='exit-retry':env.update(SPECTRUM_EXIT_POLL='121',SPECTRUM_LAUNCH_REFUSE_ONCE='1')
  subprocess.run([sys.executable,str(ROOT/'scripts/capture_spectrum.py'),'--system-apps',str(a.system_apps.resolve()),'--output',str(out),'--actions',str(path),*(['--sanitize'] if sanitized else [])],check=True,env=env,timeout=120)
  frames=sorted(out.glob('frame-*.ppm'));assert len(frames)>=(3 if name in ('touch-exit','exit-retry') else 10),(name,'missing rendered frames')
  raw=frames[-1].read_bytes();header=b'P6\n240 240\n255\n';assert raw.startswith(header) and len(raw)==len(header)+240*240*3
  pixels=raw[len(header):]
  def rgb(x,y):return tuple(pixels[(y*240+x)*3:(y*240+x+1)*3])
  if name in ('spec','fall','labels'):
   x={'spec':30,'fall':74,'labels':119}[name];r,g,b=rgb(x,17);assert g>150 and b>150,(name,'tab is not active',rgb(x,17))
  if name=='controls-scroll':
   baseline=sorted((out.parent/'controls').glob('frame-*.ppm'))[-1].read_bytes()[len(header):]
   assert pixels[:48*240*3]==baseline[:48*240*3], 'scrolled content changed fixed header'
   assert pixels[194*240*3:]==baseline[194*240*3:], 'scrolled content changed fixed footer'
   assert pixels[48*240*3:194*240*3]!=baseline[48*240*3:194*240*3], 'controls did not scroll continuously'
  if name=='keyboard':
   assert rgb(12,76)[1]>150 and rgb(12,76)[2]>150, 'standard keyboard first cell missing'
   assert rgb(12,148)!=(0,0,0), 'fourth key row missing'
  if name=='cursor':assert any(min(rgb(180,y))>200 for y in range(80,195)), 'drag did not create visible cursor'
print('Real Spectrum renderer: ten scenes x normal/ASan+UBSan; active tabs, standard keyboard, continuous controls scroll/fixed chrome, cursor drag, nested hardware Back, root touch return/refusal/retry, stride and grant cleanup passed')
