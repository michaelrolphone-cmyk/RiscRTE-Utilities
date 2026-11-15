"""Buttons canonical SDK: native Runtime prefixes override legacy copies."""
from pathlib import Path
import hashlib,json,shutil
ROOT=Path(__file__).resolve().parents[1]
def sdk(runtime,system,output):
    runtime,system,output=map(Path,(runtime,system,output));include=output/'include';include.mkdir(parents=True,exist_ok=True)
    roots=[runtime/'sdk/app',runtime/'sdk/driver',runtime/'sdk/hardware',system/'sdk/app',system/'sdk/driver',system/'lib/PortableApps/include',*sorted((ROOT/'lib').glob('*/include'))]
    chosen={};shadowed=[]
    for root in roots:
        for path in sorted(root.glob('*.h')):
            if path.name not in chosen:chosen[path.name]=path
            elif chosen[path.name].read_bytes()!=path.read_bytes():shadowed.append({'chosen':str(chosen[path.name]),'shadowed':str(path)})
    for name,path in chosen.items():shutil.copyfile(path,include/name)
    (output/'sdk-source.json').write_text(json.dumps({'precedence':list(map(str,roots)),'files':{name:{'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()} for name,path in chosen.items()},'shadowed':shadowed},indent=2)+'\n')
    return include
