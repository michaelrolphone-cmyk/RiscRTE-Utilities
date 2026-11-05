#!/usr/bin/env python3
"""Materialize local logical-canvas sleep from the exact installed policy."""
import argparse, hashlib, json
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("--watch",required=True,type=Path)
p.add_argument("--output",required=True,type=Path)
p.add_argument("--recipe",type=Path,default=Path(__file__).with_name("sleep_recipe.json"))
a=p.parse_args()
recipe=json.loads(a.recipe.read_text())
for name, entry in recipe["sources"].items():
 data=(a.watch/entry["path"]).read_bytes()
 digest=hashlib.sha1(b"blob "+str(len(data)).encode()+b"\0"+data).hexdigest()
 if digest!=entry["blob"]: raise ValueError("Sleep baseline mismatch: "+name)
 text=data.decode()
 for r in entry["rules"]:
  if text.count(r["old"])!=1: raise ValueError("Ambiguous sleep anchor: "+name)
  text=text.replace(r["old"],r["replacement"],1)
 (a.output/name).write_text(text)
adapter=a.output/"adapter.c"
text=adapter.read_text()
if "bool portable_sdr_sleep_snapshot(" in text: raise ValueError("Sleep stage already applied")
adapter.write_text(text+recipe["append_adapter"])
