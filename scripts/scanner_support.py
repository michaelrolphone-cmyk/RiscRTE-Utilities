"""Canonical header staging for the shared Scanner; Runtime API wins copies."""
from pathlib import Path
import shutil,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
def sdk(runtime,system,output):
    runtime,system,output=map(Path,(runtime,system,output))
    target=output/'include';target.mkdir(parents=True,exist_ok=True)
    sources={};shadowed=[]
    roots=[runtime/'sdk/app',runtime/'sdk/driver',runtime/'sdk/hardware',system/'sdk/app',system/'sdk/driver',system/'lib/PortableApps/include']
    roots += sorted((ROOT/'lib').glob('*/include'))
    for root in roots:
        for p in sorted(root.glob('*.h')):
            if p.name in sources:
                if sources[p.name].read_bytes()!=p.read_bytes():shadowed.append({'chosen':str(sources[p.name]),'shadowed':str(p)})
                continue
            sources[p.name]=p
    for name,p in sources.items():shutil.copyfile(p,target/name)
    (output/'sdk-source.json').write_text(json.dumps({'files':{n:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for n,p in sources.items()},'precedence':list(map(str,roots)),'shadowed':shadowed},indent=2))
    return target
