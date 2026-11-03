#!/usr/bin/env python3
"""Apply the SDR-only lifecycle stage after materialize_adapter.py.
Baseline includes are hash-checked; every replacement must match exactly once.
No networking or publication. Owning CI compiles the generated adapter.
"""
import argparse, hashlib, json
from pathlib import Path
def materialize(system, output, recipe):
    config=json.loads(recipe.read_text())
    src=system/"lib/PortableApps/src"
    for name, rules in config["rules"].items():
        original=(src/name).read_bytes()
        digest=hashlib.sha1(b"blob "+str(len(original)).encode()+b"\0"+original).hexdigest()
        if digest!=config["blobs"][name]: raise ValueError("Baseline mismatch: "+name)
        target=output/name
        text=target.read_text() if target.exists() else original.decode()
        for rule in rules:
            if text.count(rule["old"])!=1: raise ValueError("Ambiguous lifecycle anchor in "+name+": "+rule["old"][:100])
            text=text.replace(rule["old"],rule["replacement"],1)
        if name=="adapter.c": text+=config["append_adapter"]
        target.write_text(text)
    for name in config["generated"]: (output/name).write_text((recipe.parent/name).read_text())
if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--system",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    p.add_argument("--recipe",type=Path,default=Path(__file__).with_name("lifecycle_recipe.json"))
    a=p.parse_args()
    materialize(a.system,a.output,a.recipe)
