#!/usr/bin/env python3
"""Compact only provider metadata; Viewer remains the complete ELF."""
import argparse,json,pathlib,sys,subprocess
p=argparse.ArgumentParser();p.add_argument('--system',type=pathlib.Path,required=True);p.add_argument('--provider',type=pathlib.Path,required=True);a=p.parse_args()
sys.path.insert(0,str(a.system.resolve()/'scripts'))
from compact_current_elf import compact
from scene_build import compiler_path
cc=compiler_path();rows={}
for folder in ('canvas-host','repeat'):
 d=a.provider/folder
 rows[folder]=compact(d/'driver.elf',cc,d/'driver.full.elf')
subprocess.run([sys.executable,str(a.system/'scripts/test_files_elf_compaction.py'),'--full',str(a.provider/'canvas-host/driver.full.elf'),'--compact',str(a.provider/'canvas-host/driver.elf'),'--cc',str(cc)],check=True)
if (a.provider/'canvas-host/driver.elf').read_bytes()!=(a.provider/'repeat/driver.elf').read_bytes():raise ValueError('Provider compaction repeat differs')
(a.provider/'compaction.json').write_text(json.dumps(rows,indent=2)+'\n')
