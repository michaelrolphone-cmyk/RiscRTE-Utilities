"""Pinned inputs and preservation checks for offline shared SDR candidates."""
import hashlib,json,subprocess,sys,zipfile
from pathlib import Path,PurePosixPath
RUNTIME='712ecc3f894dba99d9c9638941778a82f5856b97'
SYSTEM='710b200021b983ea55eb86a6fbddbbf5ce10a984'
BOOTFS='ab9ff33975680c5bb8d9a2e7ed346d0300732a4e'
LANES=('watch','wave','x4')
ADDITIONS=(('ui.canvas.raster',1),('ui.canvas-input',1),('alarm.service.facade',2))
def check(ok,message):
    if not ok:raise ValueError(message)
def sha(b):return hashlib.sha256(b).hexdigest()
def encode(value):return (json.dumps(value,sort_keys=True,indent=2)+'\n').encode()
def safe(name):
    check(isinstance(name,str) and bool(name),'Invalid path')
    p=PurePosixPath(name)
    check(not p.is_absolute() and '..' not in p.parts and '\\' not in name and str(p)==name,'Unsafe path '+name)
    return name
def pin(root,expected):
    check(subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()==expected,'Wrong source '+str(root))
    subprocess.run(['git','-C',str(root),'diff','--exit-code','HEAD','--'],check=True,capture_output=True)
def tree(root):
    check(root.is_dir(),'Missing tree '+str(root))
    check(not any(p.is_symlink() for p in root.rglob('*')),'Symlink in tree')
    return {safe(p.relative_to(root).as_posix()):p.read_bytes() for p in root.rglob('*') if p.is_file()}
def write(root,entries):
    root.mkdir(parents=True,exist_ok=False)
    for n,b in entries.items():
        p=root/safe(n);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
def unzip(path):
    with zipfile.ZipFile(path) as z:
        names=z.namelist();check(len(set(names))==len(names),'Duplicate archive paths')
        for n in names:safe(n.rstrip('/'))
        return {n:z.read(n) for n in names if not n.endswith('/')}
def zip_roundtrip(path,entries):
    with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
        for n,b in sorted(entries.items()):
            i=zipfile.ZipInfo(safe(n),(1980,1,1,0,0,0));i.compress_type=zipfile.ZIP_DEFLATED;i.external_attr=0o100644<<16;z.writestr(i,b)
    check(unzip(path)==entries,'SD roundtrip differs')
def selected(entries):
    boot=json.loads(entries['boot.json']);rows=[]
    for i,r in enumerate(boot['app_capabilities']):
        m=json.loads(entries[r['manifest']])
        if m.get('id')=='waterfall':rows.append((i,r,m))
    check(len(rows)==1,'Expected one Waterfall selection')
    return boot,*rows[0]
def load(args):
    pin(args.system,SYSTEM);pin(args.bootfs,BOOTFS)
    sys.path[:0]=[str(args.system/'scripts'),str(args.bootfs/'scripts')]
    from current_bootfs import build
    from read_only_spiffs import read_image
    from image_envelope import MAGIC,decode_image,encode_image
    lock=json.loads(args.baseline_lock.read_text())
    names={f'product-{l}-{s}' for l in LANES for s in ('store.bin','sd-apps.zip')}
    check(len(lock['assets'])==6 and {r['name'] for r in lock['assets']}==names,'Wrong baseline asset set')
    for r in lock['assets']:
        b=(args.baseline/safe(r['name'])).read_bytes()
        check(len(b)==r['bytes'] and sha(b)==r['sha256'],'Baseline identity differs '+r['name'])
    elf=(args.shared/'waterfall.elf').read_bytes();manifest=(args.shared/'waterfall.json').read_bytes()
    record=json.loads((args.shared/'build.json').read_text());m=json.loads(manifest)
    check(len(elf)==record['bytes'] and sha(elf)==record['sha256'],'Build ELF receipt differs')
    check(sha(manifest)==record['manifest_sha256'] and record['source_pins']['runtime']==RUNTIME,'Build manifest/runtime differs')
    check(m.get('id')=='waterfall' and m.get('file_name')=='waterfall.elf','Wrong shared app')
    check((m.get('display_name'),m.get('icon'),m.get('runtime_lifecycle'))==('Waterfall','solid:f0ec','foreground'),'Shared presentation/lifecycle regression')
    decode=lambda b:decode_image(b) if b.startswith(MAGIC) else b
    lanes={}
    for lane in LANES:
        raw=(args.baseline/f'product-{lane}-store.bin').read_bytes();stored=read_image(raw,len(raw))
        lanes[lane]=(len(raw),stored,{n:decode(b) for n,b in stored.items()},unzip(args.baseline/f'product-{lane}-sd-apps.zip'))
    return elf,manifest,lanes,build,read_image,MAGIC,decode,encode_image
