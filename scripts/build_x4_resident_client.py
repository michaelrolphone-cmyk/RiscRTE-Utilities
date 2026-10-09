#!/usr/bin/env python3
"""Selected native paper/RF-only Contexts resident foreground."""
import argparse,json,sys
from pathlib import Path
from build_contexts_paper import FLAGS
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--utilities',type=Path,required=True)
    selected,_=p.parse_known_args();sys.path.insert(0,str(selected.utilities.resolve()/'scripts'))
    import resident_client_build as resident
    resident.options(p);a=p.parse_args();c=resident.prepare(a,p,a.utilities.resolve())
    removed={'PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_PAPER_TRANSITIONS','PORTABLE_X4_IDLE_POLICY','PORTABLE_LOW_BATTERY','PORTABLE_APP_SLEEP_LOCAL'}
    defines=['-D'+flag for flag in FLAGS if flag not in removed]+['-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_HOME_APP="default.elf"','-DPORTABLE_UNPADDED_HOURS']
    profile=json.loads((ROOT/'Apps/contexts-paper.json').read_text())
    grants=[g for g in profile['required_grants'] if g['capability'] not in ('x4.power','storage.volume','net.wifi','bluetooth.hci')]
    record=resident.build(c,ROOT,'contexts','0.1.6',defines,[ROOT/'Apps/contexts.c'],grants,dict(native_time=True,app_source='Apps/contexts.c',source_ancestor='e25442fc1f0a2b452c1c671e7670f5af5ad4dae6',idle_policy='resident-host',contexts='RF-only service; editor draft and launch guards preserved',scrolling='completed-frame paper lists'))
    resident.write(c['out']/'cohort-receipt.json',dict(schema=1,role='foreground',quick_render_total=0,built={'contexts':record},hardware_verified=False,installable=False))
if __name__=='__main__':main()
