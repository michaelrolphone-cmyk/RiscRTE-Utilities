#!/usr/bin/env python3
"""Assemble unscaled production framebuffer captures; Pillow is host tooling."""
import argparse,hashlib,json
from pathlib import Path
from PIL import Image,ImageDraw
p=argparse.ArgumentParser(description=__doc__);p.add_argument('captures',type=Path);a=p.parse_args()
cases=[(100,'Overview'),(101,'Details'),(102,'BLE direct view'),(103,'Read error'),(104,'BLE save error'),(105,'Back failed'),(106,'Charging info'),(107,'Zero SOC'),(108,'100 is not done'),(109,'Unknown SOC')]
cases=[row for row in cases if (a.captures/f'240x240-{row[0]}.ppm').exists()]
sizes=[(240,240),(410,502),(128,128),(320,180)];cw=430
sheet=Image.new('RGB',(cw*len(cases),sum(h+40 for w,h in sizes)),(30,34,40));d=ImageDraw.Draw(sheet);top=0;frames={}
for w,h in sizes:
 for col,(case,label) in enumerate(cases):
  source=a.captures/f'{w}x{h}-{case}.ppm';im=Image.open(source);assert im.size==(w,h)
  d.text((col*cw+8,top+5),f'{w}x{h} {label}',fill='white');sheet.paste(im,(col*cw+8,top+25))
  frames[source.name]=hashlib.sha256(source.read_bytes()).hexdigest()
  im.save(a.captures/f'{w}x{h}-{case}.png')
 top+=h+40
sheet.save(a.captures/'native-contact-sheet.png')
(a.captures/'capture-receipt.json').write_text(json.dumps({'pixels_resized':False,'source':'actual Battery native view and pinned shared native raster','hardware_tested':False,'BLE':'direct state fixture; no color entry is added','frames':frames},indent=2)+'\n')
