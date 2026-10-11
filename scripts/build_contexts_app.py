#!/usr/bin/env python3
"""Compatibility entry point for the shipping declarative Contexts package."""
import argparse
from pathlib import Path
from build_contexts_scene import run
ROOT=Path(__file__).resolve().parents[1]
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True)
 p.add_argument('--profile',choices=('watch','x4-paper'),default='watch')
 p.add_argument('--output',type=Path,default=ROOT/'dist/contexts-scene')
 p.add_argument('--catalog',type=Path);p.add_argument('--face-header',type=Path)
 a=p.parse_args();run(a.runtime.resolve(),a.system_apps.resolve(),a.output.resolve(),a.catalog,'x4' if a.profile=='x4-paper' else 'watch',a.face_header)
