#!/usr/bin/env python3
"""Run Contexts through its shipping app_main and the shared renderer on both profiles."""
import argparse
from pathlib import Path
from test_contexts_scene import run
ROOT=Path(__file__).resolve().parents[1]
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True)
 p.add_argument('--frames',type=Path,default=ROOT/'build/contexts-scene')
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.frames.resolve())