def alarm_migration(before,row):
    raw=[g for g in row['grants'] if g['capability']=='alarm.service']
    check(len(raw)==1 and raw[0] in [
        {'capability':'alarm.service','api':api,'instance_id':0} for api in (1,2)],
        'Unexpected raw alarm authority')
    facade={'capability':'alarm.service.facade','api':2,'instance_id':0}
    owners=[d for d in json.loads(before['boot.json'])['drivers']
            if any(p.get('capability')=='alarm.service.facade' and p.get('api')==2
                   for p in json.loads(before[d['manifest']]).get('provides',[]))]
    check(len(owners)==1 and owners[0].get('instance_id',0)==0,'Alarm facade owner missing or ambiguous')
    check(raw[0] in owners[0].get('bindings',[]),'Alarm facade backend differs from migrated grant')
    return {'reason':'shared-client-uses-facade-only','removed':raw,'added':facade,
            'provider_manifest':owners[0]['manifest'],'backend_binding':raw[0]}
def grants(before,row,manifest):
    declared={(r['capability'],r['api']) for r in manifest.get('requires',[])+manifest.get('optional',[])}
    check(not any(cap=='alarm.service' for cap,api in declared),'Unused raw alarm declaration')
    alarm_migration(before,row)
    result=[g for g in row['grants'] if g['capability']!='alarm.service']
    check(all((g['capability'],g['api']) in declared for g in result),'Legacy grant no longer declared')
    providers={}
    for d in json.loads(before['boot.json'])['drivers']:
        for p in json.loads(before[d['manifest']]).get('provides',[]):
            providers.setdefault((p['capability'],p['api']),[]).append((d['manifest'],d.get('instance_id',0)))
    for cap,api in ADDITIONS:
        check((cap,api) in declared,'Shared capability undeclared')
        owners=providers.get((cap,api),[]);check(len(owners)==1 and owners[0][1]==0,'Shared provider missing or ambiguous '+cap)
        want={'capability':cap,'api':api,'instance_id':0}
        matches=[g for g in result if (g['capability'],g['api'])==(cap,api)]
        if matches:check(matches==[want],'Existing shared grant differs')
        else:result.append(want)
    return result
def paths(row):
    if 'image_source' not in row:return None,None
    image=safe(row['image_source']['path'])
    return image,str(PurePosixPath(image).parent/'manifest.json')
def pack(after,stored,changed,size,build,read_image,magic,decode,encode_image):
    packed={n:stored[n] if n not in changed else encode_image(b) if n in stored and stored[n].startswith(magic) else b for n,b in after.items()}
    image,receipt=build(packed,size)
    check(len(image)==size and read_image(image,size)==packed,'SPIFFS roundtrip differs')
    check({n:decode(b) for n,b in packed.items()}==after,'Canonical envelope roundtrip differs')
    return packed,image,receipt
def arguments(description,extra=()):
    import argparse
    p=argparse.ArgumentParser(description=description)
    for n in ('system','bootfs','baseline','baseline-lock','shared','output',*extra):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args()
    for n,v in vars(a).items():setattr(a,n,v.resolve())
    check(not a.output.exists(),'Output exists')
    return a
