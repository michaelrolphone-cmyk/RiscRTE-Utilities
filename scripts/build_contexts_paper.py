#!/usr/bin/env python3
"""Build the Contexts X4 client; the selected shared scene provider owns paper UI."""
import argparse
from pathlib import Path
from build_contexts_scene import run
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('runtime','system-apps','output'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--catalog',type=Path);a=p.parse_args()
 run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve(),a.catalog,'x4')
