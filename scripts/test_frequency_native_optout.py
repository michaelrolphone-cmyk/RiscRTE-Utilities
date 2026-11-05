#!/usr/bin/env python3
"""Exact pre-native Frequency application tokens in selector-off compositions."""
import argparse,hashlib,json,os,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE='95f39010ed89d76fc583f1c09a111cfc3cf21f41'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system-apps',type=Path,required=True);a=p.parse_args();system=a.system_apps.resolve()
out=ROOT/'build/frequency-native-optout';out.mkdir(parents=True,exist_ok=True)
old=out/'frequency_generator.c';old.write_bytes(subprocess.check_output(['git','show',BASE+':Apps/frequency_generator.c'],cwd=ROOT))
profiles=[('legacy',[]),('nova',['PORTABLE_NOVA_UI']),('native-canvas',['PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS']),('native-height',['PORTABLE_NOVA_UI','PORTABLE_WATCH_NATIVE_HEIGHT']),('paper',['PORTABLE_NOVA_UI','PORTABLE_PAPER_UTILITIES','PORTABLE_PAPER_PREFERENCES']),('paper-legacy',['PORTABLE_PAPER_PREFERENCES'])]
rows=[]
for name,flags in profiles:
 for alarm in (False,True):
  for snapshot in (False,True):
   defs=flags+(['PORTABLE_ALARM_CLIENT'] if alarm else [])+(['PORTABLE_RASTER_SNAPSHOT'] if snapshot else [])
   texts=[]
   for source in (old,ROOT/'Apps/frequency_generator.c'):
    data=subprocess.check_output([os.environ.get('CC','cc'),'-E','-P',*['-D'+v for v in defs],*['-I'+str(v) for v in (ROOT/'Apps',ROOT/'lib/Alarm/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include')],str(source)],text=True)
    texts.append('\n'.join(re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_0-9]+|[^\s]''',data)))
   assert texts[0]==texts[1],(name,alarm,snapshot)
   assert 'portable_frequency_native' not in texts[1]
   rows.append({'profile':name,'alarm':alarm,'snapshot':snapshot,'identical':True,'sha256':hashlib.sha256(texts[0].encode()).hexdigest()})
(out/'receipt.json').write_text(json.dumps({'baseline':BASE,'rows':rows},indent=2)+'\n')
print(f'PASS: {len(rows)} Frequency selector-off legacy/NOVA/paper token comparisons')
